// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#ifndef _ANALOG_SIGNAL_MONITOR_H_
#define _ANALOG_SIGNAL_MONITOR_H_

// MythTV headers
#include "signalmonitor.h"

class Channel;

class AnalogSignalMonitor : public SignalMonitor
{
  public:
    AnalogSignalMonitor(
        int db_cardnum, Channel *_channel,
        uint _flags = kDTVSigMon_WaitForSig,
        const char *_name = "AnalogSignalMonitor");

    virtual void UpdateValues(void);

  private:
    bool usingv4l2;
};

#endif // _ANALOG_SIGNAL_MONITOR_H_
