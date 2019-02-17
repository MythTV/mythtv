/*
 *  Class DVBRecorder
 *
 *  Copyright (C) Daniel Thor Kristjansson 2010
 *
 *   This class glues the DVBStreamHandler which handles the DVB devices
 *   to the DTVRecorder that handles recordings in MythTV.
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

// MythTV includes
#include "dvbstreamhandler.h"
#include "mpegstreamdata.h"
#include "tsstreamdata.h"
#include "dvbrecorder.h"
#include "dvbchannel.h"
#include "ringbuffer.h"
#include "tv_rec.h"
#include "mythlogging.h"

#define LOC QString("DVBRec[%1](%2): ") \
            .arg(m_tvrec ? m_tvrec->GetInputId() : -1).arg(m_videodevice)

DVBRecorder::DVBRecorder(TVRec *rec, DVBChannel *channel)
    : DTVRecorder(rec), m_channel(channel)
{
    m_videodevice.clear();
}

bool DVBRecorder::Open(void)
{
    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Card already open");
        return true;
    }

    if (m_videodevice.isEmpty())
        return false;

    ResetForNewFile();

    if (m_channel->GetFormat().compare("MPTS") == 0)
    {
        // MPTS only.  Use TSStreamData to write out unfiltered data
        LOG(VB_RECORD, LOG_INFO, LOC + "Using TSStreamData");
        SetStreamData(new TSStreamData(m_tvrec ? m_tvrec->GetInputId() : -1));
        m_record_mpts_only = true;
        m_record_mpts = false;
    }

    m_stream_handler = DVBStreamHandler::Get(m_videodevice,
                                             m_tvrec ? m_tvrec->GetInputId() : -1);

    LOG(VB_RECORD, LOG_INFO, LOC + "Card opened successfully");

    return true;
}

bool DVBRecorder::IsOpen(void) const
{
    return (nullptr != m_stream_handler);
}

void DVBRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    DVBStreamHandler::Return(m_stream_handler, m_tvrec ? m_tvrec->GetInputId() : -1);

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

void DVBRecorder::StartNewFile(void)
{
    if (!m_record_mpts_only)
    {
        if (m_record_mpts)
            m_stream_handler->AddNamedOutputFile(m_ringBuffer->GetFilename());

        // Make sure the first things in the file are a PAT & PMT
        HandleSingleProgramPAT(m_stream_data->PATSingleProgram(), true);
        HandleSingleProgramPMT(m_stream_data->PMTSingleProgram(), true);
    }
}

void DVBRecorder::run(void)
{
    if (!Open())
    {
        m_error = "Failed to open DVB device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_request_recording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    // Listen for time table on DVB standard streams
    if (m_channel && (m_channel->GetSIStandard() == "dvb"))
        m_stream_data->AddListeningPID(DVB_TDT_PID);
    if (m_record_mpts_only)
        m_stream_data->AddListeningPID(0x2000);

    StartNewFile();

    m_stream_data->AddAVListener(this);
    m_stream_data->AddWritingListener(this);
    m_stream_handler->AddListener(m_stream_data, false, true,
                         (m_record_mpts) ? m_ringBuffer->GetFilename() : QString());

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        {   // sleep 100 milliseconds unless StopRecording() or Unpause()
            // is called, just to avoid running this too often.
            QMutexLocker locker(&m_pauseLock);
            if (!m_request_recording || m_request_pause)
                continue;
            m_unpauseWait.wait(&m_pauseLock, 100);
        }

        if (!m_input_pmt && !m_record_mpts_only)
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

    m_stream_handler->RemoveListener(m_stream_data);
    m_stream_data->RemoveWritingListener(this);
    m_stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();
}

bool DVBRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_request_pause)
    {
        if (!IsPaused(true))
        {
            m_stream_handler->RemoveListener(m_stream_data);

            m_paused = true;
            m_pauseWait.wakeAll();
            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }

        m_unpauseWait.wait(&m_pauseLock, timeout);
    }

    if (!m_request_pause && IsPaused(true))
    {
        m_paused = false;
        m_stream_handler->AddListener(m_stream_data, false, true);
        m_unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

QString DVBRecorder::GetSIStandard(void) const
{
    return m_channel->GetSIStandard();
}

void DVBRecorder::SetCAMPMT(const ProgramMapTable *pmt)
{
    if (pmt->IsEncrypted(m_channel->GetSIStandard()))
        m_channel->SetPMT(pmt);
}

void DVBRecorder::UpdateCAMTimeOffset(void)
{
    m_channel->SetTimeOffset(GetStreamData()->TimeOffset());
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
