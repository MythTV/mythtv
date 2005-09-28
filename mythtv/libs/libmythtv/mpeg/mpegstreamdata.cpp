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
    : QObject(NULL, "MPEGStreamData"), _have_pmt_CRC_bug(false),
      _pat_version(-1), _cache_tables(cacheTables), _cache_lock(true),
      _cached_pat(NULL), _desired_program(desiredProgram),
      _pat_single_program(NULL), _pmt_single_program(NULL)
{
    AddListeningPID(MPEG_PAT_PID);

    _pid_video_single_program = _pid_pmt_single_program = 0xffffffff;
}

MPEGStreamData::~MPEGStreamData()
{
    Reset(-1);
    SetPATSingleProgram(0);
    SetPMTSingleProgram(0);
}

void MPEGStreamData::Reset(int desiredProgram)
{
    _desired_program = desiredProgram;

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
        _cache_lock.lock();

        DeleteCachedTable(_cached_pat);
        _cached_pat = NULL;

        pmt_cache_t::iterator it = _cached_pmts.begin();
        for (; it != _cached_pmts.end(); ++it)
            DeleteCachedTable(*it);
        _cached_pmts.clear();

        _cache_lock.unlock();
    }

    AddListeningPID(MPEG_PAT_PID);
}

void MPEGStreamData::DeletePartialPES(uint pid)
{
    pid_pes_map_t::iterator it = _partial_pes_packet_cache.find(pid);
    PESPacket *pkt = *it;
    if (pkt)
    {
        _partial_pes_packet_cache.erase(it);
        delete pkt;
    }
}

/** \fn MPEGStreamData::AssemblePSIP(const TSPacket* tspacket)
 *  \brief PES packet assembler.
 *
 *   This only assembles PES packets when new PES packets
 *   start in their own TSPacket. This is always true of
 *   ATSC & DVB tables, but not true of Audio and Video
 *   PES packets.
 */
PSIPTable* MPEGStreamData::AssemblePSIP(const TSPacket* tspacket)
{
    if (tspacket->PayloadStart())
    {
        const int offset = tspacket->AFCOffset() +
            tspacket->StartOfFieldPointer();
        if (offset>181)
        {
            VERBOSE(VB_IMPORTANT, "Error: offset>181, pes length & "
                    "current can not be queried");
            return 0;
        }
        const unsigned char* pesdata = tspacket->data() + offset;
        const int pes_length = (pesdata[2] & 0x0f) << 8 | pesdata[3];
        const PESPacket pes = PESPacket::View(*tspacket);
        if ((pes_length + offset)>188)
        {
            if (!bool(pes.pesdata()[6]&1)/*current*/)
                return 0; // we only care about current psip packets for now
            SavePartialPES(tspacket->PID(), new PESPacket(*tspacket));
            return 0;
        }
        return new PSIPTable(*tspacket); // must be complete packet
    }

    PESPacket* partial = GetPartialPES(tspacket->PID());
    if (partial)
    {
        if (partial->AddTSPacket(tspacket))
        {
            PSIPTable* psip = new PSIPTable(*partial);
            DeletePartialPES(tspacket->PID());
            return psip;
        }
        return 0; // partial packet is not yet complete.
    }
    // We didn't see this PES packet's start, so this must be the
    // tail end of something we missed. Ignore it.
    return 0;
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
    if (pmt.FindPIDs(StreamID::AnyVideo, videoPIDs, videoTypes) < 1) 
    {
        VERBOSE(VB_RECORD,
                "No video found old PMT, can not construct new PMT");
        return false;
    }
    if (videoPIDs.size() > 1)
    {
        VERBOSE(VB_RECORD,
                "Warning: More than one video stream in old PMT, "
                "only using first one in new PMT");
    }

    _pid_video_single_program = videoPIDs[0];
    pids.push_back(videoPIDs[0]);
    types.push_back(videoTypes[0]);

    // Audio
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
    pmt2->tsheader()->SetContinuityCounter(pmt.tsheader()->ContinuityCounter());

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
        return VersionPMT(psip.tsheader()->PID()) == version;

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
                emit UpdatePAT(pat);
                if ((_desired_program >= 0) && CreatePATSingleProgram(*pat))
                    emit UpdatePATSingleProgram(PATSingleProgram());
            }
            else
            {
                ProgramAssociationTable pat(psip);
                emit UpdatePAT(&pat);
                if ((_desired_program >= 0) && CreatePATSingleProgram(pat))
                    emit UpdatePATSingleProgram(PATSingleProgram());
            }
            return true;
        }
        case TableID::PMT:
        {
            SetVersionPMT(pid, psip.Version());
            if (_cache_tables)
            {
                ProgramMapTable *pmt = new ProgramMapTable(psip);
                CachePMT(pid, pmt);
                emit UpdatePMT(pid, pmt);
                if (pid == _pid_pmt_single_program)
                {
                    if (CreatePMTSingleProgram(*pmt))
                        emit UpdatePMTSingleProgram(PMTSingleProgram());
                }
            }
            else
            {
                ProgramMapTable pmt(psip);
                emit UpdatePMT(pid, &pmt);
                if (pid == _pid_pmt_single_program)
                {
                    if (CreatePMTSingleProgram(pmt))
                        emit UpdatePMTSingleProgram(PMTSingleProgram());
                }
            }
            return true;
        }
    }
    return false;
}

