#ifndef MYTHUICACHE_H
#define MYTHUICACHE_H

// Qt
#include <QMap>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif

// MythTV
#include "mythchrono.h"
#include "mythimage.h"

enum ImageCacheMode
{
    kCacheNormal          = 0x0,
    kCacheIgnoreDisk      = 0x1,
    kCacheCheckMemoryOnly = 0x2,
    kCacheForceStat       = 0x4,
};

class MThreadPool;

class MUI_PUBLIC MythUIThemeCache
{
  public:
    MythUIThemeCache();
   ~MythUIThemeCache();

    void        UpdateImageCache();
    void        SetScreenSize(QSize Size);
    void        ClearThemeCacheDir();
    QString     GetThemeCacheDir();
    MythImage*  LoadCacheImage(QString File, const QString& Label,
                               MythPainter* Painter, ImageCacheMode cacheMode = kCacheNormal);
    MythImage*  CacheImage(const QString& URL, MythImage* Image, bool NoDisk = false);
    void        RemoveFromCacheByFile(const QString& File);
    bool        IsImageInCache(const QString& URL);
    void        IncludeInCacheSize(MythImage* Image);
    void        ExcludeFromCacheSize(MythImage* Image);
    MThreadPool* GetImageThreadPool();

  private:
    QString     GetCacheDirByUrl(const QString& URL);
    void        RemoveFromCacheByURL(const QString& URL);
    MythImage*  GetImageFromCache(const QString& URL);
    void        ClearOldImageCache();
    void        RemoveCacheDir(const QString& Dir);
    static void PruneCacheDir(const QString& Dir);

    QMap<QString, MythImage *> m_imageCache;
    QMap<QString, SystemTime> m_cacheTrack;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex m_cacheLock                    { QMutex::Recursive };
#else
    QRecursiveMutex m_cacheLock;
#endif
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
    QAtomicInt m_cacheSize                { 0 };
    QAtomicInt m_maxCacheSize             { 30 * 1024 * 1024 };
#else
    QAtomicInteger<qint64> m_cacheSize    { 0 };
    QAtomicInteger<qint64> m_maxCacheSize { 30LL * 1024 * 1024 };
#endif
    QString m_themecachedir;
    QSize   m_cacheScreenSize;
    MThreadPool* m_imageThreadPool        { nullptr };
};

#endif
