/** -*- Mode: c++ -*-
 *  FreeboxFeederRtsp
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FREEBOXFEEDERRTSP_H
#define FREEBOXFEEDERRTSP_H

// MythTV headers
#include "freeboxfeederlive.h"

class RTSPClient;
class MediaSession;


class FreeboxFeederRtsp : public FreeboxFeederLive
{
  public:
    FreeboxFeederRtsp();
    virtual ~FreeboxFeederRtsp();

    bool CanHandle(const QString &url) const;
    bool Open(const QString &url);
    bool IsOpen(void) const { return _session; }
    void Close(void);

    void AddListener(IPTVListener*);
    void RemoveListener(IPTVListener*);

  private:
    RTSPClient         *_rtsp_client;
    MediaSession       *_session;

  private:
    /// avoid default copy operator
    FreeboxFeederRtsp &operator=(const FreeboxFeederRtsp&);
    /// avoid default copy constructor
    FreeboxFeederRtsp(const FreeboxFeederRtsp&);
};

#endif // FREEBOXFEEDERRTSP_H
