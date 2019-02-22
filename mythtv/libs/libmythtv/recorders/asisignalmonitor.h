// -*- Mode: c++ -*-

#ifndef ASISIGNALMONITOR_H
#define ASISIGNALMONITOR_H

#include <QMap>

#include "dtvsignalmonitor.h"

class ASIChannel;
class ASIStreamHandler;

class ASISignalMonitor: public DTVSignalMonitor
{
  public:
    ASISignalMonitor(int db_cardnum, ASIChannel *_channel,
                     bool _release_stream = true, uint64_t _flags = 0);
    virtual ~ASISignalMonitor();

    void Stop(void) override; // SignalMonitor

  protected:
    ASISignalMonitor(void);
    ASISignalMonitor(const ASISignalMonitor&);

    void UpdateValues(void) override; // SignalMonitor
    ASIChannel *GetASIChannel(void);

  protected:
    bool              streamHandlerStarted {false};
    ASIStreamHandler *streamHandler        {nullptr};
};

#endif // ASISIGNALMONITOR_H
