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

class FirewireChannel;

class FirewireSignalMonitor;

class FirewireTableMonitorThread : public MThread
{
  public:
    explicit FirewireTableMonitorThread(FirewireSignalMonitor *p) :
        MThread("FirewireTableMonitor"), m_parent(p) { start(); }
    virtual ~FirewireTableMonitorThread() { wait(); m_parent = nullptr; }
    void run(void) override; // MThread
  private:
    FirewireSignalMonitor *m_parent;
};

class FirewireSignalMonitor : public DTVSignalMonitor, public TSDataListener
{
    friend class FirewireTableMonitorThread;
  public:
    FirewireSignalMonitor(int db_cardnum, FirewireChannel *_channel,
                          bool _release_stream,
                          uint64_t _flags = kFWSigMon_WaitForPower);

    void HandlePAT(const ProgramAssociationTable*) override; // DTVSignalMonitor
    void HandlePMT(uint, const ProgramMapTable*) override; // DTVSignalMonitor

    void Stop(void) override; // SignalMonitor

  protected:
    FirewireSignalMonitor(void);
    FirewireSignalMonitor(const FirewireSignalMonitor&);
    virtual ~FirewireSignalMonitor();

    void UpdateValues(void) override; // SignalMonitor

    void RunTableMonitor(void);

    bool SupportsTSMonitoring(void);

    void AddData(const unsigned char *data, uint len) override; // TSDataListener

  public:
    static const uint kPowerTimeout;
    static const uint kBufferTimeout;

  protected:
    volatile bool      m_dtvMonitorRunning           {false};
    FirewireTableMonitorThread *m_tableMonitorThread {nullptr};
    bool               m_stb_needs_retune            {true};
    bool               m_stb_needs_to_wait_for_pat   {false};
    bool               m_stb_needs_to_wait_for_power {false};
    MythTimer          m_stb_wait_for_pat_timer;
    MythTimer          m_stb_wait_for_power_timer;

    vector<unsigned char> m_buffer;

    static QMap<void*,uint> s_pat_keys;
    static QMutex           s_pat_keys_lock;
};

#endif // _FIREWIRESIGNALMONITOR_H_
