// Qt
#include <QDir>
#include <QDateTime>

// MythTV
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remotefile.h"

#include "mythuithemecache.h"

// Std
#include <unistd.h>
#include <sys/stat.h>

// Temp for DEFAULT_UI_THEME, ImageCacheMode etc
#include "mythuihelper.h"

#define LOC QString("UICache: ")

MythUIThemeCache::MythUIThemeCache()
  : m_imageThreadPool(new MThreadPool("MythUIHelper"))
{
    m_maxCacheSize.fetchAndStoreRelease(GetMythDB()->GetNumSetting("UIImageCacheSize", 30) * 1024LL * 1024);
    LOG(VB_GUI, LOG_INFO, LOC + QString("MythUI Image Cache size set to %1 bytes")
        .arg(m_maxCacheSize.fetchAndAddRelease(0)));
}

MythUIThemeCache::~MythUIThemeCache()
{
    PruneCacheDir(GetRemoteCacheDir());
    PruneCacheDir(GetThumbnailDir());

    QMutableMapIterator<QString, MythImage *> i(m_imageCache);
    while (i.hasNext())
    {
        i.next();
        i.value()->SetIsInCache(false);
        i.value()->DecrRef();
        i.remove();
    }
    m_cacheTrack.clear();

    delete m_imageThreadPool;
}

void MythUIThemeCache::SetScreenSize(QSize Size)
{
    m_cacheScreenSize = Size;
}

void MythUIThemeCache::ClearThemeCacheDir()
{
    m_themecachedir.clear();
}

void MythUIThemeCache::UpdateImageCache()
{
    QMutexLocker locker(&m_cacheLock);

    QMutableMapIterator<QString, MythImage *> i(m_imageCache);

    while (i.hasNext())
    {
        i.next();
        i.value()->SetIsInCache(false);
        i.value()->DecrRef();
        i.remove();
    }

    m_cacheTrack.clear();
    m_cacheSize.fetchAndStoreOrdered(0);

    ClearOldImageCache();
    PruneCacheDir(GetRemoteCacheDir());
    PruneCacheDir(GetThumbnailDir());
}

void MythUIThemeCache::ClearOldImageCache()
{
    m_themecachedir = GetThemeCacheDir();

    QString themecachedir = m_themecachedir;

    m_themecachedir += '/';

    QDir dir(GetThemeBaseCacheDir());
    dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = dir.entryInfoList();

    QMap<QDateTime, QString> dirtimes;

    for (const auto & fi : std::as_const(list))
    {
        if (fi.isDir() && !fi.isSymLink())
        {
            if (fi.absoluteFilePath() == themecachedir)
                continue;
            dirtimes[fi.lastModified()] = fi.absoluteFilePath();
        }
    }

    // Cache two themes/resolutions to allow sampling other themes without
    // incurring a penalty. Especially for those writing new themes or testing
    // changes of an existing theme. The space used is neglible when compared
    // against the average video
    while (static_cast<size_t>(dirtimes.size()) >= 2)
    {
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("Removing cache dir: %1")
            .arg(dirtimes.begin().value()));

        RemoveCacheDir(dirtimes.begin().value());
        dirtimes.erase(dirtimes.begin());
    }

    for (const auto & dirtime : std::as_const(dirtimes))
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("Keeping cache dir: %1").arg(dirtime));
}

void MythUIThemeCache::RemoveCacheDir(const QString& Dir)
{
    QString cachedirname = GetThemeBaseCacheDir();

    if (!Dir.startsWith(cachedirname))
        return;

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Removing stale cache dir: %1").arg(Dir));

    QDir dir(Dir);
    if (!dir.exists())
        return;

    dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = dir.entryInfoList();
    for (const auto & fi : std::as_const(list))
    {
        if (fi.isFile() && !fi.isSymLink())
        {
            QFile file(fi.absoluteFilePath());
            file.remove();
        }
        else if (fi.isDir() && !fi.isSymLink())
        {
            RemoveCacheDir(fi.absoluteFilePath());
        }
    }

    dir.rmdir(Dir);
}

/**
 * Remove all files in the cache that haven't been accessed in a user
 * configurable number of days.  The default number of days is seven.
 *
 * \param dirname The directory to prune.
 */
