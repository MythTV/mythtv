/** -*- Mode: c++ -*-
 *  IPTVFeederUDP
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_FEEDER_UDP_H_
#define _IPTV_FEEDER_UDP_H_

// MythTV headers
#include "iptvfeederlive.h"

class BasicUDPSource;
class IPTVMediaSink;


class IPTVFeederUDP : public IPTVFeederLive
{
  public:
    IPTVFeederUDP();
    virtual ~IPTVFeederUDP();

    bool CanHandle(const QString &url) const { return IsUDP(url); }
    bool IsOpen(void) const { return _source; }

    bool Open(const QString &url);
    void Close(void);

    void AddListener(TSDataListener*);
    void RemoveListener(TSDataListener*);

    static bool IsUDP(const QString &url);

  private:
    BasicUDPSource *_source;
    IPTVMediaSink  *_sink;
};

#endif // _IPTV_FEEDER_UDP_H_
