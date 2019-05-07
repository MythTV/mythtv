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
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Qt includes
#include <QString>

// MythTV includes
#include "ExternalStreamHandler.h"
#include "ExternalRecorder.h"
#include "ExternalChannel.h"
#include "ringbuffer.h"
#include "tv_rec.h"

#define LOC QString("ExternalRec[%1](%2): ") \
            .arg(m_channel->GetInputID())    \
            .arg(m_channel->GetDescription())

void ExternalRecorder::StartNewFile(void)
{
    // Make sure the first things in the file are a PAT & PMT
    HandleSingleProgramPAT(m_stream_data->PATSingleProgram(), true);
    HandleSingleProgramPMT(m_stream_data->PMTSingleProgram(), true);
}


void ExternalRecorder::run(void)
{
    if (!Open())
    {
        m_error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    if (!m_stream_data)
    {
        m_error = "MPEGStreamData pointer has not been set";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        Close();
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_request_recording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    if (m_channel->HasGeneratedPAT())
    {
        const ProgramAssociationTable *pat = m_channel->GetGeneratedPAT();
        const ProgramMapTable         *pmt = m_channel->GetGeneratedPMT();
        m_stream_data->Reset(pat->ProgramNumber(0));
        m_stream_data->HandleTables(MPEG_PAT_PID, *pat);
        m_stream_data->HandleTables(pat->ProgramPID(0), *pmt);
        LOG(VB_GENERAL, LOG_INFO, LOC + "PMT set");
    }

    StartNewFile();

    m_h264_parser.Reset();
    m_wait_for_keyframe_option = true;
    m_seen_sps = false;

    m_stream_data->AddAVListener(this);
    m_stream_data->AddWritingListener(this);
    m_stream_handler->AddListener(m_stream_data, false, true);

    StartStreaming();

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!m_input_pmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!m_stream_handler->IsRunning())
        {
            m_error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        }
    }

    StopStreaming();

    m_stream_handler->RemoveListener(m_stream_data);
    m_stream_data->RemoveWritingListener(this);
    m_stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();
}

bool ExternalRecorder::Open(void)
{
    QString result;

    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Card already open");
        return true;
    }

    ResetForNewFile();

    m_stream_handler = ExternalStreamHandler::Get(m_channel->GetDevice(),
                                                  m_channel->GetInputID(),
                                                  m_channel->GetMajorID());

    if (m_stream_handler)
    {
        if (m_stream_handler->IsAppOpen())
            LOG(VB_RECORD, LOG_INFO, LOC + "Opened successfully");
        else
        {
            ExternalStreamHandler::Return(m_stream_handler,
                                          (m_tvrec ? m_tvrec->GetInputId() : -1));

            return false;
        }
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Open failed");
    return false;
}

void ExternalRecorder::Close(void)
{
    QString result;

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        ExternalStreamHandler::Return(m_stream_handler,
                                      (m_tvrec ? m_tvrec->GetInputId() : -1));

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

bool ExternalRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_request_pause)
    {
        if (!IsPaused(true))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait pause");

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

        if (m_stream_data)
            m_stream_data->Reset(m_stream_data->DesiredProgram());

        m_paused = false;
    }

    // Always wait a little bit, unless woken up
    m_unpauseWait.wait(&m_pauseLock, timeout);

    return IsPaused(true);
}

bool ExternalRecorder::StartStreaming(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "StartStreaming");
    return m_stream_handler && m_stream_handler->StartStreaming();
}

bool ExternalRecorder::StopStreaming(void)
{
    return (m_stream_handler && m_stream_handler->StopStreaming());
}
