/*  -*- Mode: c++ -*-
 *   Class V4L2encRecorder
 *
 *   Copyright (C) John Poet 2014
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <sys/ioctl.h>

// Qt includes
#include <QString>

// MythTV includes
#include "recordingprofile.h"
#include "v4l2encstreamhandler.h"
#include "v4l2encrecorder.h"
#include "v4lchannel.h"
#include "ringbuffer.h"
#include "tv_rec.h"

#define LOC QString("V4L2Rec[%1](%2): ") \
            .arg(tvrec ? tvrec->GetInputId() : -1) \
            .arg(m_channel->GetDevice())

V4L2encRecorder::V4L2encRecorder(TVRec *rec, V4LChannel *channel) :
    V4LRecorder(rec), m_channel(channel), m_stream_handler(NULL)
{
    if (!Open())
    {
        _error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open() -- " + _error);
        return;
    }
}


void V4L2encRecorder::SetIntOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
    {
        if (!m_stream_handler->SetOption(name, setting->getValue().toInt()))
            V4LRecorder::SetOption(name, setting->getValue().toInt());
    }
}

void V4L2encRecorder::SetStrOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
    {
        if (!m_stream_handler->SetOption(name, setting->getValue()))
            V4LRecorder::SetOption(name, setting->getValue());
    }
}

void V4L2encRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                         const QString &videodev,
                                         const QString &audiodev,
                                         const QString &vbidev)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "SetOptionsFromProfile() -- begin");  //debugging

    (void)audiodev;
    (void)vbidev;

    SetOption("videodevice", videodev);
    SetOption("vbidevice", vbidev);
    SetOption("audiodevice", audiodev);

    SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gCoreContext->GetSetting("VbiFormat"));

    SetIntOption(profile, "mpeg2bitratemode");
    SetIntOption(profile, "mpeg2bitrate");
    SetIntOption(profile, "mpeg2maxbitrate");
    SetStrOption(profile, "mpeg2streamtype");
    SetStrOption(profile, "mpeg2aspectratio");
    SetStrOption(profile, "mpeg2language");

    SetIntOption(profile, "samplerate");
    SetStrOption(profile, "mpeg2audtype");
    SetIntOption(profile, "audbitratemode");
    SetIntOption(profile, "mpeg2audbitratel1");
    SetIntOption(profile, "mpeg2audbitratel2");
    SetIntOption(profile, "mpeg2audbitratel3");
    SetIntOption(profile, "mpeg2audvolume");

    SetIntOption(profile, "width");
    SetIntOption(profile, "height");

    SetIntOption(profile, "low_mpegbitratemode");
    SetIntOption(profile, "low_mpegavgbitrate");
    SetIntOption(profile, "low_mpegpeakbitrate");
    SetIntOption(profile, "medium_mpegbitratemode");
    SetIntOption(profile, "medium_mpegavgbitrate");
    SetIntOption(profile, "medium_mpegpeakbitrate");
    SetIntOption(profile, "high_mpegbitratemode");
    SetIntOption(profile, "high_mpegavgbitrate");
    SetIntOption(profile, "high_mpegpeakbitrate");

    SetStrOption(profile, "audiocodec");

    LOG(VB_GENERAL, LOG_INFO, LOC + "SetOptionsFromProfile -- end");  // debugging
}

void V4L2encRecorder::StartNewFile(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "StartNewFile -- begin");  // debugging
    // Make sure the first things in the file are a PAT & PMT
    HandleSingleProgramPAT(_stream_data->PATSingleProgram(), true);
    HandleSingleProgramPMT(_stream_data->PMTSingleProgram(), true);
    LOG(VB_RECORD, LOG_INFO, LOC + "StartNewFile -- end");  // debugging
}


void V4L2encRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- begin");

    bool failing, failed;
    bool is_TS = false;

    if (!_stream_data)
    {
        _error = "MPEGStreamData pointer has not been set";
        LOG(VB_GENERAL, LOG_ERR, LOC + "run() -- " + _error);
        Close();
        return;
    }

    m_stream_handler->Configure();

    {
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        recording = true;
        recordingWait.wakeAll();
    }

    if (m_channel->HasGeneratedPAT())
    {
        const ProgramAssociationTable *pat = m_channel->GetGeneratedPAT();
        const ProgramMapTable         *pmt = m_channel->GetGeneratedPMT();
        _stream_data->Reset(pat->ProgramNumber(0));
        _stream_data->HandleTables(MPEG_PAT_PID, *pat);
        _stream_data->HandleTables(pat->ProgramPID(0), *pmt);
        LOG(VB_GENERAL, LOG_INFO, LOC + "PMT set"); // debugging
    }

    StartNewFile();
    is_TS = (m_stream_handler->GetStreamType() == V4L2_MPEG_STREAM_TYPE_MPEG2_TS);

    if (is_TS)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "mpeg2ts");
        _stream_data->AddAVListener(this);
        _stream_data->AddWritingListener(this);
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "program stream (non mpeg2ts)");
        _stream_data->AddPSStreamListener(this);
    }

    m_stream_handler->AddListener(_stream_data, false, true);

    StartEncoding();

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (is_TS && !_input_pmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!m_stream_handler->Status(failed, failing))
        {
            if (failed)
            {
                _error = "Stream handler died unexpectedly.";
                LOG(VB_GENERAL, LOG_ERR, LOC + "run() -- " +  _error);
            }
            else if (failing)
            {
                SetRecordingStatus(RecStatus::Failing, __FILE__, __LINE__);
            }
        }
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "Shutting down");

    StopEncoding();

    m_stream_handler->RemoveListener(_stream_data);
    if (m_stream_handler->GetStreamType() == V4L2_MPEG_STREAM_TYPE_MPEG2_TS)
    {
        _stream_data->RemoveWritingListener(this);
        _stream_data->RemoveAVListener(this);
    }
    else
        _stream_data->RemovePSStreamListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- end");
}

bool V4L2encRecorder::Open(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- begin");

    if (IsOpen())
    {
        LOG(VB_RECORD, LOG_WARNING, LOC + "Open() -- Card already open");
        return true;
    }

// ??    ResetForNewFile();

    m_stream_handler = V4L2encStreamHandler::Get(m_channel->GetDevice(),
                                            m_channel->GetAudioDevice().toInt());
    if (!m_stream_handler)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Open() -- Failed to get a stream handler.");
        return false;
    }

    if (!m_stream_handler->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Open() -- Failed to open recorder: %1")
            .arg(m_stream_handler->ErrorString()));
        V4L2encStreamHandler::Return(m_stream_handler);
        return false;
    }

    m_h264_parser.use_I_forKeyframes(false);

    LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- Success.");
    return true;
}

void V4L2encRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        V4L2encStreamHandler::Return(m_stream_handler);


    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

bool V4L2encRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        if (!IsPaused(true))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait() -- pause");

            StopEncoding();

            paused = true;
            pauseWait.wakeAll();

            if (tvrec)
                tvrec->RecorderPaused();
        }
    }
    else if (IsPaused(true))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait() -- unpause");
        StartEncoding();

        if (_stream_data)
            _stream_data->Reset(_stream_data->DesiredProgram());

        paused = false;
    }

    // Always wait a little bit, unless woken up
    unpauseWait.wait(&pauseLock, timeout);

    return IsPaused(true);
}

bool V4L2encRecorder::StartEncoding(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "V4L2encRecorder::StartEncoding() -- begin");
    m_h264_parser.Reset();
    _wait_for_keyframe_option = true;
    _seen_sps = false;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "V4L2encRecorder::StartEncoding() -- end");
    return (m_stream_handler && m_stream_handler->StartEncoding());
}

bool V4L2encRecorder::StopEncoding(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "V4L2encRecorder::StopEncoding()");
    return (m_stream_handler && m_stream_handler->StopEncoding());
}
