/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd, and
 *    Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX includes
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#ifndef USING_MINGW
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <sys/time.h>

// C++ includes
#include <iostream>
#include <algorithm>
using namespace std;

// MythTV includes
#include "ringbuffer.h"
#include "atsctables.h"
#include "atscstreamdata.h"
#include "dvbstreamdata.h"
#include "eithelper.h"
#include "tv_rec.h"

// MythTV HDHR includes
#include "hdhrchannel.h"
#include "hdhrrecorder.h"
#include "hdhrstreamhandler.h"

#define LOC QString("HDHRRec(%1): ").arg(tvrec->GetCaptureCardNum())
#define LOC_WARN QString("HDHRRec(%1), Warning: ") \
                     .arg(tvrec->GetCaptureCardNum())
#define LOC_ERR QString("HDHRRec(%1), Error: ") \
                    .arg(tvrec->GetCaptureCardNum())

HDHRRecorder::HDHRRecorder(TVRec *rec, HDHRChannel *channel)
    : DTVRecorder(rec),
      _channel(channel),        _stream_handler(NULL),
      _pid_lock(QMutex::Recursive),
      _input_pat(NULL),         _input_pmt(NULL),
      _has_no_av(false)
{
    memset(_stream_id, 0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0, sizeof(_continuity_counter));
}

HDHRRecorder::~HDHRRecorder()
{
    TeardownAll();
}

void HDHRRecorder::TeardownAll(void)
{
    StopRecording();
    Close();

    if (_input_pat)
    {
        delete _input_pat;
        _input_pat = NULL;
    }

    if (_input_pmt)
    {
        delete _input_pmt;
        _input_pmt = NULL;
    }
}

void HDHRRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                         const QString &videodev,
                                         const QString &audiodev,
                                         const QString &vbidev)
{
    (void)audiodev;
    (void)vbidev;
    (void)profile;

    SetOption("videodevice", videodev);
    SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gCoreContext->GetSetting("VbiFormat"));

    // HACK -- begin
    // This is to make debugging easier.
    SetOption("videodevice", QString::number(tvrec->GetCaptureCardNum()));
    // HACK -- end
}

void HDHRRecorder::HandleSingleProgramPAT(ProgramAssociationTable *pat)
{
    if (!pat)
    {
        VERBOSE(VB_RECORD, LOC + "HandleSingleProgramPAT(NULL)");
        return;
    }

    if (!ringBuffer)
        return;

    uint next_cc = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next_cc);
    pat->GetAsTSPackets(_scratch, next_cc);
    for (uint i = 0; i < _scratch.size(); i++)
        DTVRecorder::BufferedWrite(_scratch[i]);
}

void HDHRRecorder::HandleSingleProgramPMT(ProgramMapTable *pmt)
{
    if (!pmt)
    {
        VERBOSE(VB_RECORD, LOC + "HandleSingleProgramPMT(NULL)");
        return;
    }

    // collect stream types for H.264 (MPEG-4 AVC) keyframe detection
    for (uint i = 0; i < pmt->StreamCount(); i++)
        _stream_id[pmt->StreamPID(i)] = pmt->StreamType(i);

    if (!ringBuffer)
        return;

    uint next_cc = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next_cc);
    pmt->GetAsTSPackets(_scratch, next_cc);

    for (uint i = 0; i < _scratch.size(); i++)
        DTVRecorder::BufferedWrite(_scratch[i]);
}

void HDHRRecorder::HandlePAT(const ProgramAssociationTable *_pat)
{
    if (!_pat)
    {
        VERBOSE(VB_RECORD, LOC + "SetPAT(NULL)");
        return;
    }

    QMutexLocker change_lock(&_pid_lock);

    int progNum = _stream_data->DesiredProgram();
    uint pmtpid = _pat->FindPID(progNum);

    if (!pmtpid)
    {
        VERBOSE(VB_RECORD, LOC + "SetPAT(): "
                "Ignoring PAT not containing our desired program...");
        return;
    }

    VERBOSE(VB_RECORD, LOC + QString("SetPAT(%1 on 0x%2)")
            .arg(progNum).arg(pmtpid,0,16));

    ProgramAssociationTable *oldpat = _input_pat;
    _input_pat = new ProgramAssociationTable(*_pat);
    delete oldpat;

    // Listen for the other PMTs for faster channel switching
    for (uint i = 0; _input_pat && (i < _input_pat->ProgramCount()); i++)
    {
        uint pmt_pid = _input_pat->ProgramPID(i);
        if (!_stream_data->IsListeningPID(pmt_pid))
            _stream_data->AddListeningPID(pmt_pid);
    }
}

