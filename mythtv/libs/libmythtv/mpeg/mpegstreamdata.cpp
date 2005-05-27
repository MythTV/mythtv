// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "mpegstreamdata.h"
#include "mpegtables.h"
#include "RingBuffer.h"
#include "mpegtables.h"

MPEGStreamData::~MPEGStreamData()
{
    if (_pat)
        delete _pat;
    if (_pmt)
        delete _pmt;
}

void MPEGStreamData::Reset(int desiredProgram)
{
    _desired_program = desiredProgram;
    SetPAT(0);
    SetPMT(0);
    //for (...) delete (partial_pes_packets::iterator); // TODO delete old PES packets
    _partial_pes_packet_cache.clear();
    _pids_listening.clear();
    
    _pids_listening[MPEG_PAT_PID] = true;
    _pids_listening[ATSC_PSIP_PID] = true;

    _pids_notlistening.clear();
    _pids_writing.clear();
    
    _pid_video = _pid_pmt = 0xffffffff;
}

void MPEGStreamData::DeletePartialPES(unsigned int pid) {
    PESPacket *pkt = _partial_pes_packet_cache[pid];
    if (pkt) {
        delete pkt;
        ClearPartialPES(pid);
    }
}

/// This only assembles PES packets when new PES packets
/// start in their own TSPacket. This is always true of
/// ATSC & DVB tables, but not true of Audio and Video
/// PES packets.
PSIPTable* MPEGStreamData::AssemblePSIP(const TSPacket* tspacket) {
    if (tspacket->PayloadStart()) {
        const int offset = tspacket->AFCOffset() + tspacket->StartOfFieldPointer();
        if (offset>181) {
            VERBOSE(VB_IMPORTANT, "Error: offset>181, pes length & current can not be queried");
            return 0;
        }
        const unsigned char* pesdata = tspacket->data() + offset;
        const int pes_length = (pesdata[2] & 0x0f) << 8 | pesdata[3];
        const PESPacket pes = PESPacket::View(*tspacket);
        if ((pes_length+offset)>188) {
            if (!bool(pes.pesdata()[6]&1)/*current*/)
                return 0; // we only care about current psip packets for now
            SavePartialPES(tspacket->PID(), new PESPacket(*tspacket));
            return 0;
        }
        return new PSIPTable(*tspacket); // must be complete packet
    }

    PESPacket* partial = GetPartialPES(tspacket->PID());
    if (partial) {
        if (partial->AddTSPacket(tspacket)) {
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

bool MPEGStreamData::CreatePAT(const ProgramAssociationTable& pat)
{
    VERBOSE(VB_RECORD, "CreatePAT()");
    VERBOSE(VB_RECORD, "PAT in input stream");
    VERBOSE(VB_RECORD, pat.toString());
    if (_desired_program < 0) {
        VERBOSE(VB_RECORD, "Desired program not set yet");
        return false;
    }
    _pid_pmt = pat.FindPID(_desired_program);
    VERBOSE(VB_RECORD, QString("desired_program(%1) pid(0x%2)").
            arg(_desired_program).arg(_pid_pmt, 0, 16));

    if (!_pid_pmt) {
        _pid_pmt = pat.FindAnyPID();
        if (!_pid_pmt) {
            VERBOSE(VB_IMPORTANT,
                    QString("No program found in PAT."
                            " This recording will not play in MythTV."));
        }
        VERBOSE(VB_IMPORTANT,
                QString("Desired program #%1 not found in PAT."
                        " Substituting program #%2").
                arg(_desired_program).arg(pat.FindProgram(_pid_pmt)));
    }

    AddListeningPID(_pid_pmt);

    vector<uint> pnums, pids;

    pnums.push_back(1);
    pids.push_back(_pid_pmt);

    uint tsid = pat.TableIDExtension();
    uint ver = pat.Version();
    ProgramAssociationTable* pat2 =
        ProgramAssociationTable::Create(tsid, ver, pnums, pids);

    if (!pat2)
    {
        VERBOSE(VB_IMPORTANT, "MPEGStreamData::CreatePAT: "
                "Failed to create Program Association Table.");
        return false;
    }

    pat2->tsheader()->SetContinuityCounter(pat.tsheader()->ContinuityCounter());

    VERBOSE(VB_RECORD, QString("pmt_pid(0x%1)").arg(_pid_pmt, 0, 16));
    VERBOSE(VB_RECORD, "PAT for output stream");
    VERBOSE(VB_RECORD, pat2->toString());

    SetPAT(pat2);

    return true;

}

bool MPEGStreamData::CreatePMT(const ProgramMapTable& pmt)
{
    VERBOSE(VB_RECORD, "CreatePMT()");
    VERBOSE(VB_RECORD, "PMT in input stream");
    VERBOSE(VB_RECORD, pmt.toString());

    if (!PAT()) {
        VERBOSE(VB_RECORD, "no PAT yet...");
        return false; // no way to properly rewrite pids without PAT
    }
    pmt.Parse();

    vector<uint> videoPIDs, audioAC3, audioMPEG;
    vector<uint> pids, types;

    // Video
    pmt.FindPIDs(StreamID::MPEG2Video, videoPIDs);
    if (videoPIDs.size()<1) 
    {
        VERBOSE(VB_RECORD, "No video found old PMT, can not construct new PMT");
        return false;
    }
    _pid_video = videoPIDs[0];
    pids.push_back(_pid_video);
    types.push_back(StreamID::MPEG2Video);

    // Audio
    pmt.FindPIDs(StreamID::AC3Audio,   audioAC3);
    pmt.FindPIDs(StreamID::MPEG2Audio, audioMPEG);

    if (audioAC3.size()+audioMPEG.size()<1)
    {
        VERBOSE(VB_RECORD, "No audio found in old PMT, can not construct new PMT");
        return false;
    }

    for (uint i=0; i<audioAC3.size(); i++) {
        AddAudioPID(audioAC3[i]);
        pids.push_back(audioAC3[i]);
        types.push_back(StreamID::AC3Audio);
    }
    for (uint i=0; i<audioMPEG.size(); i++) {
        AddAudioPID(audioMPEG[i]);
        pids.push_back(audioMPEG[i]);
        types.push_back(StreamID::MPEG2Audio);
    }
    
    // Timebase
    int pcrpidIndex = pmt.FindPID(pmt.PCRPID());
    if (pcrpidIndex<0) {
        // the timecode reference stream is not in the PMT, add stream to misc record streams
        AddWritingPID(pmt.PCRPID());
    }

    // Misc
    uint programNumber = 1;

    // Construct
    ProgramMapTable *pmt2 = ProgramMapTable::
        Create(programNumber, _pid_pmt, pmt.PCRPID(), pmt.Version(), pids, types);

    // Set Continuity Header
    pmt2->tsheader()->SetContinuityCounter(pmt.tsheader()->ContinuityCounter());

    SetPMT(pmt2);

    VERBOSE(VB_RECORD, "PMT for output stream");
    VERBOSE(VB_RECORD, pmt2->toString());

    return true;
}

