// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson

#include <algorithm> // for find & max
using namespace std;

// POSIX headers
#include <sys/time.h> // for gettimeofday

// Qt headers
#include <QString>

// MythTV headers
#include "mpegstreamdata.h"
#include "mpegtables.h"
#include "ringbuffer.h"
#include "mpegtables.h"

#include "atscstreamdata.h"
#include "atsctables.h"

//#define DEBUG_MPEG_RADIO // uncomment to strip video streams from TS stream
#define LOC QString("MPEGStream[%1]: ").arg(_cardid)

void init_sections(sections_t &sect, uint last_section)
{
    static const unsigned char init_bits[8] =
        { 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00, };

    sect.clear();

    uint endz = last_section >> 3;
    if (endz)
        sect.resize(endz, 0x00);
    sect.resize(32, 0xff);
    sect[endz] = init_bits[last_section & 0x7];

#if 0
    {
        QString msg = QString("init_sections ls(%1): ").arg(last_section);
        for (uint i = 0 ; i < 32; i++)
            msg += QString("%1 ").arg((int)sect[i], 0, 16);
        LOG(VB_GENERAL, LOG_DEBUG, LOC + msg);
    }
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
MPEGStreamData::MPEGStreamData(int desiredProgram, int cardnum,
                               bool cacheTables)
    : _cardid(cardnum),
      _sistandard("mpeg"),
      _have_CRC_bug(false),
      _si_time_offset_cnt(0),
      _si_time_offset_indx(0),
      _eit_helper(NULL), _eit_rate(0.0f),
      _listening_disabled(false),
      _encryption_lock(QMutex::Recursive), _listener_lock(QMutex::Recursive),
      _cache_tables(cacheTables), _cache_lock(QMutex::Recursive),
      // Single program stuff
      _desired_program(desiredProgram),
      _recording_type("all"),
      _strip_pmt_descriptors(false),
      _normalize_stream_type(true),
      _pid_video_single_program(0xffffffff),
      _pid_pmt_single_program(0xffffffff),
      _pmt_single_program_num_video(1),
      _pmt_single_program_num_audio(0),
      _pat_single_program(NULL), _pmt_single_program(NULL),
      _invalid_pat_seen(false), _invalid_pat_warning(false)
{
    memset(_si_time_offsets, 0, sizeof(_si_time_offsets));

    AddListeningPID(MPEG_PAT_PID);
    AddListeningPID(MPEG_CAT_PID);
}

MPEGStreamData::~MPEGStreamData()
{
    ResetMPEG(-1);
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

    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetDesiredProgram(%2)").arg(p));

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
        ResetMPEG(p);
}

void MPEGStreamData::SetRecordingType(const QString &recording_type)
{
    _recording_type = recording_type;
    _recording_type.detach();
    uint neededVideo = (_recording_type == "tv")    ? 1 : 0;
    uint neededAudio = (_recording_type == "audio") ? 1 : 0;
    SetVideoStreamsRequired(neededVideo);
    SetAudioStreamsRequired(neededAudio);
}

QString MPEGStreamData::GetRecordingType(void) const
{
    QString tmp = _recording_type;
    tmp.detach();
    return tmp;
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

void MPEGStreamData::ResetMPEG(int desiredProgram)
{
    _desired_program       = desiredProgram;
    _recording_type        = "all";
    _strip_pmt_descriptors = false;
    _normalize_stream_type = true;

    _invalid_pat_seen = false;

    SetPATSingleProgram(NULL);
    SetPMTSingleProgram(NULL);

    pid_psip_map_t old = _partial_psip_packet_cache;
    pid_psip_map_t::iterator it = old.begin();
    for (; it != old.end(); ++it)
        DeletePartialPSIP(it.key());
    _partial_psip_packet_cache.clear();

    _pids_listening.clear();
    _pids_notlistening.clear();
    _pids_writing.clear();
    _pids_audio.clear();

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

        cat_cache_t::iterator it3 = _cached_cats.begin();
        for (; it3 != _cached_cats.end(); ++it3)
            DeleteCachedTable(*it3);
        _cached_cats.clear();
    }

    ResetDecryptionMonitoringState();

    AddListeningPID(MPEG_PAT_PID);
    AddListeningPID(MPEG_CAT_PID);
}

void MPEGStreamData::DeletePartialPSIP(uint pid)
{
    pid_psip_map_t::iterator it = _partial_psip_packet_cache.find(pid);
    if (it != _partial_psip_packet_cache.end())
    {
        PSIPTable *pkt = *it;
        _partial_psip_packet_cache.erase(it);
        delete pkt;
    }
}

/** \fn MPEGStreamData::AssemblePSIP(const TSPacket*,bool&)
 *  \brief PSIP packet assembler.
 *
 *   This is not a general purpose TS->PSIP packet converter,
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

    PSIPTable* partial = GetPartialPSIP(tspacket->PID());
    if (partial && partial->AddTSPacket(tspacket, broken) && !broken)
    {
        // check if it's safe to read pespacket's Length()
        if ((partial->PSIOffset() + 1 + 3) > partial->TSSizeInBuffer())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Discarding broken PSIP packet. Packet's length at "
                        "position %1 isn't in the buffer of %2 bytes.")
                    .arg(partial->PSIOffset() + 1 + 3)
                    .arg(partial->TSSizeInBuffer()));
            DeletePartialPSIP(tspacket->PID());
            return NULL;
        }

        // Discard broken packets
        bool buggy = _have_CRC_bug &&
        ((TableID::PMT == partial->StreamID()) ||
         (TableID::PAT == partial->StreamID()));
        if (!buggy && !partial->IsGood())
        {
            LOG(VB_SIPARSER, LOG_ERR, LOC + "Discarding broken PSIP packet");
            DeletePartialPSIP(tspacket->PID());
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
#if 0 /* This doesn't work, you can't start PSIP packet like this
         because the PayloadStart() flag won't be set in this TSPacket
         -- dtk  May 4th, 2007
       */

                // If the next section starts in the new tspacket
                // create a new partial packet to prevent overflow
                if ((partial->TSSizeInBuffer() > TSPacket::kSize) &&
                    (packetStart >
                     partial->TSSizeInBuffer() - TSPacket::PAYLOAD_SIZE))
                {
                    // Saving will handle deleting the old one
                    SavePartialPSIP(tspacket->PID(),
                                   new PSIPTable(*tspacket));
                }
                else
#endif
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
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Discarding broken PSIP packet. ") +
                QString("Packet with %1 bytes doesn't fit "
                        "into a buffer of %2 bytes.")
                    .arg(packetStart).arg(partial->TSSizeInBuffer()));
            delete psip;
            psip = NULL;
        }

        moreTablePackets = false;
        DeletePartialPSIP(tspacket->PID());
        return psip;
    }
    else if (partial)
    {
        if (broken)
            DeletePartialPSIP(tspacket->PID());

        moreTablePackets = false;
        return 0; // partial packet is not yet complete.
    }

    if (!tspacket->PayloadStart())
    {
        // We didn't see this PSIP packet's start, so this must be the
        // tail end of something we missed. Ignore it.
        moreTablePackets = false;
        return 0;
    }

    const int offset = tspacket->AFCOffset() + tspacket->StartOfFieldPointer();
    if (offset>181)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error: offset>181, pes length & "
                "current cannot be queried");
        return 0;
    }

    // table_id (8 bits) and section_length(12), syntax(1), priv(1), res(2)
    // pointer_field (+8 bits), since payload start is true if we are here.
    const int extra_offset = 4;

    const unsigned char* pesdata = tspacket->data() + offset;
    const int pes_length = (pesdata[2] & 0x0f) << 8 | pesdata[3];
    if ((pes_length + offset + extra_offset) > 188)
    {
        SavePartialPSIP(tspacket->PID(), new PSIPTable(*tspacket));
        moreTablePackets = false;
        return 0;
    }

    PSIPTable *psip = new PSIPTable(*tspacket); // must be complete packet

    // There might be another section after this one in the
    // current packet. We need room before the end of the
    // packet, and it must not be packet stuffing.
    if ((offset + psip->SectionLength() < TSPacket::kSize) &&
        (pesdata[psip->SectionLength() + 1] != 0xff))
    {
        // This isn't stuffing, so we need to put this
        // on as a partial packet.
        PSIPTable *pesp = new PSIPTable(*tspacket);
        pesp->SetPSIOffset(offset + psip->SectionLength());
        SavePartialPSIP(tspacket->PID(), pesp);
        return psip;
    }

    moreTablePackets = false;
    return psip;
}

