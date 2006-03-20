/**
 *  FirewireRecorderBase
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FIREWIRERECORDERBASE_H_
#define FIREWIRERECORDERBASE_H_

#include "dtvrecorder.h"
#include "tsstats.h"
#include "mpeg/tspacket.h"

class MPEGStreamData; 
class ProgramAssociationTable; 
class ProgramMapTable;

/** \class FirewireRecorderBase
 *  \brief This is a specialization of DTVRecorder used to
 *         handle DVB and ATSC streams from a firewire input.
 *
 *  \sa DTVRecorder
 */
class FirewireRecorderBase : public DTVRecorder
{
  Q_OBJECT 
    friend class MPEGStreamData; 
    friend class TSPacketProcessor; 

  public:
    FirewireRecorderBase(TVRec *rec, char const* name); 
    ~FirewireRecorderBase(); 
 
    // Commands 
    void StartRecording(void);
    void ProcessTSPacket(const TSPacket &tspacket);
    bool PauseAndWait(int timeout = 100); 

    // Sets
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);
    void SetStreamData(MPEGStreamData*);

    // Gets 
    MPEGStreamData* StreamData(void) { return _mpeg_stream_data; }

  public slots:
    void deleteLater(void);
    void WritePAT(ProgramAssociationTable*); 
    void WritePMT(ProgramMapTable*);

  private:
    virtual void Close() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool grab_frames() = 0;

    MPEGStreamData  *_mpeg_stream_data; 
    TSStats          _ts_stats;   

  protected: 
    static const int  kTimeoutInSeconds; 
};

#endif
