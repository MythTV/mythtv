// -*- Mode: c++ -*-

#ifndef FIREWIRESIGNALMONITOR_H
#define FIREWIRESIGNALMONITOR_H

// C++ headers
#include <vector>

// Qt headers
#include <QMutex>
#include <QMap>

// MythTV headers
#include "libmythbase/mthread.h"
#include "dtvsignalmonitor.h"
#include "firewiredevice.h"

class FirewireChannel;

class FirewireSignalMonitor;

class FirewireTableMonitorThread : public MThread
{
  public:
    explicit FirewireTableMonitorThread(FirewireSignalMonitor *p) :
        MThread("FirewireTableMonitor"), m_parent(p) { start(); }
    ~FirewireTableMonitorThread() override { wait(); m_parent = nullptr; }
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

    void HandlePAT(const ProgramAssociationTable *pat) override; // DTVSignalMonitor
    void HandlePMT(uint pnum, const ProgramMapTable *pmt) override; // DTVSignalMonitor

    void Stop(void) override; // SignalMonitor

  protected:
    FirewireSignalMonitor(void);
    FirewireSignalMonitor(const FirewireSignalMonitor&);
    ~FirewireSignalMonitor() override;

    void UpdateValues(void) override; // SignalMonitor

    void RunTableMonitor(void);

    bool SupportsTSMonitoring(void);

    void AddData(const unsigned char *data, uint len) override; // TSDataListener

  public:
    static constexpr std::chrono::milliseconds kPowerTimeout  { 3s };
    static constexpr std::chrono::milliseconds kBufferTimeout { 5s };

  protected:
    volatile bool      m_dtvMonitorRunning           {false};
    FirewireTableMonitorThread *m_tableMonitorThread {nullptr};
    bool               m_stbNeedsRetune              {true};
    bool               m_stbNeedsToWaitForPat        {false};
    bool               m_stbNeedsToWaitForPower      {false};
    MythTimer          m_stbWaitForPatTimer;
    MythTimer          m_stbWaitForPowerTimer;

    std::vector<unsigned char> m_buffer;

    static QHash<void*,uint> s_patKeys;
    static QMutex            s_patKeysLock;
};

#endif // FIREWIRESIGNALMONITOR_H
