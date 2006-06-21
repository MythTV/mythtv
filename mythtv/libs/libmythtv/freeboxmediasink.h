/**
 *  FreeboxMediaSink
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FREEBOXMEDIASINK_H_
#define _FREEBOXMEDIASINK_H_

#include <MediaSink.hh>

class FreeboxRecorder;

// ============================================================================
// FreeboxMediaSink : Helper class use to receive RTSP data from socket.
// ============================================================================
class FreeboxMediaSink : public MediaSink
{
  public:
    static FreeboxMediaSink *createNew(UsageEnvironment &env,
                                       FreeboxRecorder  &pRecorder,
                                       unsigned          bufferSize);

    /// Callback function called when rtsp data are ready
    void addData(unsigned char *data,
                 unsigned int   dataSize,
                 struct timeval presentationTime);

  protected:
    FreeboxMediaSink(UsageEnvironment &env,
                     FreeboxRecorder  &pRecorder,
                     unsigned int      bufferSize);
    virtual ~FreeboxMediaSink();

    virtual void afterGettingFrame1(unsigned       frameSize,
                                    struct timeval presentationTime);

    static void afterGettingFrame(void          *clientData,
                                  unsigned int   frameSize,
                                  unsigned int   numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned int   durationInMicroseconds);
  private:
    virtual Boolean continuePlaying(void);

  private:
    unsigned char    *fBuffer;
    unsigned int      fBufferSize;
    UsageEnvironment &env;
    FreeboxRecorder  &recorder;

  private:
    // avoid default contructors & operator=
    FreeboxMediaSink();
    FreeboxMediaSink(const FreeboxMediaSink&);
    FreeboxMediaSink& operator=(const FreeboxMediaSink&);
};

#endif //_FREEBOXMEDIASINK_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
