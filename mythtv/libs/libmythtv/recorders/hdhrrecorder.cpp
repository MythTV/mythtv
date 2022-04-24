/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd, and
 *    Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// MythTV includes
#include "libmythbase/mythlogging.h"

#include "hdhrchannel.h"
#include "hdhrrecorder.h"
#include "hdhrstreamhandler.h"
#include "io/mythmediabuffer.h"
#include "mpeg/atscstreamdata.h"
#include "mpeg/tsstreamdata.h"
#include "tv_rec.h"

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

    if (m_channel->GetFormat().compare("MPTS") == 0)
    {
        // MPTS only.  Use TSStreamData to write out unfiltered data.
        LOG(VB_RECORD, LOG_INFO, LOC + "Using TSStreamData");
        SetStreamData(new TSStreamData(m_tvrec ? m_tvrec->GetInputId() : -1));
        m_recordMptsOnly = true;
        m_recordMpts = false;
    }

    m_streamHandler = HDHRStreamHandler::Get(m_channel->GetDevice(),
                                             m_channel->GetInputID(),
                                             m_channel->GetMajorID());

    LOG(VB_RECORD, LOG_INFO, LOC + "HDHR opened successfully");

    return true;
}

void HDHRRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        HDHRStreamHandler::Return(m_streamHandler,
                                  (m_tvrec ? m_tvrec->GetInputId() : -1));

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

void HDHRRecorder::StartNewFile(void)
{
    if (!m_recordMptsOnly)
    {
        if (m_recordMpts)
            m_streamHandler->AddNamedOutputFile(m_ringBuffer->GetFilename());

        // Make sure the first things in the file are a PAT & PMT
        HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
        HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
    }
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
        m_requestRecording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    // Listen for time table on DVB standard streams
    if (m_channel && (m_channel->GetSIStandard() == "dvb"))
        m_streamData->AddListeningPID(PID::DVB_TDT_PID);

    // Gives errors about invalid PID but it does work...
    if (m_recordMptsOnly)
        m_streamData->AddListeningPID(0x2000);

    StartNewFile();

    m_streamData->AddAVListener(this);
    m_streamData->AddWritingListener(this);
    m_streamHandler->AddListener(m_streamData, false, false,
                         (m_recordMpts) ? m_ringBuffer->GetFilename() : QString());

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!IsRecordingRequested())
            break;

        {   // sleep 100 milliseconds unless StopRecording() or Unpause()
            // is called, just to avoid running this too often.
            QMutexLocker locker(&m_pauseLock);
            if (!m_requestRecording || m_requestPause)
                continue;
            m_unpauseWait.wait(&m_pauseLock, 100);
        }

        if (!m_inputPmt && !m_recordMptsOnly)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                    "Recording will not commence until a PMT is set.");
            std::this_thread::sleep_for(5ms);
            continue;
        }

        if (!m_streamHandler->IsRunning())
        {
            m_error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- ending...");

    m_streamHandler->RemoveListener(m_streamData);
    m_streamData->RemoveWritingListener(this);
    m_streamData->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
}

bool HDHRRecorder::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        if (!IsPaused(true))
        {
            m_streamHandler->RemoveListener(m_streamData);

            m_paused = true;
            m_pauseWait.wakeAll();
            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }

        m_unpauseWait.wait(&m_pauseLock, timeout.count());
    }

    if (!m_requestPause && IsPaused(true))
    {
        m_paused = false;
        m_streamHandler->AddListener(m_streamData);
        m_unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

QString HDHRRecorder::GetSIStandard(void) const
{
    return m_channel->GetSIStandard();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
