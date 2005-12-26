/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FIREWIRERECORDER_H_
#define FIREWIRERECORDER_H_

#include "dtvrecorder.h"
#include "mpeg/tspacket.h"
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include <time.h>

#define FIREWIRE_TIMEOUT 15

#define FIREWIRE_CONNECTION_P2P		0
#define FIREWIRE_CONNECTION_BROADCAST	1

#define FIREWIRE_CHANNEL_BROADCAST	63

class FirewireRecorder : public DTVRecorder
{
  public:

    FirewireRecorder();
    ~FirewireRecorder();

    void StartRecording(void);
    bool Open(void); 
    void ProcessTSPacket(unsigned char *tspacket, int len);
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);

    void SetOption(const QString &name, const QString &value);
    void SetOption(const QString &name, int value);
    QString FirewireSpeedString(int speed);

  private:
    int fwport, fwchannel, fwspeed, fwbandwidth, fwfd, fwconnection;
    int fwoplug, fwiplug;
    QString fwmodel;
    nodeid_t fwnode;
    raw1394handle_t fwhandle;
    iec61883_mpeg2_t fwmpeg;
    bool isopen;
    time_t lastpacket;
    TSPacket tpkt;
};

#endif
