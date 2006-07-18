// -*- Mode: c++ -*-

#ifndef DVBSIGNALMONITOR_H
#define DVBSIGNALMONITOR_H

#include "dtvsignalmonitor.h"
#include "qstringlist.h"

class DVBChannel;

typedef QMap<uint,int> FilterMap;

#define RETUNE_TIMEOUT 5000

class DVBSignalMonitor: public DTVSignalMonitor
{
    Q_OBJECT
  public:
    DVBSignalMonitor(int db_cardnum, DVBChannel* _channel, uint _flags = 0,
                     const char *_name = "DVBSignalMonitor");
    virtual ~DVBSignalMonitor();

    virtual QStringList GetStatusList(bool kick);
    void Stop(void);

    bool UpdateFiltersFromStreamData(void);

    virtual void SetRotorTarget(float target);

  public slots:
    void deleteLater(void);

  signals:
    void StatusSignalToNoise(const SignalMonitorValue&);
    void StatusBitErrorRate(const SignalMonitorValue&);
    void StatusUncorrectedBlocks(const SignalMonitorValue&);
    void StatusRotorPosition(const SignalMonitorValue&);

  protected:
    DVBSignalMonitor(void);
    DVBSignalMonitor(const DVBSignalMonitor&);

    virtual void UpdateValues(void);
    void EmitDVBSignals(void);

    void RetuneMonitor(void);
    static void *TableMonitorThread(void *param);
    void RunTableMonitor(void);
    void RunTableMonitorTS(void);
    void RunTableMonitorSR(void);
    bool AddPIDFilter(uint pid);
    bool RemovePIDFilter(uint pid);

    int GetDVBCardNum(void) const;

    bool SupportsTSMonitoring(void);
  protected:
    SignalMonitorValue signalToNoise;
    SignalMonitorValue bitErrorRate;
    SignalMonitorValue uncorrectedBlocks;
    SignalMonitorValue rotorPosition;

    bool               useSectionReader;
    bool               dtvMonitorRunning;
    pthread_t          table_monitor_thread;

    FilterMap          filters; ///< PID filters for table monitoring
};

#endif // DVBSIGNALMONITOR_H
