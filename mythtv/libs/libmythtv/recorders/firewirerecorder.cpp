/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV includes
#include "firewirerecorder.h"
#include "firewirechannel.h"
#include "mythlogging.h"
#include "mpegtables.h"
#include "mpegstreamdata.h"
#include "tv_rec.h"

#define LOC QString("FireRecBase(%1): ").arg(channel->GetDevice())

FirewireRecorder::FirewireRecorder(TVRec *rec, FirewireChannel *chan) :
    DTVRecorder(rec),
    channel(chan), isopen(false)
{
}

FirewireRecorder::~FirewireRecorder()
{
    Close();
}

bool FirewireRecorder::Open(void)
{
    if (!isopen)
    {
        isopen = channel->GetFirewireDevice()->OpenPort();
        ResetForNewFile();
    }
    return isopen;
}

void FirewireRecorder::Close(void)
{
    if (isopen)
    {
        channel->GetFirewireDevice()->ClosePort();
        isopen = false;
    }
}

void FirewireRecorder::StartStreaming(void)
{
    channel->GetFirewireDevice()->AddListener(this);
}

void FirewireRecorder::StopStreaming(void)
{
    channel->GetFirewireDevice()->RemoveListener(this);
}

void FirewireRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run");

    if (!Open())
    {
        _error = "Failed to open firewire device";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    {
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        recording = true;
        recordingWait.wakeAll();
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
            QMutexLocker locker(&pauseLock);
            if (!request_recording || request_pause)
                continue;
            unpauseWait.wait(&pauseLock, 1000);
        }
    }

    StopStreaming();
    FinishRecording();

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();
}

void FirewireRecorder::AddData(const unsigned char *data, uint len)
{
    uint bufsz = buffer.size();
    if ((SYNC_BYTE == data[0]) && (TSPacket::kSize == len) &&
        (TSPacket::kSize > bufsz))
    {
        if (bufsz)
            buffer.clear();

        ProcessTSPacket(*(reinterpret_cast<const TSPacket*>(data)));
        return;
    }

    buffer.insert(buffer.end(), data, data + len);
    bufsz += len;

    int sync_at = -1;
    for (uint i = 0; (i < bufsz) && (sync_at < 0); i++)
    {
        if (buffer[i] == SYNC_BYTE)
            sync_at = i;
    }

    if (sync_at < 0)
        return;

    if (bufsz < 30 * TSPacket::kSize)
        return; // build up a little buffer

    while (sync_at + TSPacket::kSize < bufsz)
    {
        ProcessTSPacket(*(reinterpret_cast<const TSPacket*>(
                              &buffer[0] + sync_at)));

        sync_at += TSPacket::kSize;
    }

    buffer.erase(buffer.begin(), buffer.begin() + sync_at);

    return;
}

bool FirewireRecorder::ProcessTSPacket(const TSPacket &tspacket)
{
    if (tspacket.TransportError())
        return true;

    if (tspacket.Scrambled())
        return true;

    if (tspacket.HasAdaptationField())
        GetStreamData()->HandleAdaptationFieldControl(&tspacket);

    if (tspacket.HasPayload())
    {
        const unsigned int lpid = tspacket.PID();

        // Pass or reject packets based on PID, and parse info from them
        if (lpid == GetStreamData()->VideoPIDSingleProgram())
        {
            _buffer_packets = !FindMPEG2Keyframes(&tspacket);
            BufferedWrite(tspacket);
        }
        else if (GetStreamData()->IsAudioPID(lpid))
        {
            _buffer_packets = !FindAudioKeyframes(&tspacket);
            BufferedWrite(tspacket);
        }
        else if (GetStreamData()->IsListeningPID(lpid))
            GetStreamData()->HandleTSTables(&tspacket);
        else if (GetStreamData()->IsWritingPID(lpid))
            BufferedWrite(tspacket);
    }

    return true;
}

void FirewireRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                                 const QString &videodev,
                                                 const QString &audiodev,
                                                 const QString &vbidev)
{
    (void)videodev;
    (void)audiodev;
    (void)vbidev;
    (void)profile;
}

// documented in recorderbase.cpp
bool FirewireRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("PauseAndWait(%1) -- pause").arg(timeout));
        if (!IsPaused(true))
        {
            StopStreaming();
            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }
        unpauseWait.wait(&pauseLock, timeout);
    }

    if (!request_pause && IsPaused(true))
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("PauseAndWait(%1) -- unpause").arg(timeout));
        paused = false;
        StartStreaming();
        unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

void FirewireRecorder::InitStreamData(void)
{
    _stream_data->AddMPEGSPListener(this);

    if (_stream_data->DesiredProgram() >= 0)
        _stream_data->SetDesiredProgram(_stream_data->DesiredProgram());
}
