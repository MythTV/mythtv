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
            .arg(tvrec ? tvrec->GetCaptureCardNum() : -1) \
            .arg(m_channel->GetDevice())

ExternalRecorder::ExternalRecorder(TVRec *rec, ExternalChannel *channel) :
    DTVRecorder(rec), m_channel(channel), m_stream_handler(NULL)
{
}

void ExternalRecorder::StartNewFile(void)
{
    // Make sure the first things in the file are a PAT & PMT
    HandleSingleProgramPAT(_stream_data->PATSingleProgram(), true);
    HandleSingleProgramPMT(_stream_data->PMTSingleProgram(), true);
}


void ExternalRecorder::run(void)
{
    if (!Open())
    {
        _error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    if (!_stream_data)
    {
        _error = "MPEGStreamData pointer has not been set";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        Close();
        return;
    }

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
        LOG(VB_GENERAL, LOG_INFO, LOC + "PMT set");
    }

    StartNewFile();

    _stream_data->AddAVListener(this);
    _stream_data->AddWritingListener(this);
    m_stream_handler->AddListener(_stream_data, false, true);

    StartStreaming();

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!_input_pmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!m_stream_handler->IsRunning())
        {
            _error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        }
    }

    StopStreaming();

    m_stream_handler->RemoveListener(_stream_data);
    _stream_data->RemoveWritingListener(this);
    _stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();
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

    m_stream_handler = ExternalStreamHandler::Get(m_channel->GetDevice());
    if (m_stream_handler)
    {
        if (m_stream_handler->IsOpen())
            LOG(VB_RECORD, LOG_INFO, LOC + "Opened successfully");
        else
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Failed to open recorder: %1")
                .arg(m_stream_handler->ErrorString()));
            ExternalStreamHandler::Return(m_stream_handler);
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
        ExternalStreamHandler::Return(m_stream_handler);

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

bool ExternalRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        if (!IsPaused(true))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait pause");

            StopStreaming();

            paused = true;
            pauseWait.wakeAll();

            if (tvrec)
                tvrec->RecorderPaused();
        }
    }
    else if (IsPaused(true))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait unpause");
        StartStreaming();

        if (_stream_data)
            _stream_data->Reset(_stream_data->DesiredProgram());

        paused = false;
    }

    // Always wait a little bit, unless woken up
    unpauseWait.wait(&pauseLock, timeout);

    return IsPaused(true);
}

bool ExternalRecorder::StartStreaming(void)
{
    m_h264_parser.Reset();
    _wait_for_keyframe_option = true;
    _seen_sps = false;

    if (m_stream_handler && m_stream_handler->StartStreaming(true))
        return true;

    return false;
}

bool ExternalRecorder::StopStreaming(void)
{
    return (m_stream_handler && m_stream_handler->StopStreaming());
}
