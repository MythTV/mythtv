#ifndef MYTHDOWNLOADMANAGER_H
#define MYTHDOWNLOADMANAGER_H

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkDiskCache>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRecursiveMutex>
#include <QString>
#include <QTimer>
#include <QWaitCondition>

#include "mythbaseexp.h"
#include "mthread.h"

class MythDownloadInfo;
class RemoteFileDownloadThread;

void ShutdownMythDownloadManager(void);

/** \brief A subclassed QNetworkCookieJar that allows for reading and writing
 *         cookie files that contain raw formatted cookies and copying the
 *         cookie jar to share between threads.
 */
class MBASE_PUBLIC MythCookieJar : public QNetworkCookieJar
{
    Q_OBJECT
  public:
    MythCookieJar() = default;
    void copyAllCookies(MythCookieJar &old);
    void load(const QString &filename);
    void save(const QString &filename);
};

// TODO : Overlap/Clash with RequestType in libupnp/httprequest.h
enum MRequestType : std::uint8_t {
    kRequestGet,
    kRequestHead,
    kRequestPost
};

using AuthCallback = void (*)(QNetworkReply*, QAuthenticator*, void*);

class MBASE_PUBLIC MythDownloadManager : public QObject, public MThread
{
    Q_OBJECT

  public:
    /** \brief Constructor for MythDownloadManager.  Instantiates a
     *         QNetworkAccessManager and QNetworkDiskCache.
     */
    MythDownloadManager()
        : MThread("DownloadManager"),
          m_infoLock(new QRecursiveMutex())
        {}

   ~MythDownloadManager() override;

    // Methods for starting the queue manager thread
    void run(void) override; // MThread
    void setRunThread(void) { m_runThread = true; }
    QThread *getQueueThread(void) { return m_queueThread; }
    bool isRunning(void) const { return m_isRunning; }

    // Methods to GET a URL
    void preCache(const QString &url);
    void queueDownload(const QString &url, const QString &dest,
                       QObject *caller, bool reload = false);
    void queueDownload(QNetworkRequest *req, QByteArray *data,
                       QObject *caller);
    bool download(const QString &url, const QString &dest,
                  bool reload = false);
    bool download(const QString &url, QByteArray *data,
                  bool reload = false, QString *finalUrl = nullptr);
    QNetworkReply *download(const QString &url, bool reload = false);
    bool download(QNetworkRequest *req, QByteArray *data);
    bool downloadAuth(const QString &url, const QString &dest,
                      bool reload = false,
                      AuthCallback authCallback = nullptr,
                      void *authArg = nullptr,
                      const QHash<QByteArray, QByteArray> *headers = nullptr);

    // Methods to POST to a URL
    void queuePost(const QString &url, QByteArray *data, QObject *caller);
    void queuePost(QNetworkRequest *req, QByteArray *data, QObject *caller);
    bool post(const QString &url, QByteArray *data);
    bool post(QNetworkRequest *req, QByteArray *data);
    bool postAuth(const QString &url, QByteArray *data,
                  AuthCallback authCallback, void *authArg,
                  const QHash<QByteArray, QByteArray> *headers = nullptr);

    // Cancel a download
    void cancelDownload(const QString &url, bool block = true);
    void cancelDownload(const QStringList &urls, bool block = true);

    // Generic helpers
    void removeListener(QObject *caller);
    QDateTime GetLastModified(const QString &url);

    void loadCookieJar(const QString &filename);
    void saveCookieJar(const QString &filename);
    void setCookieJar(QNetworkCookieJar *cookieJar);

    QNetworkCookieJar *copyCookieJar(void);
    void refreshCookieJar(QNetworkCookieJar *jar);
    void updateCookieJar(void);

    QString getHeader(const QUrl &url, const QString &header);
    static QString getHeader(const QNetworkCacheMetaData &cacheData, const QString &header);

  private slots:
    // QNetworkAccessManager signals
    void downloadFinished(QNetworkReply* reply);
    void authCallback(QNetworkReply *reply, QAuthenticator *authenticator);

    // QNetworkReply signals
    void downloadError(QNetworkReply::NetworkError errorCode);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

  private:
    // Notification from RemoteFile downloads
    void downloadFinished(MythDownloadInfo *dlInfo);

    // Helper methods for initializing and performing requests
    void queueItem(const QString &url, QNetworkRequest *req,
                   const QString &dest, QByteArray *data, QObject *caller,
                   MRequestType reqType = kRequestGet,
                   bool reload = false);

    bool processItem(const QString &url, QNetworkRequest *req,
                     const QString &dest, QByteArray *data,
                     MRequestType reqType = kRequestGet,
                     bool reload = false,
                     AuthCallback authCallback = nullptr,
                     void *authArg = nullptr,
                     const QHash<QByteArray, QByteArray> *headers = nullptr,
                     QString *finalUrl = nullptr);

    void downloadRemoteFile(MythDownloadInfo *dlInfo);
    void downloadQNetworkRequest(MythDownloadInfo *dlInfo);
    bool downloadNow(MythDownloadInfo *dlInfo, bool deleteInfo = true);
#ifndef _WIN32
    static bool downloadNowLinkLocal(MythDownloadInfo *dlInfo, bool deleteInfo);
#endif
    void downloadCanceled(void);

    static QUrl redirectUrl(const QUrl& possibleRedirectUrl,
                            const QUrl& oldRedirectUrl) ;

    static bool saveFile(const QString &outFile, const QByteArray &data,
                         bool append = false);

    void updateCookieJar(QNetworkCookieJar *jar);

    QNetworkAccessManager                        *m_manager     {nullptr};
    QNetworkDiskCache                            *m_diskCache   {nullptr};
    QNetworkProxy                                *m_proxy       {nullptr};

    QWaitCondition                                m_queueWaitCond;
    QMutex                                        m_queueWaitLock;

    QRecursiveMutex                              *m_infoLock    {nullptr};
    QMap <QString, MythDownloadInfo*>             m_downloadInfos;
    QMap <QNetworkReply*, MythDownloadInfo*>      m_downloadReplies;
    QList <MythDownloadInfo*>                     m_downloadQueue;
    QList <MythDownloadInfo*>                     m_cancellationQueue;

    QThread                                      *m_queueThread {nullptr};

    bool                                          m_runThread   {false};
    bool                                          m_isRunning   {false};

    QNetworkCookieJar                            *m_inCookieJar {nullptr};
    QMutex                                        m_cookieLock;

    friend class RemoteFileDownloadThread;
};

 MBASE_PUBLIC  MythDownloadManager *GetMythDownloadManager(void);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
