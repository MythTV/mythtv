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
    AnalogSignalMonitor(int db_cardnum, V4LChannel *_channel,
                        bool _release_stream,
                        uint64_t _flags = kSigMon_WaitForSig);

    void UpdateValues(void) override; // SignalMonitor

  private:
    bool VerifyHDPVRaudio(int videofd);
    bool handleHDPVR(int videofd);

    bool      m_usingv4l2   {false};
    QString   m_card;
    QString   m_driver;
    uint32_t  m_version     {0};
    uint      m_width       {0};
    int       m_stable_time {2000};
    uint      m_lock_cnt    {0};
    MythTimer m_timer;
    int       m_log_idx     {40};
};

#endif // _ANALOG_SIGNAL_MONITOR_H_
