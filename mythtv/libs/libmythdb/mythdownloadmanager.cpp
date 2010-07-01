// qt
#include <QCoreApplication>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QDir>

// libmythdb
#include "compat.h"
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythevent.h"
#include "mythverbose.h"
#include "mythversion.h"

#include "mythdownloadmanager.h"

using namespace std;

#define LOC      QString("DownloadManager: ")
#define LOC_WARN QString("DownloadManager, Warning: ")
#define LOC_ERR  QString("DownloadManager, Error: ")

MythDownloadManager *downloadManager = NULL;

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
    }

    return downloadManager;
}

/** \fn MythDownloadManager::MythDownloadManager()
 *  \brief Constructor for MythDownloadManager.  Instantiates a
 *         QNetworkAccessManager and QNetworkDiskCache.
 */
MythDownloadManager::MythDownloadManager() :
    m_manager(new QNetworkAccessManager(this)),
    m_diskCache(new QNetworkDiskCache(this)),
    m_infoLock(new QMutex(QMutex::Recursive)),
    m_queueThread(NULL),
    m_runThread(true)
{
    m_diskCache->setCacheDirectory(GetConfDir() + "/Cache-" +
                                   gCoreContext->GetAppName() + "-" +
                                   gCoreContext->GetHostName());
    m_manager->setCache(m_diskCache);
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
    m_queueThread = currentThread();

    QObject::connect(m_manager, SIGNAL(finished(QNetworkReply*)), this,
                       SLOT(checkDownload(QNetworkReply*)));

    while (m_runThread)
    {
        m_queueWaitLock.lock();
        m_infoLock->lock();
        downloading = !m_downloadInfos.isEmpty();
        m_infoLock->unlock();

        if (downloading)
        {
            QCoreApplication::processEvents();
            m_queueWaitCond.wait(&m_queueWaitLock, 200);
        }
        else
        {
            m_queueWaitCond.wait(&m_queueWaitLock);
        }

        m_infoLock->lock();
        if (!m_downloadQueue.isEmpty())
        {
            DownloadInfo *dlInfo = m_downloadQueue.front();
            m_downloadQueue.pop_front();
            QUrl qurl(dlInfo->url);
            m_downloadInfos[qurl.toString()] = dlInfo;

            QNetworkRequest request(qurl);
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                                 QNetworkRequest::PreferNetwork);
            request.setRawHeader("User-Agent",
                                 "MythDownloadManager v" MYTH_BINARY_VERSION);

            if (!dlInfo->outFile.isEmpty() || dlInfo->data)
                dlInfo->reply = m_manager->get(request);
            else
                dlInfo->reply = m_manager->head(request);
            m_downloadReplies[dlInfo->reply] = dlInfo;

            connect(dlInfo->reply, SIGNAL(downloadProgress(qint64, qint64)),
                    this, SLOT(replyDownloadProgress(qint64, qint64))); 
        }
        m_infoLock->unlock();
        m_queueWaitLock.unlock();
    }
}

/** \fn MythDownloadManager::queueDownload(const QString &url,
                                           const QString &outFile,
                                           QObject *caller)
 *  \brief Adds a url to the download queue.
 *  \param url     URI to download.
 *  \param outFile Destination filename.
 *  \param caller  QObject caller to receive event notifications.
 *  \sa download(const QString &url, const QString &outFile)
 *      download(const QString &url, QByteArray *data)
 */
void MythDownloadManager::queueDownload(const QString &url,
                                        const QString &outFile, QObject *caller)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("queueDownload('%1', '%2', %3)")
            .arg(url).arg(outFile).arg((long long)caller));
    DownloadInfo *dlInfo = new DownloadInfo;

    dlInfo->url      = url;
    dlInfo->outFile  = outFile;
    dlInfo->reply    = NULL;
    dlInfo->data     = NULL;
    dlInfo->caller   = caller;
    dlInfo->lastStat = QDateTime::currentDateTime();
    dlInfo->syncMode = false;
    dlInfo->done     = false;

    QMutexLocker locker(m_infoLock);
    m_downloadQueue.push_back(dlInfo);
    m_queueWaitCond.wakeAll();
}

/** \fn MythDownloadManager::download(const QString &url,
                                      const QString &outFile)
 *  \brief Downloads a URI to a file in blocking mode.
 *  \param url     URI to download.
 *  \param outFile Destination filename.
 *  \return true if download was successful, false otherwise.
 *  \sa queueDownload(const QString &url, const QString &outFile, QObject *caller)
 *      download(const QString &url, QByteArray *data)
 */
