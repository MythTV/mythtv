#ifndef DVBSIGNALMONITOR_H
#define DVBSIGNALMONITOR_H

#include <pthread.h>
#include <qobject.h>

#include "dvbtypes.h"

class DVBSignalMonitor: public QObject
{
    Q_OBJECT

public:
    DVBSignalMonitor(int cardnum, int fd_frontend);
    ~DVBSignalMonitor();
    
    void Start();
    void Stop();

signals:
    void Status(dvb_stats_t &stats);
    void Status(const QString &val);

    void StatusSignalToNoise(int);
    void StatusSignalStrength(int);
    void StatusBitErrorRate(int);
    void StatusUncorrectedBlocks(int);

private:
    void MonitorLoop();
    static void* SpawnMonitorLoop(void*);
    bool FillFrontendStats(dvb_stats_t &stats);

    //int GetIDForCardNumber(int cardnum);
    //void* QualityMonitorThread();
    //void QualityMonitorSample(int cardnum, dvb_stats_t& sample);
    //void ExpireQualityData();

    static const QString TIMEOUT;
    static const QString LOCKED;
    static const QString NOTLOCKED;

    //int     signal_monitor_interval;
    //int     expire_data_days;


    pthread_t   monitor_thread;
    int         cardnum;
    int         fd_frontend;
    bool        running;
    bool        exit;
};

#endif // DVBSIGNALMONITOR_H
