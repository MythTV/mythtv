// qt
#include <QCoreApplication>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QThreadPool>

// libmythbase
#include "compat.h"
#include "mythcorecontext.h"
#include "mythcoreutil.h"
#include "mythdirs.h"
#include "mythevent.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "remotefile.h"

#include "mythdownloadmanager.h"

using namespace std;

#define LOC      QString("DownloadManager: ")
#define LOC_WARN QString("DownloadManager, Warning: ")
#define LOC_ERR  QString("DownloadManager, Error: ")

MythDownloadManager *downloadManager = NULL;

/*!
* \class MythDownloadInfo
*/
class MythDownloadInfo
{
  public:
    MythDownloadInfo() :
        m_request(NULL),         m_reply(NULL),       m_data(NULL),
        m_caller(NULL),          m_post(false),       m_reload(false),
        m_preferCache(false),    m_syncMode(false),   m_processReply(true),
        m_done(false),           m_bytesReceived(0),  m_bytesTotal(0),
        m_lastStat(QDateTime::currentDateTime()),
        m_errorCode(QNetworkReply::NoError)
    {
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
    bool             m_processReply;
    bool             m_done;
    qint64           m_bytesReceived;
    qint64           m_bytesTotal;
    QDateTime        m_lastStat;

    QNetworkReply::NetworkError m_errorCode;
};

/*!
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

        m_parent->downloadFinished(m_dlInfo);
    }

  private:
    MythDownloadManager *m_parent;
    MythDownloadInfo    *m_dlInfo;
};

/** \fn ShutdownMythDownloadManager(void)
 *  \brief Deletes the running MythDownloadManager at program exit.
 */
void ShutdownMythDownloadManager(void)
{
    if (downloadManager)
    {
        delete downloadManager;
        downloadManager = NULL;
    }
}

/** \fn GetMythDownloadManger(void)
 *  \brief Gets the pointer to the MythDownloadManager singleton.
 *  \return Pointer to the MythDownloadManager instance
 */
MythDownloadManager *GetMythDownloadManager(void)
{
    if (!downloadManager)
    {
        downloadManager = new MythDownloadManager();
        downloadManager->start();
        while (!downloadManager->getQueueThread())
            usleep(10000);

        downloadManager->moveToThread(downloadManager->getQueueThread());
        downloadManager->setRunThread();

        while (!downloadManager->isRunning())
            usleep(10000);

        atexit(ShutdownMythDownloadManager);
    }

    return downloadManager;
}

/** \fn MythDownloadManager::MythDownloadManager()
 *  \brief Constructor for MythDownloadManager.  Instantiates a
 *         QNetworkAccessManager and QNetworkDiskCache.
 */
MythDownloadManager::MythDownloadManager() :
    m_manager(NULL),
    m_diskCache(NULL),
    m_infoLock(new QMutex(QMutex::Recursive)),
    m_queueThread(NULL),
    m_runThread(false),
    m_isRunning(false)
{
}

/** \fn MythDownloadManager::~MythDownloadManager()
 *  \brief Destructor for MythDownloadManager.
 */
MythDownloadManager::~MythDownloadManager()
{
    m_runThread = false;
    m_queueWaitCond.wakeAll();

    wait();

    delete m_infoLock;
}

/** \fn MythDownloadManager::run(void)
 *  \brief Runs a loop to process incoming download requests and
 *         triggers download events to be processed.
 */
void MythDownloadManager::run(void)
{
    bool downloading = false;
    bool itemsInQueue = false;
    bool waitAnyway = false;

    m_queueThread = currentThread();

    while (!m_runThread)
        usleep(50000);

    m_manager = new QNetworkAccessManager(this);
    m_diskCache = new QNetworkDiskCache(this);
    m_diskCache->setCacheDirectory(GetConfDir() + "/Cache-" +
                                   gCoreContext->GetAppName() + "-" +
                                   gCoreContext->GetHostName());
    m_manager->setCache(m_diskCache);


    QObject::connect(m_manager, SIGNAL(finished(QNetworkReply*)), this,
                       SLOT(downloadFinished(QNetworkReply*)));

    m_isRunning = true;
    while (m_runThread)
    {
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
            QUrl qurl(dlInfo->m_url);
            m_downloadQueue.pop_front();
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
                downloadQNetworkRequest(dlInfo);

            m_downloadInfos[qurl.toString()] = dlInfo;
        }
        m_infoLock->unlock();
    }
    m_isRunning = false;
}

