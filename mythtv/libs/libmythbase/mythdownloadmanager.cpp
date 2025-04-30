// qt
#include <QCoreApplication>
#include <QRunnable>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QNetworkCookie>
#include <QAuthenticator>
#include <QTextStream>
#include <QTimeZone>
#include <QNetworkProxy>
#include <QMutexLocker>
#include <QUrl>
#include <QTcpSocket>

#include <cstdlib>
#include <unistd.h> // for usleep()

// libmythbase
#include "compat.h"
#include "mythcorecontext.h"
#include "mthreadpool.h"
#include "mythdirs.h"
#include "mythevent.h"
#include "mythversion.h"
#include "remotefile.h"
#include "mythdate.h"

#include "mythdownloadmanager.h"
#include "mythlogging.h"
#include "portchecker.h"

#define LOC      QString("DownloadManager: ")
static constexpr int CACHE_REDIRECTION_LIMIT { 10 };

MythDownloadManager *downloadManager = nullptr;
QMutex               dmCreateLock;

/**
* \class MythDownloadInfo
*/
class MythDownloadInfo
{
  public:
    MythDownloadInfo() :
        m_lastStat(MythDate::current())
    {
        qRegisterMetaType<QNetworkReply::NetworkError>("QNetworkReply::NetworkError");
    }

   ~MythDownloadInfo()
    {
        delete m_request;
        if (m_reply && m_processReply)
            m_reply->deleteLater();
    }

    bool IsDone(void)
    {
        QMutexLocker lock(&m_lock);
        return m_done;
    }

    void SetDone(bool done)
    {
        QMutexLocker lock(&m_lock);
        m_done = done;
    }

    QString          m_url;
    QUrl             m_redirectedTo;
    QString         *m_finalUrl                    {nullptr};
    QNetworkRequest *m_request                     {nullptr};
    QNetworkReply   *m_reply                       {nullptr};
    QString          m_outFile;
    QByteArray      *m_data                        {nullptr};
    QByteArray       m_privData;
    QObject         *m_caller                      {nullptr};
    MRequestType     m_requestType                 {kRequestGet};
    bool             m_reload                      {false};
    bool             m_preferCache                 {false};
    bool             m_syncMode                    {false};
    bool             m_processReply                {true};
    bool             m_done                        {false};
    qint64           m_bytesReceived               {0};
    qint64           m_bytesTotal                  {0};
    QDateTime        m_lastStat;
    AuthCallback     m_authCallback                {nullptr};
    void            *m_authArg                     {nullptr};
    const QHash<QByteArray, QByteArray> *m_headers {nullptr};

    QNetworkReply::NetworkError m_errorCode        {QNetworkReply::NoError};
    QMutex           m_lock;
};


/**
* \class RemoteFileDownloadThread
*/
class RemoteFileDownloadThread : public QRunnable
{
  public:
    RemoteFileDownloadThread(MythDownloadManager *parent,
                             MythDownloadInfo *dlInfo) :
        m_parent(parent),
        m_dlInfo(dlInfo) {}

    void run() override // QRunnable
    {
        bool ok = false;

        auto *rf = new RemoteFile(m_dlInfo->m_url, false, false, 0ms);
        ok = rf->SaveAs(m_dlInfo->m_privData);
        delete rf;

        if (!ok)
            m_dlInfo->m_errorCode = QNetworkReply::UnknownNetworkError;

        m_dlInfo->m_bytesReceived = m_dlInfo->m_privData.size();
        m_dlInfo->m_bytesTotal = m_dlInfo->m_bytesReceived;

        m_parent->downloadFinished(m_dlInfo);
    }

  private:
    MythDownloadManager *m_parent {nullptr};
    MythDownloadInfo    *m_dlInfo {nullptr};
};

/** \brief Deletes the running MythDownloadManager at program exit.
 */
void ShutdownMythDownloadManager(void)
{
    if (downloadManager)
    {
        delete downloadManager;
        downloadManager = nullptr;
    }
}

/** \brief Gets the pointer to the MythDownloadManager singleton.
 *  \return Pointer to the MythDownloadManager instance
 */
MythDownloadManager *GetMythDownloadManager(void)
{
    if (downloadManager)
        return downloadManager;

    QMutexLocker locker(&dmCreateLock);

    // Check once more in case the download manager was created
    // while we were securing the lock.
    if (downloadManager)
        return downloadManager;

    auto *tmpDLM = new MythDownloadManager();
    tmpDLM->start();
    while (!tmpDLM->getQueueThread())
        usleep(10000);

    tmpDLM->moveToThread(tmpDLM->getQueueThread());
    tmpDLM->setRunThread();

    while (!tmpDLM->isRunning())
        usleep(10000);

    downloadManager = tmpDLM;

    atexit(ShutdownMythDownloadManager);

    return downloadManager;
}

/** \brief Destructor for MythDownloadManager.
 */
MythDownloadManager::~MythDownloadManager()
{
    m_runThread = false;
    m_queueWaitCond.wakeAll();

    wait();

    delete m_infoLock;
    delete m_inCookieJar;
}

/** \brief Runs a loop to process incoming download requests and
 *         triggers download events to be processed.
 */
