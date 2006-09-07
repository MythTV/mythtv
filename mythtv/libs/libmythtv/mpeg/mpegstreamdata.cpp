// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "mpegstreamdata.h"
#include "mpegtables.h"
#include "RingBuffer.h"
#include "mpegtables.h"

void init_sections(sections_t &sect, uint last_section)
{
    static const unsigned char init_bits[8] =
        { 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00, };

    sect.clear();

    uint endz = last_section >> 3;
    if (endz)
        sect.resize(endz, 0x00);
    sect.resize(33 - endz, 0xff);
    sect[endz] = init_bits[last_section & 0x7];

#if 0
    cerr<<"init_sections ls("<<last_section<<"): "<<hex;
    for (uint i = 0 ; i < 32; i++)
        cerr<<((int)sect[i])<<" ";
    cerr<<dec<<endl;
#endif
}

const unsigned char MPEGStreamData::bit_sel[8] =
{
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
};

/** \class MPEGStreamData
 *  \brief Encapsulates data about MPEG stream and emits events for each table.
 */

/** \fn MPEGStreamData::MPEGStreamData(int, bool)
 *  \brief Initializes MPEGStreamData.
 *
 *   This adds the PID of the PAT table to "_pids_listening"
 *
 *  \param desiredProgram If you want rewritten PAT and PMTs, for
 *                        a desired program set this to a value > -1
 *  \param cacheTables    If true PAT and PMT tables will be cached
 */
MPEGStreamData::MPEGStreamData(int desiredProgram, bool cacheTables)
    : _have_CRC_bug(false), _eit_helper(NULL), _eit_rate(0.0f),
      _listener_lock(true),
      _cache_tables(cacheTables), _cache_lock(true),
      // Single program stuff
      _desired_program(desiredProgram),
      _pid_video_single_program(0xffffffff),
      _pid_pmt_single_program(0xffffffff),      
      _pmt_single_program_num_video(1),
      _pmt_single_program_num_audio(0),
      _pat_single_program(NULL), _pmt_single_program(NULL),
      _invalid_pat_seen(false), _invalid_pat_warning(false)
{
    AddListeningPID(MPEG_PAT_PID);
}

MPEGStreamData::~MPEGStreamData()
{
    Reset(-1);
    SetPATSingleProgram(NULL);
    SetPMTSingleProgram(NULL);

    // Delete any cached tables that haven't been returned
    psip_refcnt_map_t::iterator it = _cached_slated_for_deletion.begin();
    for (; it != _cached_slated_for_deletion.end(); ++it)
        delete it.key();

    QMutexLocker locker(&_listener_lock);
    _mpeg_listeners.clear();
    _mpeg_sp_listeners.clear();
}

void MPEGStreamData::SetDesiredProgram(int p)
{
    bool reset = true;
    uint pid = 0;
    const ProgramAssociationTable* pat = NULL;
    pat_vec_t pats = GetCachedPATs();

    for (uint i = (p) ? 0 : pats.size(); i < pats.size() && !pid; i++)
    {
        pat = pats[i];
        pid = pats[i]->FindPID(p);
    }

    if (pid)
    {
        reset = false;
        _desired_program = p;
        ProcessPAT(pat);
        pmt_vec_t pmts = GetCachedPMTs();
        for (uint i = 0; i < pmts.size(); i++)
        {
            if (pmts[i]->ProgramNumber() == (uint)p)
                ProcessPMT(pmts[i]);
        }
        ReturnCachedPMTTables(pmts);
    }

    ReturnCachedPATTables(pats);

    if (reset)
        Reset(p);
}

void MPEGStreamData::SetEITHelper(EITHelper *eit_helper)
{
    QMutexLocker locker(&_listener_lock);
    _eit_helper = eit_helper;
}

void MPEGStreamData::SetEITRate(float rate)
{
    QMutexLocker locker(&_listener_lock);
    _eit_rate = rate;
}

void MPEGStreamData::Reset(int desiredProgram)
{
    _desired_program = desiredProgram;
    _invalid_pat_seen = false;

    SetPATSingleProgram(NULL);
    SetPMTSingleProgram(NULL);

    pid_pes_map_t old = _partial_pes_packet_cache;
    pid_pes_map_t::iterator it = old.begin();
    for (; it != old.end(); ++it)
        DeletePartialPES(it.key());
    _partial_pes_packet_cache.clear();

    _pids_listening.clear();
    _pids_notlistening.clear();
    _pids_writing.clear();

    _pid_video_single_program = _pid_pmt_single_program = 0xffffffff;

    _pat_version.clear();
    _pat_section_seen.clear();

    _pmt_version.clear();
    _pmt_section_seen.clear();

    {
        QMutexLocker locker(&_cache_lock);

        pat_cache_t::iterator it1 = _cached_pats.begin();
        for (; it1 != _cached_pats.end(); ++it1)
            DeleteCachedTable(*it1);
        _cached_pats.clear();

        pmt_cache_t::iterator it2 = _cached_pmts.begin();
        for (; it2 != _cached_pmts.end(); ++it2)
            DeleteCachedTable(*it2);
        _cached_pmts.clear();
    }

    AddListeningPID(MPEG_PAT_PID);
}

