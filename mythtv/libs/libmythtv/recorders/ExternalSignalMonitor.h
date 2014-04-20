// -*- Mode: c++ -*-

#ifndef EXTERNALSIGNALMONITOR_H
#define EXTERNALSIGNALMONITOR_H

#include <QMap>

#include "dtvsignalmonitor.h"
#include "ExternalChannel.h"

class ExternalStreamHandler;

typedef QMap<uint,int> FilterMap;

class ExternalSignalMonitor: public DTVSignalMonitor
{
  public:
    ExternalSignalMonitor(int db_cardnum, ExternalChannel *_channel,
                          uint64_t _flags = 0);
    virtual ~ExternalSignalMonitor();

    void Stop(void);

  protected:
    ExternalSignalMonitor(void);
    ExternalSignalMonitor(const ExternalSignalMonitor &);

    virtual void UpdateValues(void);
    ExternalChannel *GetExternalChannel(void)
        { return dynamic_cast<ExternalChannel*>(channel); }

    bool HasLock(void);
    int GetSignalStrengthPercent(void);
    int GetLockTimeout(void);

//    void AddHandlerListener(MPEGStreamData *data)
//        { m_stream_handler->AddListener(data); }

  protected:
    ExternalStreamHandler *m_stream_handler;
    bool              m_stream_handler_started;
    int               m_lock_timeout;
};

#endif // EXTERNALSIGNALMONITOR_H
