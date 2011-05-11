/*
 *  Class DVBRecorder
 *
 *  Copyright (C) Kenneth Aafloy 2003
 *
 *  Description:
 *      Has the responsibility of opening the Demux device and reading
 *      data from it. Code for controlling which of the mpeg2 streams
 *      from the DVB device gets through. In ProcessData there is
 *      a 'map builder' which saves information about the stream
 *      to the database.
 *
 *  Author(s):
 *      Isaac Richards
 *          - Wrote the original class this work derived from.
 *      Kenneth Aafloy (ke-aa at frisurf.no)
 *          - Rewritten Recording Functions.
 *          - Moved PID handling here and rewritten.
 *      Ben Bucksch
 *          - Developed the original implementation.
 *      Dave Chapman (dave at dchapman.com)
 *          - The dvbstream library, which some code,
 *            in ::StartRecording, is based upon.
 *      Martin Smith (martin at spamcop.net)
 *          - The signal quality monitor
 *      David Matthews (dm at prolingua.co.uk)
 *          - Added video stream for radio channels
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// POSIX includes
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

// C++ includes
#include <iostream>
#include <algorithm>
using namespace std;

// MythTV includes
#include "ringbuffer.h"
#include "programinfo.h"
#include "mpegtables.h"
#include "iso639.h"
#include "dvbstreamdata.h"
#include "atscstreamdata.h"
#include "atsctables.h"
#include "cardutil.h"
#include "tv_rec.h"

// MythTV DVB includes
#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbstreamhandler.h"

// AVLib/FFMPEG includes
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/mpegts.h"
}

const int DVBRecorder::TSPACKETS_BETWEEN_PSIP_SYNC = 20000;
const int DVBRecorder::POLL_INTERVAL        =  50; // msec
const int DVBRecorder::POLL_WARNING_TIMEOUT = 500; // msec
const unsigned char DVBRecorder::kPayloadStartSeen;

#define TS_TICKS_PER_SEC    90000
#define DUMMY_VIDEO_PID     VIDEO_PID(0x20)

#define LOC      QString("DVBRec(%1:%2): ") \
                 .arg(tvrec->GetCaptureCardNum()).arg(videodevice)
#define LOC_WARN QString("DVBRec(%1:%2) Warning: ") \
                 .arg(tvrec->GetCaptureCardNum()).arg(videodevice)
#define LOC_ERR  QString("DVBRec(%1:%2) Error: ") \
                 .arg(tvrec->GetCaptureCardNum()).arg(videodevice)

DVBRecorder::DVBRecorder(TVRec *rec, DVBChannel* advbchannel)
    : DTVRecorder(rec),
      // DVB stuff
      dvbchannel(advbchannel),
      _stream_handler(NULL),
      _pid_lock(QMutex::Recursive),
      _input_pat(NULL),
      _input_pmt(NULL),
      _has_no_av(false),
      // Statistics
      _continuity_error_count(0), _stream_overflow_count(0),
      _bad_packet_count(0)
{
    videodevice = QString::null;

    _buffer_size = (1024*1024 / TSPacket::kSize) * TSPacket::kSize;

    _buffer = new unsigned char[_buffer_size];
    memset(_buffer, 0, _buffer_size);
    memset(_stream_id, 0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0, sizeof(_continuity_counter));
}

DVBRecorder::~DVBRecorder()
{
    TeardownAll();
}

void DVBRecorder::TeardownAll(void)
{
    // Make SURE that the device read thread is cleaned up -- John Poet
    StopRecording();

    if (IsOpen())
        Close();

    if (_buffer)
    {
        delete[] _buffer;
        _buffer = NULL;
    }

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

void DVBRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                        const QString &videodev,
                                        const QString&, const QString&)
{
    SetOption("videodevice", videodev);
    DTVRecorder::SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetStrOption(profile,  "recordingtype");
}

void DVBRecorder::HandleSingleProgramPAT(ProgramAssociationTable *pat)
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

void DVBRecorder::HandleSingleProgramPMT(ProgramMapTable *pmt)
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

void DVBRecorder::HandlePAT(const ProgramAssociationTable *_pat)
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
            _stream_data->AddListeningPID(pmt_pid, kPIDPriorityLow);
    }
}

void DVBRecorder::HandlePMT(uint progNum, const ProgramMapTable *_pmt)
{
    QMutexLocker change_lock(&_pid_lock);

    if ((int)progNum == _stream_data->DesiredProgram())
    {
        VERBOSE(VB_RECORD, LOC + "SetPMT("<<progNum<<")");
        ProgramMapTable *oldpmt = _input_pmt;
        _input_pmt = new ProgramMapTable(*_pmt);

        QString sistandard = dvbchannel->GetSIStandard();

        bool has_no_av = true;
        for (uint i = 0; i < _input_pmt->StreamCount() && has_no_av; i++)
        {
            has_no_av &= !_input_pmt->IsVideo(i, sistandard);
            has_no_av &= !_input_pmt->IsAudio(i, sistandard);
        }
        _has_no_av = has_no_av;

        dvbchannel->SetPMT(_input_pmt);
        delete oldpmt;
    }
}

void DVBRecorder::HandleSTT(const SystemTimeTable*)
{
    dvbchannel->SetTimeOffset(GetStreamData()->TimeOffset());
}

void DVBRecorder::HandleTDT(const TimeDateTable*)
{
    dvbchannel->SetTimeOffset(GetStreamData()->TimeOffset());
}

bool DVBRecorder::Open(void)
{
    if (IsOpen())
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Card already open");
        return true;
    }

    if (videodevice.isEmpty())
        return false;

    memset(_stream_id,  0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));

    _stream_handler = DVBStreamHandler::Get(videodevice);

    VERBOSE(VB_RECORD, LOC + QString("Card opened successfully fd(%1)")
            .arg(_stream_fd));

    return true;
}

void DVBRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() fd("<<_stream_fd<<") -- begin");

    DVBStreamHandler::Return(_stream_handler);

    VERBOSE(VB_RECORD, LOC + "Close() fd("<<_stream_fd<<") -- end");
}

void DVBRecorder::SetStreamData(void)
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

void DVBRecorder::StartRecording(void)
{
    if (!Open())
    {
        _error = true;
        return;
    }

    _continuity_error_count = 0;
    _stream_overflow_count = 0;

    _request_recording = true;
    _recording = true;

    // Listen for time table on DVB standard streams
    if (dvbchannel && (dvbchannel->GetSIStandard() == "dvb"))
        _stream_data->AddListeningPID(DVB_TDT_PID);

    // Make sure the first things in the file are a PAT & PMT
    bool tmp = _wait_for_keyframe_option;
    _wait_for_keyframe_option = false;
    HandleSingleProgramPAT(_stream_data->PATSingleProgram());
    HandleSingleProgramPMT(_stream_data->PMTSingleProgram());
    _wait_for_keyframe_option = tmp;

    _stream_data->AddAVListener(this);
    _stream_data->AddWritingListener(this);
    _stream_handler->AddListener(_stream_data, false, true);

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

    _stream_handler->RemoveListener(_stream_data);
    _stream_data->RemoveWritingListener(this);
    _stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    _recording = false;
}

void DVBRecorder::ResetForNewFile(void)
{
    DTVRecorder::ResetForNewFile();

    memset(_stream_id,  0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));
}

void DVBRecorder::StopRecording(void)
{
    _request_recording = false;
    while (_recording)
        usleep(2000);
}

void DVBRecorder::ReaderPaused(int /*fd*/)
{
}