void MythDownloadManager::run(void)
{
    RunProlog();

    bool downloading = false;
    bool itemsInQueue = false;
    bool itemsInCancellationQueue = false;
    bool waitAnyway = false;

    m_queueThread = QThread::currentThread();

    while (!m_runThread)
        usleep(50ms);

    m_manager = new QNetworkAccessManager(this);
    m_diskCache = new QNetworkDiskCache(this);
    m_proxy = new QNetworkProxy();
    m_diskCache->setCacheDirectory(GetConfDir() + "/cache/" +
                                   QCoreApplication::applicationName() + "-" +
                                   gCoreContext->GetHostName());
    m_manager->setCache(m_diskCache);

    // Set the proxy for the manager to be the application default proxy,
    // which has already been setup
    m_manager->setProxy(*m_proxy);

    // make sure the cookieJar is created in the same thread as the manager
    // and set its parent to nullptr so it can be shared between managers
    m_manager->cookieJar()->setParent(nullptr);

    QObject::connect(m_manager, &QNetworkAccessManager::finished,
                     this, qOverload<QNetworkReply*>(&MythDownloadManager::downloadFinished));

    m_isRunning = true;
    while (m_runThread)
    {
        if (m_inCookieJar)
        {
            LOG(VB_GENERAL, LOG_DEBUG, "Updating DLManager's Cookie Jar");
            updateCookieJar();
        }
        m_infoLock->lock();
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("items downloading %1").arg(m_downloadInfos.count()));
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("items queued %1").arg(m_downloadQueue.count()));
        downloading = !m_downloadInfos.isEmpty();
        itemsInCancellationQueue = !m_cancellationQueue.isEmpty();
        m_infoLock->unlock();

        if (itemsInCancellationQueue)
        {
            downloadCanceled();
        }
        if (downloading)
            QCoreApplication::processEvents();

        m_infoLock->lock();
        itemsInQueue = !m_downloadQueue.isEmpty();
        m_infoLock->unlock();

        if (!itemsInQueue || waitAnyway)
        {
            waitAnyway = false;
            m_queueWaitLock.lock();

            if (downloading)
            {
                LOG(VB_FILE, LOG_DEBUG, LOC + QString("waiting 200ms"));
                m_queueWaitCond.wait(&m_queueWaitLock, 200);
            }
            else
            {
                LOG(VB_FILE, LOG_DEBUG, LOC + QString("waiting for more items to download"));
                m_queueWaitCond.wait(&m_queueWaitLock);
            }

            m_queueWaitLock.unlock();
        }

        m_infoLock->lock();
        if (!m_downloadQueue.isEmpty())
        {
            MythDownloadInfo *dlInfo = m_downloadQueue.front();

            m_downloadQueue.pop_front();

            if (!dlInfo)
            {
                m_infoLock->unlock();
                continue;
            }

            if (m_downloadInfos.contains(dlInfo->m_url))
            {
                // Push request to the end of the queue to let others process.
                // If this is the only item in the queue, force the loop to
                // wait a little.
                if (m_downloadQueue.isEmpty())
                    waitAnyway = true;
                m_downloadQueue.push_back(dlInfo);
                m_infoLock->unlock();
                continue;
            }

            if (dlInfo->m_url.startsWith("myth://"))
                downloadRemoteFile(dlInfo);
            else
            {
                QMutexLocker cLock(&m_cookieLock);
                downloadQNetworkRequest(dlInfo);
            }

            m_downloadInfos[dlInfo->m_url] = dlInfo;
        }
        m_infoLock->unlock();
    }
    m_isRunning = false;

    RunEpilog();
}

/**
 *  \brief Adds a request to the download queue.
 *  \param url      URI to download.
 *  \param req      QNetworkRequest to queue
 *  \param dest     Destination filename.
 *  \param data     Location of data for request
 *  \param caller   QObject to receive event notifications.
 *  \param reqType  Issue a POST/GET/HEAD request
 *  \param reload   Force reloading of the URL
 */
void MythDownloadManager::queueItem(const QString &url, QNetworkRequest *req,
                                    const QString &dest, QByteArray *data,
                                    QObject *caller, const MRequestType reqType,
                                    const bool reload)
{
    auto *dlInfo = new MythDownloadInfo;

    dlInfo->m_url     = url;
    dlInfo->m_request = req;
    dlInfo->m_outFile = dest;
    dlInfo->m_data    = data;
    dlInfo->m_caller  = caller;
    dlInfo->m_requestType = reqType;
    dlInfo->m_reload  = reload;

    QMutexLocker locker(m_infoLock);
    m_downloadQueue.push_back(dlInfo);
    m_queueWaitCond.wakeAll();
}

/**
 *  \brief Processes a network request immediately and waits for a response.
 *  \param url      URI to download.
 *  \param req      QNetworkRequest to queue
 *  \param dest     Destination filename.
 *  \param data     Location of data for request
 *  \param reqType  Issue a POST/GET/HEAD request
 *  \param reload   Force reloading of the URL
 *  \param authCallback AuthCallback function for authentication
 *  \param authArg  Opaque argument for callback function
 *  \param headers  Hash of optional HTTP header to add to the request
 */
bool MythDownloadManager::processItem(const QString &url, QNetworkRequest *req,
                                      const QString &dest, QByteArray *data,
                                      const MRequestType reqType,
                                      const bool reload,
                                      AuthCallback authCallbackFn, void *authArg,
                                      const QHash<QByteArray, QByteArray> *headers,
                                      QString *finalUrl)
{
    auto *dlInfo = new MythDownloadInfo;

    dlInfo->m_url      = url;
    dlInfo->m_request  = req;
    dlInfo->m_outFile  = dest;
    dlInfo->m_data     = data;
    dlInfo->m_requestType = reqType;
    dlInfo->m_reload   = reload;
    dlInfo->m_syncMode = true;
    dlInfo->m_authCallback = authCallbackFn;
    dlInfo->m_authArg  = authArg;
    dlInfo->m_headers  = headers;
    dlInfo->m_finalUrl = finalUrl;

    return downloadNow(dlInfo, true);
}

/** \brief Downloads a URL but doesn't store the resulting data anywhere
 *  \param url      URI to download.
 */
void MythDownloadManager::preCache(const QString &url)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("preCache('%1')").arg(url));
    queueItem(url, nullptr, QString(), nullptr, nullptr);
}

/** \brief Adds a url to the download queue.
 *  \param url      URI to download.
 *  \param dest     Destination filename.
 *  \param caller   QObject to receive event notifications.
 *  \param reload   Whether to force reloading of the URL or not
 */
void MythDownloadManager::queueDownload(const QString &url,
                                        const QString &dest,
                                        QObject *caller,
                                        const bool reload)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("queueDownload('%1', '%2', %3)")
        .arg(url, dest, QString::number((long long)caller)));

    queueItem(url, nullptr, dest, nullptr, caller, kRequestGet, reload);
}

/** \brief Downloads a QNetworkRequest via the QNetworkAccessManager
 *  \param req      Network request to GET
 *  \param data     Location to store download data
 *  \param caller   QObject of caller for event notification
 */
void MythDownloadManager::queueDownload(QNetworkRequest *req,
                                        QByteArray *data,
                                        QObject *caller)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("queueDownload('%1', '%2', %3)")
            .arg(req->url().toString()).arg((long long)data)
            .arg((long long)caller));

    queueItem(req->url().toString(), req, QString(), data, caller,
              kRequestGet,
              (QNetworkRequest::AlwaysNetwork == req->attribute(
               QNetworkRequest::CacheLoadControlAttribute,
               QNetworkRequest::PreferNetwork).toInt()));
}

/** \brief Downloads a URL to a file in blocking mode.
 *  \param url     URI to download.
 *  \param dest    Destination filename.
 *  \param reload  Whether to force reloading of the URL or not
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::download(const QString &url, const QString &dest,
                                   const bool reload)
{
    return processItem(url, nullptr, dest, nullptr, kRequestGet, reload);
}

/** \brief Downloads a URI to a QByteArray in blocking mode.
 *  \param url     URI to download.
 *  \param data    Pointer to destination QByteArray.
 *  \param reload  Whether to force reloading of the URL or not
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::download(const QString &url, QByteArray *data,
                                   const bool reload, QString *finalUrl)
{
    QString redirected;
    if (!processItem(url, nullptr, QString(), data, kRequestGet, reload,
                     nullptr, nullptr, nullptr, &redirected))
        return false;
    if (!redirected.isEmpty() && finalUrl != nullptr)
        *finalUrl = redirected;
    return true;
}

/** \brief Downloads a URI to a QByteArray in blocking mode.
 *  \param url      URI to download.
 *  \param reload   Whether to force reloading of the URL or not
 *  \return pointer to the QNetworkReply containing the download response,
 *                  nullptr if an error
 */
