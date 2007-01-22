// -*- Mode: c++ -*-

#ifndef _IPTVSIGNALMONITOR_H_
#define _IPTVSIGNALMONITOR_H_

#include "dtvsignalmonitor.h"

class IPTVChannel;

class IPTVSignalMonitor : public DTVSignalMonitor, public TSDataListener
{
    Q_OBJECT

  public:
    IPTVSignalMonitor(int db_cardnum, IPTVChannel *_channel, uint _flags = 0,
                      const char *_name = "IPTVSignalMonitor");
    virtual ~IPTVSignalMonitor();

    void Stop(void);

    // implements TSDataListener
    void AddData(const unsigned char *data, unsigned int dataSize);

  public slots:
    void deleteLater(void);

  protected:
    IPTVSignalMonitor(void);
    IPTVSignalMonitor(const IPTVSignalMonitor&);

    virtual void UpdateValues(void);
    void EmitIPTVSignals(void);

    static void *TableMonitorThread(void *param);
    void RunTableMonitor(void);

    IPTVChannel *GetChannel(void);

  protected:
    bool               dtvMonitorRunning;
    pthread_t          table_monitor_thread;
};

#endif // _IPTVSIGNALMONITOR_H_