bool MPEGStreamData::CreatePATSingleProgram(
    const ProgramAssociationTable& pat)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "CreatePATSingleProgram()");
    LOG(VB_RECORD, LOG_INFO, LOC + "PAT in input stream");
    LOG(VB_RECORD, LOG_INFO, LOC + pat.toString());
    if (_desired_program < 0)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Desired program not set yet");
        return false;
    }
    _pid_pmt_single_program = pat.FindPID(_desired_program);
    LOG(VB_RECORD, LOG_INFO, LOC + QString("desired_program(%1) pid(0x%2)").
            arg(_desired_program).arg(_pid_pmt_single_program, 0, 16));

    if (!_pid_pmt_single_program)
    {
        _pid_pmt_single_program = pat.FindAnyPID();
        if (!_pid_pmt_single_program)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "No program found in PAT. "
                                     "This recording will not play in MythTV.");
        }
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Desired program #%1 not found in PAT."
                    "\n\t\t\tCannot create single program PAT.")
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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "MPEGStreamData::CreatePATSingleProgram: "
            "Failed to create Program Association Table.");
        return false;
    }

    pat2->tsheader()->SetContinuityCounter(pat.tsheader()->ContinuityCounter());

    LOG(VB_RECORD, LOG_INFO, LOC + QString("pmt_pid(0x%1)")
            .arg(_pid_pmt_single_program, 0, 16));
    LOG(VB_RECORD, LOG_INFO, LOC + "PAT for output stream");
    LOG(VB_RECORD, LOG_INFO, LOC + pat2->toString());

    SetPATSingleProgram(pat2);

    return true;

}