/** \fn MythDownloadManager::queueItem(const QString &url, QNetworkRequest *req,
                                       const QString &dest, QByteArray *data,
                                       QObject *caller, const bool post,
                                       const bool reload)
 *  \brief Adds a request to the download queue.
 *  \param url      URI to download.
 *  \param req      QNetworkRequest to queue
 *  \param dest     Destination filename.
 *  \param data     Location of data for request
 *  \param caller   QObject to receive event notifications.
 *  \param post     Issue a POST request instead of a GET
 *  \param reload   Force reloading of the URL
 */
void MythDownloadManager::queueItem(const QString &url, QNetworkRequest *req,
                                    const QString &dest, QByteArray *data,
                                    QObject *caller, const bool post,
                                    const bool reload)
{
    MythDownloadInfo *dlInfo = new MythDownloadInfo;

    dlInfo->m_url     = url;
    dlInfo->m_request = req;
    dlInfo->m_outFile = dest;
    dlInfo->m_data    = data;
    dlInfo->m_caller  = caller;
    dlInfo->m_post    = post;
    dlInfo->m_reload  = reload;

    dlInfo->detach();

    QMutexLocker locker(m_infoLock);
    m_downloadQueue.push_back(dlInfo);
    m_queueWaitCond.wakeAll();
}

/** \fn MythDownloadManager::processItem(const QString &url,
                                         QNetworkRequest *req,
                                         const QString &dest, QByteArray *data,
                                         const bool post, const bool reload)
 *  \brief Processes a network request immediately and waits for a response.
 *  \param url      URI to download.
 *  \param req      QNetworkRequest to queue
 *  \param dest     Destination filename.
 *  \param data     Location of data for request
 *  \param post     Issue a POST request instead of a GET
 *  \param reload   Force reloading of the URL
 */
bool MythDownloadManager::processItem(const QString &url, QNetworkRequest *req,
                                      const QString &dest, QByteArray *data,
                                      const bool post, const bool reload)
{
    MythDownloadInfo *dlInfo = new MythDownloadInfo;

    dlInfo->m_url      = url;
    dlInfo->m_request  = req;
    dlInfo->m_outFile  = dest;
    dlInfo->m_data     = data;
    dlInfo->m_post     = post;
    dlInfo->m_reload   = reload;
    dlInfo->m_syncMode = true;

    return downloadNow(dlInfo);
}

/** \fn MythDownloadManager::preCache(const QString &url)
 *  \brief Downloads a URL but doesn't store the resulting data anywhere
 *  \param url      URI to download.
 */
void MythDownloadManager::preCache(const QString &url)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("preCache('%1')").arg(url));
    queueItem(url, NULL, QString(), NULL, NULL);
}

/** \fn MythDownloadManager::queueDownload(const QString &url,
                                           const QString &dest,
                                           QObject *caller,
                                           const bool reload)
 *  \brief Adds a url to the download queue.
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
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("queueDownload('%1', '%2', %3)")
            .arg(url).arg(dest).arg((long long)caller));

    queueItem(url, NULL, dest, NULL, caller, false, reload);
}

/** \fn MythDownloadManager::queueDownload(QNetworkRequest &request,
                                           QByteArray *data,
                                           QObject *caller)
 *  \brief Downloads a QNetworkRequest via the QNetworkAccessManager
 *  \param req      Network request to GET
 *         data     Location to store download data
 *         caller   QObject of caller for event notification
 */
void MythDownloadManager::queueDownload(QNetworkRequest *req,
                                        QByteArray *data,
                                        QObject *caller)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("queueDownload('%1', '%2', %3)")
            .arg(req->url().toString()).arg((long long)data).arg((long long)caller));

    queueItem(req->url().toString(), req, QString(), data, caller);
}