void HDHRRecorder::HandlePMT(uint progNum, const ProgramMapTable *_pmt)
{
    QMutexLocker change_lock(&_pid_lock);

    if ((int)progNum == _stream_data->DesiredProgram())
    {
        VERBOSE(VB_RECORD, LOC + QString("SetPMT(%1)").arg(progNum));
        ProgramMapTable *oldpmt = _input_pmt;
        _input_pmt = new ProgramMapTable(*_pmt);

        QString sistandard = _channel->GetSIStandard();

        bool has_no_av = true;
        for (uint i = 0; i < _input_pmt->StreamCount() && has_no_av; i++)
        {
            has_no_av &= !_input_pmt->IsVideo(i, sistandard);
            has_no_av &= !_input_pmt->IsAudio(i, sistandard);
        }
        _has_no_av = has_no_av;

        delete oldpmt;
    }
}

/** \fn HDHRRecorder::HandleMGT(const MasterGuideTable*)
 *  \brief Processes Master Guide Table, by enabling the
 *         scanning of all PIDs listed.
 */
/*
void HDHRRecorder::HandleMGT(const MasterGuideTable *mgt)
{
    VERBOSE(VB_IMPORTANT, LOC + "HandleMGT()");
    for (unsigned int i = 0; i < mgt->TableCount(); i++)
    {
        GetStreamData()->AddListeningPID(mgt->TablePID(i));
        _channel->AddPID(mgt->TablePID(i), false);
    }
    _channel->UpdateFilters();
}
*/

bool HDHRRecorder::Open(void)
{
    if (IsOpen())
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Card already open");
        return true;
    }

    memset(_stream_id,  0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));

    _stream_handler = HDHRStreamHandler::Get(_channel->GetDevice());

    VERBOSE(VB_RECORD, LOC + "HDHR opened successfully");

    return true;
}

void HDHRRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");

    if (IsOpen())
        HDHRStreamHandler::Return(_stream_handler);

    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void HDHRRecorder::SetStreamData(void)
{
    _stream_data->AddMPEGSPListener(this);
    _stream_data->AddMPEGListener(this);

    DVBStreamData *dvb = dynamic_cast<DVBStreamData*>(_stream_data);
    if (dvb)
        dvb->AddDVBMainListener(this);

    ATSCStreamData *atsc = dynamic_cast<ATSCStreamData*>(_stream_data);

    if (atsc && atsc->DesiredMinorChannel())
        atsc->SetDesiredChannel(atsc->DesiredMajorChannel(),
                                    atsc->DesiredMinorChannel());
    else if (_stream_data->DesiredProgram() >= 0)
        _stream_data->SetDesiredProgram(_stream_data->DesiredProgram());
}

ATSCStreamData *HDHRRecorder::GetATSCStreamData(void)
{
    return dynamic_cast<ATSCStreamData*>(_stream_data);
}

