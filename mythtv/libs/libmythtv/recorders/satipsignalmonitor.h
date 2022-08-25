// -*- Mode: c++ -*-

#ifndef SATIPSIGNALMONITOR_H
#define SATIPSIGNALMONITOR_H

#include "dtvsignalmonitor.h"

class SatIPChannel;
class SatIPStreamHandler;

class SatIPSignalMonitor : public DTVSignalMonitor
{
    Q_DECLARE_TR_FUNCTIONS(SatIPSignalMonitor);

  public:
    SatIPSignalMonitor(int db_cardnum, SatIPChannel* channel,
                       bool release_stream, uint64_t flags = 0);
    ~SatIPSignalMonitor() override;

    void Stop(void) override; // SignalMonitor

  protected:
    void UpdateValues(void) override; // SignalMonitor
    SatIPChannel *GetSatIPChannel(void);

  protected:
    bool                m_streamHandlerStarted  {false};
    SatIPStreamHandler *m_streamHandler         {nullptr};

};

#endif // SATIPSIGNALMONITOR_H
