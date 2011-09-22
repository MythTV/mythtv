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
#include "httpcomms.h"
#include "storagegroup.h"
#include "metadataimagedownload.h"
#include "remotefile.h"
#include "mythdownloadmanager.h"
#include "mythlogging.h"

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
    m_mutex.lock();
    ThumbnailData *id = new ThumbnailData();
    id->title = title;
    id->data = data;
    id->url = url;
    m_thumbnailList.append(id);
    if (!isRunning())
        start();
    m_mutex.unlock();
}

void MetadataImageDownload::addDownloads(MetadataLookup *lookup)
{
    m_mutex.lock();
    m_downloadList.append(lookup);
    if (!isRunning())
        start();
    m_mutex.unlock();
}

void MetadataImageDownload::cancel()
{
    m_mutex.lock();
    qDeleteAll(m_thumbnailList);
    m_thumbnailList.clear();
    qDeleteAll(m_downloadList);
    m_downloadList.clear();
    m_mutex.unlock();
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
            GetMythDownloadManager()->download(thumb->url, sFilename);

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

    MetadataLookup *lookup;
    while ((lookup = moreDownloads()) != NULL)
    {
        DownloadMap downloads = lookup->GetDownloads();
        DownloadMap downloaded;

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
                        QCoreApplication::postEvent(m_parent,
                                    new ImageDLFailureEvent(lookup));
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
                        QCoreApplication::postEvent(m_parent,
                                    new ImageDLFailureEvent(lookup));
                        continue;
                    }

                    if (dest_file.open(QIODevice::WriteOnly))
                    {
                        off_t size = dest_file.write(*download,
                                                     download->size());
                        if (size != download->size())
                        {
                            LOG(VB_GENERAL, LOG_ERR,
                                QString("Image Download: Error Writing Image "
                                        "to file: %1").arg(finalfile));
                            QCoreApplication::postEvent(m_parent,
                                        new ImageDLFailureEvent(lookup));
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
                    (lookup->GetHost() == gCoreContext->GetSettingOnHost("BackendServerIP",
                                                             gCoreContext->GetHostName())))
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
                        QCoreApplication::postEvent(m_parent,
                                    new ImageDLFailureEvent(lookup));
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
                            QCoreApplication::postEvent(m_parent,
                                        new ImageDLFailureEvent(lookup));
                        }
                        else
                        {
                            off_t written = outFile->Write(*download,
                                                           download->size());
                            if (written != download->size())
                            {
                                LOG(VB_GENERAL, LOG_ERR,
                                    QString("Image Download: Error Writing Image "
                                            "to file: %1").arg(finalfile));
                                QCoreApplication::postEvent(m_parent,
                                        new ImageDLFailureEvent(lookup));
                            }
                            else
                                downloaded.insert(type, info);
                            delete outFile;
                            outFile = NULL;
                        }
                    }
                    else
                    {
                        QFile dest_file(resolvedFN);
                        if (dest_file.open(QIODevice::WriteOnly))
                        {
                            off_t size = dest_file.write(*download,
                                                         download->size());
                            if (size != download->size())
                            {
                                LOG(VB_GENERAL, LOG_ERR,
                                    QString("Image Download: Error Writing Image "
                                            "to file: %1").arg(finalfile));
                                QCoreApplication::postEvent(m_parent,
                                            new ImageDLFailureEvent(lookup));
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
        lookup->SetDownloads(downloaded);
        QCoreApplication::postEvent(m_parent, new ImageDLEvent(lookup));
    }

    RunEpilog();
}

ThumbnailData* MetadataImageDownload::moreThumbs()
{
    ThumbnailData *ret = NULL;
    m_mutex.lock();
    if (!m_thumbnailList.isEmpty())
        ret = m_thumbnailList.takeFirst();
    m_mutex.unlock();
    return ret;
}

MetadataLookup* MetadataImageDownload::moreDownloads()
{
    MetadataLookup *ret = NULL;
    m_mutex.lock();
    if (!m_downloadList.isEmpty())
        ret = m_downloadList.takeFirst();
    m_mutex.unlock();
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
        if (lastmod.addDays(2) < QDateTime::currentDateTime())
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Deleting file %1")
                  .arg(filename));
            QFile::remove(filename);
        }
    }
}