static desc_list_t extract_atsc_desc(const tvct_vec_t &tvct,
                              const cvct_vec_t &cvct,
                              uint pnum)
{
    desc_list_t desc;

    vector<const VirtualChannelTable*> vct;

    for (uint i = 0; i < tvct.size(); i++)
        vct.push_back(tvct[i]);

    for (uint i = 0; i < cvct.size(); i++)
        vct.push_back(cvct[i]);

    for (uint i = 0; i < tvct.size(); i++)
    {
        for (uint j = 0; j < vct[i]->ChannelCount(); j++)
        {
            if (vct[i]->ProgramNumber(j) == pnum)
            {
                desc_list_t ldesc = MPEGDescriptor::ParseOnlyInclude(
                    vct[i]->Descriptors(j), vct[i]->DescriptorsLength(j),
                    DescriptorID::caption_service);

                if (!ldesc.empty())
                    desc.insert(desc.end(), ldesc.begin(), ldesc.end());
            }
        }

        if (0 != vct[i]->GlobalDescriptorsLength())
        {
            desc_list_t vdesc = MPEGDescriptor::ParseOnlyInclude(
                vct[i]->GlobalDescriptors(),
                vct[i]->GlobalDescriptorsLength(),
                DescriptorID::caption_service);

            if (!vdesc.empty())
                desc.insert(desc.end(), vdesc.begin(), vdesc.end());
        }
    }

    return desc;
}

bool MPEGStreamData::CreatePMTSingleProgram(const ProgramMapTable &pmt)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "CreatePMTSingleProgram()");
    LOG(VB_RECORD, LOG_INFO, LOC + "PMT in input stream");
    LOG(VB_RECORD, LOG_INFO, LOC + pmt.toString());

    if (!PATSingleProgram())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "no PAT yet...");
        return false; // no way to properly rewrite pids without PAT
    }
    pmt.Parse();

    uint programNumber = 1; // MPEG Program Number

    ATSCStreamData *sd = NULL;
    tvct_vec_t tvct;
    cvct_vec_t cvct;

    desc_list_t gdesc;

    if (!_strip_pmt_descriptors)
    {
        gdesc = MPEGDescriptor::ParseAndExclude(
            pmt.ProgramInfo(), pmt.ProgramInfoLength(),
            DescriptorID::conditional_access);

        // If there is no caption descriptor in PMT, copy any caption
        // descriptor found in VCT to global descriptors...
        sd = dynamic_cast<ATSCStreamData*>(this);
        if (sd && !MPEGDescriptor::Find(gdesc, DescriptorID::caption_service))
        {
            tvct = sd->GetCachedTVCTs();
            cvct = sd->GetCachedCVCTs();

            desc_list_t vdesc = extract_atsc_desc(
                tvct, cvct, pmt.ProgramNumber());

            if (!vdesc.empty())
                gdesc.insert(gdesc.end(), vdesc.begin(), vdesc.end());
        }
    }

    vector<uint> pids;
    vector<uint> types;
    vector<desc_list_t> pdesc;

    uint video_cnt = 0;
    uint audio_cnt = 0;

    vector<uint> videoPIDs, audioPIDs, dataPIDs;

    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        uint pid = pmt.StreamPID(i);

        desc_list_t desc = MPEGDescriptor::ParseAndExclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::conditional_access);

        uint type = StreamID::Normalize(
            pmt.StreamType(i), desc, _sistandard);

        bool is_video = StreamID::IsVideo(type);
        bool is_audio = StreamID::IsAudio(type);

        if (is_audio)
        {
            audio_cnt++;
            audioPIDs.push_back(pid);
        }

#ifdef DEBUG_MPEG_RADIO
        if (is_video)
            continue;