QNetworkReply *MythDownloadManager::download(const QString &url,
                                             const bool reload)
{
    auto *dlInfo = new MythDownloadInfo;
    QNetworkReply *reply = nullptr;

    dlInfo->m_url          = url;
    dlInfo->m_reload       = reload;
    dlInfo->m_syncMode     = true;
    dlInfo->m_processReply = false;

    if (downloadNow(dlInfo, false))
    {
        if (dlInfo->m_reply)
        {
            reply = dlInfo->m_reply;
            // prevent dlInfo dtor from deleting the reply
            dlInfo->m_reply = nullptr;

            delete dlInfo;

            return reply;
        }

        delete dlInfo;
    }

    return nullptr;
}

/** \brief Downloads a QNetworkRequest via the QNetworkAccessManager
 *  \param req      Information on the network request
 *  \param data     Location to store download data
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::download(QNetworkRequest *req, QByteArray *data)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("download('%1', '%2')")
            .arg(req->url().toString()).arg((long long)data));
    return processItem(req->url().toString(), req, QString(), data,
                       kRequestGet,
                       (QNetworkRequest::AlwaysNetwork == req->attribute(
                        QNetworkRequest::CacheLoadControlAttribute,
                        QNetworkRequest::PreferNetwork).toInt()));
}

/** \brief Downloads a URL to a file in blocking mode.
 *  \param url     URI to download.
 *  \param dest    Destination filename.
 *  \param reload  Whether to force reloading of the URL or not
 *  \param authCallbackFn AuthCallback function for use with authentication
 *  \param authArg Opaque argument for callback function
 *  \param headers Hash of optional HTTP header to add to the request
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::downloadAuth(const QString &url, const QString &dest,
               const bool reload, AuthCallback authCallbackFn, void *authArg,
               const QHash<QByteArray, QByteArray> *headers)
{
    return processItem(url, nullptr, dest, nullptr, kRequestGet, reload, authCallbackFn,
                       authArg, headers);
}


/** \brief Queues a post to a URL via the QNetworkAccessManager
 *  \param url      URL to post to
 *  \param data     Location holding post and response data
 *  \param caller   QObject of caller for event notification
 */
void MythDownloadManager::queuePost(const QString &url,
                                    QByteArray *data,
                                    QObject *caller)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("queuePost('%1', '%2')")
            .arg(url).arg((long long)data));

    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "queuePost(), data is NULL!");
        return;
    }

    queueItem(url, nullptr, QString(), data, caller, kRequestPost);
}

/** \brief Queues a post to a URL via the QNetworkAccessManager
 *  \param req      QNetworkRequest to post
 *  \param data     Location holding post and response data
 *  \param caller   QObject of caller for event notification
 */
void MythDownloadManager::queuePost(QNetworkRequest *req,
                                    QByteArray *data,
                                    QObject *caller)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("queuePost('%1', '%2')")
            .arg(req->url().toString()).arg((long long)data));

    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "queuePost(), data is NULL!");
        return;
    }

    queueItem(req->url().toString(), req, QString(), data, caller,
              kRequestPost,
              (QNetworkRequest::AlwaysNetwork == req->attribute(
               QNetworkRequest::CacheLoadControlAttribute,
               QNetworkRequest::PreferNetwork).toInt()));

}

/** \brief Posts data to a url via the QNetworkAccessManager
 *  \param url      URL to post to
 *  \param data     Location holding post and response data
 *  \return true if post was successful, false otherwise.
 */
bool MythDownloadManager::post(const QString &url, QByteArray *data)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("post('%1', '%2')")
            .arg(url).arg((long long)data));

    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "post(), data is NULL!");
        return false;
    }

    return processItem(url, nullptr, QString(), data, kRequestPost);
}

/** \brief Posts a QNetworkRequest via the QNetworkAccessManager
 *  \param req      Information on the network request
 *  \param data     Location holding post and response data
 *  \return true if post was successful, false otherwise.
 */
bool MythDownloadManager::post(QNetworkRequest *req, QByteArray *data)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("post('%1', '%2')")
            .arg(req->url().toString()).arg((long long)data));

    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "post(), data is NULL!");
        return false;
    }

    return processItem(req->url().toString(), req, QString(), data,
                       kRequestPost,
                       (QNetworkRequest::AlwaysNetwork == req->attribute(
                        QNetworkRequest::CacheLoadControlAttribute,
                        QNetworkRequest::PreferNetwork).toInt()));

}

/** \brief Posts data to a url via the QNetworkAccessManager
 *  \param url      URL to post to
 *  \param data     Location holding post and response data
 *  \param authCallbackFn AuthCallback function for authentication
 *  \param authArg Opaque argument for callback function
 *  \param headers Hash of optional HTTP headers to add to the request
 *  \return true if post was successful, false otherwise.
 */
bool MythDownloadManager::postAuth(const QString &url, QByteArray *data,
                                   AuthCallback authCallbackFn, void *authArg,
                                   const QHash<QByteArray, QByteArray> *headers)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("postAuth('%1', '%2')")
            .arg(url).arg((long long)data));

    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "postAuth(), data is NULL!");
        return false;
    }

    return processItem(url, nullptr, nullptr, data, kRequestPost, false, authCallbackFn,
                       authArg, headers);
}

/** \brief Triggers a myth:// URI download in the background via RemoteFile
 *  \param dlInfo   MythDownloadInfo information for download
 */
void MythDownloadManager::downloadRemoteFile(MythDownloadInfo *dlInfo)
{
    auto *dlThread = new RemoteFileDownloadThread(this, dlInfo);
    MThreadPool::globalInstance()->start(dlThread, "RemoteFileDownload");
}

/** \brief Downloads a QNetworkRequest via the QNetworkAccessManager
 *  \param dlInfo   MythDownloadInfo information for download
 */