void MPEGStreamData::DeletePartialPES(uint pid)
{
    pid_pes_map_t::iterator it = _partial_pes_packet_cache.find(pid);
    if (it != _partial_pes_packet_cache.end())
    {
        PESPacket *pkt = *it;
        _partial_pes_packet_cache.erase(it);
        delete pkt;
    }
}

/** \fn MPEGStreamData::AssemblePSIP(const TSPacket*,bool&)
 *  \brief PES packet assembler.
 *
 *   This is not a general purpose TS->PES packet converter,
 *   it is only designed to work with MPEG tables which comply
 *   with certain restrictions that simplify the conversion.
 *
 *   DVB TSPackets may contain multiple segments of the PSI
 *   stream.  (see ISO 13818-1 section 2.4.3.3, particularly
 *   the definition of payload_unit_start_indicator, which
 *   indicates there is at least one segment start, but not
 *   limited to only one segment start.)
 *
 *   PSI stuffing bytes are 0xFF and will complete the
 *   remaining portion of the TSPacket.  (Section 2.4.4)
 *
 *  \note This method makes the assumption that AddTSPacket
 *        correctly handles duplicate packets.
 *
 *  \param moreTablePackets returns true if we need more packets
 */
PSIPTable* MPEGStreamData::AssemblePSIP(const TSPacket* tspacket,
                                        bool &moreTablePackets)
{
    bool broken = true;
    moreTablePackets = true;

    PESPacket* partial = GetPartialPES(tspacket->PID());
    if (partial && partial->AddTSPacket(tspacket, broken) && !broken)
    {
        // check if it's safe to read pespacket's Length()
        if ((partial->PSIOffset() + 1 + 3) > partial->TSSizeInBuffer())
        {
            VERBOSE(VB_RECORD,
                    QString("Discarding broken PES packet. Packet's length at "
                            "position %1 isn't in the buffer of %2 bytes.")
                    .arg(partial->PSIOffset() + 1 + 3)
                    .arg(partial->TSSizeInBuffer()));
            DeletePartialPES(tspacket->PID());
            return NULL;
        }

        // Discard broken packets
        bool buggy = _have_CRC_bug &&
        ((TableID::PMT == partial->StreamID()) ||
         (TableID::PAT == partial->StreamID()));
        if (!buggy && !partial->IsGood())
        {
            VERBOSE(VB_SIPARSER, "Discarding broken PES packet");
            DeletePartialPES(tspacket->PID());
            return NULL;
        }

        PSIPTable* psip = new PSIPTable(*partial);

        // Advance to the next packet
        // pesdata starts only at PSIOffset()+1
        uint packetStart = partial->PSIOffset() + 1 + psip->SectionLength();
        if (packetStart < partial->TSSizeInBuffer())
        {
            if (partial->pesdata()[psip->SectionLength()] != 0xff)
            {
                // If the next section starts in the new tspacket
                // create a new partial packet to prevent overflow
                if ((partial->TSSizeInBuffer() > TSPacket::SIZE) &&
                    (packetStart >
                     partial->TSSizeInBuffer() - TSPacket::PAYLOAD_SIZE))
                {
                    // Saving will handle deleting the old one
                    SavePartialPES(tspacket->PID(),
                                   new PESPacket(*tspacket));
                }
                else
                {
                    partial->SetPSIOffset(partial->PSIOffset() +
                                          psip->SectionLength());
                }
                return psip;
            }
        }
        // discard incomplete packets
        if (packetStart > partial->TSSizeInBuffer())
        {
            VERBOSE(VB_RECORD, QString("Discarding broken PES packet. ") +
                    QString("Packet with %1 bytes doesn't fit "
                            "into a buffer of %2 bytes.")
                    .arg(packetStart).arg(partial->TSSizeInBuffer()));
            delete psip;
            psip = NULL;
        }

        moreTablePackets = false;
        DeletePartialPES(tspacket->PID());
        return psip;
    }
    else if (partial)
    {
        if (broken)
            DeletePartialPES(tspacket->PID());

        moreTablePackets = false;
        return 0; // partial packet is not yet complete.
    }

    if (!tspacket->PayloadStart())
    {
        // We didn't see this PES packet's start, so this must be the
        // tail end of something we missed. Ignore it.
        moreTablePackets = false;
        return 0;
    }

    const int offset = tspacket->AFCOffset() + tspacket->StartOfFieldPointer();
    if (offset>181)
    {
        VERBOSE(VB_IMPORTANT, "Error: offset>181, pes length & "
                "current can not be queried");
        return 0;
    }

    // table_id (8 bits) and section_length(12), syntax(1), priv(1), res(2)
    // pointer_field (+8 bits), since payload start is true if we are here.
    const int extra_offset = 4;

    const unsigned char* pesdata = tspacket->data() + offset;
    const int pes_length = (pesdata[2] & 0x0f) << 8 | pesdata[3];
    if ((pes_length + offset + extra_offset) > 188)
    {
        SavePartialPES(tspacket->PID(), new PESPacket(*tspacket));
        moreTablePackets = false;
        return 0;
    }

    PSIPTable *psip = new PSIPTable(*tspacket); // must be complete packet

    // There might be another section after this one in the 
    // current packet. We need room before the end of the 
    // packet, and it must not be packet stuffing.
    if ((offset + psip->SectionLength() < TSPacket::SIZE) &&
        (pesdata[psip->SectionLength() + 1] != 0xff))
    {
        // This isn't sutffing, so we need to put this 
        // on as a partial packet.
        PESPacket *pesp = new PESPacket(*tspacket);
        pesp->SetPSIOffset(offset + psip->SectionLength());
        SavePartialPES(tspacket->PID(), pesp);
        return psip;
    }

    moreTablePackets = false;
    return psip;
}

