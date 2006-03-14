// -*- Mode: c++ -*-

// Qt headers
#include <qmap.h>
#include <qrect.h>
#include <qmutex.h>
#include <qstring.h>

class OSDImageCacheValue
{
  public:
    OSDImageCacheValue(unsigned char *yuv,     unsigned char *ybuffer,
                       unsigned char *ubuffer, unsigned char *vbuffer,
                       unsigned char *alpha,   QRect imagesize) :
        m_yuv(yuv),         m_ybuffer(ybuffer),
        m_ubuffer(ubuffer), m_vbuffer(vbuffer),
        m_alpha(alpha),     m_imagesize(imagesize) {}

    OSDImageCacheValue() :
        m_yuv(NULL),     m_ybuffer(NULL),
        m_ubuffer(NULL), m_vbuffer(NULL),
        m_alpha(NULL),   m_imagesize(0,0,0,0) {}

    bool IsValid(void) const { return m_alpha; }

    unsigned char *m_yuv;
    unsigned char *m_ybuffer;
    unsigned char *m_ubuffer;
    unsigned char *m_vbuffer;
    unsigned char *m_alpha;
    QRect          m_imagesize;
};

typedef QMap<QString, OSDImageCacheValue> img_cache_t;

class OSDImageCache
{
  public:
    OSDImageCache() : m_cacheLock(true), m_cached(false) {}

    bool InMemCache(void) const { return m_cached; }
    bool InFileCache(const QString &key) const;

    bool Contains(const QString &key, bool useFile) const;

    OSDImageCacheValue Load(const QString &key, bool useFile);

    void Save(const QString &key, bool useFile,
              const OSDImageCacheValue &value);

    void Reset(void);

    static QString CreateKey(const QString &filename,
                             float wmult, float hmult,
                             int scalew,  int scaleh);

    static QString ExtractOriginal(const QString &key);

  private:
    mutable QMutex m_cacheLock;
    img_cache_t    m_imageCache;
    bool           m_cached;
};
