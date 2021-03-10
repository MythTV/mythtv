// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#ifndef ANALOG_SIGNAL_MONITOR_H
#define ANALOG_SIGNAL_MONITOR_H

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

    bool      m_usingV4l2   {false};
    QString   m_card;
    QString   m_driver;
    uint32_t  m_version     {0};
    uint      m_width       {0};
    std::chrono::milliseconds m_stableTime  {2s};
    uint      m_lockCnt     {0};
    MythTimer m_timer;
    int       m_logIdx      {40};
};

#endif // ANALOG_SIGNAL_MONITOR_H
