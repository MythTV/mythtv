/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV includes
#include "firewirerecorderbase.h"
#include "mythcontext.h"
#include "mpegtables.h" 
#include "mpegstreamdata.h"
#include "tv_rec.h"

#define LOC QString("FireRecBase: ") 
#define LOC_ERR QString("FireRecBase, Error: ")

const int FirewireRecorderBase::kTimeoutInSeconds = 15;

FirewireRecorderBase::FirewireRecorderBase(TVRec *rec)
    : DTVRecorder(rec), _mpeg_stream_data(NULL)
{
    SetStreamData(new MPEGStreamData(1, true));
}

FirewireRecorderBase::~FirewireRecorderBase()
{
    SetStreamData(NULL);
}

void FirewireRecorderBase::StartRecording(void) {
  
    VERBOSE(VB_RECORD, LOC + "StartRecording");

    if (!Open()) {
        _error = true;        
        return;
    }

    _request_recording = true;
    _recording = true;
   
    start();

    while(_request_recording) {
       if (PauseAndWait())
           continue;

       if (!grab_frames())
       {
           _error = true;
           return;
       }
    }        
    
    stop();
    FinishRecording();

    _recording = false;
}  

void FirewireRecorderBase::ProcessTSPacket(const TSPacket &tspacket)
{
    if (tspacket.TransportError()) 
        return; 
 
    if (tspacket.ScramplingControl()) 
        return; 
 
    if (tspacket.HasAdaptationField()) 
        StreamData()->HandleAdaptationFieldControl(&tspacket); 
 
    if (tspacket.HasPayload()) 
    { 
        const unsigned int lpid = tspacket.PID(); 
 
        // Pass or reject packets based on PID, and parse info from them 
        if (lpid == StreamData()->VideoPIDSingleProgram()) 
        { 
            _buffer_packets = !FindKeyframes(&tspacket); 
            BufferedWrite(tspacket); 
        } 
        else if (StreamData()->IsAudioPID(lpid)) 
            BufferedWrite(tspacket); 
        else if (StreamData()->IsListeningPID(lpid)) 
            StreamData()->HandleTSTables(&tspacket); 
        else if (StreamData()->IsWritingPID(lpid)) 
            BufferedWrite(tspacket); 
    } 
  
    _ts_stats.IncrTSPacketCount(); 
    if (0 == _ts_stats.TSPacketCount()%1000000) 
        VERBOSE(VB_RECORD, _ts_stats.toString());
}

void FirewireRecorderBase::SetOptionsFromProfile(RecordingProfile *profile,
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
bool FirewireRecorderBase::PauseAndWait(int timeout)
{
    if (request_pause)
    {
        if (!paused)
        {
            stop();
            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }
        unpauseWait.wait(timeout);
    }
    if (!request_pause && paused)
    {
        start();
        paused = false;
    }
    return paused;
}

void FirewireRecorderBase::SetStreamData(MPEGStreamData *data)
{
    if (data == _mpeg_stream_data)
        return;

    MPEGStreamData *old_data = _mpeg_stream_data;
    _mpeg_stream_data = data;

    if (data)
        data->AddMPEGSPListener(this);

    if (old_data)
        delete old_data;
}

void FirewireRecorderBase::HandleSingleProgramPAT(
    ProgramAssociationTable *pat) 
{ 
    if (!pat) 
        return; 
 
    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf; 
    pat->tsheader()->SetContinuityCounter(next); 
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pat->tsheader()))); 
} 
 
void FirewireRecorderBase::HandleSingleProgramPMT(ProgramMapTable *pmt) 
{ 
    if (!pmt) 
        return; 
 
    int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf; 
    pmt->tsheader()->SetContinuityCounter(next); 
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pmt->tsheader()))); 
}
