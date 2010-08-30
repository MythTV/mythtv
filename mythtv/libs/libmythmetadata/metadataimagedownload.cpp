// qt
#include <QCoreApplication>
#include <QTimer>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QEvent>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// myth
#include "mythcorecontext.h"
#include "mythuihelper.h"
#include "mythdirs.h"
#include "mythverbose.h"
#include "httpcomms.h"
#include "metadataimagedownload.h"
#include "remotefile.h"
#include "mythdownloadmanager.h"

QEvent::Type ImageDLEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type ThumbnailDLEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

MetadataImageDownload::MetadataImageDownload(QObject *parent)
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
    // Always handle thumbnails first, they're higher priority.
    ThumbnailData *thumb;
    while ((thumb = moreThumbs()) != NULL)
    {
        QString sFilename = getDownloadFilename(thumb->title, thumb->url);

        bool exists = QFile::exists(sFilename);
        if (!exists && !thumb->url.isEmpty())
            HttpComms::getHttpFile(sFilename, thumb->url, 20000, 1, 2);

        // inform parent we have thumbnail ready for it
        if (QFile::exists(sFilename) && m_parent)
        {
            VERBOSE(VB_GENERAL|VB_EXTRA,
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
            ArtworkType type = i.key();
            ArtworkInfo info = i.value();
            QString filename = getDownloadFilename( type, lookup,
                                   info.url );
            if (lookup->GetHost().isEmpty())
            {
                QString path = getLocalWritePath(lookup->GetType(), type);
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

                    VERBOSE(VB_GENERAL,
                         QString("Metadata Image Download: %1 ->%2")
                         .arg(oldurl).arg(finalfile));
                    QByteArray *download = new QByteArray();
                    GetMythDownloadManager()->download(oldurl, download);

                    QImage testImage;
                    bool didLoad = testImage.loadFromData(*download);
                    if (!didLoad)
                    {
                        VERBOSE(VB_IMPORTANT,QString("Tried to write %1, "
                                "but it appears to be an HTML redirect "
                                "(filesize %2).")
                                .arg(oldurl).arg(download->size()));
                        delete download;
                        download = NULL;
                        continue;
                    }

                    if (dest_file.open(QIODevice::WriteOnly))
                    {
                        off_t size = dest_file.write(*download, download->size());
                        if (size != download->size())
                        {
                            VERBOSE(VB_IMPORTANT,
                            QString("Image Download: Error Writing Image "
                                    "to file: %1").arg(finalfile));
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
                if (!RemoteFile::Exists(finalfile) || lookup->GetAllowOverwrites())
                {

                    if (RemoteFile::Exists(finalfile))
                    {
                        QFileInfo fi(finalfile);
                        GetMythUI()->RemoveFromCacheByFile(fi.fileName());
                        RemoteFile::DeleteFile(finalfile);
                    }

                    VERBOSE(VB_GENERAL,
                        QString("Metadata Image Download: %1 -> %2")
                        .arg(oldurl).arg(finalfile));
                    QByteArray *download = new QByteArray();
                    GetMythDownloadManager()->download(oldurl, download);

                    QImage testImage;
                    bool didLoad = testImage.loadFromData(*download);
                    if (!didLoad)
                    {
                        VERBOSE(VB_IMPORTANT,QString("Tried to write %1, "
                                "but it appears to be an HTML redirect "
                                "or corrupt file (filesize %2).")
                                .arg(oldurl).arg(download->size()));
                        delete download;
                        download = NULL;
                        continue;
                    }

                    RemoteFile *outFile = new RemoteFile(finalfile, true);
                    if (!outFile->isOpen())
                    {
                        VERBOSE(VB_IMPORTANT,
                            QString("Image Download: Failed to open "
                                    "remote file (%1) for write.  Does "
                                    "Storage Group Exist?")
                                    .arg(finalfile));
                        delete outFile;
                        outFile = NULL;
                    }
                    else
                    {
                        off_t written = outFile->Write(*download,
                                                       download->size());
                        if (written != download->size())
                        {
                            VERBOSE(VB_IMPORTANT,
                            QString("Image Download: Error Writing Image "
                                    "to file: %1").arg(finalfile));
                        }
                        else
                            downloaded.insert(type, info);
                        delete outFile;
                        outFile = NULL;
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

QString getDownloadFilename(ArtworkType type, MetadataLookup *lookup,
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
        inter = QString(" Season %1").arg(QString::number(season));
        if (type == SCREENSHOT)
            inter += QString("x%1").arg(QString::number(episode));
    }
    else if (lookup->GetType() == VID)
        title = lookup->GetInetref();
    else if (lookup->GetType() == GAME)
        title = QString("%1 (%2)").arg(lookup->GetTitle())
                    .arg(lookup->GetSystem());

    if (tracknum > 0)
        inter = QString(" Track %1").arg(QString::number(tracknum));
    else if (!system.isEmpty())
        inter = QString(" (%1)").arg(system);

    QString suffix;
    QUrl qurl(url);
    QString ext = QFileInfo(qurl.path()).suffix();

    if (type == COVERART)
        suffix = "_coverart";
    else if (type == FANART)
        suffix = "_fanart";
    else if (type == BANNER)
        suffix = "_banner";
    else if (type == SCREENSHOT)
        suffix = "_screenshot";
    else if (type == POSTER)
        suffix = "_poster";
    else if (type == BACKCOVER)
        suffix = "_backcover";
    else if (type == INSIDECOVER)
        suffix = "_insidecover";
    else if (type == CDIMAGE)
        suffix = "_cdimage";

    basefilename = title + inter + suffix + "." + ext;

    return basefilename;
}

QString getLocalWritePath(MetadataType metadatatype, ArtworkType type)
{
    QString ret;

    if (metadatatype == VID)
    {
        if (type == COVERART)
            ret = gCoreContext->GetSetting("VideoArtworkDir");
        else if (type == FANART)
            ret = gCoreContext->GetSetting("mythvideo.fanartDir");
        else if (type == BANNER)
            ret = gCoreContext->GetSetting("mythvideo.bannerDir");
        else if (type == SCREENSHOT)
            ret = gCoreContext->GetSetting("mythvideo.screenshotDir");
    }
    else if (metadatatype == MUSIC)
    {
    }
    else if (metadatatype == GAME)
    {
        if (type == COVERART)
            ret = gCoreContext->GetSetting("mythgame.boxartdir");
        else if (type == FANART)
            ret = gCoreContext->GetSetting("mythgame.fanartdir");
        else if (type == SCREENSHOT)
            ret = gCoreContext->GetSetting("mythgame.screenshotdir");
    }

    return ret;
}

QString getStorageGroupURL(ArtworkType type, QString host)
{
    QString sgroup;
    QString ip = gCoreContext->GetSettingOnHost("BackendServerIP", host);
    uint port = gCoreContext->GetSettingOnHost("BackendServerPort",
                                               host).toUInt();

    if (type == COVERART)
        sgroup = "Coverart";
    else if (type == FANART)
        sgroup = "Fanart";
    else if (type == BANNER)
        sgroup = "Banners";
    else if (type == SCREENSHOT)
        sgroup = "Screenshots";
    else
        sgroup = "Default";

    return QString("myth://%1@%2:%3/")
        .arg(sgroup)
        .arg(ip).arg(port);
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
            VERBOSE(VB_GENERAL|VB_EXTRA, QString("Deleting file %1")
                  .arg(filename));
            QFile::remove(filename);
        }
    }
}

