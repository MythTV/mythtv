// -*- Mode: c++ -*-

#ifndef _FIREWIRESIGNALMONITOR_H_
#define _FIREWIRESIGNALMONITOR_H_

#include <qmap.h>
#include <qmutex.h>
#include <qdatetime.h>

#include "dtvsignalmonitor.h"
#include "firewiredevice.h"
#include "util.h"

class FirewireChannel;

class FirewireSignalMonitor : public DTVSignalMonitor, public TSDataListener
{
    Q_OBJECT

  public:
    FirewireSignalMonitor(int db_cardnum, FirewireChannel *_channel,
                          uint _flags = kFWSigMon_WaitForPower,
                          const char *_name = "FirewireSignalMonitor");

    virtual void HandlePAT(const ProgramAssociationTable*);
    virtual void HandlePMT(uint, const ProgramMapTable*);

    void Stop(void);

  public slots:
    void deleteLater(void);

  protected:
    FirewireSignalMonitor(void);
    FirewireSignalMonitor(const FirewireSignalMonitor&);
    virtual ~FirewireSignalMonitor();

    virtual void UpdateValues(void);
    void EmitFirewireSignals(void);

    static void *TableMonitorThread(void *param);
    void RunTableMonitor(void);

    bool SupportsTSMonitoring(void);

    void AddData(const unsigned char *data, uint dataSize);

  public:
    static const uint kPowerTimeout;
    static const uint kBufferTimeout;

  protected:
    bool               dtvMonitorRunning;
    pthread_t          table_monitor_thread;
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
