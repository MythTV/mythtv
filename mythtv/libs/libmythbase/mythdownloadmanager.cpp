// qt
#include <QCoreApplication>
#include <QRunnable>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QNetworkCookieJar>
#include <QAuthenticator>
#include <QTextStream>
#include <QNetworkProxy>
#include <QMutexLocker>

#include "stdlib.h"

// libmythbase
#include "compat.h"
#include "mythcorecontext.h"
#include "mythcoreutil.h"
#include "mthreadpool.h"
#include "mythdirs.h"
#include "mythevent.h"
#include "mythversion.h"
#include "remotefile.h"
#include "mythdate.h"

#include "mythdownloadmanager.h"
#include "mythlogging.h"
#include <QUrl>

using namespace std;

#define LOC      QString("DownloadManager: ")
#define CACHE_REDIRECTION_LIMIT     10

MythDownloadManager *downloadManager = NULL;
QMutex               dmCreateLock;

/**
* \class MythDownloadInfo
*/
class MythDownloadInfo
{
  public:
    MythDownloadInfo() :
        m_request(NULL),         m_reply(NULL),       m_data(NULL),
        m_caller(NULL),          m_requestType(kRequestGet),
        m_reload(false),         m_preferCache(false), m_syncMode(false),
        m_processReply(true),    m_done(false),        m_bytesReceived(0),
        m_bytesTotal(0),         m_lastStat(MythDate::current()),
        m_authCallback(NULL),    m_authArg(NULL),
        m_headers(NULL),         m_errorCode(QNetworkReply::NoError)
    {
        qRegisterMetaType<QNetworkReply::NetworkError>("QNetworkReply::NetworkError");
    }

   ~MythDownloadInfo()
    {
        if (m_request)
            delete m_request;
        if (m_reply && m_processReply)
            m_reply->deleteLater();
    }

    void detach(void)
    {
        m_url.detach();
        m_outFile.detach();
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
    QNetworkRequest *m_request;
    QNetworkReply   *m_reply;
    QString          m_outFile;
    QByteArray      *m_data;
    QByteArray       m_privData;
    QObject         *m_caller;
    MRequestType     m_requestType;
    bool             m_reload;
    bool             m_preferCache;
    bool             m_syncMode;
    bool             m_processReply;
    bool             m_done;
    qint64           m_bytesReceived;
    qint64           m_bytesTotal;
    QDateTime        m_lastStat;
    AuthCallback     m_authCallback;
    void            *m_authArg;
    const QHash<QByteArray, QByteArray> *m_headers;

    QNetworkReply::NetworkError m_errorCode;
    QMutex           m_lock;
};


/** \brief A subclassed QNetworkCookieJar that allows for reading and writing
 *         cookie files that contain raw formatted cookies and copying the
 *         cookie jar to share between threads.
 */
class MythCookieJar : public QNetworkCookieJar
{
  public:
    MythCookieJar();
    MythCookieJar(MythCookieJar &old);
    void load(const QString &filename);
    void save(const QString &filename);
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
        m_dlInfo(dlInfo)
    {
        m_dlInfo->detach();
    }

    void run()
    {
        bool ok = false;

        RemoteFile *rf = new RemoteFile(m_dlInfo->m_url, false, false, 0);
        ok = rf->SaveAs(m_dlInfo->m_privData);
        delete rf;

        if (!ok)
            m_dlInfo->m_errorCode = QNetworkReply::UnknownNetworkError;

        m_dlInfo->m_bytesReceived = m_dlInfo->m_privData.size();
        m_dlInfo->m_bytesTotal = m_dlInfo->m_bytesReceived;

        m_parent->downloadFinished(m_dlInfo);
    }

