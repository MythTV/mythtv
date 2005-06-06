#ifndef PCHDTVSIGNALMONITOR_H
#define PCHDTVSIGNALMONITOR_H

#include "dtvsignalmonitor.h"

class Channel;

class pcHDTVSignalMonitor: public DTVSignalMonitor
{
public:
    pcHDTVSignalMonitor(int _capturecardnum, uint _inputnum, Channel *_channel,
                        uint _flags = kDTVSigMon_WaitForSig);
    ~pcHDTVSignalMonitor();

    void Stop();
private:
    pcHDTVSignalMonitor();
    pcHDTVSignalMonitor(const pcHDTVSignalMonitor&);

    virtual void UpdateValues();

    static int GetSignal(int fd, uint input, bool usingv4l2);

    static void *TableMonitorThread(void *param);
    void RunTableMonitor();

    uint      input;
    bool      usingv4l2;
    bool      dtvMonitorRunning;
    pthread_t table_monitor_thread;
    Channel  *channel;
};

#endif // PCHDTVSIGNALMONITOR_H
