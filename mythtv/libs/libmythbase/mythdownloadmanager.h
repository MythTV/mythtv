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

#include "mythbaseexp.h"

class MythDownloadInfo;
class RemoteFileDownloadThread;

class MBASE_PUBLIC MythDownloadManager : public QThread
{
    Q_OBJECT

  public:
    MythDownloadManager();
   ~MythDownloadManager();

    // Methods for starting the queue manager thread
    void run(void);
    void setRunThread(void) { m_runThread = true; }
    QThread *getQueueThread(void) { return m_queueThread; }
    bool isRunning(void) { return m_isRunning; }

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
    QNetworkReply *download(const QString &url, const bool reload = false);
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

    bool saveFile(const QString &outFile, const QByteArray &data,
                  const bool append = false);

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
    bool                                          m_isRunning;

    friend class RemoteFileDownloadThread;
};

 MBASE_PUBLIC  MythDownloadManager *GetMythDownloadManager(void);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

