// -*- Mode: c++ -*-

#ifndef _IPTVSIGNALMONITOR_H_
#define _IPTVSIGNALMONITOR_H_

#include "dtvsignalmonitor.h"

#include <QThread>
#include <QObject>

class IPTVChannel;
class IPTVSignalMonitor;

class IPTVMonitorThread : public QThread
{
    Q_OBJECT
  public:
    IPTVMonitorThread() : m_parent(NULL) {}
    void SetParent(IPTVSignalMonitor *parent) { m_parent = parent; }
    void run(void);
  private:
    IPTVSignalMonitor *m_parent;
};

class IPTVSignalMonitor : public QObject, public DTVSignalMonitor, public TSDataListener
{
    Q_OBJECT

    friend class IPTVMonitorThread;
  public:
    IPTVSignalMonitor(int db_cardnum, IPTVChannel *_channel,
                      uint64_t _flags = 0);
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

    void RunTableMonitor(void);

    IPTVChannel *GetChannel(void);

  protected:
    IPTVMonitorThread  table_monitor_thread;
};

#endif // _IPTVSIGNALMONITOR_H_
