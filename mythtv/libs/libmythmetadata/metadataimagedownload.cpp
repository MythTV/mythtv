// qt
#include <QCoreApplication>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QEvent>

// myth
#include "mythcorecontext.h"
#include "mythuihelper.h"
#include "mythdirs.h"
#include "storagegroup.h"
#include "metadataimagedownload.h"
#include "remotefile.h"
#include "mythdownloadmanager.h"
#include "mythlogging.h"
#include "mythdate.h"

QEvent::Type ImageDLEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type ImageDLFailureEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type ThumbnailDLEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

MetadataImageDownload::MetadataImageDownload(QObject *parent) :
    MThread("MetadataImageDownload")
{
    m_parent = parent;
}

MetadataImageDownload::~MetadataImageDownload()
{
    cancel();
    wait();
}

void MetadataImageDownload::addThumb(QString title,
                                     QString url, QVariant data)
{
    QMutexLocker lock(&m_mutex);

    ThumbnailData *id = new ThumbnailData();
    id->title = title;
    id->data = data;
    id->url = url;
    m_thumbnailList.append(id);
    if (!isRunning())
        start();
}

/**
 * addLookup: Add lookup to bottom of the queue
 * MetadataDownload::m_downloadList takes ownership of the given lookup
 */
void MetadataImageDownload::addDownloads(MetadataLookup *lookup)
{
    QMutexLocker lock(&m_mutex);

    m_downloadList.append(lookup);
    lookup->DecrRef();
    if (!isRunning())
        start();
}

void MetadataImageDownload::cancel()
{
    QMutexLocker lock(&m_mutex);

    qDeleteAll(m_thumbnailList);
    m_thumbnailList.clear();
    // clearing m_downloadList automatically delete all its content
    m_downloadList.clear();
}

