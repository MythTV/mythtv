// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#ifndef PCHDTVSIGNALMONITOR_H
#define PCHDTVSIGNALMONITOR_H

#include "dtvsignalmonitor.h"

class Channel;

class pcHDTVSignalMonitor: public DTVSignalMonitor
{
public:
    pcHDTVSignalMonitor(int db_cardnum, Channel *_channel,
                        uint _flags = kDTVSigMon_WaitForSig,
                        const char *_name = "pcHDTVSignalMonitor");
    ~pcHDTVSignalMonitor();

    void Stop();
private:
    pcHDTVSignalMonitor();
    pcHDTVSignalMonitor(const pcHDTVSignalMonitor&);

    virtual void UpdateValues();

    static int GetSignal(int fd, uint input, bool usingv4l2);

    static void *TableMonitorThread(void *param);
    void RunTableMonitor();

    bool      usingv4l2;
    bool      dtvMonitorRunning;
    pthread_t table_monitor_thread;
};

#endif // PCHDTVSIGNALMONITOR_H