/** \fn MythDownloadManager::download(const QString &url,
                                      const QString &dest,
                                      const bool reload)
 *  \brief Downloads a URL to a file in blocking mode.
 *  \param url     URI to download.
 *  \param dest    Destination filename.
 *  \param reload  Whether to force reloading of the URL or not
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::download(const QString &url, const QString &dest,
                                   const bool reload)
{
    return processItem(url, NULL, dest, NULL, false, reload);
}

/** \fn MythDownloadManager::download(const QString &url,
                                      const QByteArray *data,
                                      const bool reload)
 *  \brief Downloads a URI to a QByteArray in blocking mode.
 *  \param url     URI to download.
 *  \param data    Pointer to destination QByteArray.
 *  \param reload  Whether to force reloading of the URL or not
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::download(const QString &url, QByteArray *data,
                                   const bool reload)
{
    return processItem(url, NULL, QString(), data, false, reload);
}

/** \fn MythDownloadManager::download(const QString &url,
                                      const bool reload)
 *  \brief Downloads a URI to a QByteArray in blocking mode.
 *  \param url      URI to download.
 *  \param reload   Whether to force reloading of the URL or not
 *  \return true if download was successful, false otherwise.
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

    if (ok && reply)
        return reply;

    return NULL;
}

/** \fn MythDownloadManager::download(QNetworkRequest &req, QByteArray *data)
 *  \brief Downloads a QNetworkRequest via the QNetworkAccessManager
 *  \param req      Information on the network request
 *         data     Location to store download data
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::download(QNetworkRequest *req, QByteArray *data)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("download('%1', '%2')")
            .arg(req->url().toString()).arg((long long)data));
    return processItem(req->url().toString(), req, QString(), data);
}

/** \fn MythDownloadManager::queuePost(const QString &url,
                                       QByteArray *data,
                                       QObject *caller)
 *  \brief Queues a post to a URL via the QNetworkAccessManager
 *  \param url      URL to post to
 *         data     Location holding post and response data
 *         caller   QObject of caller for event notification
 */
void MythDownloadManager::queuePost(const QString &url,
                                    QByteArray *data,
                                    QObject *caller)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("queuePost('%1', '%2')")
            .arg(url).arg((long long)data));

    if (!data)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "queuePost(), data is NULL!");
        return;
    }

    queueItem(url, NULL, QString(), data, caller, true);
}

/** \fn MythDownloadManager::queuePost(QNetworkRequest *req,
                                       QByteArray *data,
                                       QObject *caller)
 *  \brief Queues a post to a URL via the QNetworkAccessManager
 *  \param req      QNetworkRequest to post
 *         data     Location holding post and response data
 *         caller   QObject of caller for event notification
 */
void MythDownloadManager::queuePost(QNetworkRequest *req,
                                    QByteArray *data,
                                    QObject *caller)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("queuePost('%1', '%2')")
            .arg(req->url().toString()).arg((long long)data));

    if (!data)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "queuePost(), data is NULL!");
        return;
    }

    queueItem(req->url().toString(), req, QString(), data, caller, true);
}

/** \fn MythDownloadManager::post(const QString &url, QByteArray *data)
 *  \brief Posts data to a url via the QNetworkAccessManager
 *  \param url      URL to post to
 *         data     Location holding post and response data
 *  \return true if post was successful, false otherwise.
 */
bool MythDownloadManager::post(const QString &url, QByteArray *data)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("post('%1', '%2')")
            .arg(url).arg((long long)data));

    if (!data)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "post(), data is NULL!");
        return false;
    }

    return processItem(url, NULL, QString(), data, true);
}

/** \fn MythDownloadManager::post(QNetworkRequest *req, QByteArray *data)
 *  \brief Posts a QNetworkRequest via the QNetworkAccessManager
 *  \param req      Information on the network request
 *         data     Location holding post and response data
 *  \return true if post was successful, false otherwise.
 */
bool MythDownloadManager::post(QNetworkRequest *req, QByteArray *data)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("post('%1', '%2')")
            .arg(req->url().toString()).arg((long long)data));

    if (!data)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "post(), data is NULL!");
        return false;
    }

    return processItem(req->url().toString(), req, QString(), data, true);
}