#endif // DEBUG_MPEG_RADIO

        if (is_video)
        {
            video_cnt++;
            videoPIDs.push_back(pid);
        }

        if (_strip_pmt_descriptors)
            desc.clear();

        // Filter out streams not used for basic television
        if (_recording_type == "tv" && !is_audio && !is_video &&
            !MPEGDescriptor::Find(desc, DescriptorID::teletext) &&
            !MPEGDescriptor::Find(desc, DescriptorID::subtitling))
        {
            continue;
        }

        if (!is_audio && !is_video)
            dataPIDs.push_back(pid);

        pdesc.push_back(desc);
        pids.push_back(pid);
        types.push_back(type);
    }

    if (video_cnt < _pmt_single_program_num_video)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Only %1 video streams seen in PMT, but %2 are required.")
                .arg(video_cnt).arg(_pmt_single_program_num_video));
        return false;
    }

    if (audioPIDs.size() < _pmt_single_program_num_audio)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Only %1 audio streams seen in PMT, but %2 are required.")
                .arg(audioPIDs.size()).arg(_pmt_single_program_num_audio));
        return false;
    }

    desc_list_t cdesc = MPEGDescriptor::ParseOnlyInclude(
        pmt.ProgramInfo(), pmt.ProgramInfoLength(),
        DescriptorID::conditional_access);
    for (uint i = 0; i < cdesc.size(); i++)
    {
        ConditionalAccessDescriptor cad(cdesc[i]);
        if (cad.IsValid())
            AddListeningPID(cad.PID());
    }

    _pids_audio.clear();
    for (uint i = 0; i < audioPIDs.size(); i++)
        AddAudioPID(audioPIDs[i]);

    if (videoPIDs.size() >= 1)
        _pid_video_single_program = videoPIDs[0];
    for (uint i = 1; i < videoPIDs.size(); i++)
        AddWritingPID(videoPIDs[i]);

    for (uint i = 0; i < dataPIDs.size(); i++)
        AddWritingPID(dataPIDs[i]);

    // Timebase
    int pcrpidIndex = pmt.FindPID(pmt.PCRPID());
    if (pcrpidIndex < 0)
    {
        // the timecode reference stream is not in the PMT,
        // add stream to misc record streams
        AddWritingPID(pmt.PCRPID());
    }

    // Create the PMT
    ProgramMapTable *pmt2 = ProgramMapTable::Create(
        programNumber, _pid_pmt_single_program, pmt.PCRPID(),
        pmt.Version(), gdesc, pids, types, pdesc);

    // Return any TVCT & CVCT tables, once we've copied any descriptors.
    if (sd)
    {
        sd->ReturnCachedTVCTTables(tvct);
        sd->ReturnCachedCVCTTables(cvct);
    }

    // Set Continuity Header
    uint cc_cnt = pmt.tsheader()->ContinuityCounter();
    pmt2->tsheader()->SetContinuityCounter(cc_cnt);
    SetPMTSingleProgram(pmt2);

    LOG(VB_RECORD, LOG_INFO, LOC + "PMT for output stream");
    LOG(VB_RECORD, LOG_INFO, LOC + pmt2->toString());

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
    {
        if (VersionCAT(psip.TableIDExtension()) != version)
           return false;
        return CATSectionSeen(psip.TableIDExtension(), psip.Section());
    }

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
            uint tsid = psip.TableIDExtension();
            SetVersionCAT(tsid, version, psip.LastSection());
            SetCATSectionSeen(tsid, psip.Section());

            ConditionalAccessTable cat(psip);

            if (_cache_tables)
                CacheCAT(&cat);

            ProcessCAT(&cat);

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
        case TableID::SITscte:
        {
            SpliceInformationTable sit(psip);

            _listener_lock.lock();
            for (uint i = 0; i < _mpeg_listeners.size(); i++)
                _mpeg_listeners[i]->HandleSplice(&sit);
            _listener_lock.unlock();

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
        LOG(VB_RECORD, LOG_WARNING, LOC +
            "ProcessPAT: PAT is missing program, setting timeout");
    }
    else if (_invalid_pat_seen && !foundProgram &&
             (_invalid_pat_timer.elapsed() > 400) && !_invalid_pat_warning)
    {
        _invalid_pat_warning = true; // only emit one warning...
        // After 400ms emit error if we haven't found correct PAT.
        LOG(VB_GENERAL, LOG_ERR, LOC + "ProcessPAT: Program not found in PAT. "
            "Rescan your transports.");

        send_single_program = CreatePATSingleProgram(*pat);
    }
    else if (foundProgram)
    {
        if (_invalid_pat_seen)
            LOG(VB_RECORD, LOG_INFO, LOC +
                "ProcessPAT: Good PAT seen after a bad PAT");

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

void MPEGStreamData::ProcessCAT(const ConditionalAccessTable *cat)
{
    _listener_lock.lock();
    for (uint i = 0; i < _mpeg_listeners.size(); i++)
        _mpeg_listeners[i]->HandleCAT(cat);
    _listener_lock.unlock();

    desc_list_t cdesc = MPEGDescriptor::ParseOnlyInclude(
        cat->Descriptors(), cat->DescriptorsLength(),
        DescriptorID::conditional_access);
    for (uint i = 0; i < cdesc.size(); i++)
    {
        ConditionalAccessDescriptor cad(cdesc[i]);
        if (cad.IsValid())
            AddListeningPID(cad.PID());
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

double MPEGStreamData::TimeOffset(void) const
{
    QMutexLocker locker(&_si_time_lock);
    if (!_si_time_offset_cnt)
        return 0.0;

    double avg_offset = 0.0;
    double mult = 1.0 / _si_time_offset_cnt;
    for (uint i = 0; i < _si_time_offset_cnt; i++)
        avg_offset += _si_time_offsets[i] * mult;

    return avg_offset;
}

void MPEGStreamData::UpdateTimeOffset(uint64_t _si_utc_time)
{
    struct timeval tm;
    if (gettimeofday(&tm, NULL) != 0)
        return;

    double utc_time = tm.tv_sec + (tm.tv_usec * 0.000001);
    double si_time  = _si_utc_time;

    QMutexLocker locker(&_si_time_lock);
    _si_time_offsets[_si_time_offset_indx] = si_time - utc_time;

    if (_si_time_offset_indx + 1 > _si_time_offset_cnt)
        _si_time_offset_cnt = _si_time_offset_indx + 1;

    _si_time_offset_indx = (_si_time_offset_indx + 1) & 0xf;

}

#define DONE_WITH_PSIP_PACKET() { if (psip) delete psip; \
    if (morePSIPTables) goto HAS_ANOTHER_PSIP; else return; }

/** \fn MPEGStreamData::HandleTSTables(const TSPacket*)
 *  \brief Assembles PSIP packets and processes them.
 */
void MPEGStreamData::HandleTSTables(const TSPacket* tspacket)
{
    bool morePSIPTables;
  HAS_ANOTHER_PSIP:
    // Assemble PSIP
    PSIPTable *psip = AssemblePSIP(tspacket, morePSIPTables);
    if (!psip)
       return;

    // drop stuffing packets
    if ((TableID::ST       == psip->TableID()) ||
        (TableID::STUFFING == psip->TableID()))
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "Dropping Stuffing table");
        DONE_WITH_PSIP_PACKET();
    }

    // Don't do validation on tables withotu CRC
    if (!psip->HasCRC())
    {
        HandleTables(tspacket->PID(), *psip);
        DONE_WITH_PSIP_PACKET();
    }

    // Validate PSIP
    // but don't validate PMT/PAT if our driver has the PMT/PAT CRC bug.
    bool buggy = _have_CRC_bug &&
        ((TableID::PMT == psip->TableID()) ||
         (TableID::PAT == psip->TableID()));
    if (!buggy && !psip->IsGood())
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("PSIP packet failed CRC check. pid(0x%1) type(0x%2)")
                .arg(tspacket->PID(),0,16).arg(psip->TableID(),0,16));
        DONE_WITH_PSIP_PACKET();
    }

    if (TableID::MGT <= psip->TableID() && psip->TableID() <= TableID::STT &&
        !psip->IsCurrent())
    { // we don't cache the next table, for now
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("Table not current 0x%1")
            .arg(psip->TableID(),2,16,QChar('0')));
        DONE_WITH_PSIP_PACKET();
    }

    if (tspacket->Scrambled())
    { // scrambled! ATSC, DVB require tables not to be scrambled
        LOG(VB_RECORD, LOG_ERR, LOC +
            "PSIP packet is scrambled, not ATSC/DVB compiant");
        DONE_WITH_PSIP_PACKET();
    }

    if (!psip->VerifyPSIP(!_have_CRC_bug))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("PSIP table 0x%1 is invalid")
            .arg(psip->TableID(),2,16,QChar('0')));
        DONE_WITH_PSIP_PACKET();
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
        DONE_WITH_PSIP_PACKET(); // already parsed this table, toss it.
    }

    HandleTables(tspacket->PID(), *psip);

    DONE_WITH_PSIP_PACKET();
}
#undef DONE_WITH_PSIP_PACKET