void MythDownloadManager::downloadQNetworkRequest(MythDownloadInfo *dlInfo)
{
    if (!dlInfo)
        return;

    static const QString kDateFormat = "ddd, dd MMM yyyy hh:mm:ss 'GMT'";
    QUrl qurl(dlInfo->m_url);
    QNetworkRequest request;

    if (dlInfo->m_request)
    {
        request = *dlInfo->m_request;
        delete dlInfo->m_request;
        dlInfo->m_request = nullptr;
    }
    else
    {
        request.setUrl(qurl);
    }

    if (dlInfo->m_reload)
    {
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::AlwaysNetwork);
    }
    else
    {
        // Prefer the in-cache item if one exists and it is less than 5 minutes
        // old and it will not expire in the next 10 seconds
        QDateTime now = MythDate::current();

        // Handle redirects, we want the metadata of the file headers
        QString redirectLoc;
        int limit = 0;
        while (!(redirectLoc = getHeader(qurl, "Location")).isNull())
        {
            if (limit == CACHE_REDIRECTION_LIMIT)
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Cache Redirection limit "
                                                     "reached for %1")
                                                    .arg(qurl.toString()));
                return;
            }
            qurl.setUrl(redirectLoc);
            limit++;
        }

        LOG(VB_NETWORK, LOG_DEBUG, QString("Checking cache for %1")
                                                    .arg(qurl.toString()));

        m_infoLock->lock();
        QNetworkCacheMetaData urlData = m_manager->cache()->metaData(qurl);
        m_infoLock->unlock();
        if ((urlData.isValid()) &&
            ((!urlData.expirationDate().isValid()) ||
             (urlData.expirationDate().toUTC().secsTo(now) < 10)))
        {
            QString dateString = getHeader(urlData, "Date");

            if (!dateString.isNull())
            {
                QDateTime loadDate =
                    MythDate::fromString(dateString, kDateFormat);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
                loadDate.setTimeSpec(Qt::UTC);
#else
                loadDate.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
                if (loadDate.secsTo(now) <= 720)
                {
                    dlInfo->m_preferCache = true;
                    LOG(VB_NETWORK, LOG_DEBUG, QString("Preferring cache for %1")
                                                    .arg(qurl.toString()));
                }
            }
        }
    }

    if (dlInfo->m_preferCache)
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::PreferCache);

    if (!request.hasRawHeader("User-Agent"))
    {
        request.setRawHeader("User-Agent",
                             QByteArray("MythTV v") + MYTH_BINARY_VERSION +
                             " MythDownloadManager");
    }

    if (dlInfo->m_headers)
    {
        QHash<QByteArray, QByteArray>::const_iterator it =
            dlInfo->m_headers->constBegin();
        for ( ; it != dlInfo->m_headers->constEnd(); ++it )
        {
            if (!it.key().isEmpty() && !it.value().isEmpty())
            {
                request.setRawHeader(it.key(), it.value());
            }
        }
    }

    switch (dlInfo->m_requestType)
    {
        case kRequestPost :
            dlInfo->m_reply = m_manager->post(request, *dlInfo->m_data);
            break;
        case kRequestHead :
            dlInfo->m_reply = m_manager->head(request);
            break;
        case kRequestGet :
        default:
            dlInfo->m_reply = m_manager->get(request);
            break;
    }

    m_downloadReplies[dlInfo->m_reply] = dlInfo;

    if (dlInfo->m_authCallback)
    {
        connect(m_manager, &QNetworkAccessManager::authenticationRequired,
                this, &MythDownloadManager::authCallback);
    }

    connect(dlInfo->m_reply, &QNetworkReply::errorOccurred,
            this, &MythDownloadManager::downloadError);
    connect(dlInfo->m_reply, &QNetworkReply::downloadProgress,
            this, &MythDownloadManager::downloadProgress);
}

/** \brief Signal handler for authentication requests
 *  \param reply         Response from the remote server
 *  \param authenticator To fill in with authentication details
 */
void MythDownloadManager::authCallback(QNetworkReply *reply,
                                       QAuthenticator *authenticator)
{
    if (!reply)
        return;

    MythDownloadInfo *dlInfo = m_downloadReplies[reply];

    if (!dlInfo)
        return;

    if (dlInfo->m_authCallback)
    {
        LOG(VB_FILE, LOG_DEBUG, "Calling auth callback");
        dlInfo->m_authCallback(reply, authenticator, dlInfo->m_authArg);
    }
}

