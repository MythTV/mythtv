/** -*- Mode: c++ -*-
 *  HLSStreamHandler
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  based on IPTVStreamHandler
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _HLSSTREAMHANDLER_H_
#define _HLSSTREAMHANDLER_H_

#include <vector>
using namespace std;

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
    static HLSStreamHandler* Get(const IPTVTuningData& tuning);
    static void Return(HLSStreamHandler* & ref);

  protected:
    HLSStreamHandler(const IPTVTuningData &tuning);
    virtual ~HLSStreamHandler(void);

    virtual void run(void); // MThread

  protected:
    IPTVTuningData m_tuning;
    HLSReader*     m_hls;
    uint8_t*       m_buffer;

    // for implementing Get & Return
    static QMutex                            s_handlers_lock;
    static QMap<QString, HLSStreamHandler*>  s_handlers;
    static QMap<QString, uint>               s_handlers_refcnt;
};

#endif // _HLSSTREAMHANDLER_H_