bool MPEGStreamData::CreatePATSingleProgram(
    const ProgramAssociationTable& pat)
{
    VERBOSE(VB_RECORD, "CreatePATSingleProgram()");
    VERBOSE(VB_RECORD, "PAT in input stream");
    VERBOSE(VB_RECORD, pat.toString());
    if (_desired_program < 0)
    {
        VERBOSE(VB_RECORD, "Desired program not set yet");
        return false;
    }
    _pid_pmt_single_program = pat.FindPID(_desired_program);
    VERBOSE(VB_RECORD, QString("desired_program(%1) pid(0x%2)").
            arg(_desired_program).arg(_pid_pmt_single_program, 0, 16));

    if (!_pid_pmt_single_program)
    {
        _pid_pmt_single_program = pat.FindAnyPID();
        if (!_pid_pmt_single_program)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("No program found in PAT."
                            " This recording will not play in MythTV."));
        }
        VERBOSE(VB_IMPORTANT,
                QString("Desired program #%1 not found in PAT."
                        "\n\t\t\tCan Not create single program PAT.")
                .arg(_desired_program));
        SetPATSingleProgram(NULL);
        return false;
    }

    AddListeningPID(_pid_pmt_single_program);

    vector<uint> pnums, pids;

    pnums.push_back(1);
    pids.push_back(_pid_pmt_single_program);

    uint tsid = pat.TableIDExtension();
    uint ver = pat.Version();
    ProgramAssociationTable* pat2 =
        ProgramAssociationTable::Create(tsid, ver, pnums, pids);

    if (!pat2)
    {
        VERBOSE(VB_IMPORTANT, "MPEGStreamData::CreatePATSingleProgram: "
                "Failed to create Program Association Table.");
        return false;
    }

    pat2->tsheader()->SetContinuityCounter(pat.tsheader()->ContinuityCounter());

    VERBOSE(VB_RECORD, QString("pmt_pid(0x%1)")
            .arg(_pid_pmt_single_program, 0, 16));
    VERBOSE(VB_RECORD, "PAT for output stream");
    VERBOSE(VB_RECORD, pat2->toString());

    SetPATSingleProgram(pat2);

    return true;

}

