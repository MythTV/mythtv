// -*- Mode: c++ -*-
/** OSDImageCache
 *  Copyright (c) 2006 by Pekka Jaaskelainen, Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <stdint.h>

// Qt headers
#include <qdir.h>
#include <qfile.h>
#include <qdatastream.h>

// MythTV headers
#include "mythcontext.h"
#include "osdimagecache.h"

#define LOC QString("OSDImgCache: ")
#define LOC_ERR QString("OSDImgCache, Error: ")

/** \fn OSDImageCache::Contains(const QString&,bool)
 *  \brief Returns true if cached OSD image was found in the cache.
 *
 *  \param imageKey The key for this image.
 *  \param useDiskCache If true, also look from the disk cache.
 */
bool OSDImageCache::Contains(const QString &key, bool useFile) const
{
    QMutexLocker locker(&m_cacheLock);

    if (m_imageCache.find(key) != m_imageCache.end())
        return true;

    if (!useFile)
        return false;

    QDir dir(MythContext::GetConfDir() + "/osdcache/");
    QFile cacheFile(dir.path() + "/" + key);
    return cacheFile.exists();
}

/** \fn OSDImageCache::Load(const QString&,bool)
 *  \brief Returns OSD image data from cache.
 *
 *  \param key The key for this image.
 *  \param useFile If true, also look from the disk cache.
 */
OSDImageCacheValue OSDImageCache::Load(const QString &key, bool useFile)
{
    {
        QMutexLocker locker(&m_cacheLock);
        img_cache_t::const_iterator it = m_imageCache.find(key);
        if (it != m_imageCache.end())
            return *it;
    }

    OSDImageCacheValue val;

    if (!useFile)
        return val;

    QDir dir(MythContext::GetConfDir() + "/osdcache/");
    QFile cacheFile(dir.path() + "/" + key);
    if (!cacheFile.exists())
        return val;

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
        return val;
    }

    unsigned char *tmpA = new unsigned char[yuv_size];
    unsigned char *tmpB = new unsigned char[imwidth * imheight];
    stream.readRawBytes((char*)tmpA, yuv_size);
    stream.readRawBytes((char*)tmpB, imwidth * imheight);
    cacheFile.close();

    OSDImageCacheValue value(tmpA, tmpA,
                             tmpA + (imwidth * imheight),
                             tmpA + (imwidth * imheight * 5 / 4),
                             tmpB, QRect(0, 0, imwidth, imheight));

    Save(key, false, value);

    return value;
}

/** \fn OSDImageCache::Save(const QString&,bool,const OSDImageCacheValue&)
 *  \brief Saves OSD image data to cache.
 *
 *  \param key The key for this image.
 *  \param useFile If true, also save to the disk cache.
 */
void OSDImageCache::Save(const QString &key, bool useFile,
                         const OSDImageCacheValue &value)
{
    if (Contains(key, useFile))
        return;

    m_cacheLock.lock();
    m_imageCache[key] = value;
    m_cached = true;
    m_cacheLock.unlock();

    if (!useFile)
        return;

    QDir dir(MythContext::GetConfDir() + "/osdcache/");
    if (!dir.exists() && !dir.mkdir(dir.path()))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Creating osdcache directory failed.");
        return;
    }

    QFile cacheFile(dir.path() + "/" + key);
    if (!cacheFile.open(IO_WriteOnly | IO_Truncate))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Creating osdcache file failed.");
        return;
    }

    uint32_t imwidth  = value.m_imagesize.width();
    uint32_t imheight = value.m_imagesize.height();
    uint     yuv_size = imwidth * imheight * 3 / 2;

    QDataStream stream(&cacheFile);
    stream << imwidth << imheight;   
    stream.writeRawBytes((const char*)value.m_yuv, yuv_size);
    stream.writeRawBytes((const char*)value.m_alpha, imwidth * imheight);
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
    return QString("cache_%1_%2_%3_%4_%5")
        .arg(filename).arg(wmult).arg(hmult).arg(scalew).arg(scaleh)
        .replace(QChar('/'), ".").replace(QChar(' '), ".")
        .replace(QChar(' '), ".");
}

void OSDImageCache::Reset(void)
{
    // cleanup the OSD image cache in memory
    QMutexLocker locker(&m_cacheLock);
    QMapIterator<QString, OSDImageCacheValue> i = m_imageCache.begin();
    while (i != m_imageCache.end())
    {
        OSDImageCacheValue& value = i.data();
        delete [] value.m_yuv;
        delete [] value.m_alpha;
        ++i;
    }
    m_imageCache.clear();
    m_cached = false;
}
