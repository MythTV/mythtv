// -*- Mode: c++ -*-

#ifndef IPTVSIGNALMONITOR_H
#define IPTVSIGNALMONITOR_H

#include "dtvsignalmonitor.h"

class IPTVChannel;
class IPTVSignalMonitor;

class IPTVSignalMonitor : public DTVSignalMonitor
{
    friend class IPTVTableMonitorThread;
  public:
    IPTVSignalMonitor(int db_cardnum, IPTVChannel *_channel,
                      bool _release_stream, uint64_t _flags = 0);
    ~IPTVSignalMonitor() override;

    void Stop(void) override; // SignalMonitor

    // DTVSignalMonitor
    void SetStreamData(MPEGStreamData *data) override; // DTVSignalMonitor

    // MPEG
    void HandlePAT(const ProgramAssociationTable *pat) override; // DTVSignalMonitor

  protected:
    IPTVSignalMonitor(void);
    IPTVSignalMonitor(const IPTVSignalMonitor&);

    void UpdateValues(void) override; // SignalMonitor
    IPTVChannel *GetIPTVChannel(void);

  protected:
    bool m_streamHandlerStarted {false};
    bool m_locked               {false};
};

#endif // IPTVSIGNALMONITOR_H