bool MPEGStreamData::CreatePMTSingleProgram(const ProgramMapTable& pmt)
{
    VERBOSE(VB_RECORD, "CreatePMTSingleProgram()");
    VERBOSE(VB_RECORD, "PMT in input stream");
    VERBOSE(VB_RECORD, pmt.toString());

    if (!PATSingleProgram())
    {
        VERBOSE(VB_RECORD, "no PAT yet...");
        return false; // no way to properly rewrite pids without PAT
    }
    pmt.Parse();

    vector<uint> videoPIDs, audioPIDs;
    vector<uint> videoTypes, audioTypes;
    vector<uint> pids, types;

    // Video
    uint video_cnt = pmt.FindPIDs(StreamID::AnyVideo, videoPIDs, videoTypes);
    if (video_cnt < _pmt_single_program_num_video) 
    {
        VERBOSE(VB_RECORD, "Only "<<video_cnt<<" video streams seen in PMT, "
                "but "<<_pmt_single_program_num_video<<" are required.");
        return false;
    }
    if (videoPIDs.size() > 1)
    {
        VERBOSE(VB_RECORD,
                "Warning: More than one video stream in old PMT, "
                "only using first one in new PMT");
    }

    if (videoPIDs.size() > 0)
    {
        _pid_video_single_program = videoPIDs[0];
        pids.push_back(videoPIDs[0]);
        types.push_back(videoTypes[0]);
    }

    // Audio
    pmt.FindPIDs(StreamID::AnyAudio, audioPIDs, audioTypes);
    if (audioPIDs.size() < _pmt_single_program_num_audio)
    {
        VERBOSE(VB_RECORD, "Only "<<audioPIDs.size()
                <<" audio streams seen in PMT, but "
                <<_pmt_single_program_num_audio<<" are required.");
        return false;
    }
    for (uint i = 0; i < audioPIDs.size(); i++)
        AddAudioPID(audioPIDs[i]);

    // Timebase
    int pcrpidIndex = pmt.FindPID(pmt.PCRPID());
    if (pcrpidIndex < 0)
    {
        // the timecode reference stream is not in the PMT, 
        // add stream to misc record streams
        AddWritingPID(pmt.PCRPID());
    }

    // Misc
    uint programNumber = 1;

    // Construct
    pmt.FindPIDs(StreamID::AnyAudio, pids, types);
    ProgramMapTable *pmt2 = ProgramMapTable::
        Create(programNumber, _pid_pmt_single_program,
               pmt.PCRPID(), pmt.Version(), pids, types);

    // Set Continuity Header
    uint cc_cnt = pmt.tsheader()->ContinuityCounter();
    pmt2->tsheader()->SetContinuityCounter(cc_cnt);
    SetPMTSingleProgram(pmt2);

    VERBOSE(VB_RECORD, "PMT for output stream");
    VERBOSE(VB_RECORD, pmt2->toString());

    return true;
}

/** \fn MPEGStreamData::IsRedundant(uint pid, const PSIPTable&) const
 *  \brief Returns true if table already seen.
 */
bool MPEGStreamData::IsRedundant(uint pid, const PSIPTable &psip) const
{
    (void) pid;
    const int table_id = psip.TableID();
    const int version  = psip.Version();

    if (TableID::PAT == table_id)
    {
        if (VersionPAT(psip.TableIDExtension()) != version)
            return false;
        return PATSectionSeen(psip.TableIDExtension(), psip.Section());
    }

    if (TableID::CAT == table_id)
        return false;

    if (TableID::PMT == table_id)
    {
        if (VersionPMT(psip.TableIDExtension()) != version)
            return false;
        return PMTSectionSeen(psip.TableIDExtension(), psip.Section());
    }

    return false;
}

/** \fn MPEGStreamData::HandleTables(uint pid, const PSIPTable &psip)
 *  \brief Assembles PSIP packets and processes them.
 */
bool MPEGStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    if (IsRedundant(pid, psip))
        return true;

    const int version = psip.Version();
    // If we get this far decode table
    switch (psip.TableID())
    {
        case TableID::PAT:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionPAT(tsid, version, psip.LastSection());
            SetPATSectionSeen(tsid, psip.Section());

            ProgramAssociationTable pat(psip);

            if (_cache_tables)
                CachePAT(&pat);

            ProcessPAT(&pat);

            return true;
        }
        case TableID::CAT:
        {
            ConditionalAccessTable cat(psip);

            _listener_lock.lock();
            for (uint i = 0; i < _mpeg_listeners.size(); i++)
                _mpeg_listeners[i]->HandleCAT(&cat);
            _listener_lock.unlock();

            return true;
        }
        case TableID::PMT:
        {
            uint prog_num = psip.TableIDExtension();
            SetVersionPMT(prog_num, version, psip.LastSection());
            SetPMTSectionSeen(prog_num, psip.Section());

            ProgramMapTable pmt(psip);

            if (_cache_tables)
                CachePMT(&pmt);

            ProcessPMT(&pmt);

            return true;
        }
    }
    return false;
}

