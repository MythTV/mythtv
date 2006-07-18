// -*- Mode: c++ -*-

#ifndef _FREEBOXSIGNALMONITOR_H_
#define _FREEBOXSIGNALMONITOR_H_

#include "dtvsignalmonitor.h"
#include "freeboxmediasink.h"

class RTSPComms;
class FreeboxChannel;

class FreeboxSignalMonitor : public DTVSignalMonitor, public RTSPListener
{
    Q_OBJECT

  public:
    FreeboxSignalMonitor(int db_cardnum, FreeboxChannel *_channel,
                         uint _flags = 0,
                         const char *_name = "FreeboxSignalMonitor");
    virtual ~FreeboxSignalMonitor();

    void Stop(void);

    // implements RTSPListener
    void AddData(unsigned char *data,
                 unsigned       dataSize,
                 struct timeval);

  public slots:
    void deleteLater(void);

  protected:
    FreeboxSignalMonitor(void);
    FreeboxSignalMonitor(const FreeboxSignalMonitor&);

    virtual void UpdateValues(void);
    void EmitFreeboxSignals(void);

    static void *TableMonitorThread(void *param);
    void RunTableMonitor(void);

    FreeboxChannel *GetChannel(void);

  protected:
    bool               dtvMonitorRunning;
    pthread_t          table_monitor_thread;
};

#endif // _FREEBOXSIGNALMONITOR_H_
