/** -*- Mode: c++ -*-
 *  IPTVMediaSink
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_MEDIASINK_H_
#define _IPTV_MEDIASINK_H_

#include <vector>
using namespace std;

#include <qmutex.h>

#include <MediaSink.hh>

class TSDataListener;

// ============================================================================
// IPTVMediaSink : Helper class use to receive RTSP data from socket.
// ============================================================================
class IPTVMediaSink : public MediaSink
{
  public:
    static IPTVMediaSink *CreateNew(UsageEnvironment &env,
                                       unsigned          bufferSize);

    void AddListener(TSDataListener*);
    void RemoveListener(TSDataListener*);

  protected:
    IPTVMediaSink(UsageEnvironment &env,
                     unsigned int      bufferSize);
    virtual ~IPTVMediaSink();

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
    unsigned char           *_buf;
    unsigned int             _buf_size;
    UsageEnvironment        &_env;
    vector<TSDataListener*>  _listeners;
    mutable QMutex           _lock;

  private:
    // avoid default contructors & operator=
    IPTVMediaSink();
    IPTVMediaSink(const IPTVMediaSink&);
    IPTVMediaSink &operator=(const IPTVMediaSink&);
};

#endif // _IPTV_MEDIASINK_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
