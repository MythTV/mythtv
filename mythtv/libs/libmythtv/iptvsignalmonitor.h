// -*- Mode: c++ -*-

#ifndef _IPTVSIGNALMONITOR_H_
#define _IPTVSIGNALMONITOR_H_

#include "dtvsignalmonitor.h"
#include "mthread.h"

class IPTVChannel;
class IPTVSignalMonitor;

class IPTVTableMonitorThread : public MThread
{
  public:
    IPTVTableMonitorThread(IPTVSignalMonitor *p) :
        MThread("IPTVTableMonitor"), m_parent(p) { start(); }
    virtual ~IPTVTableMonitorThread() { wait(); m_parent = NULL; }
    virtual void run(void);
  private:
    IPTVSignalMonitor *m_parent;
};

class IPTVSignalMonitor : public DTVSignalMonitor, public TSDataListener
{
    friend class IPTVTableMonitorThread;
  public:
    IPTVSignalMonitor(int db_cardnum, IPTVChannel *_channel,
                      uint64_t _flags = 0);
    virtual ~IPTVSignalMonitor();

    void Stop(void);

    // implements TSDataListener
    void AddData(const unsigned char *data, unsigned int dataSize);

  protected:
    IPTVSignalMonitor(void);
    IPTVSignalMonitor(const IPTVSignalMonitor&);

    virtual void UpdateValues(void);

    void RunTableMonitor(void);

    IPTVChannel *GetChannel(void);

  protected:
    volatile bool dtvMonitorRunning;
    IPTVTableMonitorThread *tableMonitorThread;
};

#endif // _IPTVSIGNALMONITOR_H_
