// qt
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QEvent>

// myth
#include <mythcontext.h>
#include <util.h>
#include <mythuihelper.h>
#include <mythdirs.h>
#include <mythverbose.h>
#include <mythdownloadmanager.h>

#include "downloadmanager.h"

QEvent::Type VideoDLEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

DownloadManager::DownloadManager(QObject *parent)
{
    m_parent = parent;
}

DownloadManager::~DownloadManager()
{
    cancel();
    wait();
}

void DownloadManager::addDL(ResultItem *video)
{
    // Add a file to the list of videos
    m_mutex.lock();
    VideoDL *dl = new VideoDL;
    dl->url          = video->GetMediaURL();
    dl->filename     = getDownloadFilename(video);
    dl->download     = video->GetDownloader();
    dl->downloadargs = video->GetDownloaderArguments();
    m_fileList.append(dl);
    if (!isRunning())
        start();
    m_mutex.unlock();
}

void DownloadManager::cancel()
{
    m_mutex.lock();
    m_fileList.clear();
    m_mutex.unlock();
}

void DownloadManager::run()
{
    while (moreWork())
    {
        VideoDL *dl = m_fileList.takeFirst();

        QString url = dl->url;
        QString filename = dl->filename;
        QString downloader = dl->download;
        QStringList downloadargs = dl->downloadargs;

        if (!downloader.isEmpty())
        {
            QProcess externalDL;

            downloadargs.replaceInStrings("%FILE%", filename);
            downloadargs.replaceInStrings("%HOMEDIR%", QDir::homePath());
            QStringList videoDirs = gCoreContext->GetSetting("VideoStartupDir").split(":");
            if (videoDirs.size())
                downloadargs.replaceInStrings("%VIDEODIR%",
                             videoDirs.takeFirst());

            externalDL.setReadChannel(QProcess::StandardOutput);
            externalDL.start(downloader, downloadargs);
            externalDL.waitForFinished();
            QByteArray result = externalDL.readAll();
            QString resultString(result);
            dl->filename = result.trimmed();

            // inform parent we have video ready for it
            if (!result.isEmpty() && QFile::exists(result))
            {
                VERBOSE(VB_GENERAL, QString("External Video Download Finished: %1").arg(filename));
                QCoreApplication::postEvent(m_parent, new VideoDLEvent(dl));
            }
            else
                VERBOSE(VB_GENERAL, QString("External Video Download Failed: (%1) - Check external use, "
                                            "permissions...").arg(url));
        }
        else
        {
            bool exists = QFile::exists(filename);
            if (!exists && !url.isEmpty())
            {
                bool success = GetMythDownloadManager()->download(url, filename);
                if (success)
                {
                    VERBOSE(VB_GENERAL, QString("Video Download Finished: %1").arg(filename));
                    QCoreApplication::postEvent(m_parent, new VideoDLEvent(dl));
                }
                else
                    VERBOSE(VB_GENERAL, QString("Internal Video Download Failed: (%1) - Check "
                                                "permissions...").arg(url));
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("Threaded Video Download Finished: %1").arg(filename));
                QCoreApplication::postEvent(m_parent, new VideoDLEvent(dl));
            }
        }
    }
}

bool DownloadManager::moreWork()
{
    bool result;
    m_mutex.lock();
    result = !m_fileList.isEmpty();
    m_mutex.unlock();
    return result;
}

QString DownloadManager::getDownloadFilename(ResultItem *item)
{
    QByteArray urlarr(item->GetMediaURL().toLatin1());
    quint16 urlChecksum = qChecksum(urlarr.data(), urlarr.length());
    QByteArray titlearr(item->GetTitle().toLatin1());
    quint16 titleChecksum = qChecksum(titlearr.data(), titlearr.length());
    QUrl qurl(item->GetMediaURL());
    QString ext = QFileInfo(qurl.path()).suffix();
    QString basefilename = QString("download_%1_%2.%3")
                           .arg(QString::number(urlChecksum))
                           .arg(QString::number(titleChecksum)).arg(ext);

    QString finalFilename = GetConfDir() + "/" + basefilename;

    return finalFilename;
}

