// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#ifndef _CHANNEL_CHANGE_MONITOR_H_
#define _CHANNEL_CHANGE_MONITOR_H_

// MythTV headers
#include "signalmonitor.h"

class ChannelBase;

class ChannelChangeMonitor : public SignalMonitor
{
  public:
    ChannelChangeMonitor(int db_cardnum, ChannelBase *_channel,
                         uint64_t _flags = kSigMon_WaitForSig);

    virtual void UpdateValues(void);
};

#endif // _CHANNEL_CHANGE_MONITOR_H_
