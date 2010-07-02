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
    bool itemsInQueue = false;
    bool waitAnyway = false;
    m_queueThread = currentThread();

    QObject::connect(m_manager, SIGNAL(finished(QNetworkReply*)), this,
                       SLOT(downloadFinished(QNetworkReply*)));

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
            m_downloadInfos[qurl.toString()] = dlInfo;

            QNetworkRequest request(qurl);
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                                 QNetworkRequest::PreferNetwork);
            request.setRawHeader("User-Agent",
                                 "MythDownloadManager v" MYTH_BINARY_VERSION);

            if (!dlInfo->m_outFile.isEmpty() || dlInfo->m_data)
                dlInfo->m_reply = m_manager->get(request);
            else
                dlInfo->m_reply = m_manager->head(request);
            m_downloadReplies[dlInfo->m_reply] = dlInfo;

            connect(dlInfo->m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this,
                    SLOT(downloadError(QNetworkReply::NetworkError)));
            connect(dlInfo->m_reply, SIGNAL(downloadProgress(qint64, qint64)),
                    this, SLOT(downloadProgress(qint64, qint64))); 
        }
        m_infoLock->unlock();
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
    MythDownloadInfo *dlInfo = new MythDownloadInfo;

    dlInfo->m_url      = url;
    dlInfo->m_outFile  = outFile;
    dlInfo->m_caller   = caller;

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
    return download(url, outFile, NULL);
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
    return download(url, QString(), data);
}

/** \fn MythDownloadManager::download(const QString &url,
                                      const QString &outFile,
                                      const QByteArray *data)
 *  \brief Downloads a URI in blocking mode.
 *  \param url     URI to download.
 *  \param outFile Destination filename if data should be written to a file.
 *  \param data    Destination QByteArray if data should not be saved to a file.
 *  \return true if download was successful, false otherwise.
 *  \sa queueDownload(const QString &url, const QString &outFile, QObject *caller)
 *      download(const QString &url, const QString &outFile)
 *      download(const QString &url, QByteArray *data)
 */
bool MythDownloadManager::download(const QString &url, const QString &outFile,
                                   QByteArray *data)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("download('%1', '%2', %3)")
            .arg(url).arg(outFile).arg((long long)data));
    MythDownloadInfo *dlInfo = new MythDownloadInfo;

    dlInfo->m_url      = url;
    dlInfo->m_outFile  = outFile;
    dlInfo->m_data     = data;
    dlInfo->m_syncMode = true;

    return downloadNow(dlInfo);
}

/** \fn MythDownloadManager::downloadNow(MythDownloadInfo *dlInfo,
                                         bool deleteInfo)
 *  \brief Download helper for download() blocking methods.
 *  \param dlInfo     Information on URI to download.
 *  \param deleteInfo Flag to indicate whether to delete the provided
 *                    MythDownloadInfo instance when done.
 *  \return true if download was successful, false otherwise.
 *  \sa download(const QString &url, const QString &outFile)
 *      download(const QString &url, QByteArray *data)
 */
bool MythDownloadManager::downloadNow(MythDownloadInfo *dlInfo, bool deleteInfo)
{
    dlInfo->m_syncMode = true;

    m_infoLock->lock();
    m_downloadQueue.push_back(dlInfo);
    m_infoLock->unlock();
    m_queueWaitCond.wakeAll();

    // sleep for 200ms at a time for up to 20 seconds waiting for the download
    m_infoLock->lock();
    while ((!dlInfo->m_done) &&
           (dlInfo->m_lastStat.secsTo(QDateTime::currentDateTime()) < 20))
    {
        m_infoLock->unlock();
        m_queueWaitLock.lock();
        m_queueWaitCond.wait(&m_queueWaitLock, 200);
        m_queueWaitLock.unlock();
        m_infoLock->lock();
    }
    m_infoLock->unlock();

    bool success =
        dlInfo->m_done && (dlInfo->m_errorCode == QNetworkReply::NoError);

    if (deleteInfo)
        delete dlInfo;

    return success;
}
    
/** \fn MythDownloadManager::downloadError(QNetworkReply::NetworkError errorCode)
 *  \brief Slot to process download error events.
 *  \param errorCode  error code
 *  \sa redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl)
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
 *  \sa downloadFinished(QNetworkReply* reply)
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
 *  \sa redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl)
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

    if (!dlInfo)
        return;

    QUrl possibleRedirectUrl =
         reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

    dlInfo->m_redirectedTo = redirectUrl(possibleRedirectUrl, dlInfo->m_redirectedTo);

    if(!dlInfo->m_redirectedTo.isEmpty())
    {
        VERBOSE(VB_FILE+VB_EXTRA, LOC +
                QString("downloadFinished(%1): Redirect: %2 -> %3")
                        .arg((long long)reply)
                        .arg(reply->url().toString())
                        .arg(dlInfo->m_redirectedTo.toString()));

        QNetworkRequest request(dlInfo->m_redirectedTo);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::PreferNetwork);
        request.setRawHeader("User-Agent",
                             "MythDownloadManager v" MYTH_BINARY_VERSION);

        if (!dlInfo->m_outFile.isEmpty() || dlInfo->m_data)
            dlInfo->m_reply = m_manager->get(request);
        else
            dlInfo->m_reply = m_manager->head(request);

        m_downloadReplies[dlInfo->m_reply] = dlInfo;

        m_downloadReplies.remove(reply);
        reply->deleteLater();
    }
    else
    {
        VERBOSE(VB_FILE+VB_EXTRA, QString("downloadFinished(%1): COMPLETE: %2")
                .arg((long long)reply).arg(reply->url().toString()));

        dlInfo->m_redirectedTo.clear();

        if (dlInfo->m_data)
        {
            (*dlInfo->m_data) = reply->readAll();
        }
        else if (!dlInfo->m_outFile.isEmpty())
        {
            QByteArray data = reply->readAll();
            saveFile(dlInfo->m_outFile, data);
        }

        m_downloadInfos.remove(reply->url().toString());
        m_downloadReplies.remove(reply);

        dlInfo->m_done = true;

        if (!dlInfo->m_syncMode)
        {
            if (dlInfo->m_caller)
            {
                VERBOSE(VB_FILE+VB_EXTRA, QString("downloadFinished(%1): "
                        "COMPLETE: %2, sending event to caller")
                        .arg(reply->url().toString()));
                QCoreApplication::postEvent(dlInfo->m_caller,
                    new MythEvent("DOWNLOAD_COMPLETE", QStringList(dlInfo->m_outFile)));
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
 *  \return Timestamp the URI was last modified or now if an error occurred
 */
QDateTime MythDownloadManager::GetLastModified(const QString &url)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("GetLastModified('%1')").arg(url));
    MythDownloadInfo *dlInfo = new MythDownloadInfo;
    QDateTime result = QDateTime::currentDateTime();

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

    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

