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

#define LOC QString("IPTVRec: ")
#define LOC_ERR QString("IPTVRec, Error: ")

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
    VERBOSE(VB_RECORD, LOC + "Open() -- begin");

    if (_channel->GetFeeder()->IsOpen())
        _channel->GetFeeder()->Close();

    IPTVChannelInfo chaninfo = _channel->GetCurrentChanInfo();
    _error = (!chaninfo.isValid() ||
              !_channel->GetFeeder()->Open(chaninfo.m_url));

    VERBOSE(VB_RECORD, LOC + QString("Open() -- end err(%1)").arg(_error));
    return !_error;
}

void IPTVRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");
    _channel->GetFeeder()->Stop();
    _channel->GetFeeder()->Close();
    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void IPTVRecorder::Pause(bool clear)
{
    VERBOSE(VB_RECORD, LOC + "Pause() -- begin");
    DTVRecorder::Pause(clear);
    _channel->GetFeeder()->Stop();
    _channel->GetFeeder()->Close();
    VERBOSE(VB_RECORD, LOC + "Pause() -- end");
}

void IPTVRecorder::Unpause(void)
{
    VERBOSE(VB_RECORD, LOC + "Unpause() -- begin");

    if (_recording && !_channel->GetFeeder()->IsOpen())
        Open();

    if (_stream_data)
        _stream_data->Reset(_stream_data->DesiredProgram());

    DTVRecorder::Unpause();

    VERBOSE(VB_RECORD, LOC + "Unpause() -- end");
}

void IPTVRecorder::StartRecording(void)
{
    VERBOSE(VB_RECORD, LOC + "StartRecording() -- begin");
    if (!Open())
    {
        _error = true;
        return;
    }

    // Start up...
    _recording = true;
    _request_recording = true;

    while (_request_recording)
    {
        if (PauseAndWait())
            continue;

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

    VERBOSE(VB_RECORD, LOC + "StartRecording() -- end");
    _recording = false;
    _cond_recording.wakeAll();
}

void IPTVRecorder::StopRecording(void)
{
    VERBOSE(VB_RECORD, LOC + "StopRecording() -- begin");
    Pause();
    _channel->GetFeeder()->Close();

    // Qt4 requires a QMutex as a parameter...
    // not sure if this is the best solution.  Mutex Must be locked before wait.
    QMutex mutex;
    mutex.lock();

    _request_recording = false;
    while (_recording)
        _cond_recording.wait(&mutex, 500);

    VERBOSE(VB_RECORD, LOC + "StopRecording() -- end");
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
        if (IsPaused())
            return;

        // Find the next TS Header in data
        int tsPos = IPTVRecorder_findTSHeader(
            data + readIndex, dataSize - readIndex);

        // if no TS, something bad happens
        if (tsPos == -1)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "No TS header.");
            break;
        }

        // if TS Header not at start of data, we receive out of sync data
        if (tsPos > 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("TS packet at %1, not in sync.").arg(tsPos));
        }

        // Check if the next packet in buffer is complete :
        // packet size is 188 bytes long
        if ((dataSize - tsPos - readIndex) < TSPacket::SIZE)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "TS packet at stradles end of buffer.");
            break;
        }

        // Cast current found TS Packet to TSPacket structure
        const void *newData = data + tsPos + readIndex;
        ProcessTSPacket(*reinterpret_cast<const TSPacket*>(newData));

        // follow to next packet
        readIndex += tsPos + TSPacket::SIZE;
    }
}

void IPTVRecorder::ProcessTSPacket(const TSPacket& tspacket)
{
    if (!_stream_data)
        return;

    if (tspacket.TransportError() || tspacket.Scrambled())
        return;

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
}

void IPTVRecorder::SetStreamData(void)
{
    _stream_data->AddMPEGSPListener(this);
}

void IPTVRecorder::HandleSingleProgramPAT(ProgramAssociationTable *pat)
{
    if (!pat)
        return;

    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pat->tsheader())));
}

void IPTVRecorder::HandleSingleProgramPMT(ProgramMapTable *pmt)
{
    if (!pmt)
        return;

    int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pmt->tsheader())));
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
