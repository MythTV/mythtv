#ifndef DVBSIGNALMONITOR_H
#define DVBSIGNALMONITOR_H

#include "dtvsignalmonitor.h"
#include "qstringlist.h"
#include "dvbtypes.h"

class DVBChannel;

class DVBSignalMonitor: public DTVSignalMonitor
{
    Q_OBJECT
public:
    DVBSignalMonitor(int capturecardnum, int cardnum, DVBChannel* _channel);
    virtual ~DVBSignalMonitor();

    virtual QStringList GetStatusList(bool kick);

signals:
    void StatusSignalToNoise(int);
    void StatusBitErrorRate(int);
    void StatusUncorrectedBlocks(int);

private:
    virtual void UpdateValues();

    static bool FillFrontendStats(int fd_frontend, dvb_stats_t &stats);

    int cardnum;
    SignalMonitorValue signalToNoise;
    SignalMonitorValue bitErrorRate;
    SignalMonitorValue uncorrectedBlocks;
    DVBChannel* channel;
};

#endif // DVBSIGNALMONITOR_H
