// -*- Mode: c++ -*-

#ifndef HDHRSIGNALMONITOR_H
#define HDHRSIGNALMONITOR_H

#include <QMap>

#include "dtvsignalmonitor.h"

class HDHRChannel;
class HDHRStreamHandler;

class HDHRSignalMonitor: public DTVSignalMonitor
{
  public:
    HDHRSignalMonitor(int db_cardnum, HDHRChannel* _channel,
                      uint64_t _flags = 0);
    virtual ~HDHRSignalMonitor();

    void Stop(void);

  protected:
    HDHRSignalMonitor(void);
    HDHRSignalMonitor(const HDHRSignalMonitor&);

    virtual void UpdateValues(void);
    HDHRChannel *GetHDHRChannel(void);

  protected:
    bool               streamHandlerStarted;
    HDHRStreamHandler *streamHandler;
};

#endif // HDHRSIGNALMONITOR_H
