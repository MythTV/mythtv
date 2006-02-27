/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FIREWIRERECORDER_H_
#define FIREWIRERECORDER_H_

#include "dtvrecorder.h"
#include "tsstats.h"
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>
#include <time.h>

class MPEGStreamData;
class ProgramAssociationTable;
class ProgramMapTable;

/** \class FirewireRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle DVB and ATSC streams from a firewire input.
 *
 *  \sa DTVRecorder
 */
class FirewireRecorder : public DTVRecorder
{
  Q_OBJECT
    friend class MPEGStreamData;
    friend class TSPacketProcessor;
    friend int fw_tspacket_handler(unsigned char*,int,uint,void*);

  public:
    FirewireRecorder(TVRec *rec);
   ~FirewireRecorder();

    // Commands
    void StartRecording(void);
    bool Open(void); 
    bool PauseAndWait(int timeout = 100);

    // Sets
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);
    void SetOption(const QString &name, const QString &value);
    void SetOption(const QString &name, int value);
    void SetStreamData(MPEGStreamData*);

    // Gets
    MPEGStreamData* StreamData(void) { return _mpeg_stream_data; }

  public slots:
    void deleteLater(void);
    void WritePAT(ProgramAssociationTable*);
    void WritePMT(ProgramMapTable*);
        
  private:
    void Close(void);
    void ProcessTSPacket(const TSPacket &tspacket);

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
    time_t           lastpacket;
    MPEGStreamData  *_mpeg_stream_data;
    TSStats          _ts_stats;  

    static const int kBroadcastChannel;
    static const int kTimeoutInSeconds;
    static const int kConnectionP2P;
    static const int kConnectionBroadcast;
};

#endif
