/** -*- Mode: c++ -*-
 *  IPTVRecorder
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// MythTV headers
#include "iptvchannel.h"
#include "iptvrecorder.h"
#include "mpeg/mpegstreamdata.h"
#include "tv_rec.h"

#define LOC QString("IPTVRec: ")

IPTVRecorder::IPTVRecorder(TVRec *rec, IPTVChannel *channel) :
    DTVRecorder(rec), m_channel(channel)
{
}

IPTVRecorder::~IPTVRecorder()
{
    StopRecording();
    Close();
}

bool IPTVRecorder::Open(void)
{
    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Stream already open");
        return true;
    }

    ResetForNewFile();

    if (!m_channel->Open())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open: Open channel failed");

    LOG(VB_RECORD, LOG_INFO, LOC + "opened successfully");

    if (m_streamData)
        m_channel->SetStreamData(m_streamData);

    return m_streamData;
}

bool IPTVRecorder::IsOpen(void) const
{
    if (!m_channel)
        return false;
    return m_channel->IsOpen();
}

void IPTVRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close()");

    m_channel->Close();
}

void IPTVRecorder::SetStreamData(MPEGStreamData *data)
{
    DTVRecorder::SetStreamData(data);
    if (IsOpen() && !IsPaused())
        m_channel->SetStreamData(m_streamData);
}

bool IPTVRecorder::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        if (!IsPaused(true))
        {
            m_channel->SetStreamData(nullptr);
            m_paused = true;
            m_pauseWait.wakeAll();
            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }

        m_unpauseWait.wait(&m_pauseLock, timeout.count());
    }

    if (!m_requestPause && IsPaused(true))
    {
        m_channel->SetStreamData(m_streamData);
        m_paused = false;
        m_unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

void IPTVRecorder::StartNewFile(void)
{
    // Make sure the first things in the file are a PAT & PMT
    HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
    HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
}

void IPTVRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");

    if (!Open())
    {
        m_error = "Failed to open IPTVRecorder device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    StartNewFile();

    m_streamData->AddAVListener(this);
    m_streamData->AddWritingListener(this);

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

        if (!m_inputPmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                    "Recording will not commence until a PMT is set.");
            std::this_thread::sleep_for(5ms);
            continue;
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- ending...");

    m_streamData->RemoveWritingListener(this);
    m_streamData->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