void MythUIThemeCache::PruneCacheDir(const QString& dirname)
{
    int days = GetMythDB()->GetNumSetting("UIDiskCacheDays", 7);
    if (days == -1)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Pruning cache directory: %1 is disabled")
            .arg(dirname));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Pruning cache directory: %1").arg(dirname));
    QDateTime cutoff = MythDate::current().addDays(-days);
    qint64 cutoffsecs = cutoff.toSecsSinceEpoch();

    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("Removing files not accessed since %1")
        .arg(cutoff.toLocalTime().toString(Qt::ISODate)));

    // Trying to save every cycle possible within this loop.  The
    // stat() call seems significantly faster than the fi.fileRead()
    // method.  The documentation for QFileInfo says that the
    // fi.absoluteFilePath() method has to query the file system, so
    // use fi.filePath() method here and then add the directory if
    // needed.  Using dir.entryList() and adding the dirname each time
    // is also slower just using dir.entryInfoList().
    QDir dir(dirname);
    dir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::NoSort);
    QFileInfoList entries = dir.entryInfoList();
    int kept = 0;
    int deleted = 0;
    int errors = 0;
    for (const QFileInfo & fi : std::as_const(entries))
    {
        struct stat buf {};
        QString fullname = fi.filePath();
        if (not fullname.startsWith('/'))
            fullname = dirname + "/" + fullname;
        int rc = stat(fullname.toLocal8Bit(), &buf);
        if (rc >= 0)
        {
            if (buf.st_atime < cutoffsecs)
            {
                deleted += 1;
                LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC + QString("%1 Delete %2")
                    .arg(fi.lastRead().toLocalTime().toString(Qt::ISODate), fi.fileName()));
                unlink(qPrintable(fullname));
            }
            else
            {
                kept += 1;
                LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC + QString("%1 Keep   %2")
                    .arg(fi.lastRead().toLocalTime().toString(Qt::ISODate), fi.fileName()));
            }
        }
        else
        {
            errors += 1;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Kept %1 files, deleted %2 files, stat error on %3 files")
        .arg(kept).arg(deleted).arg(errors));
}

QString MythUIThemeCache::GetThemeCacheDir()
{
    static QString s_oldcachedir;
    QString tmpcachedir = GetThemeBaseCacheDir() + "/" +
                          GetMythDB()->GetSetting("Theme", DEFAULT_UI_THEME) + "." +
                          QString::number(m_cacheScreenSize.width()) + "." +
                          QString::number(m_cacheScreenSize.height());

    if (tmpcachedir != s_oldcachedir)
    {
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("Creating cache dir: %1").arg(tmpcachedir));
        QDir dir;
        dir.mkdir(tmpcachedir);
        s_oldcachedir = tmpcachedir;
    }
    return tmpcachedir;
}

/**
 * Look at the url being read and decide whether the cached version
 * should go into the theme cache or the thumbnail cache.
 *
 * \param url The resource being read.
 * \returns The path name of the appropriate cache directory.
 */
QString MythUIThemeCache::GetCacheDirByUrl(const QString& URL)
{
    if (URL.startsWith("myth:") || URL.startsWith("-"))
        return GetThumbnailDir();
    return GetThemeCacheDir();
}