/** \fn MythDownloadManager::downloadRemoteFile(MythDownloadInfo *dlInfo)
 *  \brief Triggers a myth:// URI download in the background via RemoteFile
 *  \param dlInfo   MythDownloadInfo information for download
 */
void MythDownloadManager::downloadRemoteFile(MythDownloadInfo *dlInfo)
{
    RemoteFileDownloadThread *dlThread =
        new RemoteFileDownloadThread(this, dlInfo);
    QThreadPool::globalInstance()->start(dlThread);
}

/** \fn MythDownloadManager::downloadQNetworkRequest(MythDownloadInfo *dlInfo)
 *  \brief Downloads a QNetworkRequest via the QNetworkAccessManager
 *  \param dlInfo   MythDownloadInfo information for download
 */
void MythDownloadManager::downloadQNetworkRequest(MythDownloadInfo *dlInfo)
{
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
        // Prefer the in-cache item if one exists and it is less than 60
        // seconds old and has not expired in the last 10 seconds.
        QDateTime now = QDateTime::currentDateTime();
        QNetworkCacheMetaData urlData = m_manager->cache()->metaData(qurl);
        if ((urlData.isValid()) &&
            ((!urlData.expirationDate().isValid()) ||
             (urlData.expirationDate().secsTo(now) < 10)))
        {
            QNetworkCacheMetaData::RawHeaderList headers =
                urlData.rawHeaders();
            bool found = false;
            QNetworkCacheMetaData::RawHeaderList::iterator it
                = headers.begin();
            for (; !found && it != headers.end(); ++it)
            {
                if ((*it).first == "Date")
                {
                    found = true;
                    QDateTime loadDate =
                       QDateTime::fromString((*it).second, dateFormat);
                    loadDate.setTimeSpec(Qt::UTC);
                    if (loadDate.secsTo(now) < 60)
                        dlInfo->m_preferCache = true;
                }
            }
        }
    }

    if (dlInfo->m_preferCache)
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::PreferCache);

    request.setRawHeader("User-Agent",
                         "MythDownloadManager v" MYTH_BINARY_VERSION);

    if (dlInfo->m_post)
        dlInfo->m_reply = m_manager->post(request, *dlInfo->m_data);
    else
        dlInfo->m_reply = m_manager->get(request);

    m_downloadReplies[dlInfo->m_reply] = dlInfo;

    connect(dlInfo->m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this,
            SLOT(downloadError(QNetworkReply::NetworkError)));
    connect(dlInfo->m_reply, SIGNAL(downloadProgress(qint64, qint64)),
            this, SLOT(downloadProgress(qint64, qint64))); 
}

/** \fn MythDownloadManager::downloadNow(MythDownloadInfo *dlInfo,
                                         bool deleteInfo)
 *  \brief Download helper for download() blocking methods.
 *  \param dlInfo     Information on URI to download.
 *  \param deleteInfo Flag to indicate whether to delete the provided
 *                    MythDownloadInfo instance when done.
 *  \return true if download was successful, false otherwise.
 */
bool MythDownloadManager::downloadNow(MythDownloadInfo *dlInfo, bool deleteInfo)
{
    dlInfo->m_syncMode = true;

    m_infoLock->lock();
    m_downloadQueue.push_back(dlInfo);
    m_infoLock->unlock();
    m_queueWaitCond.wakeAll();

    // sleep for 200ms at a time for up to 10 seconds waiting for the download
    m_infoLock->lock();
    while ((!dlInfo->m_done) &&
           (dlInfo->m_lastStat.secsTo(QDateTime::currentDateTime()) < 10))
    {
        m_infoLock->unlock();
        m_queueWaitLock.lock();
        m_queueWaitCond.wait(&m_queueWaitLock, 200);
        m_queueWaitLock.unlock();
        m_infoLock->lock();
    }

    bool success =
        dlInfo->m_done && (dlInfo->m_errorCode == QNetworkReply::NoError);

    if (!dlInfo->m_done)
    {
        dlInfo->m_syncMode = false; // Let downloadFinished() cleanup for us
        if (dlInfo->m_reply)
            dlInfo->m_reply->abort();
    }
    else if (deleteInfo)
        delete dlInfo;

    m_infoLock->unlock();

    return success;
}
    
