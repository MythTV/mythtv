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
const int FirewireRecorder::kTimeoutInSeconds    = 15;
const int FirewireRecorder::kConnectionP2P       = 0;
const int FirewireRecorder::kConnectionBroadcast = 1;

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

FirewireRecorder::FirewireRecorder(TVRec *rec)
    : DTVRecorder(rec, "FirewireRecorder"),
      fwport(-1),     fwchannel(-1), fwspeed(-1),   fwbandwidth(-1),
      fwfd(-1),       fwconnection(kConnectionP2P),
      fwoplug(-1),    fwiplug(-1),   fwmodel(""),   fwnode(0),
      fwhandle(NULL), fwmpeg(NULL),  isopen(false), lastpacket(0),
      _mpeg_stream_data(NULL)
{
    _mpeg_stream_data = new MPEGStreamData(1, true);
    connect(_mpeg_stream_data,
            SIGNAL(UpdatePATSingleProgram(ProgramAssociationTable*)),
            this, SLOT(WritePAT(ProgramAssociationTable*)));
    connect(_mpeg_stream_data,
            SIGNAL(UpdatePMTSingleProgram(ProgramMapTable*)),
            this, SLOT(WritePMT(ProgramMapTable*)));
}

FirewireRecorder::~FirewireRecorder()
{
   Close();
   if (_mpeg_stream_data)
   {
       delete _mpeg_stream_data;
       _mpeg_stream_data = NULL;
   }
}

void FirewireRecorder::deleteLater(void)
{
    Close();
    DTVRecorder::deleteLater();
}

bool FirewireRecorder::Open(void)
{
     if (isopen)
         return true;

     if (!_mpeg_stream_data)
         return false;

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

     // Set buffer size
     size_t buffer_size = gContext->GetNumSetting("HDRingbufferSize",
                                                  50 * TSPacket::SIZE);
     iec61883_mpeg2_set_buffers(fwmpeg, buffer_size / 2);
     VERBOSE(VB_IMPORTANT, LOC +
             QString("Buffer size %1 KB").arg(buffer_size));

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

void FirewireRecorder::SetStreamData(MPEGStreamData *stream_data)
{
    if (stream_data == _mpeg_stream_data)
        return;

    MPEGStreamData *old_data = _mpeg_stream_data;
    _mpeg_stream_data = stream_data;
    if (old_data)
        delete old_data;
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

void FirewireRecorder::StartRecording(void)
{
    struct timeval tv;
    fd_set rfds;

    VERBOSE(VB_RECORD, LOC + "StartRecording");

    if (!Open())
    {
        _error = true;
        return;
    }

    _request_recording = true;
    _recording = true;

    iec61883_mpeg2_recv_start(fwmpeg,fwchannel);
    lastpacket = time(NULL);
    while (_request_recording)
    {
       if (PauseAndWait())
           continue;

       if (time(NULL) - lastpacket > kTimeoutInSeconds)
       {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("No Input in %1 seconds [P:%2 N:%3] (time)")
                    .arg(kTimeoutInSeconds).arg(fwport).arg(fwnode));

            iec61883_mpeg2_recv_stop(fwmpeg);
            _error = true;
            return;
       }

       FD_ZERO(&rfds);
       FD_SET(fwfd, &rfds);
       tv.tv_sec = kTimeoutInSeconds;
       tv.tv_usec = 0;

       if (select(fwfd + 1, &rfds, NULL, NULL, &tv) > 0)
       {
           int ret = raw1394_loop_iterate(fwhandle);
           if (ret)
           {
               VERBOSE(VB_IMPORTANT, LOC_ERR + "libraw1394_loop_iterate() " +
                       QString("returned %1").arg(ret));

               iec61883_mpeg2_recv_stop(fwmpeg);
               _error = true;
               return;	
           }
       }
       else
       {
           VERBOSE(VB_IMPORTANT, LOC +
                   QString("No Input in %1 seconds [P:%2 N:%3] (select)")
                   .arg(kTimeoutInSeconds).arg(fwport).arg(fwnode));

           iec61883_mpeg2_recv_stop(fwmpeg);
           // to bad setting _error does nothing once recording has started..
           _error = true;
           return;
       }
    }

    iec61883_mpeg2_recv_stop(fwmpeg);

    VERBOSE(VB_IMPORTANT, QString("Firewire: Total dropped packets %1")
            .arg(iec61883_mpeg2_get_dropped(fwmpeg)));

    FinishRecording();
    _recording = false;
}

void FirewireRecorder::WritePAT(ProgramAssociationTable *pat)
{
    if (!pat)
        return;

    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pat->tsheader())));
}

void FirewireRecorder::WritePMT(ProgramMapTable *pmt)
{
    if (!pmt)
        return;

    int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<const TSPacket*>(pmt->tsheader())));
}

void FirewireRecorder::ProcessTSPacket(const TSPacket &tspacket)
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

    lastpacket = time(NULL);
    _ts_stats.IncrTSPacketCount();
    if (0 == _ts_stats.TSPacketCount()%1000000)
        VERBOSE(VB_RECORD, _ts_stats.toString());
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
            if (tvrec)
                tvrec->RecorderPaused();
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
