// -*- Mode: c++ -*-

#ifndef SCRIPT_SIGNAL_MONITOR_H
#define SCRIPT_SIGNAL_MONITOR_H

// MythTV headers
#include "signalmonitor.h"

class ScriptSignalMonitor : public SignalMonitor
{
  public:
    ScriptSignalMonitor(int db_cardnum, ChannelBase *_channel,
                        bool _release_stream,
                        uint64_t _flags = 0) :
        SignalMonitor(db_cardnum, _channel, _release_stream, _flags)
    {
        m_signalLock.SetValue(static_cast<int>(true));
        m_signalStrength.SetValue(100);
    }

    void UpdateValues(void) override // SignalMonitor
    {
        SignalMonitor::UpdateValues();

        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();
    }
};

#endif // SCRIPT_SIGNAL_MONITOR_H