  private:
    MythDownloadManager *m_parent;
    MythDownloadInfo    *m_dlInfo;
};

/** \brief Deletes the running MythDownloadManager at program exit.
 */
void ShutdownMythDownloadManager(void)
{
    if (downloadManager)
    {
        delete downloadManager;
        downloadManager = NULL;
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

    MythDownloadManager *tmpDLM = new MythDownloadManager();
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

/** \brief Constructor for MythDownloadManager.  Instantiates a
 *         QNetworkAccessManager and QNetworkDiskCache.
 */
MythDownloadManager::MythDownloadManager() :
    MThread("DownloadManager"),
    m_manager(NULL),
    m_diskCache(NULL),
    m_proxy(NULL),
    m_infoLock(new QMutex(QMutex::Recursive)),
    m_queueThread(NULL),
    m_runThread(false),
    m_isRunning(false),
    m_inCookieJar(NULL)
{
}

/** \brief Destructor for MythDownloadManager.
 */
MythDownloadManager::~MythDownloadManager()
{
    m_runThread = false;
    m_queueWaitCond.wakeAll();

    wait();

    delete m_infoLock;

    if (m_inCookieJar)
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
    bool waitAnyway = false;

    m_queueThread = QThread::currentThread();

    while (!m_runThread)
        usleep(50000);

    m_manager = new QNetworkAccessManager(this);
    m_diskCache = new QNetworkDiskCache(this);
    m_proxy = new QNetworkProxy();
    m_diskCache->setCacheDirectory(GetConfDir() + "/Cache-" +
                                   QCoreApplication::applicationName() + "-" +
                                   gCoreContext->GetHostName());
    m_manager->setCache(m_diskCache);

    // Set the proxy for the manager to be the application default proxy,
    // which has already been setup
    m_manager->setProxy(*m_proxy);

    // make sure the cookieJar is created in the same thread as the manager
    // and set its parent to NULL so it can be shared between managers
    m_manager->cookieJar()->setParent(NULL);

    QObject::connect(m_manager, SIGNAL(finished(QNetworkReply*)), this,
                       SLOT(downloadFinished(QNetworkReply*)));

    m_isRunning = true;
    while (m_runThread)
    {
        if (m_inCookieJar)
        {
            LOG(VB_GENERAL, LOG_DEBUG, "Updating DLManager's Cookie Jar");
            updateCookieJar();
        }
        m_infoLock->lock();
        downloading = !m_downloadInfos.isEmpty();
        itemsInQueue = !m_downloadQueue.isEmpty();
        m_infoLock->unlock();

        if (downloading)
            QCoreApplication::processEvents();

        if (!itemsInQueue || waitAnyway)
        {
            waitAnyway = false;
            m_queueWaitLock.lock();

            if (downloading)
                m_queueWaitCond.wait(&m_queueWaitLock, 200);
            else
                m_queueWaitCond.wait(&m_queueWaitLock);

            m_queueWaitLock.unlock();
        }

        m_infoLock->lock();
        if (!m_downloadQueue.isEmpty())
        {
            MythDownloadInfo *dlInfo = m_downloadQueue.front();

            m_downloadQueue.pop_front();

            if (!dlInfo)
                continue;

            QUrl qurl(dlInfo->m_url);
            if (m_downloadInfos.contains(qurl.toString()))
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

            m_downloadInfos[qurl.toString()] = dlInfo;
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
    MythDownloadInfo *dlInfo = new MythDownloadInfo;

    dlInfo->m_url     = url;
    dlInfo->m_request = req;
    dlInfo->m_outFile = dest;
    dlInfo->m_data    = data;
    dlInfo->m_caller  = caller;
    dlInfo->m_requestType = reqType;
    dlInfo->m_reload  = reload;

    dlInfo->detach();

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
                                      AuthCallback authCallback, void *authArg,
                                   const QHash<QByteArray, QByteArray> *headers)
{
    MythDownloadInfo *dlInfo = new MythDownloadInfo;

    dlInfo->m_url      = url;
    dlInfo->m_request  = req;
    dlInfo->m_outFile  = dest;
    dlInfo->m_data     = data;
    dlInfo->m_requestType = reqType;
    dlInfo->m_reload   = reload;
    dlInfo->m_syncMode = true;
    dlInfo->m_authCallback = authCallback;
    dlInfo->m_authArg  = authArg;
    dlInfo->m_headers  = headers;

    return downloadNow(dlInfo);
}

/** \brief Downloads a URL but doesn't store the resulting data anywhere
 *  \param url      URI to download.
 */
void MythDownloadManager::preCache(const QString &url)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("preCache('%1')").arg(url));
    queueItem(url, NULL, QString(), NULL, NULL);
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
            .arg(url).arg(dest).arg((long long)caller));

    queueItem(url, NULL, dest, NULL, caller, kRequestGet, reload);
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

    queueItem(req->url().toString(), req, QString(), data, caller);
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
    return processItem(url, NULL, dest, NULL, kRequestGet, reload);
}

/** \brief Downloads a URI to a QByteArray in blocking mode.
 *  \param url     URI to download.
 *  \param data    Pointer to destination QByteArray.
 *  \param reload  Whether to force reloading of the URL or not
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::download(const QString &url, QByteArray *data,
                                   const bool reload)
{
    return processItem(url, NULL, QString(), data, kRequestGet, reload);
}

/** \brief Downloads a URI to a QByteArray in blocking mode.
 *  \param url      URI to download.
 *  \param reload   Whether to force reloading of the URL or not
 *  \return pointer to the QNetworkReply containing the download response,
 *                  NULL if an error
 */
QNetworkReply *MythDownloadManager::download(const QString &url,
                                             const bool reload)
{
    MythDownloadInfo *dlInfo = new MythDownloadInfo;

    dlInfo->m_url          = url;
    dlInfo->m_reload       = reload;
    dlInfo->m_syncMode     = true;
    dlInfo->m_processReply = false;

    bool ok = downloadNow(dlInfo, false);

    QNetworkReply *reply = dlInfo->m_reply;

    if (reply)
        dlInfo->m_reply = NULL;

    delete dlInfo;
    dlInfo = NULL;

    if (ok && reply)
        return reply;

    return NULL;
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
    return processItem(req->url().toString(), req, QString(), data);
}

/** \brief Downloads a URL to a file in blocking mode.
 *  \param url     URI to download.
 *  \param dest    Destination filename.
 *  \param reload  Whether to force reloading of the URL or not
 *  \param authCallback AuthCallback function for use with authentication
 *  \param authArg Opaque argument for callback function
 *  \param headers Hash of optional HTTP header to add to the request
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::downloadAuth(const QString &url, const QString &dest,
               const bool reload, AuthCallback authCallback, void *authArg,
               const QHash<QByteArray, QByteArray> *headers)
{
    return processItem(url, NULL, dest, NULL, kRequestGet, reload, authCallback,
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

    queueItem(url, NULL, QString(), data, caller, kRequestPost);
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
              kRequestPost);
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

    return processItem(url, NULL, QString(), data, kRequestPost);
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
                       kRequestPost);
}

/** \brief Posts data to a url via the QNetworkAccessManager
 *  \param url      URL to post to
 *  \param data     Location holding post and response data
 *  \param authCallback AuthCallback function for authentication
 *  \param authArg Opaque argument for callback function
 *  \param headers Hash of optional HTTP headers to add to the request
 *  \return true if post was successful, false otherwise.
 */
bool MythDownloadManager::postAuth(const QString &url, QByteArray *data,
                                   AuthCallback authCallback, void *authArg,
                                   const QHash<QByteArray, QByteArray> *headers)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("postAuth('%1', '%2')")
            .arg(url).arg((long long)data));

    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "postAuth(), data is NULL!");
        return false;
    }

    return processItem(url, NULL, NULL, data, kRequestPost, false, authCallback,
                       authArg, headers);
}

