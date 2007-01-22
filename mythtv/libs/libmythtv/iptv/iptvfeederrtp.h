/** -*- Mode: c++ -*-
 *  IPTVFeederRTP
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_FEEDER_RTP_H_
#define _IPTV_FEEDER_RTP_H_

// MythTV headers
#include "iptvfeederlive.h"

class SimpleRTPSource;
class IPTVMediaSink;


class IPTVFeederRTP : public IPTVFeederLive
{
  public:
    IPTVFeederRTP();
    virtual ~IPTVFeederRTP();

    bool CanHandle(const QString &url) const { return IsRTP(url); }
    bool IsOpen(void) const { return _source; }

    bool Open(const QString &url);
    void Close(void);

    void AddListener(TSDataListener*);
    void RemoveListener(TSDataListener*);

    static bool IsRTP(const QString &url);

  private:
    IPTVFeederRTP &operator=(const IPTVFeederRTP&);
    IPTVFeederRTP(const IPTVFeederRTP&);

  private:
    SimpleRTPSource *_source;
    IPTVMediaSink   *_sink;
};

#endif // _IPTV_FEEDER_RTP_H_
