// -*- Mode: c++ -*-

#ifndef _FIREWIRESIGNALMONITOR_H_
#define _FIREWIRESIGNALMONITOR_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QMutex>
#include <QMap>

// MythTV headers
#include "dtvsignalmonitor.h"
#include "firewiredevice.h"
#include "mthread.h"
#include "mythdate.h"

class FirewireChannel;

class FirewireSignalMonitor;

class FirewireTableMonitorThread : public MThread
{
  public:
    FirewireTableMonitorThread(FirewireSignalMonitor *p) :
        MThread("FirewireTableMonitor"), m_parent(p) { start(); }
    virtual ~FirewireTableMonitorThread() { wait(); m_parent = NULL; }
    virtual void run(void);
  private:
    FirewireSignalMonitor *m_parent;
};

class FirewireSignalMonitor : public DTVSignalMonitor, public TSDataListener
{
    friend class FirewireTableMonitorThread;
  public:
    FirewireSignalMonitor(int db_cardnum, FirewireChannel *_channel,
                          uint64_t _flags = kFWSigMon_WaitForPower);

    virtual void HandlePAT(const ProgramAssociationTable*);
    virtual void HandlePMT(uint, const ProgramMapTable*);

    void Stop(void);

  protected:
    FirewireSignalMonitor(void);
    FirewireSignalMonitor(const FirewireSignalMonitor&);
    virtual ~FirewireSignalMonitor();

    virtual void UpdateValues(void);

    void RunTableMonitor(void);

    bool SupportsTSMonitoring(void);

    void AddData(const unsigned char *data, uint dataSize);

  public:
    static const uint kPowerTimeout;
    static const uint kBufferTimeout;

  protected:
    volatile bool      dtvMonitorRunning;
    FirewireTableMonitorThread *tableMonitorThread;
    bool               stb_needs_retune;
    bool               stb_needs_to_wait_for_pat;
    bool               stb_needs_to_wait_for_power;
    MythTimer          stb_wait_for_pat_timer;
    MythTimer          stb_wait_for_power_timer;

    vector<unsigned char> buffer;

    static QMap<void*,uint> pat_keys;
    static QMutex           pat_keys_lock;
};

#endif // _FIREWIRESIGNALMONITOR_H_
