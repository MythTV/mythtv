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
                     uint64_t _flags = 0);
    virtual ~ASISignalMonitor();

    void Stop(void);

  protected:
    ASISignalMonitor(void);
    ASISignalMonitor(const ASISignalMonitor&);

    virtual void UpdateValues(void);
    ASIChannel *GetASIChannel(void);

  protected:
    bool              streamHandlerStarted;
    ASIStreamHandler *streamHandler;
};

#endif // ASISIGNALMONITOR_H
