/*  -*- Mode: c++ -*-
 *   Class ExternalRecorder
 *
 *   Copyright (C) John Poet 2013
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
 *   along with this program; if not, write to the Free Software Foundation,
 *   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// Qt includes
#include <QString>

// MythTV includes
#include "ExternalStreamHandler.h"
#include "ExternalRecorder.h"
#include "ExternalChannel.h"
#include "io/mythmediabuffer.h"
#include "tv_rec.h"

#define LOC QString("ExternalRec[%1](%2): ") \
            .arg(m_channel->GetInputID())    \
            .arg(m_channel->GetDescription())

void ExternalRecorder::StartNewFile(void)
{
    // Make sure the first things in the file are a PAT & PMT
    HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
    HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
}


void ExternalRecorder::run(void)
{
    if (!Open())
    {
        m_error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    if (!m_streamData)
    {
        m_error = "MPEGStreamData pointer has not been set";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        Close();
        return;
    }

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
        LOG(VB_GENERAL, LOG_INFO, LOC + "PMT set");
    }

    StartNewFile();

    m_waitForKeyframeOption = true;
    m_seenSps = false;

    m_streamData->AddAVListener(this);
    m_streamData->AddWritingListener(this);
    m_streamHandler->AddListener(m_streamData, false, true);

    StartStreaming();

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!m_inputPmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!m_streamHandler->IsRunning())
        {
            m_error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        }

        if (m_streamHandler->IsDamaged())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Recording is damaged. Setting status to %1")
                .arg(RecStatus::toString(RecStatus::Failing, kSingleRecord)));
            SetRecordingStatus(RecStatus::Failing, __FILE__, __LINE__);
            m_streamHandler->ClearDamaged();
        }
    }

    StopStreaming();

    m_streamHandler->RemoveListener(m_streamData);
    m_streamData->RemoveWritingListener(this);
    m_streamData->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();
}

bool ExternalRecorder::Open(void)
{
    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Card already open");
        return true;
    }

    ResetForNewFile();

    m_streamHandler = ExternalStreamHandler::Get(m_channel->GetDevice(),
                                                  m_channel->GetInputID(),
                                                  m_channel->GetMajorID());

    if (m_streamHandler)
    {
        if (m_streamHandler->IsAppOpen())
            LOG(VB_RECORD, LOG_INFO, LOC + "Opened successfully");
        else
        {
            ExternalStreamHandler::Return(m_streamHandler,
                                          (m_tvrec ? m_tvrec->GetInputId() : -1));

            return false;
        }
        return true;
    }

    Close();
    LOG(VB_GENERAL, LOG_ERR, LOC + "Open failed");
    return false;
}

void ExternalRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        ExternalStreamHandler::Return(m_streamHandler,
                                      (m_tvrec ? m_tvrec->GetInputId() : -1));

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

bool ExternalRecorder::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        if (!IsPaused(true))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait pause");

            m_streamHandler->RemoveListener(m_streamData);
            StopStreaming();

            m_paused = true;
            m_pauseWait.wakeAll();

            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }
    }
    else if (IsPaused(true))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait unpause");

        // The SignalMonitor will StartStreaming

        if (m_h2645Parser != nullptr)
            m_h2645Parser->Reset();

        if (m_streamData)
            m_streamData->Reset(m_streamData->DesiredProgram());

        m_paused = false;
        m_streamHandler->AddListener(m_streamData);
        StartStreaming();
    }

    // Always wait a little bit, unless woken up
    m_unpauseWait.wait(&m_pauseLock, timeout.count());

    return IsPaused(true);
}

bool ExternalRecorder::StartStreaming(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "StartStreaming");
    return m_streamHandler && m_streamHandler->StartStreaming();
}

bool ExternalRecorder::StopStreaming(void)
{
    return (m_streamHandler && m_streamHandler->StopStreaming());
}
