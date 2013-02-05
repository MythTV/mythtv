// -*- Mode: c++ -*-

#ifndef _IPTVSIGNALMONITOR_H_
#define _IPTVSIGNALMONITOR_H_

#include "dtvsignalmonitor.h"

class IPTVChannel;
class IPTVSignalMonitor;

class IPTVSignalMonitor : public DTVSignalMonitor
{
    friend class IPTVTableMonitorThread;
  public:
    IPTVSignalMonitor(int db_cardnum, IPTVChannel *_channel,
                      uint64_t _flags = 0);
    virtual ~IPTVSignalMonitor();

    void Stop(void);

    // DTVSignalMonitor
    virtual void SetStreamData(MPEGStreamData *data);

    // MPEG
    virtual void HandlePAT(const ProgramAssociationTable*);

  protected:
    IPTVSignalMonitor(void);
    IPTVSignalMonitor(const IPTVSignalMonitor&);

    virtual void UpdateValues(void);
    IPTVChannel *GetChannel(void);

  protected:
    bool m_streamHandlerStarted;
    int  m_lock_timeout;
};

#endif // _IPTVSIGNALMONITOR_H_