/** \brief Download helper for download() blocking methods.
 *  \param dlInfo     Information on URI to download.
 *  \param deleteInfo Flag to indicate whether to delete the provided
 *                    MythDownloadInfo instance when done.
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::downloadNow(MythDownloadInfo *dlInfo, bool deleteInfo)
{
    if (!dlInfo)
        return false;

    dlInfo->m_syncMode = true;

    // Special handling for link-local
    // Not needed for Windows because windows does not need
    // the scope id.
#ifndef _WIN32
    if (dlInfo->m_url.startsWith("http://[fe80::",Qt::CaseInsensitive))
        return downloadNowLinkLocal(dlInfo, deleteInfo);
#endif
    m_infoLock->lock();
    m_downloadQueue.push_back(dlInfo);
    m_infoLock->unlock();
    m_queueWaitCond.wakeAll();

    // timeout myth:// RemoteFile transfers 20 seconds from now
    // timeout non-myth:// QNetworkAccessManager transfers 60 seconds after
    //    their last progress update
    QDateTime startedAt = MythDate::current();
    m_infoLock->lock();
    while ((!dlInfo->IsDone()) &&
           (dlInfo->m_errorCode == QNetworkReply::NoError) &&
           (((!dlInfo->m_url.startsWith("myth://")) &&
             (MythDate::secsInPast(dlInfo->m_lastStat) < 60s)) ||
            ((dlInfo->m_url.startsWith("myth://")) &&
             (MythDate::secsInPast(startedAt) < 20s))))
    {
        m_infoLock->unlock();
        m_queueWaitLock.lock();
        m_queueWaitCond.wait(&m_queueWaitLock, 200);
        m_queueWaitLock.unlock();
        m_infoLock->lock();
    }
    bool done = dlInfo->IsDone();
    bool success =
       done && (dlInfo->m_errorCode == QNetworkReply::NoError);

    if (!done)
    {
        dlInfo->m_data = nullptr;   // Prevent downloadFinished() from updating
        dlInfo->m_syncMode = false; // Let downloadFinished() cleanup for us
        if ((dlInfo->m_reply) &&
            (dlInfo->m_errorCode == QNetworkReply::NoError))
        {
            LOG(VB_FILE, LOG_DEBUG,
                LOC + QString("Aborting download - lack of data transfer"));
            dlInfo->m_reply->abort();
        }
    }
    else if (deleteInfo)
    {
        delete dlInfo;
    }

    m_infoLock->unlock();

    return success;
}

#ifndef _WIN32
/** \brief Download blocking methods with link-local address.
 *
 * Special processing for IPV6 link-local addresses, which
 * are not accepted in the normal way, because QT classes
 * QUrl and QNetworkManager do not support a scope id
 * or percent sign in the ip address.
 * This performs the call using base sockets. It is a bit
 * less efficient as it does not keep the connection open
 * or permit compressed content. However these calls are
 * few and far between, only happening once during start
 * of frontend.
 *
 * This also has limited capabilities and will return an error
 * if an unsupported operation is attempted.
 *
 * This should never be used in Windows systems.
 *
 * \param dlInfo     Information on URI to download.
 * \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::downloadNowLinkLocal(MythDownloadInfo *dlInfo, bool deleteInfo)
{
    bool ok = true;

    // No buffer - no reply...
    if (!dlInfo->m_data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("No data buffer provided for %1").arg(dlInfo->m_url));
        ok = false;
    }

    // Only certain features are supported here
    if (dlInfo->m_authCallback || dlInfo->m_authArg)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported authentication for %1").arg(dlInfo->m_url));
        ok = false;
    }

    if (ok && !dlInfo->m_outFile.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported File output %1 for %2")
            .arg(dlInfo->m_outFile, dlInfo->m_url));
        ok = false;
    }

    if (ok && (!deleteInfo || dlInfo->m_requestType == kRequestHead))
    {
        // We do not have the ability to return a network reply in dlInfo
        // so if we are asked to do that, return an error.
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported link-local operation %1")
            .arg(dlInfo->m_url));
        ok = false;
    }

    QUrl url(dlInfo->m_url);
    QString host(url.host());
    int port(url.port(80));
    if (ok && PortChecker::resolveLinkLocal(host, port))
    {
        QString reqType;
        switch (dlInfo->m_requestType)
        {
            case kRequestPost :
                reqType = "POST";
                break;
            case kRequestGet :
            default:
                reqType = "GET";
                break;
        }
        QByteArray* buffer = dlInfo->m_data;
        QHash<QByteArray, QByteArray> headers;
        if (dlInfo->m_headers)
            headers = *dlInfo->m_headers;
        if (!headers.contains("User-Agent"))
            headers.insert("User-Agent", QByteArray("MythDownloadManager v") +
                           MYTH_BINARY_VERSION);
        headers.insert("Connection", "close");
        headers.insert("Accept-Encoding", "identity");
        if (!buffer->isEmpty())
            headers.insert("Content-Length", QString::number(buffer->size()).toUtf8());
        headers.insert("Host", (url.host() + ":" + QString::number(port)).toUtf8());

        QByteArray requestMessage;
        QString path (url.path());
        requestMessage.append("POST ");
        requestMessage.append(path.toLatin1());
        requestMessage.append(" HTTP/1.1\r\n");
        QHashIterator<QByteArray, QByteArray> it(headers);
        while (it.hasNext())
        {
            it.next();
            requestMessage.append(it.key());
            requestMessage.append(": ");
            requestMessage.append(it.value());
            requestMessage.append("\r\n");
        }
        requestMessage.append("\r\n");
        if (!buffer->isEmpty())
            requestMessage.append(*buffer);

        QTcpSocket socket;
        socket.connectToHost(host, static_cast<uint16_t>(port));
        // QT Warning - this may not work on Windows
        if (!socket.waitForConnected(5000))
            ok = false;
        if (ok)
            ok = socket.write(requestMessage) > 0;
        if (ok)
            // QT Warning - this may not work on Windows
            ok = socket.waitForDisconnected(5000);
        if (ok)
        {
            *buffer = socket.readAll();
            // Find the start of the content
            QByteArray delim("\r\n\r\n");
            int delimLoc = buffer->indexOf(delim);
            if (delimLoc > -1)
                *buffer = buffer->right(buffer->size() - delimLoc - 4);
            else
                ok=false;
        }
        socket.close();
    }
    else
    {
        ok = false;
    }

    if (deleteInfo)
        delete dlInfo;

    if (ok)
        return true;

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Link Local request failed: %1").arg(url.toString()));
    return false;
}
#endif

/** \brief Cancel a queued or current download.
 *  \param url URL for download to cancel
 *  \param block If true, wait until all the cancellations have finished.
 */
void MythDownloadManager::cancelDownload(const QString &url, bool block)
{
    cancelDownload(QStringList(url), block);
}

/** \brief Cancel a queued or current download.
 *  \param urls List of URLs for download to cancel
 *  \param block If true, wait until all the cancellations have finished.
 */
void MythDownloadManager::cancelDownload(const QStringList &urls, bool block)
{
    m_infoLock->lock();
    for (const auto& url : std::as_const(urls))
    {
        QMutableListIterator<MythDownloadInfo*> lit(m_downloadQueue);
        while (lit.hasNext())
        {
            lit.next();
            MythDownloadInfo *dlInfo = lit.value();
            if (dlInfo->m_url == url)
            {
                if (!m_cancellationQueue.contains(dlInfo))
                    m_cancellationQueue.append(dlInfo);
                lit.remove();
            }
        }

        if (m_downloadInfos.contains(url))
        {
            MythDownloadInfo *dlInfo = m_downloadInfos[url];

            if (!m_cancellationQueue.contains(dlInfo))
                m_cancellationQueue.append(dlInfo);

            if (dlInfo->m_reply)
                m_downloadReplies.remove(dlInfo->m_reply);

            m_downloadInfos.remove(url);
        }
    }
    m_infoLock->unlock();

    if (QThread::currentThread() == this->thread())
    {
        downloadCanceled();
        return;
    }

    // wake-up running thread
    m_queueWaitCond.wakeAll();

    if (!block)
        return;

    while (!m_cancellationQueue.isEmpty())
    {
        usleep(50ms); // re-test in another 50ms
    }
}

void MythDownloadManager::downloadCanceled()
{
    QMutexLocker locker(m_infoLock);

    QMutableListIterator<MythDownloadInfo*> lit(m_cancellationQueue);
    while (lit.hasNext())
    {
        lit.next();
        MythDownloadInfo *dlInfo = lit.value();
        dlInfo->m_lock.lock();

        if (dlInfo->m_reply)
        {
            LOG(VB_FILE, LOG_DEBUG,
                LOC + QString("Aborting download - user request"));
            dlInfo->m_reply->abort();
        }
        lit.remove();
        if (dlInfo->m_done)
        {
            dlInfo->m_lock.unlock();
            continue;
        }
        dlInfo->m_errorCode = QNetworkReply::OperationCanceledError;
        dlInfo->m_done = true;
        dlInfo->m_lock.unlock();
    }
}

/** \brief Disconnects the specified caller from any existing
 *         MythDownloadInfo instances.
 *  \param caller  QObject listener to remove
 */
void MythDownloadManager::removeListener(QObject *caller)
{
    QMutexLocker locker(m_infoLock);

    QList <MythDownloadInfo*>::iterator lit = m_downloadQueue.begin();
    for (; lit != m_downloadQueue.end(); ++lit)
    {
        MythDownloadInfo *dlInfo = *lit;
        if (dlInfo->m_caller == caller)
        {
            dlInfo->m_caller  = nullptr;
            dlInfo->m_outFile = QString();
            dlInfo->m_data    = nullptr;
        }
    }

    QMap <QString, MythDownloadInfo*>::iterator mit = m_downloadInfos.begin();
    for (; mit != m_downloadInfos.end(); ++mit)
    {
        MythDownloadInfo *dlInfo = mit.value();
        if (dlInfo->m_caller == caller)
        {
            dlInfo->m_caller  = nullptr;
            dlInfo->m_outFile = QString();
            dlInfo->m_data    = nullptr;
        }
    }
}

