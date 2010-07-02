#ifndef MYTHDOWNLOADMANAGER_H
#define MYTHDOWNLOADMANAGER_H

#include <QDateTime>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QWaitCondition>

#include "mythexp.h"

class MythDownloadInfo
{
  public:
    MythDownloadInfo() :
        m_reply(NULL),
        m_data(NULL),
        m_caller(NULL),
        m_lastStat(QDateTime::currentDateTime()),
        m_syncMode(false),
        m_done(false),
        m_errorCode(QNetworkReply::NoError)
    {
    }

   ~MythDownloadInfo()
    {
        if (m_reply)
            m_reply->deleteLater();
    }

    QString        m_url;
    QUrl           m_redirectedTo;
    QNetworkReply *m_reply;
    QString        m_outFile;
    QByteArray    *m_data;
    QObject       *m_caller;
    QDateTime      m_lastStat;
    bool           m_syncMode;
    bool           m_done;

    QNetworkReply::NetworkError m_errorCode;
};

class MPUBLIC MythDownloadManager : public QThread
{
    Q_OBJECT

  public:
    MythDownloadManager();
   ~MythDownloadManager();

    void run(void);

    void queueDownload(const QString &url, const QString &dest,
                     QObject *caller);
    bool download(const QString &url, const QString &dest);
    bool download(const QString &url, QByteArray *data);

    QThread *getQueueThread(void) { return m_queueThread; }

    QDateTime GetLastModified(const QString &url);

  private slots:
    // QNetworkAccessManager signals
    void downloadFinished(QNetworkReply* reply);

    // QNetworkReply signals
    void downloadError(QNetworkReply::NetworkError errorCode);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

  private:
    bool download(const QString &url, const QString &dest, QByteArray *data);
    bool downloadNow(MythDownloadInfo *dlInfo, bool deleteInfo = true);
    QUrl redirectUrl(const QUrl& possibleRedirectUrl,
                     const QUrl& oldRedirectUrl) const;

    bool saveFile(const QString &outFile, const QByteArray &data);

    QNetworkAccessManager                        *m_manager;
    QNetworkDiskCache                            *m_diskCache;

    QWaitCondition                                m_queueWaitCond;
    QMutex                                        m_queueWaitLock;

    QMutex                                       *m_infoLock;
    QMap <QString, MythDownloadInfo*>             m_downloadInfos;
    QMap <QNetworkReply*, MythDownloadInfo*>      m_downloadReplies;
    QList <MythDownloadInfo*>                     m_downloadQueue;

    QThread                                      *m_queueThread;

    bool                                          m_runThread;
};

MPUBLIC MythDownloadManager *GetMythDownloadManager(void);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

