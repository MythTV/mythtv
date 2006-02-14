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

#define FIREWIRE_TIMEOUT 15

#define FIREWIRE_CONNECTION_P2P		0
#define FIREWIRE_CONNECTION_BROADCAST	1

#define FIREWIRE_CHANNEL_BROADCAST	63

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

  public:
    FirewireRecorder(TVRec *rec);
    ~FirewireRecorder();

    void StartRecording(void);
    bool Open(void); 
    void ProcessTSPacket(const TSPacket &tspacket);
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    void SetOption(const QString &name, const QString &value);
    void SetOption(const QString &name, int value);
    QString FirewireSpeedString(int speed);
    void SetStreamData(MPEGStreamData*);
    MPEGStreamData* StreamData(void) { return _mpeg_stream_data; }

    bool PauseAndWait(int timeout = 100);

  public slots:
    void deleteLater(void);
        
  private:
    void Close(void);

  private slots:
    void WritePAT(ProgramAssociationTable*);
    void WritePMT(ProgramMapTable*);

  private:
    int fwport, fwchannel, fwspeed, fwbandwidth, fwfd, fwconnection;
    int fwoplug, fwiplug;
    QString fwmodel;
    nodeid_t fwnode;
    raw1394handle_t fwhandle;
    iec61883_mpeg2_t fwmpeg;
    bool isopen;
    time_t lastpacket;
    MPEGStreamData *_mpeg_stream_data;
    TSStats         _ts_stats;
    
};

#endif
