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

#include <chrono> // for milliseconds
#include <sys/ioctl.h>
#include <thread> // for sleep_for

// Qt includes
#include <QString>

// MythTV includes
#include "io/mythmediabuffer.h"
#include "recorders/v4l2encrecorder.h"
#include "recorders/v4l2encstreamhandler.h"
#include "recorders/v4lchannel.h"
#include "recordingprofile.h"
#include "tv_rec.h"

#define LOC QString("V4L2Rec[%1](%2): ") \
            .arg(m_tvrec ? m_tvrec->GetInputId() : -1) \
            .arg(m_channel->GetDevice())

V4L2encRecorder::V4L2encRecorder(TVRec *rec, V4LChannel *channel) :
    V4LRecorder(rec), m_channel(channel)
{
    if (!Open())
    {
        m_error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open() -- " + m_error);
        return;
    }
}


void V4L2encRecorder::SetIntOption(RecordingProfile *profile, const QString &name)
{
    const StandardSetting *setting = profile->byName(name);
    if (setting)
    {
        if (!m_streamHandler->SetOption(name, setting->getValue().toInt()))
            V4LRecorder::SetOption(name, setting->getValue().toInt());
    }
}

void V4L2encRecorder::SetStrOption(RecordingProfile *profile, const QString &name)
{
    const StandardSetting *setting = profile->byName(name);
    if (setting)
    {
        if (!m_streamHandler->SetOption(name, setting->getValue()))
            V4LRecorder::SetOption(name, setting->getValue());
    }
}

void V4L2encRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                         const QString &videodev,
                                         [[maybe_unused]] const QString &audiodev,
                                         [[maybe_unused]] const QString &vbidev)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "SetOptionsFromProfile() -- begin");  //debugging

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
    HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
    HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
    LOG(VB_RECORD, LOG_INFO, LOC + "StartNewFile -- end");  // debugging
}


void V4L2encRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- begin");

    bool is_TS = false;

    if (!m_streamData)
    {
        m_error = "MPEGStreamData pointer has not been set";
        LOG(VB_GENERAL, LOG_ERR, LOC + "run() -- " + m_error);
        Close();
        return;
    }

    m_streamHandler->Configure();

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    if (m_channel->HasGeneratedPAT())
    {
        const ProgramAssociationTable *pat = m_channel->GetGeneratedPAT();
        const ProgramMapTable         *pmt = m_channel->GetGeneratedPMT();
        m_streamData->Reset(pat->ProgramNumber(0));
        m_streamData->HandleTables(PID::MPEG_PAT_PID, *pat);
        m_streamData->HandleTables(pat->ProgramPID(0), *pmt);
        LOG(VB_GENERAL, LOG_INFO, LOC + "PMT set"); // debugging
    }

    StartNewFile();
    is_TS = (m_streamHandler->GetStreamType() == V4L2_MPEG_STREAM_TYPE_MPEG2_TS);

    if (is_TS)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "mpeg2ts");
        m_streamData->AddAVListener(this);
        m_streamData->AddWritingListener(this);
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "program stream (non mpeg2ts)");
        m_streamData->AddPSStreamListener(this);
    }

    m_streamHandler->AddListener(m_streamData, false, true);

    StartEncoding();

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (is_TS && !m_inputPmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Recording will not commence until a PMT is set.");
            std::this_thread::sleep_for(5ms);
            continue;
        }

        bool failing = false;
        bool failed = false;
        if (!m_streamHandler->Status(failed, failing))
        {
            if (failed)
            {
                m_error = "Stream handler died unexpectedly.";
                LOG(VB_GENERAL, LOG_ERR, LOC + "run() -- " +  m_error);
            }
            else if (failing)
            {
                SetRecordingStatus(RecStatus::Failing, __FILE__, __LINE__);
            }
        }
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "Shutting down");

    StopEncoding();

    m_streamHandler->RemoveListener(m_streamData);
    if (m_streamHandler->GetStreamType() == V4L2_MPEG_STREAM_TYPE_MPEG2_TS)
    {
        m_streamData->RemoveWritingListener(this);
        m_streamData->RemoveAVListener(this);
    }
    else
    {
        m_streamData->RemovePSStreamListener(this);
    }

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();

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

    m_streamHandler = V4L2encStreamHandler::Get(m_channel->GetDevice(),
                                         m_channel->GetAudioDevice().toInt(),
                                         m_tvrec ? m_tvrec->GetInputId() : -1);
    if (!m_streamHandler)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Open() -- Failed to get a stream handler.");
        return false;
    }

    if (!m_streamHandler->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Open() -- Failed to open recorder: %1")
            .arg(m_streamHandler->ErrorString()));
        V4L2encStreamHandler::Return(m_streamHandler,
                                     m_tvrec ? m_tvrec->GetInputId() : -1);
        return false;
    }

    m_useIForKeyframe = false;

    LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- Success.");
    return true;
}

void V4L2encRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        V4L2encStreamHandler::Return(m_streamHandler,
                                     m_tvrec ? m_tvrec->GetInputId() : -1);


    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

bool V4L2encRecorder::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        if (!IsPaused(true))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait() -- pause");

            StopEncoding();

            m_paused = true;
            m_pauseWait.wakeAll();

            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }
    }
    else if (IsPaused(true))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait() -- unpause");
        StartEncoding();

        if (m_streamData)
            m_streamData->Reset(m_streamData->DesiredProgram());

        m_paused = false;
    }

    // Always wait a little bit, unless woken up
    m_unpauseWait.wait(&m_pauseLock, timeout.count());

    return IsPaused(true);
}

bool V4L2encRecorder::StartEncoding(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "V4L2encRecorder::StartEncoding() -- begin");
    if (m_h2645Parser != nullptr)
        m_h2645Parser->Reset();
    m_waitForKeyframeOption = true;
    m_seenSps = false;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "V4L2encRecorder::StartEncoding() -- end");
    return (m_streamHandler && m_streamHandler->StartEncoding());
}

bool V4L2encRecorder::StopEncoding(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "V4L2encRecorder::StopEncoding()");
    return (m_streamHandler && m_streamHandler->StopEncoding());
}
