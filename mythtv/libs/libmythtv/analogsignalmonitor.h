// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#ifndef _ANALOG_SIGNAL_MONITOR_H_
#define _ANALOG_SIGNAL_MONITOR_H_

// MythTV headers
#include "signalmonitor.h"

class V4LChannel;

class AnalogSignalMonitor : public SignalMonitor
{
  public:
    AnalogSignalMonitor(
        int db_cardnum, V4LChannel *_channel,
        uint64_t _flags = kSigMon_WaitForSig);

    virtual void UpdateValues(void);

  private:
    bool usingv4l2;
};

#endif // _ANALOG_SIGNAL_MONITOR_H_
