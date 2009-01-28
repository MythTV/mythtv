// -*- Mode: c++ -*-
/** OSDImageCache
 *  Copyright (c) 2006 by Pekka J‰‰skel‰inen, Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <stdint.h>

// Qt headers
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDataStream>

// MythTV headers
#include "mythcontext.h"
#include "osdimagecache.h"
#include "mythdirs.h"
#include "mythverbose.h"

// Print statistics of OSD image access in the destructor of OSDImageCache
//#define PRINT_OSD_IMAGE_CACHE_STATS

#define LOC QString("OSDImgCache: ")
#define LOC_ERR QString("OSDImgCache, Error: ")

uint OSDImageCache::kMaximumMemoryCacheSize = 5 * 1024 * 1024;

/**
 *  \brief The main constructor that takes the image data as arguments.
 *
 *   The image data becomes property of the OSDImageCacheValue 
 *   and will be deleted by it.
 */
OSDImageCacheValue::OSDImageCacheValue(
    QString cacheKey,
    unsigned char *yuv,     unsigned char *ybuffer,
    unsigned char *ubuffer, unsigned char *vbuffer,
    unsigned char *alpha,   QRect imagesize) :
    m_yuv(yuv),         m_ybuffer(ybuffer),
    m_ubuffer(ubuffer), m_vbuffer(vbuffer),
    m_alpha(alpha),     m_imagesize(imagesize),
    m_time(0),          m_cacheKey(cacheKey)
{
    uint yuv_size = m_imagesize.width() * m_imagesize.height() * 3 / 2;
    m_size_in_bytes =
        (sizeof(OSDImageCacheValue)) + yuv_size + 
        (m_imagesize.width() * m_imagesize.height());
}

/** \fn OSDImageCacheValue::~OSDImageCacheValue()
 *  \brief Destructor, frees the cached bitmaps.
 */
OSDImageCacheValue::~OSDImageCacheValue()
{
    delete [] m_yuv;
    m_yuv = NULL;
    delete [] m_alpha;
    m_alpha = NULL;
}

/** \fn OSDImageCache::OSDImageCache()
 *  \brief Constructor, initializes the internal cache structures.
 */
OSDImageCache::OSDImageCache() : 
    m_cacheLock(QMutex::Recursive),
    m_memHits(0), m_diskHits(0), m_misses(0), m_cacheSize(0)
{
}

/** \fn OSDImageCache::~OSDImageCache()
 *  \brief Destructor, frees all cached OSD images.
 */
OSDImageCache::~OSDImageCache() 
{
#ifdef PRINT_OSD_IMAGE_CACHE_STATS
    int totalAccess = m_memHits + m_diskHits + m_misses;
    if (totalAccess == 0)
        return;

#define LOG_PREFIX "OSDImageCache: "
    VERBOSE(VB_IMPORTANT, LOC << " Statistics: " << endl
            << LOG_PREFIX << m_imageCache.totalCost() << " bytes in cache\n" 
            << LOG_PREFIX << " memory hits: " 
            << m_memHits << ", " << m_memHits*100.0/totalAccess << "%\n"
            << LOG_PREFIX << "   disk hits: " 
            << m_diskHits << ", " << m_diskHits*100.0/totalAccess << "%\n"
            << LOG_PREFIX << "      misses: " 
            << m_misses << ", " << m_misses*100.0/totalAccess << "%");
#undef LOC_PREFIX
#endif
    Reset();
}

/**
 *  \brief Returns true if cached OSD image was found in the cache.
 *
 *  \param key The key for this image.
 *  \param useFile If true, also look from the disk cache.
 */
bool OSDImageCache::Contains(const QString &key, bool useFile) const
{
    QMutexLocker locker(&m_cacheLock);

    if (m_imageCache.find(key) != m_imageCache.end())
        return true;

    if (!useFile)
        return false;

    return InFileCache(key);
}

bool OSDImageCache::InFileCache(const QString &key) const
{
    // check if cache file exists
    QDir dir(GetConfDir() + "/osdcache/");
    QFileInfo cFile(dir.path() + "/" + key);
    if (!cFile.exists() || !cFile.isReadable())
        return false;

    // check if backing file exists
    QString orig = ExtractOriginal(key);
    if (orig.isEmpty())
        return false;

    QFileInfo oFile(orig);
    if (!oFile.exists())
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("Can't find '%1'").arg(orig));
        return false;
    }

    // if cache file is older than backing file, delete cache file
    if (cFile.lastModified() < oFile.lastModified())
    {
        cFile.dir().remove(cFile.completeBaseName());
        return false;
    } 

    return true;
}

/** \fn OSDImageCache::Get(const QString&,bool)
 *  \brief Returns OSD image data from cache.
 *
 *   This also removes the image from the cache so it won't be deleted
 *   while in use. The deletion of the taken item becomes responsibility
 *   of the client. Returns NULL if item with the given key is not found.
 *
 *  \param key The key for this image.
 *  \param useFile If true, also check the disk cache.
 */