/** \brief Slot to process download error events.
 *  \param errorCode  error code
 */
void MythDownloadManager::downloadError(QNetworkReply::NetworkError errorCode)
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr)
        return;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("downloadError %1 ")
                    .arg(errorCode) + reply->errorString() );

    QMutexLocker locker(m_infoLock);
    if (!m_downloadReplies.contains(reply))
    {
        reply->deleteLater();
        return;
    }

    MythDownloadInfo *dlInfo = m_downloadReplies[reply];

    if (!dlInfo)
        return;

    dlInfo->m_errorCode = errorCode;
}

/** \brief Checks whether we were redirected to the given URL.
 *  \param possibleRedirectUrl Possible Redirect URL
 *  \param oldRedirectUrl      Old Redirect URL
 *  \return empty QUrl if we were not redirected, otherwise the redirected URL
 */
QUrl MythDownloadManager::redirectUrl(const QUrl& possibleRedirectUrl,
                                      const QUrl& oldRedirectUrl)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("redirectUrl()"));
    QUrl redirectUrl;

    if(!possibleRedirectUrl.isEmpty() && possibleRedirectUrl != oldRedirectUrl)
        redirectUrl = possibleRedirectUrl;

    return redirectUrl;
}

/** \brief Slot to process download finished events.
 *  \param reply  QNetworkReply for completed download.
 */
void MythDownloadManager::downloadFinished(QNetworkReply* reply)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("downloadFinished(%1)")
                                            .arg((long long)reply));

    QMutexLocker locker(m_infoLock);
    if (!m_downloadReplies.contains(reply))
    {
        reply->deleteLater();
        return;
    }

    MythDownloadInfo *dlInfo = m_downloadReplies[reply];

    if (!dlInfo || !dlInfo->m_reply)
        return;

    downloadFinished(dlInfo);
}

/** \brief Callback to process download finished events.
 *  \param dlInfo  MythDownloadInfo for completed download.
 */
void MythDownloadManager::downloadFinished(MythDownloadInfo *dlInfo)
{
    if (!dlInfo)
        return;

    int statusCode = -1;
    static const QString kDateFormat = "ddd, dd MMM yyyy hh:mm:ss 'GMT'";
    QNetworkReply *reply = dlInfo->m_reply;

    if (reply)
    {
        QUrl possibleRedirectUrl =
             reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

        if (!possibleRedirectUrl.isEmpty() &&
            possibleRedirectUrl.isValid() &&
            possibleRedirectUrl.isRelative())  // Turn relative Url to absolute
            possibleRedirectUrl = reply->url().resolved(possibleRedirectUrl);

        if (!possibleRedirectUrl.isEmpty() && dlInfo->m_finalUrl != nullptr)
            *dlInfo->m_finalUrl = QString(possibleRedirectUrl.toString());

        dlInfo->m_redirectedTo =
             redirectUrl(possibleRedirectUrl, dlInfo->m_redirectedTo);

        QVariant status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (status.isValid())
            statusCode = status.toInt();
    }

    if(reply && !dlInfo->m_redirectedTo.isEmpty() &&
       ((dlInfo->m_requestType != kRequestPost) ||
       (statusCode == 301 || statusCode == 302 ||
       statusCode == 303)))
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("downloadFinished(%1): Redirect: %2 -> %3")
                .arg(QString::number((long long)dlInfo),
                     reply->url().toString(),
                     dlInfo->m_redirectedTo.toString()));

        if (dlInfo->m_data)
            dlInfo->m_data->clear();

        dlInfo->m_bytesReceived = 0;
        dlInfo->m_bytesTotal    = 0;

        QNetworkRequest request(dlInfo->m_redirectedTo);

        if (dlInfo->m_reload)
        {
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                                 QNetworkRequest::AlwaysNetwork);
        }
        else if (dlInfo->m_preferCache)
        {
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                                 QNetworkRequest::PreferCache);
        }

        request.setRawHeader("User-Agent",
                             "MythDownloadManager v" +
                             QByteArray(MYTH_BINARY_VERSION));

        switch (dlInfo->m_requestType)
        {
            case kRequestHead :
                dlInfo->m_reply = m_manager->head(request);
                break;
            case kRequestGet :
            default:
                dlInfo->m_reply = m_manager->get(request);
                break;
        }

        m_downloadReplies[dlInfo->m_reply] = dlInfo;

        connect(dlInfo->m_reply, &QNetworkReply::errorOccurred,
                this, &MythDownloadManager::downloadError);
        connect(dlInfo->m_reply, &QNetworkReply::downloadProgress,
                this, &MythDownloadManager::downloadProgress);

        m_downloadReplies.remove(reply);
        reply->deleteLater();
    }
    else
    {
        LOG(VB_FILE, LOG_DEBUG, QString("downloadFinished(%1): COMPLETE: %2")
                .arg((long long)dlInfo).arg(dlInfo->m_url));

        // HACK Insert a Date header into the cached metadata if one doesn't
        // already exist
        QUrl fileUrl = dlInfo->m_url;
        QString redirectLoc;
        int limit = 0;
        while (!(redirectLoc = getHeader(fileUrl, "Location")).isNull())
        {
            if (limit == CACHE_REDIRECTION_LIMIT)
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Cache Redirection limit "
                                                     "reached for %1")
                                                    .arg(fileUrl.toString()));
                return;
            }
            fileUrl.setUrl(redirectLoc);
            limit++;
        }

        m_infoLock->lock();
        QNetworkCacheMetaData urlData = m_manager->cache()->metaData(fileUrl);
        m_infoLock->unlock();
        if (getHeader(urlData, "Date").isNull())
        {
            QNetworkCacheMetaData::RawHeaderList headers = urlData.rawHeaders();
            QNetworkCacheMetaData::RawHeader newheader;
            QDateTime now = MythDate::current();
            newheader = QNetworkCacheMetaData::RawHeader("Date",
                                        now.toString(kDateFormat).toLatin1());
            headers.append(newheader);
            urlData.setRawHeaders(headers);
            m_infoLock->lock();
            m_manager->cache()->updateMetaData(urlData);
            m_infoLock->unlock();
        }
        // End HACK

        dlInfo->m_redirectedTo.clear();

        int dataSize = -1;

        // If we downloaded via the QNetworkAccessManager
        // AND the caller isn't handling the reply directly
        if (reply && dlInfo->m_processReply)
        {
            bool append = (!dlInfo->m_syncMode && dlInfo->m_caller);
            QByteArray data = reply->readAll();
            dataSize = data.size();

            if (append)
                dlInfo->m_bytesReceived += dataSize;
            else
                dlInfo->m_bytesReceived = dataSize;

            dlInfo->m_bytesTotal = dlInfo->m_bytesReceived;

            if (dlInfo->m_data)
            {
                if (append)
                    dlInfo->m_data->append(data);
                else
                    *dlInfo->m_data = data;
            }
            else if (!dlInfo->m_outFile.isEmpty())
            {
                saveFile(dlInfo->m_outFile, data, append);
            }
        }
        else if (!reply)  // If we downloaded via RemoteFile
        {
            if (dlInfo->m_data)
            {
                (*dlInfo->m_data) = dlInfo->m_privData;
            }
            else if (!dlInfo->m_outFile.isEmpty())
            {
                saveFile(dlInfo->m_outFile, dlInfo->m_privData);
            }
            dlInfo->m_bytesReceived += dataSize;
            dlInfo->m_bytesTotal = dlInfo->m_bytesReceived;
        }
        // else we downloaded via QNetworkAccessManager
        // AND the caller is handling the reply

        m_infoLock->lock();
        if (!m_downloadInfos.remove(dlInfo->m_url))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("ERROR download finished but failed to remove url: %1")
                        .arg(dlInfo->m_url));
        }

        if (reply)
            m_downloadReplies.remove(reply);
        m_infoLock->unlock();

        dlInfo->SetDone(true);

        if (!dlInfo->m_syncMode)
        {
            if (dlInfo->m_caller)
            {
                LOG(VB_FILE, LOG_DEBUG, QString("downloadFinished(%1): "
                        "COMPLETE: %2, sending event to caller")
                        .arg((long long)dlInfo).arg(dlInfo->m_url));

                QStringList args;
                args << dlInfo->m_url;
                args << dlInfo->m_outFile;
                args << QString::number(dlInfo->m_bytesTotal);
                // placeholder for error string
                args << (reply ? reply->errorString() : QString());
                args << QString::number((int)(reply ? reply->error() :
                                        dlInfo->m_errorCode));

                QCoreApplication::postEvent(dlInfo->m_caller,
                    new MythEvent("DOWNLOAD_FILE FINISHED", args));
            }

            delete dlInfo;
        }

        m_queueWaitCond.wakeAll();
    }
}

