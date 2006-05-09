// -*- Mode: c++ -*-

// POSIX headers
#include <stdint.h>

// Qt headers
#include <qmap.h>
#include <qrect.h>
#include <qmutex.h>
#include <qstring.h>
#include <qasciicache.h>

class OSDImageCacheValue
{
  public:
    OSDImageCacheValue(QString cacheKey,
		       unsigned char *yuv,     unsigned char *ybuffer,
                       unsigned char *ubuffer, unsigned char *vbuffer,
                       unsigned char *alpha,   QRect imagesize);

    virtual ~OSDImageCacheValue();

    uint    GetSize(void) const { return m_size_in_bytes; }
    QString GetKey(void)  const { return m_cacheKey;      }

  public:
    unsigned char *m_yuv;
    unsigned char *m_ybuffer;
    unsigned char *m_ubuffer;
    unsigned char *m_vbuffer;
    unsigned char *m_alpha;
    QRect          m_imagesize;

  private:
    uint           m_size_in_bytes;
    QString        m_cacheKey;
};

typedef QAsciiCache<OSDImageCacheValue> img_cache_t;

class OSDImageCache
{
  public:
    OSDImageCache();
    virtual ~OSDImageCache();

    bool InFileCache(const QString &key) const;

    bool Contains(const QString &key, bool useFile) const;

    OSDImageCacheValue *Get(const QString &key, bool useFile);

    void Insert(OSDImageCacheValue* value);

    void SaveToDisk(const OSDImageCacheValue *value);

    void Reset(void);

    static QString CreateKey(const QString &filename,
                             float wmult, float hmult,
                             int scalew,  int scaleh);

    static QString ExtractOriginal(const QString &key);

  private:
    mutable QMutex m_cacheLock;
    img_cache_t    m_imageCache;
    int            m_memHits;
    int            m_diskHits;
    int            m_misses;

    /// Limit on the maximum total size of OSD images cached in *memory*.
    static uint    kMaximumMemoryCacheSize;
};
