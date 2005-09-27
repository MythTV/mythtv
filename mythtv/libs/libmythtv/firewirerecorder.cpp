/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <iostream>
using namespace std;

#include <pthread.h>
#include "RingBuffer.h"
#include "mythcontext.h"
#include "firewirerecorder.h"
#include "mpeg/tspacket.h"
#include <sys/select.h>

// callback function for libiec61883
int read_tspacket (unsigned char *tspacket, int len, unsigned int dropped, void *callback_data) {

     FirewireRecorder *fw = (FirewireRecorder*)callback_data;
     if(!fw) return 0;

     if(dropped) {
         VERBOSE(VB_GENERAL,QString("Firewire: %1 packet(s) dropped.").arg(dropped));
     }

     if(tspacket[0] == SYNC_BYTE) {
         fw->ProcessTSPacket(tspacket,len);
     } else {
        VERBOSE(VB_GENERAL,QString("Firewire: out of sync mpeg2ts packet"));
     }

     return 1;
}

void FirewireRecorder::deleteLater(void)
{
    Close();
    DTVRecorder::deleteLater();
}

bool FirewireRecorder::Open() {

     if(isopen)
         return true;
    
     VERBOSE(VB_GENERAL,QString("Firewire: Initializing Port: %1, Node: %2, Speed: %3")
                               .arg(fwport)
                               .arg(fwnode)
                               .arg(FirewireSpeedString(fwspeed)));

     if((fwhandle = raw1394_new_handle_on_port(fwport)) == NULL) {
         VERBOSE(VB_IMPORTANT, QString("Firewire: unable to get handle for port: %1, bailing").arg(fwport));
         perror("firewire port");
         return false;
     }

     if(fwconnection == FIREWIRE_CONNECTION_P2P) {
          VERBOSE(VB_GENERAL,QString("Firewire: Creating P2P Connection with Node: %1").arg(fwnode));
          fwchannel = iec61883_cmp_connect (fwhandle, fwnode | 0xffc0, &fwoplug, 
                        raw1394_get_local_id (fwhandle), &fwiplug, &fwbandwidth);
          if(fwchannel > -1) {
	      VERBOSE(VB_GENERAL,QString("Firewire: Created Channel: %1, Bandwidth Allocation: %2").arg(fwchannel).arg(fwbandwidth));
          }
     } else {
          VERBOSE(VB_GENERAL,QString("Firewire: Creating Broadcast Connection with Node: %1").arg(fwnode));
          if(iec61883_cmp_create_bcast_output(fwhandle, fwnode | 0xffc0, 0, FIREWIRE_CHANNEL_BROADCAST, fwspeed) != 0) {
	      VERBOSE(VB_IMPORTANT, QString("Firewire: Failed to create connection"));
	      // release raw1394 object;
              raw1394_destroy_handle(fwhandle);
              return false;
          }
          fwchannel = FIREWIRE_CHANNEL_BROADCAST;
          fwbandwidth = 0;
     }


     if((fwmpeg = iec61883_mpeg2_recv_init (fwhandle, read_tspacket, this)) == NULL) {
         VERBOSE(VB_IMPORTANT, QString("Firewire: unable to init iec61883_mpeg2 object, bailing"));
         perror("iec61883_mpeg2 object");
 
         // release raw1394 object;
	 raw1394_destroy_handle(fwhandle);
         return false;
     }
     
     // set speed if needed
     // probably shouldnt even allow user to set, 100Mbps should be more the enough
     int curspeed = iec61883_mpeg2_get_speed(fwmpeg);
     if(curspeed != fwspeed) {
         VERBOSE(VB_GENERAL,QString("Firewire: Changing Speed %1 -> %2")
                                    .arg(FirewireSpeedString(curspeed))
                                    .arg(FirewireSpeedString(fwspeed)));
         iec61883_mpeg2_set_speed(fwmpeg, fwspeed);
         if(fwspeed != iec61883_mpeg2_get_speed(fwmpeg)) {
              VERBOSE(VB_IMPORTANT, QString("Firewire: unable to set firewire speed, continuing"));
         }
     }
     
     isopen = true;
     fwfd = raw1394_get_fd(fwhandle); 
     return true;
}

void FirewireRecorder::Close(void)
{
    if (!isopen)
        return;

    isopen = false;

    VERBOSE(VB_RECORD, "Firewire: releasing iec61883_mpeg2 object");
    iec61883_mpeg2_close(fwmpeg);

    if (fwconnection == FIREWIRE_CONNECTION_P2P && fwchannel > -1)
    {
        VERBOSE(VB_RECORD, "Firewire: disconnecting channel " << fwchannel);
        iec61883_cmp_disconnect(fwhandle, fwnode | 0xffc0, fwoplug,
                                raw1394_get_local_id (fwhandle),
                                fwiplug, fwchannel, fwbandwidth);
    }

    VERBOSE(VB_RECORD, "Firewire: releasing raw1394 handle");
    raw1394_destroy_handle(fwhandle);
}

