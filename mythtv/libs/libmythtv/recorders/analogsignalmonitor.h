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
    bool VerifyHDPVRaudio(int videofd);
    bool handleHDPVR(int videofd);

    bool     m_usingv4l2;
    QString  m_card;
    QString  m_driver;
    uint32_t m_version;
    uint     m_width;
    int      m_stable_time;
    uint     m_lock_cnt;
    MythTimer m_timer;
};

#endif // _ANALOG_SIGNAL_MONITOR_H_