/** \fn MythDownloadManager::removeListener(QObject *caller)
 *  \brief Disconnects the specify caller from any existing
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
            dlInfo->m_caller = NULL;
    }

    QMap <QString, MythDownloadInfo*>::iterator mit = m_downloadInfos.begin();
    for (; mit != m_downloadInfos.end(); ++mit)
    {
        dlInfo = mit.value();
        if (dlInfo->m_caller == caller)
            dlInfo->m_caller = NULL;
    }
}

/** \fn MythDownloadManager::downloadError(QNetworkReply::NetworkError errorCode)
 *  \brief Slot to process download error events.
 *  \param errorCode  error code
 */
void MythDownloadManager::downloadError(QNetworkReply::NetworkError errorCode)
{
    QNetworkReply *reply = (QNetworkReply*)sender();

    VERBOSE(VB_FILE+VB_EXTRA, LOC +
            QString("downloadError(%1) (for reply %2)")
                    .arg(errorCode).arg((long long)reply));

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

/** \fn MythDownloadManager::redirectUrl(const QUrl& possibleRedirectUrl,
                                         const QUrl& oldRedirectUrl)
 *  \brief Checks whether we were redirected to the given URL.
 *  \param possibleRedirectUrl Possible Redirect URL
 *  \param oldRedirectUrl      Old Redirect URL
 *  \return empty QUrl if we were not redirected, otherwise the redirected URL
 */
QUrl MythDownloadManager::redirectUrl(const QUrl& possibleRedirectUrl,
                                      const QUrl& oldRedirectUrl) const
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("redirectUrl()"));
    QUrl redirectUrl;

    if(!possibleRedirectUrl.isEmpty() && possibleRedirectUrl != oldRedirectUrl)
        redirectUrl = possibleRedirectUrl;

    return redirectUrl;
}

/** \fn MythDownloadManager::downloadFinished(QNetworkReply* reply)
 *  \brief Slot to process download finished events.
 *  \param reply  QNetworkReply for completed download.
 */
void MythDownloadManager::downloadFinished(QNetworkReply* reply)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("downloadFinished(%1)")
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

/** \fn MythDownloadManager::downloadFinished(MythDownloadInfo* dlInfo)
 *  \brief Callback to process download finished events.
 *  \param dlInfo  MythDownloadInfo for completed download.
 */
void MythDownloadManager::downloadFinished(MythDownloadInfo *dlInfo)
{
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
        VERBOSE(VB_FILE+VB_EXTRA, LOC +
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

        if (dlInfo->m_post)
            dlInfo->m_reply = m_manager->post(request, *dlInfo->m_data);
        else
            dlInfo->m_reply = m_manager->get(request);

        m_downloadReplies[dlInfo->m_reply] = dlInfo;

        connect(dlInfo->m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this,
                SLOT(downloadError(QNetworkReply::NetworkError)));
        connect(dlInfo->m_reply, SIGNAL(downloadProgress(qint64, qint64)),
                this, SLOT(downloadProgress(qint64, qint64))); 

        m_downloadReplies.remove(reply);
        reply->deleteLater();
    }
    else
    {
        VERBOSE(VB_FILE+VB_EXTRA, QString("downloadFinished(%1): COMPLETE: %2")
                .arg((long long)dlInfo).arg(dlInfo->m_url));

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

        dlInfo->m_done = true;

        if (!dlInfo->m_syncMode)
        {
            if (dlInfo->m_caller)
            {
                VERBOSE(VB_FILE+VB_EXTRA, QString("downloadFinished(%1): "
                        "COMPLETE: %2, sending event to caller")
                        .arg((long long)dlInfo).arg(dlInfo->m_url));

                QStringList args;
                args << dlInfo->m_url;
                args << dlInfo->m_outFile;
                args << QString::number(dlInfo->m_bytesTotal);
                args << QString();  // placeholder for error string
                args << QString::number((int)dlInfo->m_errorCode);

                QCoreApplication::postEvent(dlInfo->m_caller,
                    new MythEvent("DOWNLOAD_FILE FINISHED", args));
            }

            delete dlInfo;
        }

        m_queueWaitCond.wakeAll();
    }
}

