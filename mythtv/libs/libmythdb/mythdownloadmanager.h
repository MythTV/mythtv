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

typedef struct downloadInfo
{
    QString        url;
    QNetworkReply *reply;
    QString        outFile;
    QByteArray    *data;
    QObject       *caller;
    QDateTime      lastStat;
    bool           syncMode;
    bool           done;
} DownloadInfo;

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
    void checkDownload(QNetworkReply* reply);
    void replyDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

  private:
    bool downloadNow(DownloadInfo *dlInfo, bool deleteInfo = true);
    QUrl redirectUrl(const QUrl& possibleRedirectUrl,
                     const QUrl& oldRedirectUrl) const;

    bool saveFile(const QString &outFile, const QByteArray &data);

    QNetworkAccessManager                        *m_manager;
    QNetworkDiskCache                            *m_diskCache;

    QWaitCondition                                m_queueWaitCond;
    QMutex                                        m_queueWaitLock;

    QMutex                                       *m_infoLock;
    QMap <QString, struct downloadInfo*>          m_downloadInfos;
    QMap <QNetworkReply*, struct downloadInfo*>   m_downloadReplies;
    QList <struct downloadInfo*>                  m_downloadQueue;

    QThread                                      *m_queueThread;

    bool                                          m_runThread;
};

MPUBLIC MythDownloadManager *GetMythDownloadManager(void);

#endif
