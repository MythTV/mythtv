/** -*- Mode: c++ -*-
 *  CetonSignalMonitor
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2006 Daniel Kristansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONSIGNALMONITOR_H
#define CETONSIGNALMONITOR_H

#include <QMap>

#include "dtvsignalmonitor.h"

class CetonChannel;
class CetonStreamHandler;

class CetonSignalMonitor: public DTVSignalMonitor
{
  public:
    CetonSignalMonitor(int db_cardnum, CetonChannel* _channel,
                      uint64_t _flags = 0);
    virtual ~CetonSignalMonitor();

    void Stop(void);

  protected:
    CetonSignalMonitor(void);
    CetonSignalMonitor(const CetonSignalMonitor&);

    virtual void UpdateValues(void);
    CetonChannel *GetCetonChannel(void);

  protected:
    bool                streamHandlerStarted;
    CetonStreamHandler *streamHandler;
    int                 lock_timeout;
};

#endif // CETONSIGNALMONITOR_H
