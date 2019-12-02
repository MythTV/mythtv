// -*- Mode: c++ -*-

#ifndef EXTERNALSIGNALMONITOR_H
#define EXTERNALSIGNALMONITOR_H

#include <QMap>

#include "dtvsignalmonitor.h"
#include "ExternalChannel.h"

class ExternalStreamHandler;

using FilterMap = QMap<uint,int>;

class ExternalSignalMonitor: public DTVSignalMonitor
{
  public:
    ExternalSignalMonitor(int db_cardnum, ExternalChannel *_channel,
                          bool _release_stream, uint64_t _flags = 0);
    virtual ~ExternalSignalMonitor();

    void Stop(void) override; // SignalMonitor

  protected:
    ExternalSignalMonitor(void);
    ExternalSignalMonitor(const ExternalSignalMonitor &);

    void UpdateValues(void) override; // SignalMonitor
    ExternalChannel *GetExternalChannel(void)
        { return dynamic_cast<ExternalChannel*>(m_channel); }

    bool HasLock(void);
    int GetSignalStrengthPercent(void);
    int GetLockTimeout(void);

//    void AddHandlerListener(MPEGStreamData *data)
//        { m_stream_handler->AddListener(data); }

  protected:
    ExternalStreamHandler *m_stream_handler         {nullptr};
    bool                   m_stream_handler_started {false};
    int                    m_lock_timeout           {0};
};

#endif // EXTERNALSIGNALMONITOR_H