void MPEGStreamData::ProcessPAT(const ProgramAssociationTable *pat)
{
    bool foundProgram = pat->FindPID(_desired_program);

    _listener_lock.lock();
    for (uint i = 0; i < _mpeg_listeners.size(); i++)
        _mpeg_listeners[i]->HandlePAT(pat);
    _listener_lock.unlock();

    if (_desired_program < 0)
        return;

    bool send_single_program = false;
    if (!_invalid_pat_seen && !foundProgram)
    {
        _invalid_pat_seen = true;
        _invalid_pat_warning = false;
        _invalid_pat_timer.start();
        VERBOSE(VB_RECORD, "ProcessPAT: "
                "PAT is missing program, setting timeout");
    }
    else if (_invalid_pat_seen && !foundProgram &&
             (_invalid_pat_timer.elapsed() > 400) && !_invalid_pat_warning)
    {
        _invalid_pat_warning = true; // only emit one warning...
        // After 400ms emit error if we haven't found correct PAT.
        VERBOSE(VB_IMPORTANT, "ProcessPAT: Program not found in PAT. "
                "\n\t\t\tRescan your transports.");

        send_single_program = CreatePATSingleProgram(*pat);
    }
    else if (foundProgram)
    {
        if (_invalid_pat_seen)
            VERBOSE(VB_RECORD, "ProcessPAT: Good PAT seen after a bad PAT");

        _invalid_pat_seen = false;

        send_single_program = CreatePATSingleProgram(*pat);
    }

    if (send_single_program)
    {
        QMutexLocker locker(&_listener_lock);
        ProgramAssociationTable *pat_sp = PATSingleProgram();
        for (uint i = 0; i < _mpeg_sp_listeners.size(); i++)
            _mpeg_sp_listeners[i]->HandleSingleProgramPAT(pat_sp);
    }
}

void MPEGStreamData::ProcessPMT(const ProgramMapTable *pmt)
{
    _listener_lock.lock();
    for (uint i = 0; i < _mpeg_listeners.size(); i++)
        _mpeg_listeners[i]->HandlePMT(pmt->ProgramNumber(), pmt);
    _listener_lock.unlock();

    bool desired = pmt->ProgramNumber() == (uint) _desired_program;
    if (desired && CreatePMTSingleProgram(*pmt))
    {
        QMutexLocker locker(&_listener_lock);
        ProgramMapTable *pmt_sp = PMTSingleProgram();
        for (uint i = 0; i < _mpeg_sp_listeners.size(); i++)
            _mpeg_sp_listeners[i]->HandleSingleProgramPMT(pmt_sp);
    }
}

#define DONE_WITH_PES_PACKET() { if (psip) delete psip; \
    if (morePSIPPackets) goto HAS_ANOTHER_PES; else return; }

/** \fn MPEGStreamData::HandleTSTables(const TSPacket*)
 *  \brief Assembles PSIP packets and processes them.
 */
void MPEGStreamData::HandleTSTables(const TSPacket* tspacket)
{
    bool morePSIPPackets;
  HAS_ANOTHER_PES:
    // Assemble PSIP
    PSIPTable *psip = AssemblePSIP(tspacket, morePSIPPackets);
    if (!psip)
       return;

    // Validate PSIP
    // but don't validate PMT/PAT if our driver has the PMT/PAT CRC bug.
    bool buggy = _have_CRC_bug &&
        ((TableID::PMT == psip->TableID()) ||
         (TableID::PAT == psip->TableID()));
    if (!buggy && !psip->IsGood())
    {
        VERBOSE(VB_RECORD, QString("PSIP packet failed CRC check. "
                                   "pid(0x%1) type(0x%2)")
                .arg(tspacket->PID(),0,16).arg(psip->TableID(),0,16));
        DONE_WITH_PES_PACKET();
    }

    if (!psip->IsCurrent()) // we don't cache the next table, for now
        DONE_WITH_PES_PACKET();

    if (tspacket->ScramplingControl())
    { // scrambled! ATSC, DVB require tables not to be scrambled
        VERBOSE(VB_RECORD,
                "PSIP packet is scrambled, not ATSC/DVB compiant");
        DONE_WITH_PES_PACKET();
    }

    // Don't decode redundant packets,
    // but if it is a desired PAT or PMT emit a "heartbeat" signal.
    if (IsRedundant(tspacket->PID(), *psip))
    {
        if (TableID::PAT == psip->TableID())
        {
            QMutexLocker locker(&_listener_lock);
            ProgramAssociationTable *pat_sp = PATSingleProgram();
            for (uint i = 0; i < _mpeg_sp_listeners.size(); i++)
                _mpeg_sp_listeners[i]->HandleSingleProgramPAT(pat_sp);
        }
        if (TableID::PMT == psip->TableID() &&
            tspacket->PID() == _pid_pmt_single_program)
        {
            QMutexLocker locker(&_listener_lock);
            ProgramMapTable *pmt_sp = PMTSingleProgram();
            for (uint i = 0; i < _mpeg_sp_listeners.size(); i++)
                _mpeg_sp_listeners[i]->HandleSingleProgramPMT(pmt_sp);
        }
        DONE_WITH_PES_PACKET(); // already parsed this table, toss it.
    }

    HandleTables(tspacket->PID(), *psip);
    DONE_WITH_PES_PACKET();
}
#undef DONE_WITH_PES_PACKET

