/** -*- Mode: c++ -*-
 *  FreeboxFeederUdp
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FREEBOX_FEEDER_UDP_H_
#define _FREEBOX_FEEDER_UDP_H_

// MythTV headers
#include "freeboxfeederlive.h"

class BasicUDPSource;
class FreeboxMediaSink;


class FreeboxFeederUdp : public FreeboxFeederLive
{
  public:
    FreeboxFeederUdp();
    virtual ~FreeboxFeederUdp();

    bool CanHandle(const QString &url) const;
    bool Open(const QString &url);
    bool IsOpen(void) const { return _source; }
    void Close(void);

    void AddListener(IPTVListener*);
    void RemoveListener(IPTVListener*);

  private:
    BasicUDPSource     *_source;
    FreeboxMediaSink   *_sink;
};

#endif // _FREEBOX_FEEDER_UDP_H_
