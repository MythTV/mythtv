/** -*- Mode: c++ -*-
 *  External Recorder Channel Fetcher
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef EXTERNREC_CHANNEL_FETCHER_H
#define EXTERNREC_CHANNEL_FETCHER_H

// Qt headers
#include <QString>
#include <QRunnable>
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QCoreApplication>

// MythTV headers
#include "libmythbase/mthread.h"

class ScanMonitor;

class ExternRecChannelScanner : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(ExternRecChannelScanner);

  public:
    ExternRecChannelScanner(uint cardid, QString inputname, uint sourceid,
                            ScanMonitor *monitor = nullptr);
    ~ExternRecChannelScanner() override;

    void Scan(void);
    void Stop(void);

  private:
    void SetNumChannelsParsed(uint val);
    void SetNumChannelsInserted(uint val);
    void SetMessage(const QString &status);

  protected:
    void run(void) override; // QRunnable

  private:
    ScanMonitor *m_scanMonitor   {nullptr};
    uint         m_cardId;
    QString      m_inputName;
    uint         m_sourceId;
    uint         m_channelTotal  {0};
    uint         m_channelCnt    {1};
    bool         m_threadRunning {false};
    bool         m_stopNow       {false};
    MThread     *m_thread        {nullptr};
    QMutex       m_lock;
};

#endif // EXTERNREC_CHANNEL_FETCHER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