int MPEGStreamData::ProcessData(unsigned char *buffer, int len)
{
    int pos = 0;

    while (pos + 187 < len) // while we have a whole packet left
    {
        if (buffer[pos] != SYNC_BYTE)
        {
            int newpos = ResyncStream(buffer, pos, len);
            if (newpos == -1)
                return len - pos;
            if (newpos == -2)
                return TSPacket::SIZE;

            pos = newpos;
        }

        const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[pos]);
        if (ProcessTSPacket(*pkt))
            pos += TSPacket::SIZE; // Advance to next TS packet
        else // Let it resync in case of dropped bytes
            buffer[pos] = SYNC_BYTE + 1;
    }

    return len - pos;
}

bool MPEGStreamData::ProcessTSPacket(const TSPacket& tspacket)
{
    bool ok = !tspacket.TransportError();
    if (ok && !tspacket.ScramplingControl() && tspacket.HasPayload() &&
        IsListeningPID(tspacket.PID()))
    {
        HandleTSTables(&tspacket);
    }
    return ok;
}

int MPEGStreamData::ResyncStream(unsigned char *buffer, int curr_pos, int len)
{
    // Search for two sync bytes 188 bytes apart, 
    int pos = curr_pos;
    int nextpos = pos + TSPacket::SIZE;
    if (nextpos >= len)
        return -1; // not enough bytes; caller should try again
    
    while (buffer[pos] != SYNC_BYTE || buffer[nextpos] != SYNC_BYTE)
    {
        pos++;
        nextpos++;
        if (nextpos == len)
            return -2; // not found
    }

    return pos;
}

bool MPEGStreamData::IsListeningPID(uint pid) const
{
    QMap<uint, bool>::const_iterator it = _pids_listening.find(pid);
    return it != _pids_listening.end();
}

bool MPEGStreamData::IsNotListeningPID(uint pid) const
{
    QMap<uint, bool>::const_iterator it = _pids_notlistening.find(pid);
    return it != _pids_notlistening.end();
}

bool MPEGStreamData::IsWritingPID(uint pid) const
{
    QMap<uint, bool>::const_iterator it = _pids_writing.find(pid);
    return it != _pids_writing.end();
}

bool MPEGStreamData::IsAudioPID(uint pid) const
{
    QMap<uint, bool>::const_iterator it = _pids_audio.find(pid);
    return it != _pids_audio.end();
}

void MPEGStreamData::SavePartialPES(uint pid, PESPacket* packet)
{
    pid_pes_map_t::iterator it = _partial_pes_packet_cache.find(pid);
    if (it == _partial_pes_packet_cache.end())
        _partial_pes_packet_cache[pid] = packet;
    else
    {
        PESPacket *old = *it;
        _partial_pes_packet_cache.replace(pid, packet);
        delete old;
    }
}