/** \brief Triggers a myth:// URI download in the background via RemoteFile
 *  \param dlInfo   MythDownloadInfo information for download
 */
void MythDownloadManager::downloadRemoteFile(MythDownloadInfo *dlInfo)
{
    RemoteFileDownloadThread *dlThread =
        new RemoteFileDownloadThread(this, dlInfo);
    MThreadPool::globalInstance()->start(dlThread, "RemoteFileDownload");
}

/** \brief Downloads a QNetworkRequest via the QNetworkAccessManager
 *  \param dlInfo   MythDownloadInfo information for download
 */
void MythDownloadManager::downloadQNetworkRequest(MythDownloadInfo *dlInfo)
{
    if (!dlInfo)
        return;

    static const char dateFormat[] = "ddd, dd MMM yyyy hh:mm:ss 'GMT'";
    QUrl qurl(dlInfo->m_url);
    QNetworkRequest request;

    if (dlInfo->m_request)
    {
        request = *dlInfo->m_request;
        delete dlInfo->m_request;
        dlInfo->m_request = NULL;
    }
    else
        request.setUrl(qurl);

    if (!dlInfo->m_reload)
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
             (QDateTime(urlData.expirationDate().toUTC()).secsTo(now) < 10)))
        {
            QString dateString = getHeader(urlData, "Date");

            if (!dateString.isNull())
            {
                QDateTime loadDate =
                    MythDate::fromString(dateString, dateFormat);
                loadDate.setTimeSpec(Qt::UTC);
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

    request.setRawHeader("User-Agent",
                         "MythTV v" MYTH_BINARY_VERSION " MythDownloadManager");

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
        connect(m_manager, SIGNAL(authenticationRequired(QNetworkReply *,
                                                         QAuthenticator *)),
                this, SLOT(authCallback(QNetworkReply *, QAuthenticator *)));
    }

    connect(dlInfo->m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this,
            SLOT(downloadError(QNetworkReply::NetworkError)));
    connect(dlInfo->m_reply, SIGNAL(downloadProgress(qint64, qint64)),
            this, SLOT(downloadProgress(qint64, qint64)));
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

    m_infoLock->lock();
    m_downloadQueue.push_back(dlInfo);
    m_infoLock->unlock();
    m_queueWaitCond.wakeAll();

    // timeout myth:// RemoteFile transfers 20 seconds from now
    // timeout non-myth:// QNetworkAccessManager transfers 10 seconds after
    //    their last progress update
    QDateTime startedAt = MythDate::current();
    m_infoLock->lock();
    while ((!dlInfo->IsDone()) &&
           (dlInfo->m_errorCode == QNetworkReply::NoError) &&
           (((!dlInfo->m_url.startsWith("myth://")) &&
             (dlInfo->m_lastStat.secsTo(MythDate::current()) < 10)) ||
            ((dlInfo->m_url.startsWith("myth://")) &&
             (startedAt.secsTo(MythDate::current()) < 20))))
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
        dlInfo->m_data = NULL;      // Prevent downloadFinished() from updating
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
        dlInfo = NULL;
    }

    m_infoLock->unlock();

    return success;
}

