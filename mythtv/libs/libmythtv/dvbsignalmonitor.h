// -*- Mode: c++ -*-

#ifndef DVBSIGNALMONITOR_H
#define DVBSIGNALMONITOR_H

#include "dtvsignalmonitor.h"
#include "qstringlist.h"

class DVBChannel;

typedef QMap<uint,int> FilterMap;
typedef QMap<uint,uint> RemainderMap;
typedef QMap<uint,unsigned char*> BufferMap;

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
  signals:
    void StatusSignalToNoise(const SignalMonitorValue&);
    void StatusBitErrorRate(const SignalMonitorValue&);
    void StatusUncorrectedBlocks(const SignalMonitorValue&);

  private:
    DVBSignalMonitor(void);
    DVBSignalMonitor(const DVBSignalMonitor&);

    virtual void UpdateValues(void);
    void EmitDVBSignals(void);

    static void *TableMonitorThread(void *param);
    void RunTableMonitor(void);
    bool AddPIDFilter(uint pid);
    bool RemovePIDFilter(uint pid);

    int GetDVBCardNum(void) const;

  private:
    SignalMonitorValue signalToNoise;
    SignalMonitorValue bitErrorRate;
    SignalMonitorValue uncorrectedBlocks;

    bool               dtvMonitorRunning;
    pthread_t          table_monitor_thread;

    FilterMap          filters; ///< PID filters for table monitoring
    BufferMap          buffers; ///< Stream buffers for table monitoring
    RemainderMap       remainders;///< buffers remainders for table monitoring
};

#endif // DVBSIGNALMONITOR_H
