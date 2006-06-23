/**
 *  FreeboxRecorder
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FREEBOXRECORDER_H_
#define FREEBOXRECORDER_H_

#include "dtvrecorder.h"

class FreeboxChannel;
class FreeboxChannelInfo;
class FreeboxRecorderImpl;

/**
 *  Constructs a FreeboxRecorder
 */
class FreeboxRecorder : public DTVRecorder
{
    friend class FreeboxChannel;
    friend class FreeboxMediaSink;
    friend class FreeboxRecorderImpl;
  public:
    FreeboxRecorder(TVRec *rec, FreeboxChannel *channel);
    ~FreeboxRecorder();

    void StartRecording(void);
    void StopRecording(void);

    void SetOptionsFromProfile(RecordingProfile*, const QString&,
                               const QString&, const QString&) {}

  private:
    bool Open(void);
    void Close(void);

    void ChannelChanged(const FreeboxChannelInfo &chaninfo);

    /// Callback function to add MPEG2 TS data
    void AddData(unsigned char *data,
                 unsigned int   dataSize,
                 struct timeval presentationTime);

    void ProcessTSPacket(const TSPacket& tspacket);

    bool StartRtsp(void);
    void ResetEventLoop(void);

  private:
    FreeboxRecorderImpl *_impl;
    char                 _abort_rtsp; ///< Used to abort rtsp session
    bool                 _abort_recording; ///< Used to request abort

  private:
    FreeboxRecorder& operator=(const FreeboxRecorder&); //< avoid default impl
    FreeboxRecorder(const FreeboxRecorder&);            //< avoid default impl
    FreeboxRecorder();                                  //< avoid default impl
};

#endif //FREEBOXRECORDER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
