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
                       bool _release_stream, uint64_t _flags = 0);
    ~CetonSignalMonitor() override;

    void Stop(void) override; // SignalMonitor

  protected:
    CetonSignalMonitor(void);
    CetonSignalMonitor(const CetonSignalMonitor&);

    void UpdateValues(void) override; // SignalMonitor
    CetonChannel *GetCetonChannel(void);

  protected:
    bool                m_streamHandlerStarted {false};
    CetonStreamHandler *m_streamHandler        {nullptr};
};

#endif // CETONSIGNALMONITOR_H
