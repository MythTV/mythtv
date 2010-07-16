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

class RemoteFileDownloadThread;

class MythDownloadInfo
{
  public:
    MythDownloadInfo() :
        m_request(NULL),         m_reply(NULL),       m_data(NULL),
        m_caller(NULL),          m_post(false),       m_reload(false),
        m_preferCache(false),    m_syncMode(false),   m_done(false),
        m_lastStat(QDateTime::currentDateTime()),
        m_errorCode(QNetworkReply::NoError)
    {
    }

   ~MythDownloadInfo()
    {
        if (m_request)
            delete m_request;
        if (m_reply)
            m_reply->deleteLater();
    }

    void detach(void)
    {
        m_url.detach();
        m_outFile.detach();
    }       

    QString          m_url;
    QUrl             m_redirectedTo;
    QNetworkRequest *m_request;
    QNetworkReply   *m_reply;
    QString          m_outFile;
    QByteArray      *m_data;
    QByteArray       m_privData;
    QObject         *m_caller;
    bool             m_post;
    bool             m_reload;
    bool             m_preferCache;
    bool             m_syncMode;
    bool             m_done;
    QDateTime        m_lastStat;

    QNetworkReply::NetworkError m_errorCode;
};

class MPUBLIC MythDownloadManager : public QThread
{
    Q_OBJECT

  public:
    MythDownloadManager();
   ~MythDownloadManager();

    // Methods for starting the queue manager thread
    void run(void);
    void setRunThread(void) { m_runThread = true; }
    QThread *getQueueThread(void) { return m_queueThread; }

    // Methods to GET a URL
    void preCache(const QString &url);
    void queueDownload(const QString &url, const QString &dest,
                       QObject *caller, const bool reload = false);
    void queueDownload(QNetworkRequest *req, QByteArray *data,
                       QObject *caller);
    bool download(const QString &url, const QString &dest,
                  const bool reload = false);
    bool download(const QString &url, QByteArray *data,
                  const bool reload = false);
    bool download(QNetworkRequest *req, QByteArray *data);

    // Methods to POST to a URL
    void queuePost(const QString &url, QByteArray *data, QObject *caller);
    void queuePost(QNetworkRequest *req, QByteArray *data, QObject *caller);
    bool post(const QString &url, QByteArray *data);
    bool post(QNetworkRequest *req, QByteArray *data);

    // Generic helpers
    void removeListener(QObject *caller);
    QDateTime GetLastModified(const QString &url);

  private slots:
    // QNetworkAccessManager signals
    void downloadFinished(QNetworkReply* reply);

    // QNetworkReply signals
    void downloadError(QNetworkReply::NetworkError errorCode);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

  private:
    // Notification from RemoteFile downloads
    void downloadFinished(MythDownloadInfo *dlInfo);

    // Helper methods for initializing and performing requests
    void queueItem(const QString &url, QNetworkRequest *req,
                   const QString &dest, QByteArray *data, QObject *caller,
                   const bool post = false, const bool reload = false);

    bool processItem(const QString &url, QNetworkRequest *req,
                     const QString &dest, QByteArray *data,
                     const bool post = false, const bool reload = false);

    void downloadRemoteFile(MythDownloadInfo *dlInfo);
    void downloadQNetworkRequest(MythDownloadInfo *dlInfo);
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

    friend class RemoteFileDownloadThread;
};

MPUBLIC MythDownloadManager *GetMythDownloadManager(void);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

