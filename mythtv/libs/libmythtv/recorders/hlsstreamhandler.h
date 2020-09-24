/** -*- Mode: c++ -*-
 *  HLSStreamHandler
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  based on IPTVStreamHandler
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HLSSTREAMHANDLER_H
#define HLSSTREAMHANDLER_H

#include <vector>

#include <QString>
#include <QMutex>
#include <QMap>

#include "channelutil.h"
#include "iptvstreamhandler.h"

class MPEGStreamData;
class HLSReader;

class HLSStreamHandler : public IPTVStreamHandler
{
  public:
    static HLSStreamHandler* Get(const IPTVTuningData& tuning, int inputid);
    static void Return(HLSStreamHandler* & ref, int inputid);

    // Deleted functions should be public.
    HLSStreamHandler(const HLSStreamHandler &) = delete;            // not copyable
    HLSStreamHandler &operator=(const HLSStreamHandler &) = delete; // not copyable

  protected:
    explicit HLSStreamHandler(const IPTVTuningData &tuning, int inputid);
    ~HLSStreamHandler(void) override;

    void run(void) override; // MThread

  protected:
    HLSReader*     m_hls        {nullptr};
    uint8_t*       m_readbuffer {nullptr};
    bool           m_throttle   {true};

    // for implementing Get & Return
    static QMutex                            s_hlshandlers_lock;
    static QMap<QString, HLSStreamHandler*>  s_hlshandlers;
    static QMap<QString, uint>               s_hlshandlers_refcnt;
};

#endif // HLSSTREAMHANDLER_H
