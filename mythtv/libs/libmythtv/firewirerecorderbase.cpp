/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV includes
#include "firewirerecorderbase.h"
#include "mythcontext.h"
#include "tspacket.h"
#include "tv_rec.h"

// callback function for libiec61883
int FirewireRecorderBase::read_tspacket (unsigned char *tspacket, int /*len*/,
                   uint dropped, void *callback_data)
{
    FirewireRecorderBase *fw = (FirewireRecorderBase*) callback_data;

    if (!fw)
        return 0;

    if (dropped)
    {
        VERBOSE(VB_RECORD,
                QString("Firewire: %1 packet(s) dropped.").arg(dropped));
    }

    if (SYNC_BYTE != tspacket[0])
    {
        VERBOSE(VB_IMPORTANT, "Firewire: Got out of sync TS Packet");
        return 1;
    }

    fw->ProcessTSPacket(*(reinterpret_cast<TSPacket*>(tspacket)));

    return 1;
}
 
void FirewireRecorderBase::deleteLater(void)
{
    Close();
    DTVRecorder::deleteLater();
}

void FirewireRecorderBase::StartRecording(void) {
  
    VERBOSE(VB_RECORD, QString("StartRecording"));

    if (!Open()) {
        _error = true;        
        return;
    }

    _request_recording = true;
    _recording = true;
   
    this->start();

    lastpacket = time(NULL);
    while(_request_recording) {
       if (PauseAndWait())
           continue;

       if (time(NULL) - lastpacket > FIREWIRE_TIMEOUT) {
           this->no_data();
           this->stop();
           _error = true;
           return;
       }

       if (!this->grab_frames())
       {
           _error = true;
           return;
       }
    }        
    
    this->stop();
    FinishRecording();

    _recording = false;
}  

void FirewireRecorderBase::ProcessTSPacket(const TSPacket &tspacket)
{
    lastpacket = time(NULL);
    _buffer_packets = !FindKeyframes(&tspacket);
    BufferedWrite(tspacket);
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
            this->stop();
            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }
        unpauseWait.wait(timeout);
    }
    if (!request_pause && paused)
    {
        this->start();
        paused = false;
    }
    return paused;
}
