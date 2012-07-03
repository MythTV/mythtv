//
//  iptvfeederhls.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 24/05/12.
//  Copyright (c) 2012 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_iptvfeederhls_h
#define MythTV_iptvfeederhls_h

// MythTV headers
#include "mythcorecontext.h"
#include "iptvfeederlive.h"

class TSDataListener;
class HLSRingBuffer;

class IPTVFeederHLS : public IPTVFeederLive
{
public:
    IPTVFeederHLS();
    virtual ~IPTVFeederHLS();

    bool CanHandle(const QString &url) const { return IsHLS(url); }
    bool IsOpen(void) const;

    bool Open(const QString &url);
    void Stop(void);
    void Close(void);
    void Run(void);

    static bool IsHLS(const QString &url);

private:
    uint8_t        *m_buffer;
    HLSRingBuffer  *m_hls;
    mutable QMutex  m_lock;
    QWaitCondition  m_waitcond;
    bool            m_interrupted;
    bool            m_running;
};

#endif
