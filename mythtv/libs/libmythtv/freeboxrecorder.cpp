/**
 *  FreeboxRecorder
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV headers
#include "mpegstreamdata.h"
#include "tspacket.h"
#include "freeboxchannel.h"
#include "freeboxrecorder.h"
#include "rtspcomms.h"

#define LOC QString("FBRec: ")
#define LOC_ERR QString("FBRec, Error: ")

// ============================================================================
// FreeboxRecorder : Processes data from RTSPComms and writes it to disk
// ============================================================================

FreeboxRecorder::FreeboxRecorder(TVRec *rec, FreeboxChannel *channel) :
    DTVRecorder(rec),
    _channel(channel),
    _stream_data(NULL)
{
    _channel->GetRTSP()->AddListener(this);
}

FreeboxRecorder::~FreeboxRecorder()
{
    StopRecording();
    _channel->GetRTSP()->RemoveListener(this);
}

bool FreeboxRecorder::Open(void)
{
    VERBOSE(VB_RECORD, LOC + "Open() -- begin");

    if (_channel->GetRTSP()->IsOpen())
        _channel->GetRTSP()->Close();

    FreeboxChannelInfo chaninfo = _channel->GetCurrentChanInfo();
    if (!chaninfo.isValid())
    {
        _error = true;
    }
    else
    {
        _error = !(_channel->GetRTSP()->Init()); 
        if (!_error)
            _error = !(_channel->GetRTSP()->Open(chaninfo.m_url));
    }

    VERBOSE(VB_RECORD, LOC + "Open() -- end err("<<_error<<")");
    return !_error;
}

void FreeboxRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");
    _channel->GetRTSP()->Stop();
    _channel->GetRTSP()->Close();
    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void FreeboxRecorder::Pause(bool clear)
{
    VERBOSE(VB_RECORD, LOC + "Pause() -- begin");
    DTVRecorder::Pause(clear);
    _channel->GetRTSP()->Stop();
    _channel->GetRTSP()->Close();
    VERBOSE(VB_RECORD, LOC + "Pause() -- end");
}

void FreeboxRecorder::Unpause(void)
{
    VERBOSE(VB_RECORD, LOC + "Unpause() -- begin");
    if (_recording && !_channel->GetRTSP()->IsOpen())
        Open();
    DTVRecorder::Unpause();
    VERBOSE(VB_RECORD, LOC + "Unpause() -- end");
}

void FreeboxRecorder::StartRecording(void)
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

        if (!_channel->GetRTSP()->IsOpen())
        {
            usleep(5000);
            continue;
        }

        // Go into main RTSP loop, feeding data to AddData
        _channel->GetRTSP()->Run();
    }

    // Finish up...
    FinishRecording();
    Close();

    VERBOSE(VB_RECORD, LOC + "StartRecording() -- end");
    _recording = false;
    _cond_recording.wakeAll();
}

void FreeboxRecorder::StopRecording(void)
{
    VERBOSE(VB_RECORD, LOC + "StopRecording() -- begin");
    Pause();
    _channel->GetRTSP()->Close();

    _request_recording = false;
    while (_recording)
        _cond_recording.wait(500);

    VERBOSE(VB_RECORD, LOC + "StopRecording() -- end");
}

// ===================================================
// findTSHeader : find a TS Header in flow
// ===================================================
static int FreeboxRecorder_findTSHeader(const unsigned char *data,
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
void FreeboxRecorder::AddData(unsigned char *data,
                              unsigned       dataSize,
                              struct timeval)
{
    unsigned int readIndex = 0;

    // data may be compose from more than one packet, loop to consume all data
    while (readIndex < dataSize)
    {
        // If recorder is paused, stop there
        if (IsPaused())
            return;

        // Find the next TS Header in data
        int tsPos = FreeboxRecorder_findTSHeader(data + readIndex, dataSize);

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
        if ((dataSize - tsPos) < TSPacket::SIZE)
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

void FreeboxRecorder::ProcessTSPacket(const TSPacket& tspacket)
{
    if (!_stream_data)
        return;

    if (tspacket.TransportError() || tspacket.ScramplingControl())
        return;

    if (tspacket.HasAdaptationField())
        _stream_data->HandleAdaptationFieldControl(&tspacket);

    if (tspacket.HasPayload())
    {
        const unsigned int lpid = tspacket.PID();

        // Pass or reject packets based on PID, and parse info from them
        if (lpid == _stream_data->VideoPIDSingleProgram())
        {
            _buffer_packets = !FindMPEG2Keyframes(&tspacket);
            BufferedWrite(tspacket);
        }
        else if (_stream_data->IsAudioPID(lpid))
            BufferedWrite(tspacket);
        else if (_stream_data->IsListeningPID(lpid))
            _stream_data->HandleTSTables(&tspacket);
        else if (_stream_data->IsWritingPID(lpid))
            BufferedWrite(tspacket);
    }
}

void FreeboxRecorder::SetStreamData(MPEGStreamData *data)
{
    VERBOSE(VB_RECORD, LOC + "SetStreamData()");
    if (data == _stream_data)
        return;

    MPEGStreamData *old_data = _stream_data;
    _stream_data = data;
    if (old_data)
        delete old_data;

    if (data)
        data->AddMPEGSPListener(this);
}

void FreeboxRecorder::HandleSingleProgramPAT(ProgramAssociationTable *pat)
{
    if (!pat)
        return;

    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pat->tsheader())));
}

void FreeboxRecorder::HandleSingleProgramPMT(ProgramMapTable *pmt)
{
    if (!pmt)
        return;

    int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pmt->tsheader())));
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