void HDHRRecorder::StartRecording(void)
{
    VERBOSE(VB_RECORD, LOC + "StartRecording -- begin");

    /* Create video socket. */
    if (!Open())
    {
        _error = true;
        VERBOSE(VB_RECORD, LOC + "StartRecording -- end 1");
        return;
    }

    _request_recording = true;
    _recording = true;

    // Make sure the first things in the file are a PAT & PMT
    bool tmp = _wait_for_keyframe_option;
    _wait_for_keyframe_option = false;
    HandleSingleProgramPAT(_stream_data->PATSingleProgram());
    HandleSingleProgramPMT(_stream_data->PMTSingleProgram());
    _wait_for_keyframe_option = tmp;

    _stream_data->AddAVListener(this);
    _stream_data->AddWritingListener(this);
    _stream_handler->AddListener(_stream_data);

    while (_request_recording && !_error)
    {
        usleep(50000);

        if (PauseAndWait())
            continue;

        if (!_input_pmt)
        {
            VERBOSE(VB_GENERAL, LOC_WARN +
                    "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!_stream_handler->IsRunning())
        {
            _error = true;

            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Stream handler died unexpectedly.");
        }
    }

    VERBOSE(VB_RECORD, LOC + "StartRecording -- ending...");

    _stream_handler->RemoveListener(_stream_data);
    _stream_data->RemoveWritingListener(this);
    _stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    _recording = false;

    VERBOSE(VB_RECORD, LOC + "StartRecording -- end");
}

void HDHRRecorder::ResetForNewFile(void)
{
    DTVRecorder::ResetForNewFile();

    memset(_stream_id,  0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));
}

void HDHRRecorder::StopRecording(void)
{
    _request_recording = false;
    while (_recording)
        usleep(2000);
}

bool HDHRRecorder::PauseAndWait(int timeout)
{
    if (request_pause)
    {
        QMutex waitlock;
        if (!paused)
        {
            assert(_stream_handler);
            assert(_stream_data);

            _stream_handler->RemoveListener(_stream_data);

            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }
        waitlock.lock();
        unpauseWait.wait(&waitlock, timeout);
    }

    if (!request_pause && paused)
    {
        paused = false;

        assert(_stream_handler);
        assert(_stream_data);

        _stream_handler->AddListener(_stream_data);
    }

    return paused;
}

bool HDHRRecorder::ProcessVideoTSPacket(const TSPacket &tspacket)
{
    if (!ringBuffer)
        return true;

    uint streamType = _stream_id[tspacket.PID()];

    // Check for keyframes and count frames
    if (streamType == StreamID::H264Video)
    {
        _buffer_packets = !FindH264Keyframes(&tspacket);
        if (!_seen_sps)
            return true;
    }
    else
    {
        _buffer_packets = !FindMPEG2Keyframes(&tspacket);
    }

    return ProcessAVTSPacket(tspacket);
}

bool HDHRRecorder::ProcessAudioTSPacket(const TSPacket &tspacket)
{
    if (!ringBuffer)
        return true;

    _buffer_packets = !FindAudioKeyframes(&tspacket);
    return ProcessAVTSPacket(tspacket);
}

/// Common code for processing either audio or video packets
bool HDHRRecorder::ProcessAVTSPacket(const TSPacket &tspacket)
{
    const uint pid = tspacket.PID();
    // Sync recording start to first keyframe
    if (_wait_for_keyframe_option && _first_keyframe < 0)
        return true;

    // Sync streams to the first Payload Unit Start Indicator
    // _after_ first keyframe iff _wait_for_keyframe_option is true
    if (!(_pid_status[pid] & kPayloadStartSeen) && tspacket.HasPayload())
    {
        if (!tspacket.PayloadStart())
            return true; // not payload start - drop packet

        VERBOSE(VB_RECORD,
                QString("PID 0x%1 Found Payload Start").arg(pid,0,16));

        _pid_status[pid] |= kPayloadStartSeen;
    }

    BufferedWrite(tspacket);

    return true;
}

bool HDHRRecorder::ProcessTSPacket(const TSPacket &tspacket)
{
    // Only create fake keyframe[s] if there are no audio/video streams
    if (_input_pmt && _has_no_av)
    {
        _buffer_packets = !FindOtherKeyframes(&tspacket);
    }
    else
    {
        // There are audio/video streams. Only write the packet
        // if audio/video key-frames have been found
        if (_wait_for_keyframe_option && _first_keyframe < 0)
            return true;

        _buffer_packets = true;
    }

    BufferedWrite(tspacket);

    return true;
}

void HDHRRecorder::BufferedWrite(const TSPacket &tspacket)
{
    // Care must be taken to make sure that the packet actually gets written
    // as the decision to actually write it has already been made

    // Do we have to buffer the packet for exact keyframe detection?
    if (_buffer_packets)
    {
        int idx = _payload_buffer.size();
        _payload_buffer.resize(idx + TSPacket::kSize);
        memcpy(&_payload_buffer[idx], tspacket.data(), TSPacket::kSize);
        return;
    }

    // We are free to write the packet, but if we have buffered packet[s]
    // we have to write them first...
    if (!_payload_buffer.empty())
    {
        if (ringBuffer)
            ringBuffer->Write(&_payload_buffer[0], _payload_buffer.size());
        _payload_buffer.clear();
    }

    if (ringBuffer)
        ringBuffer->Write(tspacket.data(), TSPacket::kSize);
}