int MPEGStreamData::ProcessData(const unsigned char *buffer, int len)
{
    int pos = 0;
    bool resync = false;

    while (pos + int(TSPacket::kSize) <= len)
    { // while we have a whole packet left...
        if (buffer[pos] != SYNC_BYTE || resync)
        {
            int newpos = ResyncStream(buffer, pos+1, len);
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Resyncing @ %1+1 w/len %2 -> %3")
                .arg(pos).arg(len).arg(newpos));
            if (newpos == -1)
                return len - pos;
            if (newpos == -2)
                return TSPacket::kSize;
            pos = newpos;
        }

        const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[pos]);
        pos += TSPacket::kSize; // Advance to next TS packet
        resync = false;
        if (!ProcessTSPacket(*pkt))
        {
            if (pos + int(TSPacket::kSize) > len)
                continue;
            if (buffer[pos] != SYNC_BYTE)
            {
                // if ProcessTSPacket fails, and we don't appear to be
                // in sync on the next packet, then resync. Otherwise
                // just process the next packet normally.
                pos -= TSPacket::kSize;
                resync = true;
            }
        }
    }

    return len - pos;
}

bool MPEGStreamData::ProcessTSPacket(const TSPacket& tspacket)
{
    bool ok = !tspacket.TransportError();

    if (IsEncryptionTestPID(tspacket.PID()))
    {
        ProcessEncryptedPacket(tspacket);
    }

    if (!ok)
        return false;

    if (tspacket.Scrambled())
        return true;

    if (IsVideoPID(tspacket.PID()))
    {
        for (uint j = 0; j < _ts_av_listeners.size(); j++)
            _ts_av_listeners[j]->ProcessVideoTSPacket(tspacket);

        return true;
    }

    if (IsAudioPID(tspacket.PID()))
    {
        for (uint j = 0; j < _ts_av_listeners.size(); j++)
            _ts_av_listeners[j]->ProcessAudioTSPacket(tspacket);

        return true;
    }

    if (IsWritingPID(tspacket.PID()))
    {
        for (uint j = 0; j < _ts_writing_listeners.size(); j++)
            _ts_writing_listeners[j]->ProcessTSPacket(tspacket);
    }

    if (IsListeningPID(tspacket.PID()) && tspacket.HasPayload())
    {
        HandleTSTables(&tspacket);
    }

    return true;
}

int MPEGStreamData::ResyncStream(const unsigned char *buffer, int curr_pos,
                                 int len)
{
    // Search for two sync bytes 188 bytes apart,
    int pos = curr_pos;
    int nextpos = pos + TSPacket::kSize;
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
    if (_listening_disabled || IsNotListeningPID(pid))
        return false;
    pid_map_t::const_iterator it = _pids_listening.find(pid);
    return it != _pids_listening.end();
}

bool MPEGStreamData::IsNotListeningPID(uint pid) const
{
    pid_map_t::const_iterator it = _pids_notlistening.find(pid);
    return it != _pids_notlistening.end();
}

bool MPEGStreamData::IsWritingPID(uint pid) const
{
    pid_map_t::const_iterator it = _pids_writing.find(pid);
    return it != _pids_writing.end();
}

bool MPEGStreamData::IsAudioPID(uint pid) const
{
    pid_map_t::const_iterator it = _pids_audio.find(pid);
    return it != _pids_audio.end();
}

