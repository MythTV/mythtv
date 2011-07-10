// -*- Mode: c++ -*-
/**
 *  IPTVRecorder
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <unistd.h>

// MythTV headers
#include "mpegstreamdata.h"
#include "tspacket.h"
#include "iptvchannel.h"
#include "iptvfeederwrapper.h"
#include "iptvrecorder.h"
#include "tv_rec.h"

#define LOC QString("IPTVRec: ")

// ============================================================================
// IPTVRecorder : Processes data from RTSPComms and writes it to disk
// ============================================================================

IPTVRecorder::IPTVRecorder(TVRec *rec, IPTVChannel *channel) :
    DTVRecorder(rec),
    _channel(channel)
{
    _channel->GetFeeder()->AddListener(this);
}

IPTVRecorder::~IPTVRecorder()
{
    StopRecording();
    _channel->GetFeeder()->RemoveListener(this);
}

bool IPTVRecorder::Open(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Open() -- begin");

    if (_channel->GetFeeder()->IsOpen())
        _channel->GetFeeder()->Close();

    IPTVChannelInfo chaninfo = _channel->GetCurrentChanInfo();

    if (!chaninfo.isValid())
        _error = "Channel Info is invalid";
    else if (!_channel->GetFeeder()->Open(chaninfo.m_url))
        _error = QString("Failed to open URL %1")
            .arg(chaninfo.m_url);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Open() -- end err(%1)")
            .arg(_error));

    return !IsErrored();
}

void IPTVRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");
    _channel->GetFeeder()->Stop();
    _channel->GetFeeder()->Close();
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}

bool IPTVRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        if (!IsPaused(true))
        {
            _channel->GetFeeder()->Stop();
            _channel->GetFeeder()->Close();

            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }

        unpauseWait.wait(&pauseLock, timeout);
    }

    if (!request_pause && IsPaused(true))
    {
        paused = false;

        if (recording && !_channel->GetFeeder()->IsOpen())
            Open();

        if (_stream_data)
            _stream_data->Reset(_stream_data->DesiredProgram());

        unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

void IPTVRecorder::StartRecording(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "StartRecording() -- begin");
    if (!Open())
    {
        _error = "Failed to open IPTV stream";
        return;
    }

    // Start up...
    {
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        recording = true;
        recordingWait.wakeAll();
    }

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!IsRecordingRequested())
            break;

        if (!_channel->GetFeeder()->IsOpen())
        {
            usleep(5000);
            continue;
        }

        // Go into main RTSP loop, feeding data to AddData
        _channel->GetFeeder()->Run();
    }

    // Finish up...
    FinishRecording();
    Close();

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "StartRecording() -- end");
}

// ===================================================
// findTSHeader : find a TS Header in flow
// ===================================================
static int IPTVRecorder_findTSHeader(const unsigned char *data,
                                        uint dataSize)
{
    unsigned int pos = 0;

    while (pos < dataSize)
    {
        if (data[pos] == 0x47)
            return pos;
        pos++;
    }

    return -1;
}

// ===================================================
// AddData : feed data from RTSP flow to mythtv
// ===================================================
void IPTVRecorder::AddData(const unsigned char *data, unsigned int dataSize)
{
    unsigned int readIndex = 0;

    // data may be compose from more than one packet, loop to consume all data
    while (readIndex < dataSize)
    {
        // If recorder is paused, stop there
        if (IsPaused(false))
            return;

        // Find the next TS Header in data
        int tsPos = IPTVRecorder_findTSHeader(
            data + readIndex, dataSize - readIndex);

        // if no TS, something bad happens
        if (tsPos == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "No TS header.");
            break;
        }

        // if TS Header not at start of data, we receive out of sync data
        if (tsPos > 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("TS packet at %1, not in sync.").arg(tsPos));
        }

        // Check if the next packet in buffer is complete :
        // packet size is 188 bytes long
        if ((dataSize - tsPos - readIndex) < TSPacket::kSize)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "TS packet at stradles end of buffer.");
            break;
        }

        // Cast current found TS Packet to TSPacket structure
        const void *newData = data + tsPos + readIndex;
        ProcessTSPacket(*reinterpret_cast<const TSPacket*>(newData));

        // follow to next packet
        readIndex += tsPos + TSPacket::kSize;
    }
}

bool IPTVRecorder::ProcessTSPacket(const TSPacket& tspacket)
{
    if (!_stream_data)
        return true;

    if (tspacket.TransportError() || tspacket.Scrambled())
        return true;

    if (tspacket.HasAdaptationField())
        _stream_data->HandleAdaptationFieldControl(&tspacket);

    if (tspacket.HasPayload())
    {
        const unsigned int lpid = tspacket.PID();

        // Pass or reject packets based on PID, and parse info from them
        if (lpid == _stream_data->VideoPIDSingleProgram())
        {
            ProgramMapTable *pmt = _stream_data->PMTSingleProgram();
            uint video_stream_type = pmt->StreamType(pmt->FindPID(lpid));

            if (video_stream_type == StreamID::H264Video)
                _buffer_packets = !FindH264Keyframes(&tspacket);
            else if (StreamID::IsVideo(video_stream_type))
                _buffer_packets = !FindMPEG2Keyframes(&tspacket);

            if ((video_stream_type != StreamID::H264Video) || _seen_sps)
                BufferedWrite(tspacket);
        }
        else if (_stream_data->IsAudioPID(lpid))
        {
            _buffer_packets = !FindAudioKeyframes(&tspacket);
            BufferedWrite(tspacket);
        }
        else if (_stream_data->IsListeningPID(lpid))
            _stream_data->HandleTSTables(&tspacket);
        else if (_stream_data->IsWritingPID(lpid))
            BufferedWrite(tspacket);
    }

    return true;
}

void IPTVRecorder::SetStreamData(void)
{
    _stream_data->AddMPEGSPListener(this);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
