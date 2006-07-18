/** -*- Mode: c++ -*-
 *  FreeboxMediaSink
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FREEBOXMEDIASINK_H_
#define _FREEBOXMEDIASINK_H_

#include <MediaSink.hh>

class RTSPListener
{
  public:
    /// Callback function to add MPEG2 TS data
    virtual void AddData(unsigned char *data,
                         unsigned int   dataSize,
                         struct timeval presentationTime) = 0;
  protected:
    virtual ~RTSPListener() {}
};

// ============================================================================
// FreeboxMediaSink : Helper class use to receive RTSP data from socket.
// ============================================================================
class FreeboxMediaSink : public MediaSink
{
  public:
    static FreeboxMediaSink *CreateNew(UsageEnvironment &env,
                                       RTSPListener     &pRecorder,
                                       unsigned          bufferSize);

  protected:
    FreeboxMediaSink(UsageEnvironment &env,
                     RTSPListener     &pRecorder,
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
    RTSPListener     &recorder;

  private:
    // avoid default contructors & operator=
    FreeboxMediaSink();
    FreeboxMediaSink(const FreeboxMediaSink&);
    FreeboxMediaSink& operator=(const FreeboxMediaSink&);
};

#endif //_FREEBOXMEDIASINK_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