bool DVBRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        if (!IsPaused(true))
        {
            _stream_handler->RemoveListener(_stream_data);

            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }

        unpauseWait.wait(&pauseLock, timeout);
    }

    if (!request_pause && IsPaused(true))
    {
        paused = false;
        _stream_handler->AddListener(_stream_data, false, true);
        unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

bool DVBRecorder::ProcessVideoTSPacket(const TSPacket &tspacket)
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

bool DVBRecorder::ProcessAudioTSPacket(const TSPacket &tspacket)
{
    if (!ringBuffer)
        return true;

    _buffer_packets = !FindAudioKeyframes(&tspacket);
    return ProcessAVTSPacket(tspacket);
}

/// Common code for processing either audio or video packets
bool DVBRecorder::ProcessAVTSPacket(const TSPacket &tspacket)
{
    const uint pid = tspacket.PID();

    // Check continuity counter
    if ((pid != 0x1fff) && !CheckCC(pid, tspacket.ContinuityCounter()))
    {
        VERBOSE(VB_RECORD, LOC +
                QString("PID 0x%1 discontinuity detected").arg(pid,0,16));
        _continuity_error_count++;
    }

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

bool DVBRecorder::ProcessTSPacket(const TSPacket &tspacket)
{
    const uint pid = tspacket.PID();

    // Check continuity counter
    if ((pid != 0x1fff) && !CheckCC(pid, tspacket.ContinuityCounter()))
    {
        VERBOSE(VB_RECORD, LOC +
                QString("PID 0x%1 discontinuity detected").arg(pid,0,16));
        _continuity_error_count++;
    }

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
    // return value is not used and there is no doc for the return value
    // assume true means the packet is successfully processed
    return true;
}

void DVBRecorder::BufferedWrite(const TSPacket &tspacket)
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