bool MythDownloadManager::download(const QString &url, const QString &outFile)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("download('%1', '%2')")
            .arg(url).arg(outFile));
    DownloadInfo *dlInfo = new DownloadInfo;

    dlInfo->url      = url;
    dlInfo->outFile  = outFile;
    dlInfo->reply    = NULL;
    dlInfo->data     = NULL;
    dlInfo->caller   = NULL;
    dlInfo->lastStat = QDateTime::currentDateTime();
    dlInfo->syncMode = true;
    dlInfo->done     = false;

    return downloadNow(dlInfo);
}

/** \fn MythDownloadManager::download(const QString &url,
                                      const QByteArray *data)
 *  \brief Downloads a URI to a QByteArray in blocking mode.
 *  \param url     URI to download.
 *  \param data    Pointer to destination QByteArray.
 *  \return true if download was successful, false otherwise.
 *  \sa queueDownload(const QString &url, const QString &outFile, QObject *caller)
 *      download(const QString &url, const QString &outFile)
 */
bool MythDownloadManager::download(const QString &url, QByteArray *data)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("download('%1', %2)")
            .arg(url).arg((long long)data));
    DownloadInfo *dlInfo = new DownloadInfo;

    dlInfo->url      = url;
    dlInfo->data     = data;
    dlInfo->reply    = NULL;
    dlInfo->caller   = NULL;
    dlInfo->lastStat = QDateTime::currentDateTime();
    dlInfo->syncMode = true;
    dlInfo->done     = false;

    return downloadNow(dlInfo);
}

/** \fn MythDownloadManager::downloadNow(DownloadInfo *dlInfo,
                                         bool deleteInfo)
 *  \brief Download helper for download() blocking methods.
 *  \param dlInfo     Information on URI to download.
 *  \param deleteInfo Flag to indicate whether to delete the provided
 *                    DownloadInfo structure when done.
 *  \return true if download was successful, false otherwise.
 *  \sa download(const QString &url, const QString &outFile)
 *      download(const QString &url, QByteArray *data)
 */
bool MythDownloadManager::downloadNow(DownloadInfo *dlInfo, bool deleteInfo)
{
    m_infoLock->lock();
    m_downloadQueue.push_back(dlInfo);
    m_infoLock->unlock();
    m_queueWaitCond.wakeAll();

    int loops = 0;
    m_infoLock->lock();
    while ((!dlInfo->done) &&
           (dlInfo->lastStat.secsTo(QDateTime::currentDateTime()) < 20))
    {
        m_infoLock->unlock();
        m_queueWaitLock.lock();
        m_queueWaitCond.wait(&m_queueWaitLock, 200);
        loops++;
        m_queueWaitLock.unlock();
        m_infoLock->lock();
    }
    m_infoLock->unlock();

    bool finished = dlInfo->done;

    if (deleteInfo)
        delete dlInfo;

    if (finished)
        return true;

    return false;
}
    
/** \fn MythDownloadManager::redirectUrl(const QUrl& possibleRedirectUrl,
                                         const QUrl& oldRedirectUrl)
 *  \brief Checks whether we were redirected to the given URL.
 *  \param possibleRedirectUrl Possible Redirect URL
 *  \param oldRedirectUrl      Old Redirect URL
 *  \return empty QUrl if we were not redirected, otherwise the redirected URL
 *  \sa checkDownload(QNetworkReply* reply)
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

/** \fn MythDownloadManager::checkDownload(QNetworkReply* reply)
 *  \brief Slot to process download finished events.
 *  \param reply  QNetworkReply for completed download.
 *  \sa redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl)
 */