void MetadataImageDownload::run()
{
    RunProlog();

    // Always handle thumbnails first, they're higher priority.
    ThumbnailData *thumb;
    while ((thumb = moreThumbs()) != NULL)
    {
        QString sFilename = getDownloadFilename(thumb->title, thumb->url);

        bool exists = QFile::exists(sFilename);
        if (!exists && !thumb->url.isEmpty())
        {
            if (!GetMythDownloadManager()->download(thumb->url, sFilename))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("MetadataImageDownload: failed to download thumbnail from: %1")
                    .arg(thumb->url));

                delete thumb;
                continue;
            }
        }

        // inform parent we have thumbnail ready for it
        if (QFile::exists(sFilename) && m_parent)
        {
            LOG(VB_GENERAL, LOG_DEBUG,
                    QString("Threaded Image Thumbnail Download: %1")
                    .arg(sFilename));
            thumb->url = sFilename;
            QCoreApplication::postEvent(m_parent,
                           new ThumbnailDLEvent(thumb));
        }
        else
            delete thumb;
    }

    while (true)
    {
        m_mutex.lock();
        if (m_downloadList.isEmpty())
        {
            // no more to process, we're done
            m_mutex.unlock();
            break;
        }
        // Ref owns the MetadataLookup object for the duration of the loop
        // and it will be deleted automatically when the loop completes
        RefCountHandler<MetadataLookup> ref = m_downloadList.takeFirstAndDecr();
        m_mutex.unlock();
        MetadataLookup *lookup = ref;
        DownloadMap downloads = lookup->GetDownloads();
        DownloadMap downloaded;

        bool errored = false;
        for (DownloadMap::iterator i = downloads.begin();
                i != downloads.end(); ++i)
        {
            VideoArtworkType type = i.key();
            ArtworkInfo info = i.value();
            QString filename = getDownloadFilename( type, lookup,
                                   info.url );
            if (lookup->GetHost().isEmpty())
            {
                QString path = getLocalWritePath(lookup->GetType(), type);
                QDir dirPath(path);
                if (!dirPath.exists())
                    if (!dirPath.mkpath(path))
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("Metadata Image Download: Unable to create "
                                    "path %1, aborting download.").arg(path));
                        errored = true;
                        continue;
                    }
                QString finalfile = path + "/" + filename;
                QString oldurl = info.url;
                info.url = finalfile;
                if (!QFile::exists(finalfile) || lookup->GetAllowOverwrites())
                {
                    QFile dest_file(finalfile);
                    if (dest_file.exists())
                    {
                        QFileInfo fi(finalfile);
                        GetMythUI()->RemoveFromCacheByFile(fi.fileName());
                        dest_file.remove();
                    }

                    LOG(VB_GENERAL, LOG_INFO,
                        QString("Metadata Image Download: %1 ->%2")
                         .arg(oldurl).arg(finalfile));
                    QByteArray *download = new QByteArray();
                    GetMythDownloadManager()->download(oldurl, download);

                    QImage testImage;
                    bool didLoad = testImage.loadFromData(*download);
                    if (!didLoad)
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("Tried to write %1, but it appears to be "
                                    "an HTML redirect (filesize %2).")
                                .arg(oldurl).arg(download->size()));
                        delete download;
                        download = NULL;
                        errored = true;
                        continue;
                    }

                    if (dest_file.open(QIODevice::WriteOnly))
                    {
                        off_t size = dest_file.write(*download,
                                                     download->size());
                        dest_file.close();
                        if (size != download->size())
                        {
                            // File creation failed for some reason, delete it
                            RemoteFile::DeleteFile(finalfile);
                            LOG(VB_GENERAL, LOG_ERR,
                                QString("Image Download: Error Writing Image "
                                        "to file: %1").arg(finalfile));
                            errored = true;
                        }
                        else
                            downloaded.insert(type, info);
                    }

                    delete download;
                }
                else
                    downloaded.insert(type, info);
            }
            else
            {
                QString path = getStorageGroupURL(type, lookup->GetHost());
                QString finalfile = path + filename;
                QString oldurl = info.url;
                info.url = finalfile;
                bool exists = false;
                bool onMaster = false;
                QString resolvedFN;
                if ((lookup->GetHost().toLower() == gCoreContext->GetHostName().toLower()) ||
                    (gCoreContext->IsThisHost(lookup->GetHost())))
                {
                    StorageGroup sg;
                    resolvedFN = sg.FindFile(filename);
                    exists = QFile::exists(resolvedFN);
                    if (!exists)
                    {
                        resolvedFN = getLocalStorageGroupPath(type,
                                                 lookup->GetHost()) + "/" + filename;
                    }
                    onMaster = true;
                }
                else
                    exists = RemoteFile::Exists(finalfile);

                if (!exists || lookup->GetAllowOverwrites())
                {

                    if (exists && !onMaster)
                    {
                        QFileInfo fi(finalfile);
                        GetMythUI()->RemoveFromCacheByFile(fi.fileName());
                        RemoteFile::DeleteFile(finalfile);
                    }
                    else if (exists)
                        QFile::remove(resolvedFN);

                    LOG(VB_GENERAL, LOG_INFO,
                        QString("Metadata Image Download: %1 -> %2")
                            .arg(oldurl).arg(finalfile));
                    QByteArray *download = new QByteArray();
                    GetMythDownloadManager()->download(oldurl, download);

                    QImage testImage;
                    bool didLoad = testImage.loadFromData(*download);
                    if (!didLoad)
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("Tried to write %1, but it appears to be "
                                    "an HTML redirect or corrupt file "
                                    "(filesize %2).")
                                .arg(oldurl).arg(download->size()));
                        delete download;
                        download = NULL;
                        errored = true;
                        continue;
                    }

                    if (!onMaster)
                    {
                        RemoteFile *outFile = new RemoteFile(finalfile, true);
                        if (!outFile->isOpen())
                        {
                            LOG(VB_GENERAL, LOG_ERR,
                                QString("Image Download: Failed to open "
                                        "remote file (%1) for write.  Does "
                                        "Storage Group Exist?")
                                        .arg(finalfile));
                            delete outFile;
                            outFile = NULL;
                            errored = true;
                        }
                        else
                        {
                            off_t written = outFile->Write(*download,
                                                           download->size());
                            delete outFile;
                            outFile = NULL;
                            if (written != download->size())
                            {
                                // File creation failed for some reason, delete it
                                RemoteFile::DeleteFile(finalfile);

                                LOG(VB_GENERAL, LOG_ERR,
                                    QString("Image Download: Error Writing Image "
                                            "to file: %1").arg(finalfile));
                                errored = true;
                            }
                            else
                                downloaded.insert(type, info);
                        }
                    }
                    else
                    {
                        QFile dest_file(resolvedFN);
                        if (dest_file.open(QIODevice::WriteOnly))
                        {
                            off_t size = dest_file.write(*download,
                                                         download->size());
                            dest_file.close();
                            if (size != download->size())
                            {
                                // File creation failed for some reason, delete it
                                RemoteFile::DeleteFile(resolvedFN);
                                LOG(VB_GENERAL, LOG_ERR,
                                    QString("Image Download: Error Writing Image "
                                            "to file: %1").arg(finalfile));
                                errored = true;
                            }
                            else
                                downloaded.insert(type, info);
                        }
                    }

                    delete download;
                }
                else
                    downloaded.insert(type, info);
            }
        }
        if (errored)
        {
            QCoreApplication::postEvent(m_parent,
                    new ImageDLFailureEvent(lookup));
        }
        lookup->SetDownloads(downloaded);
        QCoreApplication::postEvent(m_parent, new ImageDLEvent(lookup));
    }

    RunEpilog();
}

ThumbnailData* MetadataImageDownload::moreThumbs()
{
    QMutexLocker lock(&m_mutex);
    ThumbnailData *ret = NULL;

    if (!m_thumbnailList.isEmpty())
        ret = m_thumbnailList.takeFirst();
    return ret;
}

QString getDownloadFilename(QString title, QString url)
{
    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/thumbcache";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    QByteArray titlearr(title.toLatin1());
    quint16 titleChecksum = qChecksum(titlearr.data(), titlearr.length());
    QByteArray urlarr(url.toLatin1());
    quint16 urlChecksum = qChecksum(urlarr.data(), urlarr.length());
    QUrl qurl(url);
    QString ext = QFileInfo(qurl.path()).suffix();
    QString basefilename = QString("thumbnail_%1_%2.%3")
                           .arg(QString::number(urlChecksum))
                           .arg(QString::number(titleChecksum)).arg(ext);

    QString outputfile = QString("%1/%2").arg(fileprefix).arg(basefilename);

    return outputfile;
}

