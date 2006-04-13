/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C includes
#include <pthread.h>
#include <sys/select.h>

// C++ includes
#include <iostream>
using namespace std;

// MythTV includes
#include "firewirerecorder.h"
#include "mythcontext.h"
#include "mpegtables.h"
#include "mpegstreamdata.h"
#include "tv_rec.h"

#define LOC QString("FireRec: ")
#define LOC_ERR QString("FireRec, Error: ")

const int FirewireRecorder::kBroadcastChannel    = 63;
const int FirewireRecorder::kConnectionP2P       = 0;
const int FirewireRecorder::kConnectionBroadcast = 1;
const uint FirewireRecorder::kMaxBufferedPackets = 8000;

// callback function for libiec61883
int fw_tspacket_handler(unsigned char *tspacket, int /*len*/,
                        uint dropped, void *callback_data)
{
    if (dropped)
    {
        VERBOSE(VB_RECORD, LOC_ERR +
                QString("Dropped %1 packet(s).").arg(dropped));
    }

    if (SYNC_BYTE != tspacket[0])
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "TS packet out of sync.");
        return 1;
    }

    FirewireRecorder *fw = (FirewireRecorder*) callback_data;
    if (fw)
        fw->ProcessTSPacket(*(reinterpret_cast<TSPacket*>(tspacket)));

    return (fw) ? 1 : 0;
}

static QString speed_to_string(uint speed)
{
    if (speed > RAW1394_ISO_SPEED_400)
        return QString("Invalid Speed (%1)").arg(speed);

    static const uint speeds[] = { 100, 200, 400, };
    return QString("%1Mbps").arg(speeds[speed]);
}

bool FirewireRecorder::Open(void)
{
     if (isopen)
         return true;

     VERBOSE(VB_RECORD, LOC +
             QString("Initializing Port: %1, Node: %2, Speed: %3")
             .arg(fwport).arg(fwnode).arg(speed_to_string(fwspeed)));

     fwhandle = raw1394_new_handle_on_port(fwport);
     if (!fwhandle)
     {
         VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to get handle for " +
                 QString("port: %1, bailing").arg(fwport) + ENO);
         return false;
     }

     if (kConnectionP2P == fwconnection)
     {
          VERBOSE(VB_RECORD, LOC + "Creating P2P Connection " +
                  QString("with Node: %1").arg(fwnode));
          fwchannel = iec61883_cmp_connect(fwhandle,
                                           fwnode | 0xffc0, &fwoplug,
                                           raw1394_get_local_id(fwhandle),
                                           &fwiplug, &fwbandwidth);
          if (fwchannel > -1)
          {
	      VERBOSE(VB_RECORD, LOC +
                      QString("Created Channel: %1, "
                              "Bandwidth Allocation: %2")
                      .arg(fwchannel).arg(fwbandwidth));
          }
     }
     else
     {
         VERBOSE(VB_RECORD, LOC + "Creating Broadcast Connection" +
                 QString("with Node: %1").arg(fwnode));
         if (iec61883_cmp_create_bcast_output(fwhandle,
                                              fwnode | 0xffc0, 0,
                                              kBroadcastChannel,
                                              fwspeed) != 0)
         {
             VERBOSE(VB_IMPORTANT, LOC + "Failed to create connection");
             // release raw1394 object;
             raw1394_destroy_handle(fwhandle);
             return false;
         }
         fwchannel   = kBroadcastChannel;
         fwbandwidth = 0;
     }

     fwmpeg = iec61883_mpeg2_recv_init(fwhandle, fw_tspacket_handler, this);
     if (!fwmpeg)
     {
         VERBOSE(VB_IMPORTANT, LOC +
                 "Unable to init iec61883_mpeg2 object, bailing" + ENO);

         // release raw1394 object;
	 raw1394_destroy_handle(fwhandle);
         return false;
     }

     // Set buffered packets size
     size_t buffer_size = gContext->GetNumSetting("HDRingbufferSize",
                                                  50 * TSPacket::SIZE);
     size_t buffered_packets = min(buffer_size / 4,
                                   (size_t) kMaxBufferedPackets);
     iec61883_mpeg2_set_buffers(fwmpeg, buffered_packets);
     VERBOSE(VB_IMPORTANT, LOC +
             QString("Buffered packets %1 (%2 KB)")
             .arg(buffered_packets).arg(buffered_packets * 4));

     // Set speed if needed.
     // Probably shouldn't even allow user to set,
     // 100Mbps should be more the enough.
     int curspeed = iec61883_mpeg2_get_speed(fwmpeg);
     if (curspeed != fwspeed)
     {
         VERBOSE(VB_RECORD, LOC +
                 QString("Changing Speed %1 -> %2")
                 .arg(speed_to_string(curspeed))
                 .arg(speed_to_string(fwspeed)));

         iec61883_mpeg2_set_speed(fwmpeg, fwspeed);
         if (fwspeed != iec61883_mpeg2_get_speed(fwmpeg))
         {
              VERBOSE(VB_IMPORTANT, LOC +
                      "Unable to set firewire speed, continuing");
         }
     }

     fwfd = raw1394_get_fd(fwhandle);

     return isopen = true;
}