void MPEGStreamData::SetPATSectionSeen(uint tsid, uint section)
{
    sections_map_t::iterator it = _pat_section_seen.find(tsid);
    if (it == _pat_section_seen.end())
    {
        _pat_section_seen[tsid].resize(32, 0);
        it = _pat_section_seen.find(tsid);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool MPEGStreamData::PATSectionSeen(uint tsid, uint section) const
{
    sections_map_t::const_iterator it = _pat_section_seen.find(tsid);
    if (it == _pat_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool MPEGStreamData::HasAllPATSections(uint tsid) const
{
    sections_map_t::const_iterator it = _pat_section_seen.find(tsid);
    if (it == _pat_section_seen.end())
        return false;
    for (uint i = 0; i < 32; i++)
        if ((*it)[i] != 0xff)
            return false;
    return true;
}

void MPEGStreamData::SetPMTSectionSeen(uint prog_num, uint section)
{
    sections_map_t::iterator it = _pmt_section_seen.find(prog_num);
    if (it == _pmt_section_seen.end())
    {
        _pmt_section_seen[prog_num].resize(32, 0);
        it = _pmt_section_seen.find(prog_num);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool MPEGStreamData::PMTSectionSeen(uint prog_num, uint section) const
{
    sections_map_t::const_iterator it = _pmt_section_seen.find(prog_num);
    if (it == _pmt_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool MPEGStreamData::HasAllPMTSections(uint prog_num) const
{
    sections_map_t::const_iterator it = _pmt_section_seen.find(prog_num);
    if (it == _pmt_section_seen.end())
        return false;
    for (uint i = 0; i < 32; i++)
        if ((*it)[i] != 0xff)
            return false;
    return true;
}

bool MPEGStreamData::HasProgram(uint progNum) const
{
    pmt_ptr_t pmt = GetCachedPMT(progNum, 0);
    bool hasit = pmt;
    ReturnCachedTable(pmt);

    return hasit;
}

bool MPEGStreamData::HasCachedAllPAT(uint tsid) const
{
    QMutexLocker locker(&_cache_lock);

    pat_cache_t::const_iterator it = _cached_pats.find(tsid << 8);
    if (it == _cached_pats.end())
        return false;

    uint last_section = (*it)->LastSection();
    if (!last_section)
        return true;

    for (uint i = 1; i <= last_section; i++)
        if (_cached_pats.find((tsid << 8) | i) == _cached_pats.end())
            return false;

    return true;
}

bool MPEGStreamData::HasCachedAnyPAT(uint tsid) const
{
    QMutexLocker locker(&_cache_lock);

    for (uint i = 0; i <= 255; i++)
        if (_cached_pats.find((tsid << 8) | i) != _cached_pats.end())
            return true;

    return false;
}

bool MPEGStreamData::HasCachedAnyPAT(void) const
{
    QMutexLocker locker(&_cache_lock);
    return _cached_pats.size();
}

bool MPEGStreamData::HasCachedAllPMT(uint pnum) const
{
    QMutexLocker locker(&_cache_lock);

    pmt_cache_t::const_iterator it = _cached_pmts.find(pnum << 8);
    if (it == _cached_pmts.end())
        return false;

    uint last_section = (*it)->LastSection();
    if (!last_section)
        return true;

    for (uint i = 1; i <= last_section; i++)
        if (_cached_pmts.find((pnum << 8) | i) == _cached_pmts.end())
            return false;

    return true;
}

bool MPEGStreamData::HasCachedAnyPMT(uint pnum) const
{
    QMutexLocker locker(&_cache_lock);

    for (uint i = 0; i <= 255; i++)
        if (_cached_pmts.find((pnum << 8) | i) != _cached_pmts.end())
            return true;

    return false;
}

bool MPEGStreamData::HasCachedAllPMTs(void) const
{
    QMutexLocker locker(&_cache_lock);

    pat_cache_t::const_iterator it = _cached_pats.begin();
    for (; it != _cached_pats.end(); ++it)
    {
        const ProgramAssociationTable *pat = *it;
        if (!HasCachedAllPAT(pat->TransportStreamID()))
            return false;

        for (uint i = 0; i < pat->ProgramCount(); i++)
            if (!HasCachedAllPMT(pat->ProgramNumber(i)))
                return false;
    }

    return true;
}

const pat_ptr_t MPEGStreamData::GetCachedPAT(
    uint tsid, uint section_num) const
{
    QMutexLocker locker(&_cache_lock);
    ProgramAssociationTable *pat = NULL;

    uint key = (tsid << 8) | section_num;
    pat_cache_t::const_iterator it = _cached_pats.find(key);
    if (it != _cached_pats.end())
        IncrementRefCnt(pat = *it);

    return pat;
}

pat_vec_t MPEGStreamData::GetCachedPATs(void) const
{
    QMutexLocker locker(&_cache_lock);
    pat_vec_t pats;

    pat_cache_t::const_iterator it = _cached_pats.begin();
    for (; it != _cached_pats.end(); ++it)
    {
        ProgramAssociationTable* pat = *it;
        IncrementRefCnt(pat);
        pats.push_back(pat);
    }

    return pats;
}

const pmt_ptr_t MPEGStreamData::GetCachedPMT(
    uint program_num, uint section_num) const
{
    QMutexLocker locker(&_cache_lock);
    ProgramMapTable *pmt = NULL;

    uint key = (program_num << 8) | section_num;
    pmt_cache_t::const_iterator it = _cached_pmts.find(key);
    if (it != _cached_pmts.end())
        IncrementRefCnt(pmt = *it);

    return pmt;
}

pmt_vec_t MPEGStreamData::GetCachedPMTs(void) const
{
    QMutexLocker locker(&_cache_lock);
    vector<const ProgramMapTable*> pmts;

    pmt_cache_t::const_iterator it = _cached_pmts.begin();
    for (; it != _cached_pmts.end(); ++it)
    {
        ProgramMapTable* pmt = *it;
        IncrementRefCnt(pmt);
        pmts.push_back(pmt);
    }

    return pmts;
}

pmt_map_t MPEGStreamData::GetCachedPMTMap(void) const
{
    QMutexLocker locker(&_cache_lock);
    pmt_map_t pmts;

    pmt_cache_t::const_iterator it = _cached_pmts.begin();
    for (; it != _cached_pmts.end(); ++it)
    {
        ProgramMapTable* pmt = *it;
        IncrementRefCnt(pmt);
        pmts[pmt->ProgramNumber()].push_back(pmt);
    }

    return pmts;
}

void MPEGStreamData::ReturnCachedTable(const PSIPTable *psip) const
{
    QMutexLocker locker(&_cache_lock);

    int val = _cached_ref_cnt[psip] - 1;
    _cached_ref_cnt[psip] = val;

    // if ref <= 0 and table was slated for deletion, delete it.
    if (val <= 0)
    {
        psip_refcnt_map_t::iterator it;
        it = _cached_slated_for_deletion.find(psip);
        if (it != _cached_slated_for_deletion.end())
            DeleteCachedTable((PSIPTable*)psip);
    }
}

void MPEGStreamData::ReturnCachedPATTables(pat_vec_t &pats) const
{
    for (pat_vec_t::iterator it = pats.begin(); it != pats.end(); ++it)
        ReturnCachedTable(*it);
    pats.clear();
}

void MPEGStreamData::ReturnCachedPATTables(pat_map_t &pats) const
{
    for (pat_map_t::iterator it = pats.begin(); it != pats.end(); ++it)
        ReturnCachedPATTables(*it);
    pats.clear();
}

void MPEGStreamData::ReturnCachedPMTTables(pmt_vec_t &pmts) const
{
    for (pmt_vec_t::iterator it = pmts.begin(); it != pmts.end(); ++it)
        ReturnCachedTable(*it);
    pmts.clear();
}

void MPEGStreamData::ReturnCachedPMTTables(pmt_map_t &pmts) const
{
    for (pmt_map_t::iterator it = pmts.begin(); it != pmts.end(); ++it)
        ReturnCachedPMTTables(*it);
    pmts.clear();
}

void MPEGStreamData::IncrementRefCnt(const PSIPTable *psip) const
{
    QMutexLocker locker(&_cache_lock);
    _cached_ref_cnt[psip] = _cached_ref_cnt[psip] + 1;
}

void MPEGStreamData::DeleteCachedTable(PSIPTable *psip) const
{
    if (!psip)
        return;

    uint tid = psip->TableIDExtension();

    QMutexLocker locker(&_cache_lock);
    if (_cached_ref_cnt[psip] > 0)
    {
        _cached_slated_for_deletion[psip] = 1;
        return;
    }
    else if (TableID::PAT == psip->TableID() &&
             (_cached_pats[(tid << 8) | psip->Section()] == psip))
    {
        _cached_pats[(tid << 8) | psip->Section()] = NULL;
        delete psip;
    }
    else if ((TableID::PMT == psip->TableID()) &&
             (_cached_pmts[(tid << 8) | psip->Section()] == psip))
    {
        _cached_pmts[(tid << 8) | psip->Section()] = NULL;
        delete psip;
    }
    else
    {
        _cached_slated_for_deletion[psip] = 2;
        return;
    }
    psip_refcnt_map_t::iterator it;
    it = _cached_slated_for_deletion.find(psip);
    if (it != _cached_slated_for_deletion.end())
        _cached_slated_for_deletion.erase(it);
}

void MPEGStreamData::CachePAT(const ProgramAssociationTable *_pat)
{
    ProgramAssociationTable *pat = new ProgramAssociationTable(*_pat);
    uint key = (_pat->TransportStreamID() << 8) | _pat->Section();

    QMutexLocker locker(&_cache_lock);

    pat_cache_t::iterator it = _cached_pats.find(key);
    if (it != _cached_pats.end())
        DeleteCachedTable(*it);

    _cached_pats[key] = pat;
}

void MPEGStreamData::CachePMT(const ProgramMapTable *_pmt)
{
    ProgramMapTable *pmt = new ProgramMapTable(*_pmt);
    uint key = (_pmt->ProgramNumber() << 8) | _pmt->Section();

    QMutexLocker locker(&_cache_lock);

    pmt_cache_t::iterator it = _cached_pmts.find(key);
    if (it != _cached_pmts.end())
        DeleteCachedTable(*it);

    _cached_pmts[key] = pmt;
}

void MPEGStreamData::AddMPEGListener(MPEGStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    mpeg_listener_vec_t::iterator it = _mpeg_listeners.begin();
    for (; it != _mpeg_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _mpeg_listeners.push_back(val);
}

void MPEGStreamData::RemoveMPEGListener(MPEGStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    mpeg_listener_vec_t::iterator it = _mpeg_listeners.begin();
    for (; it != _mpeg_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _mpeg_listeners.erase(it);
            return;
        }
    }
}

void MPEGStreamData::AddMPEGSPListener(MPEGSingleProgramStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    mpeg_sp_listener_vec_t::iterator it = _mpeg_sp_listeners.begin();
    for (; it != _mpeg_sp_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _mpeg_sp_listeners.push_back(val);
}

void MPEGStreamData::RemoveMPEGSPListener(MPEGSingleProgramStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    mpeg_sp_listener_vec_t::iterator it = _mpeg_sp_listeners.begin();
    for (; it != _mpeg_sp_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _mpeg_sp_listeners.erase(it);
            return;
        }
    }
}