/** \fn MPEGStreamData::HandleTSTables(const TSPacket*)
 *  \brief Assembles PSIP packets and processes them.
 */
bool MPEGStreamData::HandleTSTables(const TSPacket* tspacket)
{
#define HT_RETURN(HANDLED) { delete psip; return HANDLED; }
    // Assemble PSIP
    PSIPTable *psip = AssemblePSIP(tspacket);
    if (!psip)
        return true;

    // Validate PSIP
    // but don't validate PMT if our driver has the PMT CRC bug.
    bool validate = !_have_pmt_CRC_bug || (TableID::PMT != psip->TableID());
    if (validate && !psip->IsGood())
    {
        VERBOSE(VB_RECORD, QString("PSIP packet failed CRC check. "
                                   "pid(0x%1) type(0x%2)")
                .arg(tspacket->PID(),0,16).arg(psip->TableID(),0,16));
        HT_RETURN(true);
    }

    if (!psip->IsCurrent()) // we don't cache the next table, for now
        HT_RETURN(true);

    if (1!=tspacket->AdaptationFieldControl())
    { // payload only, ATSC req.
        VERBOSE(VB_RECORD,
                "PSIP packet has Adaptation Field Control, not ATSC compiant");
        HT_RETURN(true);
    }

    if (tspacket->ScramplingControl())
    { // scrambled! ATSC, DVB require tables not to be scrambled
        VERBOSE(VB_RECORD,
                "PSIP packet is scrambled, not ATSC/DVB compiant");
        HT_RETURN(true);
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
        HT_RETURN(true); // already parsed this table, toss it.
    }

    bool handled = HandleTables(tspacket->PID(), *psip);
    HT_RETURN(handled);
#undef HT_RETURN
}

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
    _cache_lock.lock();
    const ProgramAssociationTable *pat = _cached_pat;
    IncrementRefCnt(pat);
    _cache_lock.unlock();

    return pat;
}

const ProgramMapTable *MPEGStreamData::GetCachedPMT(uint pid) const
{
    ProgramMapTable *pmt = NULL;

    _cache_lock.lock();
    pmt_cache_t::const_iterator it = _cached_pmts.find(pid);
    if (it != _cached_pmts.end())
        IncrementRefCnt(pmt = *it);
    _cache_lock.unlock();

    return pmt;
}

vector<const ProgramMapTable*> MPEGStreamData::GetCachedPMTs(void) const
{
    vector<const ProgramMapTable*> pmts;

    _cache_lock.lock();
    pmt_cache_t::const_iterator it = _cached_pmts.begin();
    for (; it != _cached_pmts.end(); ++it)
    {
        ProgramMapTable* pmt = *it;
        IncrementRefCnt(pmt);
        pmts.push_back(pmt);
    }
    _cache_lock.unlock();

    return pmts;
}

void MPEGStreamData::ReturnCachedTable(const PSIPTable *psip) const
{
    // TODO
}

void MPEGStreamData::ReturnCachedTables(pmt_vec_t &pmts) const
{
    for (pmt_vec_t::iterator it = pmts.begin(); it != pmts.end(); ++it)
        ReturnCachedTable(*it);
    pmts.clear();
}

void MPEGStreamData::IncrementRefCnt(const PSIPTable *psip) const
{
    // TODO
}

void MPEGStreamData::DeleteCachedTable(PSIPTable *psip) const
{
    // TODO
}

void MPEGStreamData::CachePAT(ProgramAssociationTable *pat)
{
    _cache_lock.lock();
    DeleteCachedTable(_cached_pat);
    _cached_pat = pat;
    _cache_lock.unlock();
}

void MPEGStreamData::CachePMT(uint pid, ProgramMapTable *pmt)
{
    _cache_lock.lock();
    pmt_cache_t::iterator it = _cached_pmts.find(pid);
    if (it != _cached_pmts.end())
        DeleteCachedTable(*it);
    _cached_pmts[pid] = pmt;
    _cache_lock.unlock();
}