/** \fn MythDownloadManager::downloadProgress(qint64 bytesReceived,
                                              qint64 bytesTotal)
 *  \brief Slot to process download update events.
 *  \param bytesReceived Bytes received so far
 *  \param bytesTotal    Bytes total for the download, -1 if the total is unknown
 */
void MythDownloadManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply *reply = (QNetworkReply*)sender();

    VERBOSE(VB_FILE+VB_EXTRA, LOC +
            QString("downloadProgress(%1, %2) (for reply %3)")
                    .arg(bytesReceived).arg(bytesTotal).arg((long long)reply));

    QMutexLocker locker(m_infoLock);
    if (!m_downloadReplies.contains(reply))
        return;

    MythDownloadInfo *dlInfo = m_downloadReplies[reply];

    if (!dlInfo)
        return;

    dlInfo->m_lastStat = QDateTime::currentDateTime();

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("downloadProgress: %1 "
            "to %2 is at %3 of %4 bytes downloaded")
            .arg(dlInfo->m_url).arg(dlInfo->m_outFile)
            .arg(bytesReceived).arg(bytesTotal));

    if (!dlInfo->m_syncMode && dlInfo->m_caller)
    {
        VERBOSE(VB_FILE+VB_EXTRA, QString("downloadProgress(%1): "
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

/** \fn MythDownloadManager::saveFile(const QString &outFile,
                                      const QByteArray &data,
                                      const bool append)
 *  \brief Saves a QByteArray of data to a given filename.  Any parent
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
        VERBOSE(VB_IMPORTANT, QString("Failed to create: '%1'")
                .arg(fileInfo.absolutePath()));
        return false;
    }

    QIODevice::OpenMode mode = QIODevice::Unbuffered|QIODevice::WriteOnly;
    if (append)
        mode |= QIODevice::Append;

    if (!file.open(mode))
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to open: '%1'")
                .arg(outFile));
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

/** \fn MythDownloadManager::GetLastModified(const QString &url)
 *  \brief Gets the Last Modified timestamp for a URI
 *  \param url    URI to test.
 *  \return Timestamp the URI was last modified or now if an error occurred
 */
QDateTime MythDownloadManager::GetLastModified(const QString &url)
{
    static const char dateFormat[] = "ddd, dd MMM yyyy hh:mm:ss 'GMT'";
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("GetLastModified('%1')").arg(url));
    QDateTime result;

    QDateTime now = QDateTime::currentDateTime();
    QNetworkCacheMetaData urlData = m_manager->cache()->metaData(QUrl(url));

    if (urlData.isValid())
    {
        if (urlData.lastModified().secsTo(now) <= 60)
        {
            result = urlData.lastModified();
        }
        else
        {
            // If the last modification date is older than 60 seconds, and
            // we loaded the page over 60 seconds ago, then redownload the
            // page to re-verify it's last modified date.
            QNetworkCacheMetaData::RawHeaderList headers =
                urlData.rawHeaders();
            bool found = false;
            QNetworkCacheMetaData::RawHeaderList::iterator it
                = headers.begin();
            for (; !found && it != headers.end(); ++it)
            {
                if ((*it).first == "Date")
                {
                    found = true;
                    QDateTime loadDate =
                       QDateTime::fromString((*it).second, dateFormat);
                    loadDate.setTimeSpec(Qt::UTC);
                    if (loadDate.secsTo(now) <= 60)
                        result = urlData.lastModified();
                }
            }
        }
    }

    if (!result.isValid())
    {
        MythDownloadInfo *dlInfo = new MythDownloadInfo;
        dlInfo->m_url      = url;
        dlInfo->m_syncMode = true;

        if (downloadNow(dlInfo, false) && dlInfo->m_reply)
        {
            QVariant lastMod =
                dlInfo->m_reply->header(QNetworkRequest::LastModifiedHeader);
            if (lastMod.isValid())
                result = lastMod.toDateTime();
        }

        delete dlInfo;
    }

    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