/** \brief Slot to process download update events.
 *  \param bytesReceived Bytes received so far
 *  \param bytesTotal    Bytes total for the download, -1 if the total is
 *                       unknown
 */
void MythDownloadManager::downloadProgress(qint64 bytesReceived,
                                           qint64 bytesTotal)
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr)
        return;

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("downloadProgress(%1, %2) (for reply %3)")
            .arg(bytesReceived).arg(bytesTotal).arg((long long)reply));

    QMutexLocker locker(m_infoLock);
    if (!m_downloadReplies.contains(reply))
        return;

    MythDownloadInfo *dlInfo = m_downloadReplies[reply];

    if (!dlInfo)
        return;

    dlInfo->m_lastStat = MythDate::current();

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("downloadProgress: %1 to %2 is at %3 of %4 bytes downloaded")
            .arg(dlInfo->m_url, dlInfo->m_outFile)
            .arg(bytesReceived).arg(bytesTotal));

    if (!dlInfo->m_syncMode && dlInfo->m_caller)
    {
        LOG(VB_FILE, LOG_DEBUG, QString("downloadProgress(%1): "
                "sending event to caller")
                .arg(reply->url().toString()));

        bool appendToFile = (dlInfo->m_bytesReceived != 0);
        QByteArray data = reply->readAll();
        if (!dlInfo->m_outFile.isEmpty())
            saveFile(dlInfo->m_outFile, data, appendToFile);

        if (dlInfo->m_data)
            dlInfo->m_data->append(data);

        dlInfo->m_bytesReceived = bytesReceived;
        dlInfo->m_bytesTotal = bytesTotal;

        QStringList args;
        args << dlInfo->m_url;
        args << dlInfo->m_outFile;
        args << QString::number(bytesReceived);
        args << QString::number(bytesTotal);

        QCoreApplication::postEvent(dlInfo->m_caller,
            new MythEvent("DOWNLOAD_FILE UPDATE", args));
    }
}

/** \brief Saves a QByteArray of data to a given filename.  Any parent
 *         directories are created automatically.
 *  \param outFile Filename to save to.
 *  \param data    Data to save.
 *  \param append  Append data to output file instead of overwriting.
 *  \return true if successful, false otherwise
 */
bool MythDownloadManager::saveFile(const QString &outFile,
                                   const QByteArray &data,
                                   const bool append)
{
    if (outFile.isEmpty() || data.isEmpty())
        return false;

    QFile file(outFile);
    QFileInfo fileInfo(outFile);
    QDir qdir(fileInfo.absolutePath());

    if (!qdir.exists() && !qdir.mkpath(fileInfo.absolutePath()))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to create: '%1'")
                .arg(fileInfo.absolutePath()));
        return false;
    }

    QIODevice::OpenMode mode = QIODevice::Unbuffered|QIODevice::WriteOnly;
    if (append)
        mode |= QIODevice::Append;

    if (!file.open(mode))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open: '%1'") .arg(outFile));
        return false;
    }

    off_t offset = 0;
    size_t remaining = data.size();
    uint failure_cnt = 0;
    while ((remaining > 0) && (failure_cnt < 5))
    {
        ssize_t written = file.write(data.data() + offset, remaining);
        if (written < 0)
        {
            failure_cnt++;
            usleep(50ms);
            continue;
        }

        failure_cnt  = 0;
        offset      += written;
        remaining   -= written;
    }

    return remaining <= 0;
}

/** \brief Gets the Last Modified timestamp for a URI
 *  \param url    URI to test.
 *  \return Timestamp the URI was last modified or now if an error occurred
 */
