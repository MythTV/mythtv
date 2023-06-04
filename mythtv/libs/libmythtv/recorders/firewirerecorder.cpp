/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV includes
#include "libmythbase/mythlogging.h"

#include "firewirechannel.h"
#include "firewirerecorder.h"
#include "mpeg/mpegstreamdata.h"
#include "mpeg/mpegtables.h"
#include "tv_rec.h"

#define LOC QString("FireRecBase[%1](%2): ") \
            .arg(m_tvrec ? m_tvrec->GetInputId() : -1) \
            .arg(m_channel->GetDevice())

FirewireRecorder::FirewireRecorder(TVRec *rec, FirewireChannel *chan) :
    DTVRecorder(rec),
    m_channel(chan)
{
}

FirewireRecorder::~FirewireRecorder()
{
    Close();
}

bool FirewireRecorder::Open(void)
{
    if (!m_isopen)
    {
        m_isopen = m_channel->GetFirewireDevice()->OpenPort();
        ResetForNewFile();
    }
    return m_isopen;
}

void FirewireRecorder::Close(void)
{
    if (m_isopen)
    {
        m_channel->GetFirewireDevice()->ClosePort();
        m_isopen = false;
    }
}

void FirewireRecorder::StartStreaming(void)
{
    m_channel->GetFirewireDevice()->AddListener(this);
}

void FirewireRecorder::StopStreaming(void)
{
    m_channel->GetFirewireDevice()->RemoveListener(this);
}

void FirewireRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run");

    if (!Open())
    {
        m_error = "Failed to open firewire device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    StartStreaming();

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!IsRecordingRequested())
            break;

        {   // sleep 1 seconds unless StopRecording() or Unpause() is called,
            // just to avoid running this too often.
            QMutexLocker locker(&m_pauseLock);
            if (!m_requestRecording || m_requestPause)
                continue;
            m_unpauseWait.wait(&m_pauseLock, 1000);
        }
    }

    StopStreaming();
    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();
}

void FirewireRecorder::AddData(const unsigned char *data, uint len)
{
    uint bufsz = m_buffer.size();
    if ((SYNC_BYTE == data[0]) && (TSPacket::kSize == len) &&
        (TSPacket::kSize > bufsz))
    {
        if (bufsz)
            m_buffer.clear();

        ProcessTSPacket(*(reinterpret_cast<const TSPacket*>(data)));
        return;
    }

    m_buffer.insert(m_buffer.end(), data, data + len);
    bufsz += len;

    int sync_at = -1;
    for (uint i = 0; (i < bufsz) && (sync_at < 0); i++)
    {
        if (m_buffer[i] == SYNC_BYTE)
            sync_at = i;
    }

    if (sync_at < 0)
        return;

    if (bufsz < 30 * TSPacket::kSize)
        return; // build up a little buffer

    while (sync_at + TSPacket::kSize < bufsz)
    {
        ProcessTSPacket(*(reinterpret_cast<const TSPacket*>(
                              (m_buffer).data() + sync_at)));

        sync_at += TSPacket::kSize;
    }

    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + sync_at);
}

bool FirewireRecorder::ProcessTSPacket(const TSPacket &tspacket)
{
    const uint pid = tspacket.PID();

    if (pid == 0x1fff) // Stuffing
        return true;

    if (tspacket.TransportError())
        return true;

    if (tspacket.Scrambled())
        return true;

    if (tspacket.HasAdaptationField())
        GetStreamData()->HandleAdaptationFieldControl(&tspacket);

    if (GetStreamData()->IsVideoPID(tspacket.PID()))
        return ProcessVideoTSPacket(tspacket);

    if (GetStreamData()->IsAudioPID(tspacket.PID()))
        return ProcessAudioTSPacket(tspacket);

    if (GetStreamData()->IsWritingPID(tspacket.PID()))
        BufferedWrite(tspacket);

    if (GetStreamData()->IsListeningPID(tspacket.PID()) && tspacket.HasPayload())
        GetStreamData()->HandleTSTables(&tspacket);

    return true;
}

// documented in recorderbase.cpp
bool FirewireRecorder::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("PauseAndWait(%1) -- pause").arg(timeout.count()));
        if (!IsPaused(true))
        {
            StopStreaming();
            m_paused = true;
            m_pauseWait.wakeAll();
            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }
        m_unpauseWait.wait(&m_pauseLock, timeout.count());
    }

    if (!m_requestPause && IsPaused(true))
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("PauseAndWait(%1) -- unpause").arg(timeout.count()));
        m_paused = false;
        StartStreaming();
        m_unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

void FirewireRecorder::InitStreamData(void)
{
    m_streamData->AddMPEGSPListener(this);

    if (m_streamData->DesiredProgram() >= 0)
        m_streamData->SetDesiredProgram(m_streamData->DesiredProgram());
}