OSDImageCacheValue *OSDImageCache::Get(const QString &key, bool useFile)
{
    QMutexLocker locker(&m_cacheLock);
    img_cache_t::iterator it = m_imageCache.find(key);
    if (it != m_imageCache.end())
    {
        m_memHits++;
        OSDImageCacheValue *tmp = *it;
        m_imageCache.erase(it);
        return tmp;
    }

    if (!useFile || !InFileCache(key))
    {
        m_misses++;
        return NULL;
    }

    QDir dir(GetConfDir() + "/osdcache/");
    QString fname = QString("%1/%2").arg(dir.path()).arg(key);
    QFile cacheFile(fname);
    if (!cacheFile.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to open read-only cache file '" + fname + "'");

        return NULL;
    }

    uint32_t imwidth  = 0;
    uint32_t imheight = 0;

    QDataStream stream(&cacheFile);
    stream >> imwidth >> imheight;   

    uint yuv_size = imwidth * imheight * 3 / 2;
    uint tot_size = (sizeof(imwidth) * 2) + yuv_size + (imwidth * imheight);

    if (cacheFile.size() != tot_size)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + key + " wrong cache file size!"
                << cacheFile.size() << " != " << tot_size);
        return NULL;
    }

    unsigned char *yuv = new unsigned char[yuv_size];
    unsigned char *alpha = new unsigned char[imwidth * imheight];
    stream.readRawData((char*)yuv,   yuv_size);
    stream.readRawData((char*)alpha, imwidth * imheight);
    cacheFile.close();

    OSDImageCacheValue* value = 
        new OSDImageCacheValue(
            key,
            yuv, yuv,
            yuv + (imwidth * imheight),
            yuv + (imwidth * imheight * 5 / 4),
            alpha, QRect(0, 0, imwidth, imheight));

    m_diskHits++;
    return value;
}

/** \fn OSDImageCache::Insert(OSDImageCacheValue*)
 *  \brief Inserts OSD image data to memory cache.
 *
 *   The item becomes property of the OSDImageCache and may be 
 *   deleted any time by it.
 *
 *  \param value The cache item.
 */
void OSDImageCache::Insert(OSDImageCacheValue *value)
{
    if (!value)
        return;

    if (value->GetSize() >= kMaximumMemoryCacheSize)
    {
        delete value;
        return;
    }

    value->m_time = QDateTime::currentDateTime().toTime_t();

    QMutexLocker locker(&m_cacheLock);
    img_cache_t::iterator it = m_imageCache.find(value->GetKey());
    if (it != m_imageCache.end())
    {
        m_cacheSize -= (*it)->GetSize();
        delete *it;
        *it = value;
        m_cacheSize += value->GetSize();
    }
    else
    {
        m_imageCache[value->GetKey()] = value;
        m_cacheSize += value->GetSize();
    }

    // delete the oldest cached images until we fall below threshold.
    while (m_cacheSize >= kMaximumMemoryCacheSize && m_imageCache.size())
    {
        img_cache_t::iterator mit = m_imageCache.begin();
        uint min_time = (*mit)->m_time;
        img_cache_t::iterator it = m_imageCache.begin();
        for ( ; it != m_imageCache.end(); ++it)
        {
            if ((*it)->m_time < min_time)
            {
                min_time = (*it)->m_time;
                mit = it;
            }
        }

        m_cacheSize -= (*mit)->GetSize();
        delete *mit;
        m_imageCache.erase(mit);
    }
}


/** \fn OSDImageCache::SaveToDisk(const OSDImageCacheValue*)
 *  \brief Saves OSD image data to disk cache.
 *
 *   Item is not written to the memory cache, i.e., it stays as
 *   property of the client.
 *
 *  \param value The cached OSD image to save.
 */
void OSDImageCache::SaveToDisk(const OSDImageCacheValue *value)
{
    if (InFileCache(value->GetKey()))
        return;

    QDir dir(GetConfDir() + "/osdcache/");
    if (!dir.exists() && !dir.mkdir(dir.path()))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Creating osdcache directory failed.");
        return;
    }

    QFile cacheFile(dir.path() + "/" + value->GetKey());
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Creating osdcache file failed.");
        return;
    }

    uint32_t imwidth  = value->m_imagesize.width();
    uint32_t imheight = value->m_imagesize.height();
    uint     yuv_size = imwidth * imheight * 3 / 2;

    QDataStream stream(&cacheFile);
    stream << imwidth << imheight;   
    stream.writeRawData((const char*)value->m_yuv, yuv_size);
    stream.writeRawData((const char*)value->m_alpha, imwidth * imheight);
    cacheFile.close();
}

/** \fn OSDImageCache::CreateKey(const QString&,float,float,int,int)
 *  \brief Generates a cache key from the given OSD image parameters.
 *
 *   The returned key is a string that can be safely used as a file name.
 */
QString OSDImageCache::CreateKey(const QString &filename, float wmult, 
                                 float hmult, int scalew, int scaleh)
{
    QString tmp = filename;
    return QString("cache_%1@%2_%3_%4_%5").arg(tmp.replace(QChar('/'), "+"))
        .arg(wmult).arg(hmult).arg(scalew).arg(scaleh);
}

QString OSDImageCache::ExtractOriginal(const QString &key)
{
    QString tmp0 = key.mid(6);
    QString tmp1 = tmp0.left(tmp0.indexOf("@"));
    QString tmp2 = tmp1.replace(QChar('+'), "/");
    return tmp2;
}

void OSDImageCache::Reset(void)
{
    QMutexLocker locker(&m_cacheLock);

    img_cache_t::iterator it = m_imageCache.begin();
    for (; it != m_imageCache.end(); ++it)
        delete *it;
    m_imageCache.clear();
    m_cacheSize = 0;
}
