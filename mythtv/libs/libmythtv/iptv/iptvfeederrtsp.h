/** -*- Mode: c++ -*-
 *  IPTVFeederRtsp
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_FEEDER_RTSP_H_
#define _IPTV_FEEDER_RTSP_H_

// MythTV headers
#include "iptvfeederlive.h"

class RTSPClient;
class MediaSession;

class IPTVFeederRTSP : public IPTVFeederLive
{
  public:
    IPTVFeederRTSP();
    virtual ~IPTVFeederRTSP();

    bool CanHandle(const QString &url) const { return IsRTSP(url); }
    bool IsOpen(void) const { return _session; }

    bool Open(const QString &url);
    void Close(void);

    void AddListener(TSDataListener*);
    void RemoveListener(TSDataListener*);

    static bool IsRTSP(const QString &url);

  private:
    IPTVFeederRTSP &operator=(const IPTVFeederRTSP&);
    IPTVFeederRTSP(const IPTVFeederRTSP&);

  private:
    RTSPClient   *_rtsp_client;
    MediaSession *_session;
};

#endif // _IPTV_FEEDER_RTSP_H_
