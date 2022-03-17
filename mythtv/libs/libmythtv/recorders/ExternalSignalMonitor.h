// -*- Mode: c++ -*-

#ifndef EXTERNALSIGNALMONITOR_H
#define EXTERNALSIGNALMONITOR_H

#include <cstdint>

#include <QMap>

#include "libmythbase/mythchrono.h"

#include "dtvsignalmonitor.h"
#include "ExternalChannel.h"

class ExternalStreamHandler;

using FilterMap = QMap<uint,int>;

class ExternalSignalMonitor: public DTVSignalMonitor
{
  public:
    ExternalSignalMonitor(int db_cardnum, ExternalChannel *_channel,
                          bool _release_stream, uint64_t _flags = 0);
    ~ExternalSignalMonitor() override;

    void Stop(void) override; // SignalMonitor

  protected:
    ExternalSignalMonitor(void);
    ExternalSignalMonitor(const ExternalSignalMonitor &);

    void UpdateValues(void) override; // SignalMonitor
    ExternalChannel *GetExternalChannel(void)
        { return dynamic_cast<ExternalChannel*>(m_channel); }

    bool HasLock(void);
    int GetSignalStrengthPercent(void);
    std::chrono::seconds GetLockTimeout(void);

//    void AddHandlerListener(MPEGStreamData *data)
//        { m_streamHandler->AddListener(data); }

  protected:
    ExternalStreamHandler *m_streamHandler          {nullptr};
    bool                   m_streamHandlerStarted   {false};
    std::chrono::milliseconds m_lockTimeout         {0ms};
    QString                m_loc;
};

#endif // EXTERNALSIGNALMONITOR_H
