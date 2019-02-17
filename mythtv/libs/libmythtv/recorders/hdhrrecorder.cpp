/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd, and
 *    Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// MythTV includes
#include "hdhrstreamhandler.h"
#include "atscstreamdata.h"
#include "hdhrrecorder.h"
#include "hdhrchannel.h"
#include "ringbuffer.h"
#include "tv_rec.h"
#include "mythlogging.h"

#define LOC QString("HDHRRec[%1]: ") \
            .arg(m_tvrec ? m_tvrec->GetInputId() : -1)

bool HDHRRecorder::Open(void)
{
    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Card already open");
        return true;
    }

    ResetForNewFile();

    m_stream_handler = HDHRStreamHandler::Get(m_channel->GetDevice(),
                                              m_channel->GetInputID(),
                                              m_channel->GetMajorID());

    LOG(VB_RECORD, LOG_INFO, LOC + "HDHR opened successfully");

    return true;
}

void HDHRRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        HDHRStreamHandler::Return(m_stream_handler,
                                  (m_tvrec ? m_tvrec->GetInputId() : -1));

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

void HDHRRecorder::StartNewFile(void)
{
    if (m_record_mpts)
        m_stream_handler->AddNamedOutputFile(m_ringBuffer->GetFilename());

    // Make sure the first things in the file are a PAT & PMT
    HandleSingleProgramPAT(m_stream_data->PATSingleProgram(), true);
    HandleSingleProgramPMT(m_stream_data->PMTSingleProgram(), true);
}

void HDHRRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");

    /* Create video socket. */
    if (!Open())
    {
        m_error = "Failed to open HDHRRecorder device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_request_recording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    StartNewFile();

    m_stream_data->AddAVListener(this);
    m_stream_data->AddWritingListener(this);
    m_stream_handler->AddListener(m_stream_data, false, false,
                         (m_record_mpts) ? m_ringBuffer->GetFilename() : QString());

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!IsRecordingRequested())
            break;

        {   // sleep 100 milliseconds unless StopRecording() or Unpause()
            // is called, just to avoid running this too often.
            QMutexLocker locker(&m_pauseLock);
            if (!m_request_recording || m_request_pause)
                continue;
            m_unpauseWait.wait(&m_pauseLock, 100);
        }

        if (!m_input_pmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                    "Recording will not commence until a PMT is set.");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (!m_stream_handler->IsRunning())
        {
            m_error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- ending...");

    m_stream_handler->RemoveListener(m_stream_data);
    m_stream_data->RemoveWritingListener(this);
    m_stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
}

bool HDHRRecorder::PauseAndWait(int timeout)
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
        m_stream_handler->AddListener(m_stream_data);
        m_unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

QString HDHRRecorder::GetSIStandard(void) const
{
    return m_channel->GetSIStandard();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