/** \brief Cancel a queued or current download.
 *  \param url for download to cancel
 */
void MythDownloadManager::cancelDownload(const QString &url)
{
    QMutexLocker locker(m_infoLock);
    MythDownloadInfo *dlInfo;

    QMutableListIterator<MythDownloadInfo*> lit(m_downloadQueue);
    while (lit.hasNext())
    {
        lit.next();
        dlInfo = lit.value();
        if (dlInfo->m_url == url)
        {
            // this shouldn't happen
            if (dlInfo->m_reply)
            {
                LOG(VB_FILE, LOG_DEBUG, 
                    LOC + QString("Aborting download - user request"));
                dlInfo->m_reply->abort();
            }
            lit.remove();
            dlInfo->m_lock.lock();
            dlInfo->m_errorCode = QNetworkReply::OperationCanceledError;
            dlInfo->m_done = true;
            dlInfo->m_lock.unlock();
        }
    }

    if (m_downloadInfos.contains(url))
    {
        dlInfo = m_downloadInfos[url];
        if (dlInfo->m_reply)
        {
            LOG(VB_FILE, LOG_DEBUG, 
                LOC + QString("Aborting download - user request"));
            m_downloadReplies.remove(dlInfo->m_reply);
            dlInfo->m_reply->abort();
        }
        m_downloadInfos.remove(url);
        dlInfo->m_lock.lock();
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
    MythDownloadInfo *dlInfo;

    QList <MythDownloadInfo*>::iterator lit = m_downloadQueue.begin();
    for (; lit != m_downloadQueue.end(); ++lit)
    {
        dlInfo = *lit;
        if (dlInfo->m_caller == caller)
        {
            dlInfo->m_caller  = NULL;
            dlInfo->m_outFile = QString();
            dlInfo->m_data    = NULL;
        }
    }

    QMap <QString, MythDownloadInfo*>::iterator mit = m_downloadInfos.begin();
    for (; mit != m_downloadInfos.end(); ++mit)
    {
        dlInfo = mit.value();
        if (dlInfo->m_caller == caller)
        {
            dlInfo->m_caller  = NULL;
            dlInfo->m_outFile = QString();
            dlInfo->m_data    = NULL;
        }
    }
}

/** \brief Slot to process download error events.
 *  \param errorCode  error code
 */
void MythDownloadManager::downloadError(QNetworkReply::NetworkError errorCode)
{
    QNetworkReply *reply = (QNetworkReply*)sender();

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
                                      const QUrl& oldRedirectUrl) const
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

    static const char dateFormat[] = "ddd, dd MMM yyyy hh:mm:ss 'GMT'";
    QNetworkReply *reply = dlInfo->m_reply;

    if (reply)
    {
        QUrl possibleRedirectUrl =
             reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

        dlInfo->m_redirectedTo =
             redirectUrl(possibleRedirectUrl, dlInfo->m_redirectedTo);
    }

    if(!dlInfo->m_redirectedTo.isEmpty())
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("downloadFinished(%1): Redirect: %2 -> %3")
                .arg((long long)dlInfo)
                .arg(reply->url().toString())
                .arg(dlInfo->m_redirectedTo.toString()));

        QNetworkRequest request(dlInfo->m_redirectedTo);
        if (dlInfo->m_preferCache)
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                                 QNetworkRequest::PreferCache);
        request.setRawHeader("User-Agent",
                             "MythDownloadManager v" MYTH_BINARY_VERSION);

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

        connect(dlInfo->m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(downloadError(QNetworkReply::NetworkError)));
        connect(dlInfo->m_reply, SIGNAL(downloadProgress(qint64, qint64)),
                this, SLOT(downloadProgress(qint64, qint64)));

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
                                        now.toString(dateFormat).toAscii());
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

        m_downloadInfos.remove(dlInfo->m_url);
        if (reply)
            m_downloadReplies.remove(reply);

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
            dlInfo = NULL;
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
    QNetworkReply *reply = (QNetworkReply*)sender();

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
            .arg(dlInfo->m_url).arg(dlInfo->m_outFile)
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
    if (outFile.isEmpty() || !data.size())
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
            usleep(50000);
            continue;
        }

        failure_cnt  = 0;
        offset      += written;
        remaining   -= written;
    }

    if (remaining > 0)
        return false;

    return true;
}