void MythDownloadManager::checkDownload(QNetworkReply* reply)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("checkDownload()"));
    QVariant possibleRedirectUrl =
         reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    bool deleteReply = true;
    QUrl urlRedirectedTo = redirectUrl(possibleRedirectUrl.toUrl(),
                                       urlRedirectedTo);

    if(!urlRedirectedTo.isEmpty())
    {
        if (m_downloadInfos.contains(reply->url().toString()))
        {
            DownloadInfo *dlInfo = m_downloadInfos[reply->url().toString()];
            QNetworkRequest request(urlRedirectedTo);
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                                 QNetworkRequest::PreferNetwork);
            request.setRawHeader("User-Agent",
                                 "MythDownloadManager v" MYTH_BINARY_VERSION);
            dlInfo->reply = m_manager->get(request);
            m_downloadReplies.remove(reply);
            m_downloadReplies[dlInfo->reply] = dlInfo;
        }
        VERBOSE(VB_FILE+VB_EXTRA, LOC +
                QString("checkDownload(): Redirect: %1 -> %2")
                        .arg(reply->url().toString())
                        .arg(urlRedirectedTo.toString()));
    }
    else
    {
        VERBOSE(VB_FILE+VB_EXTRA, QString("checkDownload: COMPLETE: %1")
                .arg(reply->url().toString()));
        QMutexLocker locker(m_infoLock);
        if (m_downloadInfos.contains(reply->url().toString()))
        {
            DownloadInfo *dlInfo = m_downloadInfos[reply->url().toString()];

            if (dlInfo->data)
            {
                (*dlInfo->data) = reply->readAll();
            }
            else if (!dlInfo->outFile.isEmpty())
            {
                QByteArray data = reply->readAll();

                if (!dlInfo->outFile.isEmpty())
                    saveFile(dlInfo->outFile, data);
            }

            m_downloadInfos.remove(reply->url().toString());
            m_downloadReplies.remove(reply);

            dlInfo->done = true;
            if (dlInfo->caller)
            {
                VERBOSE(VB_FILE+VB_EXTRA, QString("checkDownload: COMPLETE: %1,"
                        "sending event to caller")
                        .arg(reply->url().toString()));
                QCoreApplication::postEvent(dlInfo->caller,
                    new MythEvent("DOWNLOAD_COMPLETE", QStringList(dlInfo->outFile)));

                delete dlInfo;
            }

            if (dlInfo->syncMode)
                deleteReply = false;
        }

        m_queueWaitCond.wakeAll();
    }

    if (deleteReply)
        reply->deleteLater();
}

/** \fn MythDownloadManager::replyDownloadProgress(qint64 bytesReceived,
                                                   qint64 bytesTotal)
 *  \brief Slot to process download update events.
 *  \param bytesReceived Bytes received so far
 *  \param bytesTotal    Bytes total for the download, -1 if the total is unknown
 */
void MythDownloadManager::replyDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("replyDownloadProgress(%1, %2)")
            .arg(bytesReceived).arg(bytesTotal));

    QNetworkReply *reply = (QNetworkReply*)sender();

    if (m_downloadReplies.contains(reply))
    {
        DownloadInfo *dlInfo = m_downloadReplies[reply];

        if (dlInfo)
        {
            dlInfo->lastStat = QDateTime::currentDateTime();
            VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("replyDownloadProgress: %1 "
                    "to %2 is at %3 of %4 bytes downloaded")
                    .arg(dlInfo->url).arg(dlInfo->outFile)
                    .arg(bytesReceived).arg(bytesTotal));
        }
    }
}

/** \fn MythDownloadManager::saveFile(const QString &outFile,
                                      const QByteArray &data)
 *  \brief Saves a QByteArray of data to a given filename.  Any parent
 *         directories are created automatically.
 *  \param outFile Filename to save to.
 *  \param data    Data to save.
 *  \return true if successful, false otherwise
 */
bool MythDownloadManager::saveFile(const QString &outFile,
                                   const QByteArray &data)
{
    bool ok = false;

    if (data.size())
    {
        QFile file(outFile);
        QFileInfo fileInfo(outFile);
        QDir qdir(fileInfo.absolutePath());

        if (!qdir.exists() && !qdir.mkpath(fileInfo.absolutePath()))
        {
            VERBOSE(VB_IMPORTANT, QString("Failed to create: '%1'")
                    .arg(fileInfo.absolutePath()));
            return false;
        }

        ok = file.open(QIODevice::Unbuffered|QIODevice::WriteOnly);
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, QString("Failed to open: '%1'")
                    .arg(outFile));
            return false;
        }

        off_t offset = 0;
        size_t remaining = (ok) ? data.size() : 0;
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
            ok = false;
    }

    return ok;
}

/** \fn MythDownloadManager::GetLastModified(const QString &url)
 *  \brief Gets the Last Modified timestamp for a URI
 *  \param url    URI to test.
 *  \return Timestamp the URI was last modified
 */
QDateTime MythDownloadManager::GetLastModified(const QString &url)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("GetLastModified('%1')").arg(url));
    DownloadInfo *dlInfo = new DownloadInfo;
    QDateTime result;

    dlInfo->url      = url;
    dlInfo->reply    = NULL;
    dlInfo->data     = NULL;
    dlInfo->caller   = NULL;
    dlInfo->lastStat = QDateTime::currentDateTime();
    dlInfo->syncMode = true;
    dlInfo->done     = false;

    bool success = downloadNow(dlInfo, false);

    if (success && dlInfo->reply)
    {
        QVariant lastMod =
            dlInfo->reply->header(QNetworkRequest::LastModifiedHeader);
        if (lastMod.isValid())
            result = lastMod.toDateTime();
    }

    delete dlInfo;

    return result;
}