QString getDownloadFilename(VideoArtworkType type, MetadataLookup *lookup,
                            QString url)
{
    QString basefilename;
    QString title;
    QString inter;
    uint tracknum = lookup->GetTrackNumber();
    uint season = lookup->GetSeason();
    uint episode = lookup->GetEpisode();
    QString system = lookup->GetSystem();
    if (season > 0 || episode > 0)
    {
        title = lookup->GetTitle();
        if (title.contains("/"))
            title.replace("/", "-");
        if (title.contains("?"))
            title.replace("?", "");
        if (title.contains("*"))
            title.replace("*", "");
        inter = QString(" Season %1").arg(QString::number(season));
        if (type == kArtworkScreenshot)
            inter += QString("x%1").arg(QString::number(episode));
    }
    else if (lookup->GetType() == kMetadataVideo ||
             lookup->GetType() == kMetadataRecording)
        title = lookup->GetInetref();
    else if (lookup->GetType() == kMetadataGame)
        title = QString("%1 (%2)").arg(lookup->GetTitle())
                    .arg(lookup->GetSystem());

    if (tracknum > 0)
        inter = QString(" Track %1").arg(QString::number(tracknum));
    else if (!system.isEmpty())
        inter = QString(" (%1)").arg(system);

    QString suffix;
    QUrl qurl(url);
    QString ext = QFileInfo(qurl.path()).suffix();

    if (type == kArtworkCoverart)
        suffix = "_coverart";
    else if (type == kArtworkFanart)
        suffix = "_fanart";
    else if (type == kArtworkBanner)
        suffix = "_banner";
    else if (type == kArtworkScreenshot)
        suffix = "_screenshot";
    else if (type == kArtworkPoster)
        suffix = "_poster";
    else if (type == kArtworkBackCover)
        suffix = "_backcover";
    else if (type == kArtworkInsideCover)
        suffix = "_insidecover";
    else if (type == kArtworkCDImage)
        suffix = "_cdimage";

    basefilename = title + inter + suffix + "." + ext;

    return basefilename;
}

QString getLocalWritePath(MetadataType metadatatype, VideoArtworkType type)
{
    QString ret;

    if (metadatatype == kMetadataVideo)
    {
        if (type == kArtworkCoverart)
            ret = gCoreContext->GetSetting("VideoArtworkDir");
        else if (type == kArtworkFanart)
            ret = gCoreContext->GetSetting("mythvideo.fanartDir");
        else if (type == kArtworkBanner)
            ret = gCoreContext->GetSetting("mythvideo.bannerDir");
        else if (type == kArtworkScreenshot)
            ret = gCoreContext->GetSetting("mythvideo.screenshotDir");
    }
    else if (metadatatype == kMetadataMusic)
    {
    }
    else if (metadatatype == kMetadataGame)
    {
        if (type == kArtworkCoverart)
            ret = gCoreContext->GetSetting("mythgame.boxartdir");
        else if (type == kArtworkFanart)
            ret = gCoreContext->GetSetting("mythgame.fanartdir");
        else if (type == kArtworkScreenshot)
            ret = gCoreContext->GetSetting("mythgame.screenshotdir");
    }

    return ret;
}

QString getStorageGroupURL(VideoArtworkType type, QString host)
{
    QString sgroup;
    QString ip = gCoreContext->GetSettingOnHost("BackendServerIP", host);
    uint port = gCoreContext->GetSettingOnHost("BackendServerPort",
                                               host).toUInt();

    if (type == kArtworkCoverart)
        sgroup = "Coverart";
    else if (type == kArtworkFanart)
        sgroup = "Fanart";
    else if (type == kArtworkBanner)
        sgroup = "Banners";
    else if (type == kArtworkScreenshot)
        sgroup = "Screenshots";
    else
        sgroup = "Default";

    return gCoreContext->GenMythURL(ip,port,"",sgroup);
}

QString getLocalStorageGroupPath(VideoArtworkType type, QString host)
{
    QString path;

    StorageGroup sg;

    if (type == kArtworkCoverart)
        sg.Init("Coverart", host);
    else if (type == kArtworkFanart)
        sg.Init("Fanart", host);
    else if (type == kArtworkBanner)
        sg.Init("Banners", host);
    else if (type == kArtworkScreenshot)
        sg.Init("Screenshots", host);
    else
        sg.Init("Default", host);

    path = sg.FindNextDirMostFree();

    return path;
}

void cleanThumbnailCacheDir()
{
    QString cache = QString("%1/thumbcache")
               .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (QStringList::const_iterator i = thumbs.end() - 1;
            i != thumbs.begin() - 1; --i)
    {
        QString filename = QString("%1/%2").arg(cache).arg(*i);
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(2) < MythDate::current())
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Deleting file %1")
                  .arg(filename));
            QFile::remove(filename);
        }
    }
}

