// -*- Mode: c++ -*-
/** OSDImageCache
 *  Copyright (c) 2006 by Pekka J‰‰skel‰inen, Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <stdint.h>

// Qt headers
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qdatastream.h>
#include <qdeepcopy.h>

// MythTV headers
#include "mythcontext.h"
#include "osdimagecache.h"

// Print statistics of OSD image access in the destructor of OSDImageCache
//#define PRINT_OSD_IMAGE_CACHE_STATS

#define LOC QString("OSDImgCache: ")
#define LOC_ERR QString("OSDImgCache, Error: ")

uint OSDImageCache::kMaximumMemoryCacheSize = 5 * 1024 * 1024;

/** \fn OSDImageCacheValue::OSDImageCacheValue(unsigned char*,unsigned char*,unsighed char*,unsigned char*,unsighed char*, QRect)
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
    m_cacheKey(QDeepCopy<QString>(cacheKey))
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
    m_cacheLock(true), m_imageCache(kMaximumMemoryCacheSize, 50),
    m_memHits(0), m_diskHits(0), m_misses(0) 
{
    // When the cache gets too large, items are
    // automatically deleted from it in LRU order.
    m_imageCache.setAutoDelete(true);
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

    if (m_imageCache.find(key) != NULL)
        return true;

    if (!useFile)
        return false;

    return InFileCache(key);
}

bool OSDImageCache::InFileCache(const QString &key) const
{
    // check if cache file exists
    QDir dir(MythContext::GetConfDir() + "/osdcache/");
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
        cFile.dir().remove(cFile.baseName(true));
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
    OSDImageCacheValue* item = m_imageCache.find(key);
    if (item)
    {
        m_memHits++;
        return m_imageCache.take(key);
    }

    if (!useFile || !InFileCache(key))
    {
        m_misses++;
        return NULL;
    }

    QDir dir(MythContext::GetConfDir() + "/osdcache/");
    QFile cacheFile(dir.path() + "/" + key);
    cacheFile.open(IO_ReadOnly);
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
    stream.readRawBytes((char*)yuv,   yuv_size);
    stream.readRawBytes((char*)alpha, imwidth * imheight);
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

    QMutexLocker locker(&m_cacheLock);
    if (!m_imageCache.insert(value->GetKey(), value, value->GetSize()))
    {
        VERBOSE(VB_IMPORTANT, 
                LOC_ERR + QString("inserting image to memory cache failed"));
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

    QDir dir(MythContext::GetConfDir() + "/osdcache/");
    if (!dir.exists() && !dir.mkdir(dir.path()))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Creating osdcache directory failed.");
        return;
    }

    QFile cacheFile(dir.path() + "/" + value->GetKey());
    if (!cacheFile.open(IO_WriteOnly | IO_Truncate))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Creating osdcache file failed.");
        return;
    }

    uint32_t imwidth  = value->m_imagesize.width();
    uint32_t imheight = value->m_imagesize.height();
    uint     yuv_size = imwidth * imheight * 3 / 2;

    QDataStream stream(&cacheFile);
    stream << imwidth << imheight;   
    stream.writeRawBytes((const char*)value->m_yuv, yuv_size);
    stream.writeRawBytes((const char*)value->m_alpha, imwidth * imheight);
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
    QString tmp1 = tmp0.left(tmp0.find("@"));
    QString tmp2 = tmp1.replace(QChar('+'), "/");
    return tmp2;
}

void OSDImageCache::Reset(void)
{
    QMutexLocker locker(&m_cacheLock);
    // this also deletes the images due to setAutoDelete(true)
    m_imageCache.clear();
}