uint MPEGStreamData::GetPIDs(pid_map_t &pids) const
{
    uint sz = pids.size();

    if (_pid_video_single_program < 0x1fff)
        pids[_pid_video_single_program] = kPIDPriorityHigh;

    pid_map_t::const_iterator it = _pids_listening.begin();
    for (; it != _pids_listening.end(); ++it)
        pids[it.key()] = max(pids[it.key()], *it);

    it = _pids_audio.begin();
    for (; it != _pids_audio.end(); ++it)
        pids[it.key()] = max(pids[it.key()], *it);

    it = _pids_writing.begin();
    for (; it != _pids_writing.end(); ++it)
        pids[it.key()] = max(pids[it.key()], *it);

    return pids.size() - sz;
}

PIDPriority MPEGStreamData::GetPIDPriority(uint pid) const
{
    if (_pid_video_single_program == pid)
        return kPIDPriorityHigh;

    pid_map_t::const_iterator it;
    it = _pids_listening.find(pid);
    if (it != _pids_listening.end())
        return *it;
    it = _pids_notlistening.find(pid);
    if (it != _pids_notlistening.end())
        return *it;
    it = _pids_writing.find(pid);
    if (it != _pids_writing.end())
        return *it;
    it = _pids_audio.find(pid);
    if (it != _pids_audio.end())
        return *it;

    return kPIDPriorityNone;
}