MythImage* MythUIThemeCache::LoadCacheImage(QString File, const QString& Label,
                                        MythPainter *Painter,
                                        ImageCacheMode cacheMode)
{
    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
        QString("LoadCacheImage(%1,%2)").arg(File, Label));

    if (File.isEmpty() || Label.isEmpty())
        return nullptr;

    if (!(kCacheForceStat & cacheMode))
    {
        // Some screens include certain images dozens or even hundreds of
        // times.  Even if the image is in the cache, there is still a
        // stat system call on the original file to see if it has changed.
        // This code relaxes the original-file check so that the check
        // isn't repeated if it was already done within kImageCacheTimeout
        // seconds.

        // This only applies to the MEMORY cache
        constexpr std::chrono::seconds kImageCacheTimeout { 60s };
        SystemTime now = SystemClock::now();

        QMutexLocker locker(&m_cacheLock);

        if (m_imageCache.contains(Label) &&
            m_cacheTrack[Label] + kImageCacheTimeout > now)
        {
            m_imageCache[Label]->IncrRef();
            return m_imageCache[Label];
        }
    }

    MythImage *ret = nullptr;

    // Check Memory Cache
    ret = GetImageFromCache(Label);

    // If the image is in the memory or we are not ignoring the disk cache
    // then proceed to check whether the source file is newer than our cached
    // copy
    if (ret || !(cacheMode & kCacheIgnoreDisk))
    {
        // Create url to image in disk cache
        QString cachefilepath;
        cachefilepath = GetCacheDirByUrl(Label) + '/' + Label;
        QFileInfo cacheFileInfo(cachefilepath);

        // If the file isn't in the disk cache, then we don't want to bother
        // checking the last modified times of the original
        if (!cacheFileInfo.exists())
            return nullptr;

        // Now compare the time on the source versus our cached copy
        QDateTime srcLastModified;

        // For internet images this involves querying the headers of the remote
        // image. This is slow even without redownloading the whole image
        if ((File.startsWith("http://")) ||
            (File.startsWith("https://")) ||
            (File.startsWith("ftp://")))
        {
            // If the image is in the memory cache then skip the last modified
            // check, since memory cached images are loaded in the foreground
            // this can cause an intolerable delay. The images won't stay in
            // the cache forever and so eventually they will be checked.
            if (ret)
                srcLastModified = cacheFileInfo.lastModified();
            else
                srcLastModified = GetMythDownloadManager()->GetLastModified(File);
        }
        else if (File.startsWith("myth://"))
        {
            srcLastModified = RemoteFile::LastModified(File);
        }
        else
        {
            if (!GetMythUI()->FindThemeFile(File))
                return nullptr;

            QFileInfo original(File);

            if (original.exists())
                srcLastModified = original.lastModified();
        }

        // Now compare the timestamps, if the cached image is newer than the
        // source image we can use it, otherwise we want to remove it from the
        // cache
        if (cacheFileInfo.lastModified() >= srcLastModified)
        {
            // If we haven't already loaded the image from the memory cache
            // and we're not ignoring the disk cache, then it's time to load
            // it from there instead
            if (!ret && (cacheMode == kCacheNormal))
            {

                if (Painter)
                {
                    ret = Painter->GetFormatImage();

                    // Load file from disk cache to memory cache
                    if (ret->Load(cachefilepath))
                    {
                        // Add to ram cache, and skip saving to disk since that is
                        // where we found this in the first place.
                        CacheImage(Label, ret, true);
                    }
                    else
                    {
                        LOG(VB_GUI | VB_FILE, LOG_WARNING, LOC +
                            QString("LoadCacheImage: Could not load: %1")
                            .arg(cachefilepath));

                        ret->SetIsInCache(false);
                        ret->DecrRef();
                        ret = nullptr;
                    }
                }
            }
        }
        else
        {
            ret = nullptr;
            // If file has changed on disk, then remove it from the memory
            // and disk cache
            RemoveFromCacheByURL(Label);
        }
    }

    return ret;
}

MythImage* MythUIThemeCache::GetImageFromCache(const QString& URL)
{
    QMutexLocker locker(&m_cacheLock);

    if (m_imageCache.contains(URL))
    {
        m_cacheTrack[URL] = SystemClock::now();
        m_imageCache[URL]->IncrRef();
        return m_imageCache[URL];
    }

    /*
        if (QFileInfo(URL).exists())
        {
            MythImage *im = GetMythPainter()->GetFormatImage();
            im->Load(URL,false);
            return im;
        }
    */

    return nullptr;
}