/** \brief Gets the Last Modified timestamp for a URI
 *  \param url    URI to test.
 *  \return Timestamp the URI was last modified or now if an error occurred
 */
QDateTime MythDownloadManager::GetLastModified(const QString &url)
{
    // If the header has not expired and
    // the last modification date is less than 30 minutes old or if
    // the cache object is less than 5 minutes old,
    // then use the cached header otherwise redownload the header

    static const char dateFormat[] = "ddd, dd MMM yyyy hh:mm:ss 'GMT'";
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
        if (QDateTime(urlData.lastModified().toUTC()).secsTo(now) <= 1800)
        {
            result = urlData.lastModified().toUTC();
        }
        else
        {
            QString date = getHeader(urlData, "Date");
            if (!date.isNull())
            {
                QDateTime loadDate =
                    MythDate::fromString(date, dateFormat);
                loadDate.setTimeSpec(Qt::UTC);
                if (loadDate.secsTo(now) <= 720)
                {
                    result = urlData.lastModified().toUTC();
                }
            }
        }
    }

    if (!result.isValid())
    {
        MythDownloadInfo *dlInfo = new MythDownloadInfo;
        dlInfo->m_url      = url;
        dlInfo->m_syncMode = true;
        // Head request, we only want to inspect the headers
        dlInfo->m_requestType = kRequestHead;

        if (downloadNow(dlInfo, false) && dlInfo->m_reply)
        {
            QVariant lastMod =
                dlInfo->m_reply->header(QNetworkRequest::LastModifiedHeader);
            if (lastMod.isValid())
                result = lastMod.toDateTime().toUTC();
        }

        delete dlInfo;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetLastModified('%1'): Result %2")
                                            .arg(url).arg(result.toString()));

    return result;
}


