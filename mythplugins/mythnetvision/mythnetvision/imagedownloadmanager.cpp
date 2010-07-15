// qt
#include <QCoreApplication>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QEvent>

// myth
#include <mythcontext.h>
#include <util.h>
#include <mythuihelper.h>
#include <mythdirs.h>
#include <mythverbose.h>
#include <httpcomms.h>

#include "imagedownloadmanager.h"

QEvent::Type ImageDLEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

ImageDownloadManager::ImageDownloadManager(QObject *parent)
{
    m_parent = parent;
}

ImageDownloadManager::~ImageDownloadManager()
{
    cancel();
    wait();
}

void ImageDownloadManager::addURL(const QString& title, const QString& url,
                                  const uint& pos)
{
    // Add a file to the list of thumbs.
    // Must remember to call start after adding all the files!
    m_mutex.lock();
    ImageData *idat = new ImageData;
    idat->title    = title;
    idat->url      = url;
    idat->pos      = pos;
    idat->filename = QString();
    m_fileList.append(idat);
    if (!isRunning())
        start();
    m_mutex.unlock();
}

void ImageDownloadManager::cancel()
{
    m_mutex.lock();
    qDeleteAll(m_fileList);
    m_fileList.clear();
    m_parent = NULL;
    m_mutex.unlock();
}

void ImageDownloadManager::run()
{
    ImageData* idat;
    while ((idat = moreWork()) != NULL)
    {
        QString fileprefix = GetConfDir();

        QDir dir(fileprefix);
        if (!dir.exists())
            dir.mkdir(fileprefix);

        fileprefix += "/MythNetvision";

        dir = QDir(fileprefix);
        if (!dir.exists())
            dir.mkdir(fileprefix);

        fileprefix += "/thumbcache";

        dir = QDir(fileprefix);
        if (!dir.exists())
            dir.mkdir(fileprefix);

        QString url = idat->url;
        QString title = idat->title;
        QString sFilename = QString("%1/%2_%3")
                .arg(fileprefix)
                .arg(qChecksum(url.toLocal8Bit().constData(),
                               url.toLocal8Bit().size()))
                .arg(qChecksum(title.toLocal8Bit().constData(),
                               title.toLocal8Bit().size()));

        bool exists = QFile::exists(sFilename);
        if (!exists && !url.isEmpty())
            HttpComms::getHttpFile(sFilename, url, 20000, 1, 2);

        // inform parent we have thumbnail ready for it
        if (QFile::exists(sFilename) && m_parent)
        {
            VERBOSE(VB_GENERAL|VB_EXTRA, QString("Threaded Image Download: %1").arg(sFilename));
            idat->filename = sFilename;
            QCoreApplication::postEvent(m_parent, new ImageDLEvent(idat));
        }
        else
        {
            delete idat;
        }
    }
}

ImageData* ImageDownloadManager::moreWork()
{
    ImageData* result = NULL;
    m_mutex.lock();
    if (!m_fileList.isEmpty())
        result = m_fileList.takeFirst();
    m_mutex.unlock();
    return result;
}