QDateTime MythDownloadManager::GetLastModified(const QString &url)
{
    // If the header has not expired and
    // the last modification date is less than 1 hours old or if
    // the cache object is less than 20 minutes old,
    // then use the cached header otherwise redownload the header

    static const QString kDateFormat = "ddd, dd MMM yyyy hh:mm:ss 'GMT'";
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetLastModified('%1')").arg(url));
    QDateTime result;

    QDateTime now = MythDate::current();

    QUrl cacheUrl = QUrl(url);

    // Deal with redirects, we want the cached data for the final url
    QString redirectLoc;
    int limit = 0;
    while (!(redirectLoc = getHeader(cacheUrl, "Location")).isNull())
    {
        if (limit == CACHE_REDIRECTION_LIMIT)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Cache Redirection limit "
                                                    "reached for %1")
                                                .arg(cacheUrl.toString()));
            return result;
        }
        cacheUrl.setUrl(redirectLoc);
        limit++;
    }

    m_infoLock->lock();
    QNetworkCacheMetaData urlData = m_manager->cache()->metaData(cacheUrl);
    m_infoLock->unlock();

    if (urlData.isValid() &&
        ((!urlData.expirationDate().isValid()) ||
         (urlData.expirationDate().secsTo(now) < 0)))
    {
        if (urlData.lastModified().toUTC().secsTo(now) <= 3600) // 1 Hour
        {
            result = urlData.lastModified().toUTC();
        }
        else
        {
            QString date = getHeader(urlData, "Date");
            if (!date.isNull())
            {
                QDateTime loadDate =
                    MythDate::fromString(date, kDateFormat);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
                loadDate.setTimeSpec(Qt::UTC);
#else
                loadDate.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
                if (loadDate.secsTo(now) <= 1200) // 20 Minutes
                {
                    result = urlData.lastModified().toUTC();
                }
            }
        }
    }

    if (!result.isValid())
    {
        auto *dlInfo = new MythDownloadInfo;
        dlInfo->m_url      = url;
        dlInfo->m_syncMode = true;
        // Head request, we only want to inspect the headers
        dlInfo->m_requestType = kRequestHead;

        if (downloadNow(dlInfo, false))
        {
            if (dlInfo->m_reply)
            {
                QVariant lastMod =
                    dlInfo->m_reply->header(
                        QNetworkRequest::LastModifiedHeader);
                if (lastMod.isValid())
                    result = lastMod.toDateTime().toUTC();
            }

            // downloadNow() will set a flag to trigger downloadFinished()
            // to delete the dlInfo if the download times out
            delete dlInfo;
        }
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetLastModified('%1'): Result %2")
                                            .arg(url, result.toString()));

    return result;
}


/** \brief Loads the cookie jar from a cookie file
 *  \param filename Filename of the cookie file to read.
 */
void MythDownloadManager::loadCookieJar(const QString &filename)
{
    QMutexLocker locker(&m_cookieLock);

    auto *jar = new MythCookieJar;
    jar->load(filename);
    m_manager->setCookieJar(jar);
}

/** \brief Saves the cookie jar to a cookie file
 *  \param filename Filename of the cookie file to write.
 */
void MythDownloadManager::saveCookieJar(const QString &filename)
{
    QMutexLocker locker(&m_cookieLock);

    if (!m_manager->cookieJar())
        return;

    auto *jar = qobject_cast<MythCookieJar *>(m_manager->cookieJar());
    if (jar == nullptr)
        return;
    jar->save(filename);
}

void MythDownloadManager::setCookieJar(QNetworkCookieJar *cookieJar)
{
    QMutexLocker locker(&m_cookieLock);
    m_manager->setCookieJar(cookieJar);
}

/** \brief Copy from one cookie jar to another
 *  \return new copy of the cookie jar
 */
QNetworkCookieJar *MythDownloadManager::copyCookieJar(void)
{
    QMutexLocker locker(&m_cookieLock);

    if (!m_manager->cookieJar())
        return nullptr;

    auto *inJar = qobject_cast<MythCookieJar *>(m_manager->cookieJar());
    if (inJar == nullptr)
        return nullptr;
    auto *outJar = new MythCookieJar;
    outJar->copyAllCookies(*inJar);

    return outJar;
}

/** \brief Refresh the temporary cookie jar from another cookie jar
 *  \param jar other cookie jar to update from
 */
void MythDownloadManager::refreshCookieJar(QNetworkCookieJar *jar)
{
    QMutexLocker locker(&m_cookieLock);
    delete m_inCookieJar;

    auto *inJar = qobject_cast<MythCookieJar *>(jar);
    if (inJar == nullptr)
        return;

    auto *outJar = new MythCookieJar;
    outJar->copyAllCookies(*inJar);
    m_inCookieJar = outJar;

    QMutexLocker locker2(&m_queueWaitLock);
    m_queueWaitCond.wakeAll();
}

/** \brief Update the cookie jar from the temporary cookie jar
 */
void MythDownloadManager::updateCookieJar(void)
{
    QMutexLocker locker(&m_cookieLock);

    auto *inJar = qobject_cast<MythCookieJar *>(m_inCookieJar);
    if (inJar != nullptr)
    {
        auto *outJar = new MythCookieJar;
        outJar->copyAllCookies(*inJar);
        m_manager->setCookieJar(outJar);
    }

    delete m_inCookieJar;
    m_inCookieJar = nullptr;
}

QString MythDownloadManager::getHeader(const QUrl& url, const QString& header)
{
    if (!m_manager || !m_manager->cache())
        return {};

    m_infoLock->lock();
    QNetworkCacheMetaData metadata = m_manager->cache()->metaData(url);
    m_infoLock->unlock();

    return getHeader(metadata, header);
}

/** \brief Gets the value of an HTTP header from the cache
 *  \param cacheData The cache data to search through
 *  \param header Which HTTP header to get the value of
 *  \return a QString containing the value of the HTTP header
 */
QString MythDownloadManager::getHeader(const QNetworkCacheMetaData &cacheData,
                                       const QString& header)
{
    auto headers = cacheData.rawHeaders();
    for (const auto& rh : std::as_const(headers))
        if (QString(rh.first) == header)
            return {rh.second};
    return {};
}


/** \brief Copies all cookies from one MythCookieJar to another
 *  \param old the MythCookieJar to copy
 */
void MythCookieJar::copyAllCookies(MythCookieJar &old)
{
    const QList<QNetworkCookie> cookieList = old.allCookies();
    setAllCookies(cookieList);
}

/** \brief Loads the cookie jar from a cookie file
 *  \param filename Filename of the cookie file to read.
 */
void MythCookieJar::load(const QString &filename)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("MythCookieJar: loading cookies from: %1").arg(filename));

    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("MythCookieJar::load() failed to open file for reading: %1").arg(filename));
        return;
    }

    QList<QNetworkCookie> cookieList;
    QTextStream stream(&f);
    while (!stream.atEnd())
    {
        QString cookie = stream.readLine();
        cookieList << QNetworkCookie::parseCookies(cookie.toLocal8Bit());
    }

    setAllCookies(cookieList);
}

/** \brief Saves the cookie jar to a cookie file
 *  \param filename Filename of the cookie file to write.
 */
void MythCookieJar::save(const QString &filename)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("MythCookieJar: saving cookies to: %1").arg(filename));

    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythCookieJar::save() failed to open file for writing: %1").arg(filename));
        return;
    }

    QList<QNetworkCookie> cookieList = allCookies();
    QTextStream stream(&f);

    for (const auto& cookie : std::as_const(cookieList))
        stream << cookie.toRawForm() << Qt::endl;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
