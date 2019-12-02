/** -*- Mode: c++ -*-
 *  External Recorder Channel Fetcher
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _EXTERNREC_CHANNEL_FETCHER_H_
#define _EXTERNREC_CHANNEL_FETCHER_H_

// Qt headers
#include <QString>
#include <QRunnable>
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QCoreApplication>

// MythTV headers
#include "mthread.h"

class ScanMonitor;

class ExternRecChannelScanner : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(ExternRecChannelScanner);

  public:
    ExternRecChannelScanner(uint cardid, QString inputname, uint sourceid,
                            ScanMonitor *monitor = nullptr);
    ~ExternRecChannelScanner();

    void Scan(void);
    void Stop(void);

  private:
    void SetNumChannelsParsed(uint val);
    void SetNumChannelsInserted(uint val);
    void SetMessage(const QString &status);

  protected:
    void run(void) override; // QRunnable

  private:
    ScanMonitor *m_scan_monitor   {nullptr};
    uint         m_cardid;
    QString      m_inputname;
    uint         m_sourceid;
    uint         m_channel_total  {0};
    uint         m_channel_cnt    {1};
    bool         m_thread_running {false};
    bool         m_stop_now       {false};
    MThread     *m_thread         {nullptr};
    QMutex       m_lock;
};

#endif // _EXTERNREC_CHANNEL_FETCHER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