/** \brief Loads the cookie jar from a cookie file
 *  \param filename Filename of the cookie file to read.
 */
void MythDownloadManager::loadCookieJar(const QString &filename)
{
    QMutexLocker locker(&m_cookieLock);

    MythCookieJar *jar = new MythCookieJar;
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

    MythCookieJar *jar = static_cast<MythCookieJar *>(m_manager->cookieJar());
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
        return NULL;

    MythCookieJar *inJar = static_cast<MythCookieJar *>(m_manager->cookieJar());
    MythCookieJar *outJar = new MythCookieJar(*inJar);

    return static_cast<QNetworkCookieJar *>(outJar);
}

/** \brief Refresh the temporary cookie jar from another cookie jar
 *  \param jar other cookie jar to update from
 */
void MythDownloadManager::refreshCookieJar(QNetworkCookieJar *jar)
{
    QMutexLocker locker(&m_cookieLock);
    if (m_inCookieJar)
        delete m_inCookieJar;

    MythCookieJar *inJar = static_cast<MythCookieJar *>(jar);
    MythCookieJar *outJar = new MythCookieJar(*inJar);
    m_inCookieJar = static_cast<QNetworkCookieJar *>(outJar);

    QMutexLocker locker2(&m_queueWaitLock);
    m_queueWaitCond.wakeAll();
}

/** \brief Update the cookie jar from the temporary cookie jar
 */
void MythDownloadManager::updateCookieJar(void)
{
    QMutexLocker locker(&m_cookieLock);

    MythCookieJar *inJar = static_cast<MythCookieJar *>(m_inCookieJar);
    MythCookieJar *outJar = new MythCookieJar(*inJar);
    m_manager->setCookieJar(static_cast<QNetworkCookieJar *>(outJar));

    delete m_inCookieJar;
    m_inCookieJar = NULL;
}

QString MythDownloadManager::getHeader(const QUrl& url, const QString& header)
{
    if (!m_manager || !m_manager->cache())
        return QString::null;

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
    QNetworkCacheMetaData::RawHeaderList headers = cacheData.rawHeaders();
    bool found = false;
    QNetworkCacheMetaData::RawHeaderList::iterator it = headers.begin();
    for (; !found && it != headers.end(); ++it)
    {
        if (QString((*it).first) == header)
        {
            found = true;
            return QString((*it).second);
        }
    }

    return QString::null;
}


/** \brief Creates a MythCookieJar from another MythCookieJar
 *  \param old the MythCookieJar to copy
 */
MythCookieJar::MythCookieJar(MythCookieJar &old)
{
    const QList<QNetworkCookie> cookieList = old.allCookies();
    setAllCookies(cookieList);
}

/** \brief Creates an empty MythCookieJar
 */
MythCookieJar::MythCookieJar()
{
}

/** \brief Loads the cookie jar from a cookie file
 *  \param filename Filename of the cookie file to read.
 */
void MythCookieJar::load(const QString &filename)
{
    QList<QNetworkCookie> cookieList;
    QTextStream stream((QString *)&filename, QIODevice::ReadOnly);
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
    QList<QNetworkCookie> cookieList = allCookies();
    QTextStream stream((QString *)&filename, QIODevice::WriteOnly);

    for (QList<QNetworkCookie>::iterator it = cookieList.begin();
         it != cookieList.end(); ++it)
    {
        stream << (*it).toRawForm() << endl;
    }
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */

