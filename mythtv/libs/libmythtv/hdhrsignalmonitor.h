// -*- Mode: c++ -*-

#ifndef HDHRSIGNALMONITOR_H
#define HDHRSIGNALMONITOR_H

#include "dtvsignalmonitor.h"
#include "qstringlist.h"

class HDHRChannel;

typedef QMap<uint,int> FilterMap;

class HDHRSignalMonitor: public DTVSignalMonitor
{
  public:
    HDHRSignalMonitor(int db_cardnum, HDHRChannel* _channel,
                      uint64_t _flags = 0);
    virtual ~HDHRSignalMonitor();

    void Stop(void);

    bool UpdateFiltersFromStreamData(void);

  protected:
    HDHRSignalMonitor(void);
    HDHRSignalMonitor(const HDHRSignalMonitor&);

    virtual void UpdateValues(void);

    static void *TableMonitorThread(void *param);
    void RunTableMonitor(void);

    bool SupportsTSMonitoring(void);

  protected:
    bool               dtvMonitorRunning;
    pthread_t          table_monitor_thread;

    FilterMap          filters; ///< PID filters for table monitoring
};

#endif // HDHRSIGNALMONITOR_H