MythImage *MythUIThemeCache::CacheImage(const QString& URL, MythImage* Image, bool NoDisk)
{
    if (!Image)
        return nullptr;

    if (!NoDisk)
    {
        QString dstfile = GetCacheDirByUrl(URL) + '/' + URL;
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("Saved to Cache (%1)").arg(dstfile));
        // Save to disk cache
        Image->save(dstfile, "PNG");
    }

    // delete the oldest cached images until we fall below threshold.
    QMutexLocker locker(&m_cacheLock);

    while ((m_cacheSize.fetchAndAddOrdered(0) + Image->sizeInBytes()) >=
           m_maxCacheSize.fetchAndAddOrdered(0) && !m_imageCache.empty())
    {
        QMap<QString, MythImage *>::iterator it = m_imageCache.begin();
        auto oldestTime = SystemClock::now();
        QString oldestKey = it.key();

        int count = 0;

        for (; it != m_imageCache.end(); ++it)
        {
            if (m_cacheTrack[it.key()] < oldestTime)
            {
                if ((2 == it.value()->IncrRef()) && (it.value() != Image))
                {
                    oldestTime = m_cacheTrack[it.key()];
                    oldestKey = it.key();
                    count++;
                }
                it.value()->DecrRef();
            }
        }

        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +QString("%1 images are eligible for expiry").arg(count));
        if (count > 0)
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("Cache too big (%1), removing :%2:")
                .arg(m_cacheSize.fetchAndAddOrdered(0) + Image->sizeInBytes())
                .arg(oldestKey));

            m_imageCache[oldestKey]->SetIsInCache(false);
            m_imageCache[oldestKey]->DecrRef();
            m_imageCache.remove(oldestKey);
            m_cacheTrack.remove(oldestKey);
        }
        else
        {
            break;
        }
    }

    QMap<QString, MythImage *>::iterator it = m_imageCache.find(URL);

    if (it == m_imageCache.end())
    {
        Image->IncrRef();
        m_imageCache[URL] = Image;
        m_cacheTrack[URL] = SystemClock::now();

        Image->SetIsInCache(true);
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("NOT IN RAM CACHE, Adding, and adding to size :%1: :%2:").arg(URL)
        .arg(Image->sizeInBytes()));
    }

    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("MythUIHelper::CacheImage : Cache Count = :%1: size :%2:")
        .arg(m_imageCache.count()).arg(m_cacheSize.fetchAndAddRelaxed(0)));

    return m_imageCache[URL];
}

void MythUIThemeCache::RemoveFromCacheByURL(const QString& URL)
{
    QMutexLocker locker(&m_cacheLock);
    QMap<QString, MythImage *>::iterator it = m_imageCache.find(URL);

    if (it != m_imageCache.end())
    {
        m_imageCache[URL]->SetIsInCache(false);
        m_imageCache[URL]->DecrRef();
        m_imageCache.remove(URL);
        m_cacheTrack.remove(URL);
    }

    QString dstfile = GetCacheDirByUrl(URL) + '/' + URL;
    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("RemoveFromCacheByURL removed :%1: from cache").arg(dstfile));
    QFile::remove(dstfile);
}

void MythUIThemeCache::RemoveFromCacheByFile(const QString& File)
{
    QList<QString>::iterator it;

    QString partialKey = File;
    partialKey.replace('/', '-');

    m_cacheLock.lock();
    QList<QString> m_imageCacheKeys = m_imageCache.keys();
    m_cacheLock.unlock();

    for (it = m_imageCacheKeys.begin(); it != m_imageCacheKeys.end(); ++it)
    {
        if ((*it).contains(partialKey))
            RemoveFromCacheByURL(*it);
    }

    // Loop through files to cache any that were not caught by
    // RemoveFromCacheByURL
    QDir dir(GetThemeCacheDir());
    QFileInfoList list = dir.entryInfoList();

    for (const auto & fileInfo : std::as_const(list))
    {
        if (fileInfo.fileName().contains(partialKey))
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
                QString("RemoveFromCacheByFile removed: %1: from cache")
                .arg(fileInfo.fileName()));

            if (!dir.remove(fileInfo.fileName()))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to delete %1 from the theme cache")
                    .arg(fileInfo.fileName()));
            }
        }
    }
}

bool MythUIThemeCache::IsImageInCache(const QString& URL)
{
    QMutexLocker locker(&m_cacheLock);
    if (m_imageCache.contains(URL))
        return true;
    if (QFileInfo::exists(URL))
        return true;
    return false;
}

void MythUIThemeCache::IncludeInCacheSize(MythImage* Image)
{
    if (Image)
        m_cacheSize.fetchAndAddOrdered(Image->sizeInBytes());
}

void MythUIThemeCache::ExcludeFromCacheSize(MythImage* Image)
{
    if (Image)
        m_cacheSize.fetchAndAddOrdered(-Image->sizeInBytes());
}

MThreadPool* MythUIThemeCache::GetImageThreadPool()
{
    return m_imageThreadPool;
}