void FirewireRecorder::StartRecording(void) {

    struct timeval tv;
    fd_set rfds;
  
    VERBOSE(VB_RECORD, QString("StartRecording"));

    if (!Open()) {
        _error = true;        
        return;
    }

    _request_recording = true;
    _recording = true;
   
    iec61883_mpeg2_recv_start(fwmpeg,fwchannel);
    lastpacket = time(NULL);
    while(_request_recording) {
       if (PauseAndWait())
           continue;

       if(time(NULL) - lastpacket > FIREWIRE_TIMEOUT) {
            VERBOSE(VB_IMPORTANT, QString("Firewire: No Input in %1 seconds [P:%2 N:%3] (time)").arg(FIREWIRE_TIMEOUT).arg(fwport).arg(fwnode));
            iec61883_mpeg2_recv_stop(fwmpeg);
            _error = true;
            return;
       }

       FD_ZERO (&rfds);
       FD_SET (fwfd, &rfds);
       tv.tv_sec = FIREWIRE_TIMEOUT;
       tv.tv_usec = 0;

       if(select (fwfd + 1, &rfds, NULL, NULL, &tv) > 0) {
            if(raw1394_loop_iterate(fwhandle) != 0) {
                 VERBOSE(VB_IMPORTANT, "Firewire: libraw1394_loop_iterate() returned error.");
                 iec61883_mpeg2_recv_stop(fwmpeg);
                 _error = true;
	         return;	
            }
       } else { 
            VERBOSE(VB_IMPORTANT, QString("Firewire: No Input in %1 seconds [P:%2 N:%3] (select)").arg(FIREWIRE_TIMEOUT).arg(fwport).arg(fwnode));
            iec61883_mpeg2_recv_stop(fwmpeg);
            // to bad setting _error does nothing once recording has started..
            _error = true;
            return;
       }
    }        

    iec61883_mpeg2_recv_stop(fwmpeg);
    FinishRecording();
    _recording = false;
}  

void FirewireRecorder::ProcessTSPacket(unsigned char *tspacket, int len) {
    tpkt.InitHeader(tspacket);
    tpkt.InitPayload(tspacket+4);
    FindKeyframes(&tpkt);
    lastpacket = time(NULL);
    ringBuffer->Write(tspacket,len);
}

void FirewireRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                         const QString &videodev,
                                         const QString &audiodev,
                                         const QString &vbidev, int ispip) {
    (void)videodev;
    (void)audiodev;
    (void)vbidev;
    (void)profile;
    (void)ispip;
}

void FirewireRecorder::SetOption(const QString &name, const QString &value) {

    if(name == "model") {
        fwmodel = value;
    }
}

void FirewireRecorder::SetOption(const QString &name, int value) {

    if(name == "port") {
	fwport = value;
    } else if(name == "node") {
        fwnode = value;
    } else if(name == "speed") {
        fwspeed = value;
        if(fwspeed != RAW1394_ISO_SPEED_100 && fwspeed != RAW1394_ISO_SPEED_200 && fwspeed != RAW1394_ISO_SPEED_400) {
            VERBOSE(VB_IMPORTANT, QString("Firewire: Invalid speed '%1', assuming 0 (100Mbps)").arg(fwspeed));
            fwspeed = 0;
        }
    } else if(name == "connection") {
	fwconnection = value;
	if(fwconnection != FIREWIRE_CONNECTION_P2P && fwconnection != FIREWIRE_CONNECTION_BROADCAST) {
	    VERBOSE(VB_IMPORTANT, QString("Firewire: Invalid Connection type '%1', assuming P2P").arg(fwconnection));
            fwconnection = FIREWIRE_CONNECTION_P2P;
        }
    }
}

QString FirewireRecorder::FirewireSpeedString (int speed) {
    switch(speed) {
        case RAW1394_ISO_SPEED_100:
               return("100Mbps");
        case RAW1394_ISO_SPEED_200: 
               return("200Mbps");
        case RAW1394_ISO_SPEED_400:
               return("400Mbps");
        default:
               return(QString("Invalid (%1)").arg(speed));
     }
}

// documented in recorderbase.cpp
bool FirewireRecorder::PauseAndWait(int timeout)
{
    if (request_pause)
    {
        if (!paused)
        {
            iec61883_mpeg2_recv_stop(fwmpeg);
            paused = true;
            pauseWait.wakeAll();
            emit RecorderPaused();
        }
        unpauseWait.wait(timeout);
    }
    if (!request_pause && paused)
    {
        iec61883_mpeg2_recv_start(fwmpeg, fwchannel);
        paused = false;
    }
    return paused;
}
