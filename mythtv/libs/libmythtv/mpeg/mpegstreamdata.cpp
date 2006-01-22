// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "mpegstreamdata.h"
#include "mpegtables.h"
#include "RingBuffer.h"
#include "mpegtables.h"

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
    : QObject(NULL, "MPEGStreamData"), _have_CRC_bug(false),
      _pat_version(-1), _cache_tables(cacheTables), _cache_lock(true),
      _cached_pat(NULL), _desired_program(desiredProgram),
      _pmt_single_program_num_video(1),
      _pmt_single_program_num_audio(0),
      _pat_single_program(NULL), _pmt_single_program(NULL),
      _invalid_pat_seen(false), _invalid_pat_warning(false)
{
    AddListeningPID(MPEG_PAT_PID);

    _pid_video_single_program = _pid_pmt_single_program = 0xffffffff;
}

MPEGStreamData::~MPEGStreamData()
{
    Reset(-1);
    SetPATSingleProgram(0);
    SetPMTSingleProgram(0);

    // Delete any cached tables that haven't been returned
    psip_refcnt_map_t::iterator it = _cached_slated_for_deletion.begin();
    for (; it != _cached_slated_for_deletion.end(); ++it)
        delete it.key();
}

void MPEGStreamData::Reset(int desiredProgram)
{
    _desired_program = desiredProgram;
    _invalid_pat_seen = false;

    SetPATSingleProgram(0);
    SetPMTSingleProgram(0);

    pid_pes_map_t old = _partial_pes_packet_cache;
    pid_pes_map_t::iterator it = old.begin();
    for (; it != old.end(); ++it)
        DeletePartialPES(it.key());
    _partial_pes_packet_cache.clear();

    _pids_listening.clear();
    _pids_notlistening.clear();
    _pids_writing.clear();

    _pid_video_single_program = _pid_pmt_single_program = 0xffffffff;
    _pmt_version.clear();

    {
        QMutexLocker locker(&_cache_lock);

        DeleteCachedTable(_cached_pat);
        _cached_pat = NULL;

        pmt_cache_t::iterator it = _cached_pmts.begin();
        for (; it != _cached_pmts.end(); ++it)
            DeleteCachedTable(*it);
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

/** \fn MPEGStreamData::AssemblePSIP(const TSPacket* tspacket)
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
 *   \note This method makes the assumption that AddTSPacket
 *   correctly handles duplicate packets.
 */
PSIPTable* MPEGStreamData::AssemblePSIP(const TSPacket* tspacket,
                                        bool &moreTablePackets)
{
    moreTablePackets = true;

    PESPacket* partial = GetPartialPES(tspacket->PID());
    if (partial && partial->AddTSPacket(tspacket))
    {
        PSIPTable* psip = new PSIPTable(*partial);

        // Advance to the next packet
        uint packetStart = partial->PSIOffset() + psip->SectionLength();
        if (packetStart < partial->TSSizeInBuffer())
        {
            if (partial->pesdata()[psip->SectionLength() + 1] != 0xff)
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

        moreTablePackets = false;
        DeletePartialPES(tspacket->PID());
        return psip;
    }
    else if (partial)
    {    
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
    const PESPacket pes = PESPacket::View(*tspacket);
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
    vector<uint> videoTypes, audioTypess;
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
    if (audioPIDs.size() < _pmt_single_program_num_audio)
    {
        VERBOSE(VB_RECORD, "Only "<<audioPIDs.size()
                <<" audio streams seen in PMT, but "
                <<_pmt_single_program_num_audio<<" are required.");
        return false;
    }
    pmt.FindPIDs(StreamID::AnyAudio, pids, types);
    for (uint i = 1; i < audioPIDs.size(); i++)
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

/** \fn MPEGStreamData::IsRedundant(const PSIPTable&) const
 *  \brief Returns true if table already seen.
 */
bool MPEGStreamData::IsRedundant(const PSIPTable &psip) const
{
    const int table_id = psip.TableID();
    const int version  = psip.Version();

    if (TableID::PAT == table_id)
        return (version == VersionPATSingleProgram());

    if (TableID::PMT == table_id)
        return version == VersionPMT(psip.TableIDExtension());

    return false;
}

/** \fn MPEGStreamData::HandleTables(uint pid, const PSIPTable &psip)
 *  \brief Assembles PSIP packets and processes them.
 */
bool MPEGStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    // If we get this far decode table
    switch (psip.TableID())
    {
        case TableID::PAT:
        { 
            SetVersionPAT(psip.Version());
            if (_cache_tables)
            {
                ProgramAssociationTable *pat =
                    new ProgramAssociationTable(psip);
                CachePAT(pat);
                ProcessPAT(pat);
            }
            else
            {
                ProgramAssociationTable pat(psip);
                ProcessPAT(&pat);
            }
            return true;
        }
        case TableID::PMT:
        {
            SetVersionPMT(psip.TableIDExtension(), psip.Version());
            if (_cache_tables)
            {
                ProgramMapTable *pmt = new ProgramMapTable(psip);
                CachePMT(pmt->ProgramNumber(), pmt);
                ProcessPMT(pid, pmt);
            }
            else
            {
                ProgramMapTable pmt(psip);
                ProcessPMT(pid, &pmt);
            }
            return true;
        }
    }
    return false;
}

void MPEGStreamData::ProcessPAT(const ProgramAssociationTable *pat)
{
    bool foundProgram = pat->FindPID(_desired_program);

    if (_desired_program < 0)
    {
        emit UpdatePAT(pat);
        return;
    }

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

        // This will trigger debug PAT print
        emit UpdatePAT(pat);
        if (CreatePATSingleProgram(*pat))
            emit UpdatePATSingleProgram(PATSingleProgram());
        _invalid_pat_seen = false;
    }
    else if (foundProgram)
    {
        if (_invalid_pat_seen)
            VERBOSE(VB_RECORD, "ProcessPAT: Good PAT seen after a bad PAT");

        _invalid_pat_seen = false;
        emit UpdatePAT(pat);
        if (CreatePATSingleProgram(*pat))
            emit UpdatePATSingleProgram(PATSingleProgram());
    }
}

void MPEGStreamData::ProcessPMT(const uint pid, const ProgramMapTable *pmt)
{
    emit UpdatePMT(pmt->ProgramNumber(), pmt);
    if (pid == _pid_pmt_single_program)
    {
        if (CreatePMTSingleProgram(*pmt))
            emit UpdatePMTSingleProgram(PMTSingleProgram());
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

    if (1!=tspacket->AdaptationFieldControl())
    { // payload only, ATSC req.
        VERBOSE(VB_RECORD,
                "PSIP packet has Adaptation Field Control, not ATSC compiant");
        DONE_WITH_PES_PACKET();
    }

    if (tspacket->ScramplingControl())
    { // scrambled! ATSC, DVB require tables not to be scrambled
        VERBOSE(VB_RECORD,
                "PSIP packet is scrambled, not ATSC/DVB compiant");
        DONE_WITH_PES_PACKET();
    }

    // Don't decode redundant packets,
    // but if it is a desired PAT or PMT emit a "heartbeat" signal.
    if (IsRedundant(*psip))
    {
        if (TableID::PAT == psip->TableID())
            emit UpdatePATSingleProgram(PATSingleProgram());
        if (TableID::PMT == psip->TableID() &&
            tspacket->PID() == _pid_pmt_single_program)
            emit UpdatePMTSingleProgram(PMTSingleProgram());
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

bool MPEGStreamData::HasCachedPAT(void) const
{
    return (bool)(_cached_pat);
}

bool MPEGStreamData::HasCachedPMT(uint pid) const
{
    _cache_lock.lock();
    pmt_cache_t::const_iterator it = _cached_pmts.find(pid);
    bool exists = (it != _cached_pmts.end());
    _cache_lock.unlock();
    return exists;
}

bool MPEGStreamData::HasAllPMTsCached(void) const
{
    if (_cached_pat)
        return false;

    bool ret = true;
    _cache_lock.lock();

    if (_cached_pat->ProgramCount() > _cached_pmts.size())
        ret = false;
    else
    {
        // Verify we have the right ones
        pmt_cache_t::const_iterator it = _cached_pmts.begin();
        for (; it != _cached_pmts.end() && ret; ++it)
            ret &= (bool)(_cached_pat->FindProgram(it.key()));
    }

    _cache_lock.unlock();
    return ret;
}

const ProgramAssociationTable *MPEGStreamData::GetCachedPAT(void) const
{
    QMutexLocker locker(&_cache_lock);

    const ProgramAssociationTable *pat = _cached_pat;
    IncrementRefCnt(pat);

    return pat;
}

const ProgramMapTable *MPEGStreamData::GetCachedPMT(uint program_num) const
{
    QMutexLocker locker(&_cache_lock);
    ProgramMapTable *pmt = NULL;

    pmt_cache_t::const_iterator it = _cached_pmts.find(program_num);
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
        pmts[pmt->ProgramNumber()] = pmt;
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

void MPEGStreamData::ReturnCachedTables(pmt_vec_t &pmts) const
{
    for (pmt_vec_t::iterator it = pmts.begin(); it != pmts.end(); ++it)
        ReturnCachedTable(*it);
    pmts.clear();
}

void MPEGStreamData::ReturnCachedTables(pmt_map_t &pmts) const
{
    for (pmt_map_t::iterator it = pmts.begin(); it != pmts.end(); ++it)
        ReturnCachedTable(*it);
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

    QMutexLocker locker(&_cache_lock);
    if (_cached_ref_cnt[psip] > 0)
    {
        _cached_slated_for_deletion[psip] = 1;
        return;
    }
    else if (TableID::PMT == psip->TableID())
    {
        if (psip == _cached_pat)
            _cached_pat = NULL;
        delete psip;
    }
    else if ((TableID::PMT == psip->TableID()) &&
             _cached_pmts[psip->TableIDExtension()])
    {
        _cached_pmts[psip->TableIDExtension()] = NULL;
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

void MPEGStreamData::CachePAT(ProgramAssociationTable *pat)
{
    QMutexLocker locker(&_cache_lock);

    DeleteCachedTable(_cached_pat);
    _cached_pat = pat;
}

void MPEGStreamData::CachePMT(uint program_num, ProgramMapTable *pmt)
{
    QMutexLocker locker(&_cache_lock);

    pmt_cache_t::iterator it = _cached_pmts.find(program_num);
    if (it != _cached_pmts.end())
        DeleteCachedTable(*it);
    _cached_pmts[program_num] = pmt;
}
