/**
 *  FreeboxRecorder
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FREEBOXRECORDER_H_
#define FREEBOXRECORDER_H_

#include <qwaitcondition.h>

#include "dtvrecorder.h"
#include "freeboxmediasink.h"
#include "rtspcomms.h"
#include "streamlisteners.h"

class TSPacket;
class FreeboxChannel;
class FreeboxChannelInfo;
class MPEGStreamData;

/** \brief Processes data from RTSPComms and writes it to disk.
 */
class FreeboxRecorder : public DTVRecorder, public RTSPListener,
                        public MPEGSingleProgramStreamListener
{
    friend class FreeboxMediaSink;
    friend class RTSPComms;

  public:
    FreeboxRecorder(TVRec *rec, FreeboxChannel *channel);
    ~FreeboxRecorder();

    bool Open(void);
    void Close(void);

    virtual void Pause(bool clear = true);
    virtual void Unpause(void);

    virtual void StartRecording(void);
    virtual void StopRecording(void);

    virtual void SetOptionsFromProfile(RecordingProfile*, const QString&,
                                       const QString&, const QString&) {}

    virtual void SetStreamData(MPEGStreamData*);
    virtual MPEGStreamData *GetStreamData(void) { return _stream_data; }

  private:
    void ProcessTSPacket(const TSPacket& tspacket);

    // implements RTSPListener
    void AddData(unsigned char *data,
                 unsigned int   dataSize,
                 struct timeval presentationTime);

    // implements MPEGSingleProgramStreamListener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

  private:
    FreeboxChannel *_channel;
    MPEGStreamData *_stream_data;
    RTSPComms      *_rtsp;
    QWaitCondition  _cond_recording;


  private:
    FreeboxRecorder& operator=(const FreeboxRecorder&); //< avoid default impl
    FreeboxRecorder(const FreeboxRecorder&);            //< avoid default impl
    FreeboxRecorder();                                  //< avoid default impl
};

#endif //FREEBOXRECORDER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