void FirewireRecorder::Close(void)
{
    if (!isopen)
        return;

    isopen = false;

    VERBOSE(VB_RECORD, LOC + "Releasing iec61883_mpeg2 object");
    iec61883_mpeg2_close(fwmpeg);

    if (fwconnection == kConnectionP2P && fwchannel > -1)
    {
        VERBOSE(VB_RECORD, LOC +
                QString("Disconnecting channel %1").arg(fwchannel));

        iec61883_cmp_disconnect(fwhandle, fwnode | 0xffc0, fwoplug,
                                raw1394_get_local_id (fwhandle),
                                fwiplug, fwchannel, fwbandwidth);
    }

    VERBOSE(VB_RECORD, LOC + "Releasing raw1394 handle");
    raw1394_destroy_handle(fwhandle);
}

bool FirewireRecorder::grab_frames()
{
    struct timeval tv;
    fd_set rfds;

    FD_ZERO(&rfds); 
    FD_SET(fwfd, &rfds); 
    tv.tv_sec = kTimeoutInSeconds; 
    tv.tv_usec = 0;

    if (select(fwfd + 1, &rfds, NULL, NULL, &tv)  <= 0) 
    { 
        VERBOSE(VB_IMPORTANT, LOC + 
                QString("No Input in %1 seconds [P:%2 N:%3] (select)") 
                .arg(kTimeoutInSeconds).arg(fwport).arg(fwnode)); 
        return false; 
    }

    int ret = raw1394_loop_iterate(fwhandle); 
    if (ret)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "libraw1394_loop_iterate() " + 
                QString("returned %1").arg(ret)); 
        return false;
    }

    return true;
}

void FirewireRecorder::SetOption(const QString &name, const QString &value)
{
    if (name == "model")
        fwmodel = value;
}

void FirewireRecorder::SetOption(const QString &name, int value)
{
    if (name == "port")
	fwport = value;
    else if (name == "node")
        fwnode = value;
    else if (name == "speed")
    {
        if (RAW1394_ISO_SPEED_100 != value &&
            RAW1394_ISO_SPEED_200 != value &&
            RAW1394_ISO_SPEED_400 != value)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Unknown speed '%1', will use 100Mbps")
                    .arg(value));

            value = RAW1394_ISO_SPEED_100;
        }
        fwspeed = value;
    }
    else if (name == "connection")
    {
	if (kConnectionP2P       != value &&
            kConnectionBroadcast != value)
        {
	    VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Unknown connection type '%1', will use P2P")
                    .arg(fwconnection));

            fwconnection = kConnectionP2P;
        }
	fwconnection = value;
    }
}