void MPEGStreamData::SavePartialPSIP(uint pid, PSIPTable* packet)
{
    pid_psip_map_t::iterator it = _partial_psip_packet_cache.find(pid);
    if (it == _partial_psip_packet_cache.end())
        _partial_psip_packet_cache[pid] = packet;
    else
    {
        PSIPTable *old = *it;
        _partial_psip_packet_cache.remove(pid);
        _partial_psip_packet_cache.insert(pid, packet);
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

void MPEGStreamData::SetCATSectionSeen(uint tsid, uint section)
{
    sections_map_t::iterator it = _cat_section_seen.find(tsid);
    if (it == _cat_section_seen.end())
    {
        _cat_section_seen[tsid].resize(32, 0);
        it = _cat_section_seen.find(tsid);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool MPEGStreamData::CATSectionSeen(uint tsid, uint section) const
{
    sections_map_t::const_iterator it = _cat_section_seen.find(tsid);
    if (it == _cat_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool MPEGStreamData::HasAllCATSections(uint tsid) const
{
    sections_map_t::const_iterator it = _cat_section_seen.find(tsid);
    if (it == _cat_section_seen.end())
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
    pmt_const_ptr_t pmt = GetCachedPMT(progNum, 0);
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

bool MPEGStreamData::HasCachedAllCAT(uint tsid) const
{
    QMutexLocker locker(&_cache_lock);

    cat_cache_t::const_iterator it = _cached_cats.find(tsid << 8);
    if (it == _cached_cats.end())
        return false;

    uint last_section = (*it)->LastSection();
    if (!last_section)
        return true;

    for (uint i = 1; i <= last_section; i++)
        if (_cached_cats.find((tsid << 8) | i) == _cached_cats.end())
            return false;

    return true;
}

bool MPEGStreamData::HasCachedAnyCAT(uint tsid) const
{
    QMutexLocker locker(&_cache_lock);

    for (uint i = 0; i <= 255; i++)
        if (_cached_cats.find((tsid << 8) | i) != _cached_cats.end())
            return true;

    return false;
}

bool MPEGStreamData::HasCachedAnyCAT(void) const
{
    QMutexLocker locker(&_cache_lock);
    return _cached_cats.size();
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
        {
            uint prognum = pat->ProgramNumber(i);
            if (prognum && !HasCachedAllPMT(prognum))
                return false;
        }
    }

    return true;
}

bool MPEGStreamData::HasCachedAnyPMTs(void) const
{
    QMutexLocker locker(&_cache_lock);
    return _cached_pmts.size();
}

pat_const_ptr_t MPEGStreamData::GetCachedPAT(uint tsid, uint section_num) const
{
    QMutexLocker locker(&_cache_lock);
    ProgramAssociationTable *pat = NULL;

    uint key = (tsid << 8) | section_num;
    pat_cache_t::const_iterator it = _cached_pats.find(key);
    if (it != _cached_pats.end())
        IncrementRefCnt(pat = *it);

    return pat;
}

pat_vec_t MPEGStreamData::GetCachedPATs(uint tsid) const
{
    QMutexLocker locker(&_cache_lock);
    pat_vec_t pats;

    for (uint i=0; i < 256; i++)
    {
        pat_const_ptr_t pat = GetCachedPAT(tsid, i);
        if (pat)
            pats.push_back(pat);
    }

    return pats;
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

cat_const_ptr_t MPEGStreamData::GetCachedCAT(uint tsid, uint section_num) const
{
    QMutexLocker locker(&_cache_lock);
    ConditionalAccessTable *cat = NULL;

    uint key = (tsid << 8) | section_num;
    cat_cache_t::const_iterator it = _cached_cats.find(key);
    if (it != _cached_cats.end())
        IncrementRefCnt(cat = *it);

    return cat;
}

cat_vec_t MPEGStreamData::GetCachedCATs(uint tsid) const
{
    QMutexLocker locker(&_cache_lock);
    cat_vec_t cats;

    for (uint i=0; i < 256; i++)
    {
        cat_const_ptr_t cat = GetCachedCAT(tsid, i);
        if (cat)
            cats.push_back(cat);
    }

    return cats;
}

cat_vec_t MPEGStreamData::GetCachedCATs(void) const
{
    QMutexLocker locker(&_cache_lock);
    cat_vec_t cats;

    cat_cache_t::const_iterator it = _cached_cats.begin();
    for (; it != _cached_cats.end(); ++it)
    {
        ConditionalAccessTable* cat = *it;
        IncrementRefCnt(cat);
        cats.push_back(cat);
    }

    return cats;
}

pmt_const_ptr_t MPEGStreamData::GetCachedPMT(
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
            DeleteCachedTable(const_cast<PSIPTable*>(psip));
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

void MPEGStreamData::ReturnCachedCATTables(cat_vec_t &cats) const
{
    for (cat_vec_t::iterator it = cats.begin(); it != cats.end(); ++it)
        ReturnCachedTable(*it);
    cats.clear();
}

void MPEGStreamData::ReturnCachedCATTables(cat_map_t &cats) const
{
    for (cat_map_t::iterator it = cats.begin(); it != cats.end(); ++it)
        ReturnCachedCATTables(*it);
    cats.clear();
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

bool MPEGStreamData::DeleteCachedTable(PSIPTable *psip) const
{
    if (!psip)
        return false;

    uint tid = psip->TableIDExtension();

    QMutexLocker locker(&_cache_lock);
    if (_cached_ref_cnt[psip] > 0)
    {
        _cached_slated_for_deletion[psip] = 1;
        return false;
    }
    else if (TableID::PAT == psip->TableID() &&
             (_cached_pats[(tid << 8) | psip->Section()] == psip))
    {
        _cached_pats[(tid << 8) | psip->Section()] = NULL;
        delete psip;
    }
    else if (TableID::CAT == psip->TableID() &&
             (_cached_cats[(tid << 8) | psip->Section()] == psip))
    {
        _cached_cats[(tid << 8) | psip->Section()] = NULL;
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
        return false;
    }
    psip_refcnt_map_t::iterator it;
    it = _cached_slated_for_deletion.find(psip);
    if (it != _cached_slated_for_deletion.end())
        _cached_slated_for_deletion.erase(it);

    return true;
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

void MPEGStreamData::CacheCAT(const ConditionalAccessTable *_cat)
{
    ConditionalAccessTable *cat = new ConditionalAccessTable(*_cat);
    uint key = (_cat->TableIDExtension() << 8) | _cat->Section();

    QMutexLocker locker(&_cache_lock);

    cat_cache_t::iterator it = _cached_cats.find(key);
    if (it != _cached_cats.end())
        DeleteCachedTable(*it);

    _cached_cats[key] = cat;
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

void MPEGStreamData::AddWritingListener(TSPacketListener *val)
{
    QMutexLocker locker(&_listener_lock);

    ts_listener_vec_t::iterator it = _ts_writing_listeners.begin();
    for (; it != _ts_writing_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _ts_writing_listeners.push_back(val);
}

void MPEGStreamData::RemoveWritingListener(TSPacketListener *val)
{
    QMutexLocker locker(&_listener_lock);

    ts_listener_vec_t::iterator it = _ts_writing_listeners.begin();
    for (; it != _ts_writing_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _ts_writing_listeners.erase(it);
            return;
        }
    }
}

void MPEGStreamData::AddAVListener(TSPacketListenerAV *val)
{
    QMutexLocker locker(&_listener_lock);

    ts_av_listener_vec_t::iterator it = _ts_av_listeners.begin();
    for (; it != _ts_av_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _ts_av_listeners.push_back(val);
}

void MPEGStreamData::RemoveAVListener(TSPacketListenerAV *val)
{
    QMutexLocker locker(&_listener_lock);

    ts_av_listener_vec_t::iterator it = _ts_av_listeners.begin();
    for (; it != _ts_av_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _ts_av_listeners.erase(it);
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

void MPEGStreamData::AddEncryptionTestPID(uint pnum, uint pid, bool isvideo)
{
    QMutexLocker locker(&_encryption_lock);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("AddEncryptionTestPID(%1, 0x%2)")
            .arg(pnum) .arg(pid, 0, 16));
#endif

    AddListeningPID(pid);

    _encryption_pid_to_info[pid] = CryptInfo((isvideo) ? 10000 : 500, 8);

    _encryption_pid_to_pnums[pid].push_back(pnum);
    _encryption_pnum_to_pids[pnum].push_back(pid);
    _encryption_pnum_to_status[pnum] = kEncUnknown;
}

void MPEGStreamData::RemoveEncryptionTestPIDs(uint pnum)
{
    QMutexLocker locker(&_encryption_lock);

#if 0
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Tearing down up decryption monitoring for program %1")
            .arg(pnum));
#endif

    QMap<uint, uint_vec_t>::iterator list;
    uint_vec_t::iterator it;

    uint_vec_t pids = _encryption_pnum_to_pids[pnum];
    for (uint i = 0; i < pids.size(); i++)
    {
        uint pid = pids[i];

#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Removing 0x%1 PID Enc monitoring").arg(pid,0,16));
#endif

        RemoveListeningPID(pid);

        list = _encryption_pid_to_pnums.find(pid);
        if (list != _encryption_pid_to_pnums.end())
        {
            it = find((*list).begin(), (*list).end(), pnum);

            if (it != (*list).end())
                (*list).erase(it);

            if ((*list).empty())
            {
                _encryption_pid_to_pnums.remove(pid);
                _encryption_pid_to_info.remove(pid);
            }
        }
    }

    _encryption_pnum_to_pids.remove(pnum);
}

bool MPEGStreamData::IsEncryptionTestPID(uint pid) const
{
    QMutexLocker locker(&_encryption_lock);

    QMap<uint, CryptInfo>::const_iterator it =
        _encryption_pid_to_info.find(pid);

    return it != _encryption_pid_to_info.end();
}

void MPEGStreamData::TestDecryption(const ProgramMapTable *pmt)
{
    QMutexLocker locker(&_encryption_lock);

#if 0
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Setting up decryption monitoring for program %1")
            .arg(pmt->ProgramNumber()));
#endif

    bool encrypted = pmt->IsProgramEncrypted();
    for (uint i = 0; i < pmt->StreamCount(); i++)
    {
        if (!encrypted && !pmt->IsStreamEncrypted(i))
            continue;

        bool is_vid = pmt->IsVideo(i, _sistandard);
        bool is_aud = pmt->IsAudio(i, _sistandard);
        if (is_vid || is_aud)
        {
            AddEncryptionTestPID(
                pmt->ProgramNumber(), pmt->StreamPID(i), is_vid);
        }
    }
}

void MPEGStreamData::ResetDecryptionMonitoringState(void)
{
    QMutexLocker locker(&_encryption_lock);

    _encryption_pid_to_info.clear();
    _encryption_pid_to_pnums.clear();
    _encryption_pnum_to_pids.clear();
}

bool MPEGStreamData::IsProgramDecrypted(uint pnum) const
{
    QMutexLocker locker(&_encryption_lock);
    return _encryption_pnum_to_status[pnum] == kEncDecrypted;
}

bool MPEGStreamData::IsProgramEncrypted(uint pnum) const
{
    QMutexLocker locker(&_encryption_lock);
    return _encryption_pnum_to_status[pnum] == kEncEncrypted;
}

static QString toString(CryptStatus status)
{
    if (kEncDecrypted == status)
        return "Decrypted";
    else if (kEncEncrypted == status)
        return "Encrypted";
    else
        return "Unknown";
}

/** \fn MPEGStreamData::ProcessEncryptedPacket(const TSPacket& tspacket)
 *  \brief counts en/decrypted packets to decide if a stream is en/decrypted
 */
void MPEGStreamData::ProcessEncryptedPacket(const TSPacket& tspacket)
{
    QMutexLocker locker(&_encryption_lock);

    const uint pid = tspacket.PID();
    CryptInfo &info = _encryption_pid_to_info[pid];

    CryptStatus status = kEncUnknown;

    if (tspacket.Scrambled())
    {
        info.decrypted_packets = 0;

        // If a fair amount of encrypted packets is passed assume that
        // the stream is not decryptable
        if (++info.encrypted_packets >= info.encrypted_min)
            status = kEncEncrypted;
    }
    else
    {
        info.encrypted_packets = 0;
        if (++info.decrypted_packets > info.decrypted_min)
            status = kEncDecrypted;
    }

    if (status == info.status)
        return; // pid encryption status unchanged

    info.status = status;

    LOG(status != kEncDecrypted ? VB_GENERAL : VB_RECORD, LOG_INFO, LOC +
        QString("PID 0x%1 status: %2") .arg(pid,0,16).arg(toString(status)));

    uint_vec_t pnum_del_list;
    const uint_vec_t &pnums = _encryption_pid_to_pnums[pid];
    for (uint i = 0; i < pnums.size(); i++)
    {
        CryptStatus status = _encryption_pnum_to_status[pnums[i]];

        const uint_vec_t &pids = _encryption_pnum_to_pids[pnums[i]];
        if (!pids.empty())
        {
            uint enc_cnt[3] = { 0, 0, 0 };
            for (uint j = 0; j < pids.size(); j++)
            {
                CryptStatus stat = _encryption_pid_to_info[pids[j]].status;
                enc_cnt[stat]++;

#if 0
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("\tpnum %1 PID 0x%2 status: %3")
                        .arg(pnums[i]).arg(pids[j],0,16) .arg(toString(stat)));
#endif
            }
            status = kEncUnknown;

            if (enc_cnt[kEncEncrypted])
                status = kEncEncrypted;
            else if (enc_cnt[kEncDecrypted] >= min((size_t) 2, pids.size()))
                status = kEncDecrypted;
        }

        if (status == _encryption_pnum_to_status[pnums[i]])
            continue; // program encryption status unchanged

        LOG(VB_RECORD, LOG_INFO, LOC + QString("Program %1 status: %2")
                .arg(pnums[i]).arg(toString(status)));

        _encryption_pnum_to_status[pnums[i]] = status;

        bool encrypted = kEncUnknown == status || kEncEncrypted == status;
        _listener_lock.lock();
        for (uint j = 0; j < _mpeg_listeners.size(); j++)
            _mpeg_listeners[j]->HandleEncryptionStatus(pnums[i], encrypted);
        _listener_lock.unlock();

        if (kEncDecrypted == status)
            pnum_del_list.push_back(pnums[i]);
    }

    for (uint i = 0; i < pnum_del_list.size(); i++)
        RemoveEncryptionTestPIDs(pnums[i]);
}
