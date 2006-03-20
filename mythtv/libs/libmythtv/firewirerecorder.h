/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FIREWIRERECORDER_H_
#define FIREWIRERECORDER_H_

#include "firewirerecorderbase.h"
#include "tsstats.h"
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

/** \class FirewireRecorder
 *  \brief Linux FirewireRFecorder
 *
 *  \sa FirewireRecorderBase
 */
class FirewireRecorder : public FirewireRecorderBase
{
    friend int fw_tspacket_handler(unsigned char*,int,uint,void*);

  public:
    FirewireRecorder(TVRec *rec) 
        : FirewireRecorderBase(rec, "FirewireRecorder"), 
        fwport(-1),     fwchannel(-1), fwspeed(-1),   fwbandwidth(-1), 
        fwfd(-1),       fwconnection(kConnectionP2P), 
        fwoplug(-1),    fwiplug(-1),   fwmodel(""),   fwnode(0), 
        fwhandle(NULL), fwmpeg(NULL),  isopen(false) { } 
   ~FirewireRecorder() { Close(); }

    // Commands
    bool Open(void); 

    // Sets
    void SetOption(const QString &name, const QString &value);
    void SetOption(const QString &name, int value);

  private:
    void Close(void);
    void start() { iec61883_mpeg2_recv_start(fwmpeg, fwchannel); } 
    void stop() { iec61883_mpeg2_recv_stop(fwmpeg); } 
    bool grab_frames();

  private:
    int              fwport;
    int              fwchannel;
    int              fwspeed;
    int              fwbandwidth;
    int              fwfd;
    int              fwconnection;
    int              fwoplug;
    int              fwiplug;
    QString          fwmodel;
    nodeid_t         fwnode;
    raw1394handle_t  fwhandle;
    iec61883_mpeg2_t fwmpeg;
    bool             isopen;

    static const int kBroadcastChannel;
    static const int kConnectionP2P;
    static const int kConnectionBroadcast;
    static const uint kMaxBufferedPackets;
};

#endif
