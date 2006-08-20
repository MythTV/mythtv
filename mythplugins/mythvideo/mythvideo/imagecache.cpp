#include <qpixmap.h>
#include <qimage.h>

#include <map>
#include <list>

#include "mythtv/mythcontext.h"

#include "cleanup.h"
#include "imagecache.h"

class ImageCacheImp
{
  public:
    ImageCacheImp(unsigned int cache_max) : m_max_cache_size(cache_max),
        m_clean_stub(this)
    {
        ensure_cache_min();
    }

    const QPixmap *load(const QString &image_file)
    {
        cache_entry *ce = addImage(image_file);
        if (ce)
        {
            return &ce->image;
        }

        return NULL;
    }

    const QPixmap *load(const QString &image_file, int width, int height,
                        QImage::ScaleMode scale)
    {
        QPixmap *ret = NULL;
        cache_entry *ce = addScaleImage(image_file, width, height, scale);
        if (ce && !ce->scale_image.isNull())
        {
            ret = &ce->scale_image;
        }

        return ret;
    }

    const QPixmap *load(const QString &image_file, QPixmap *pre_loaded)
    {
        QPixmap *ret = NULL;
        if (pre_loaded)
        {
            m_cache.push_back(cache_entry(image_file));
            cache_entry *ce = &m_cache.back();
            ce->image = *pre_loaded;
            ret = &ce->image;
            m_cache_index.insert(cache_map::value_type(ce->filename, ce));
            trim_cache();
        }
        return ret;
    }

    bool hitTest(const QString &image_file) const
    {
        return m_cache_index.find(image_file) != m_cache_index.end();
    }

    void resize(unsigned int new_size)
    {
        while (m_cache.size() > new_size)
        {
            unload_first();
        }

        m_max_cache_size = new_size;
        ensure_cache_min();
    }

    unsigned int size()
    {
        return m_cache.size();
    }

    void clear()
    {
        m_cache.clear();
        m_cache_index.clear();
    }

    void cleanup()
    {
        clear();
    }

  private:
    struct cache_entry
    {
        cache_entry(const QString &file) :
            filename(file), scale_mode(QImage::ScaleFree),
				scale_width(0), scale_height(0)
        {
        }

        QString filename;
        QPixmap image;
        QPixmap scale_image;
        QImage::ScaleMode scale_mode;
		int scale_width;
		int scale_height;
    };

    typedef std::list<cache_entry> cache_list;
    typedef std::map<QString, cache_entry *> cache_map;

  private:
    cache_entry *addImage(const QString &image_file)
    {
        cache_entry *ret = NULL;
        cache_map::iterator p = m_cache_index.find(image_file);
        if (p != m_cache_index.end())
        {
            ret = p->second;
            VERBOSE(VB_GENERAL, QString("ImageCache hit for: %1")
                    .arg(image_file));
        }
        else
        {
            VERBOSE(VB_GENERAL, QString("ImageCache miss for: %1")
                    .arg(image_file));

            m_cache.push_back(cache_entry(image_file));
            cache_entry *ce = &m_cache.back();
            if (!ce->image.load(ce->filename))
            {
                m_cache.pop_back();
                ret = NULL;
            }
            else
            {
                m_cache_index.insert(cache_map::value_type(ce->filename, ce));
                ret = ce;
            }

            trim_cache();
        }

        return ret;
    }

    cache_entry *addScaleImage(const QString &image_file, int width, int height,
                               QImage::ScaleMode scale)
    {
        cache_entry *ret = addImage(image_file);
        if (ret && !ret->image.isNull())
        {
            if (ret->scale_image.isNull() || ret->scale_mode != scale ||
					ret->scale_width != width || ret->scale_height != height)
            {
                VERBOSE(VB_GENERAL,
                        QString("ImageCache miss for scale image: %1")
                        .arg(image_file));
                ret->scale_mode = scale;
                QImage scale_me(ret->image.convertToImage());
                ret->scale_image.
                        convertFromImage(scale_me.smoothScale(width,height,
                                                              scale));
                ret->scale_width = width;
                ret->scale_height = height;
            }
            else
            {
                VERBOSE(VB_GENERAL,
                        QString("ImageCache hit for scale image: %1")
                        .arg(image_file));
            }
        }

        return ret;
    }

    void unload_first()
    {
        if (m_cache.size())
        {
            const QString &filename = m_cache.front().filename;
            cache_map::iterator p = m_cache_index.find(filename);
            if (p != m_cache_index.end())
            {
                m_cache_index.erase(p);
            }
            m_cache.pop_front();
        }
    }

    void trim_cache()
    {
        if (m_cache.size() > m_max_cache_size)
        {
            unload_first();
        }
    }

    void ensure_cache_min()
    {
        if (m_max_cache_size < 2) m_max_cache_size = 2;
    }

  private:
    cache_list m_cache;
    cache_map m_cache_index;
    unsigned int m_max_cache_size;
    SimpleCleanup<ImageCacheImp> m_clean_stub;
};

ImageCache &ImageCache::getImageCache()
{
    static ImageCache image_cache;
    return image_cache;
}

const QPixmap *ImageCache::load(const QString &image_file)
{
    return m_imp->load(image_file);
}

const QPixmap *ImageCache::load(const QString &image_file, int width,
                                int height, QImage::ScaleMode scale)
{
    return m_imp->load(image_file, width, height, scale);
}

const QPixmap *ImageCache::load(const QString &image_file, QPixmap *pre_loaded)
{
    return m_imp->load(image_file, pre_loaded);
}

bool ImageCache::hitTest(const QString &image_file) const
{
    return m_imp->hitTest(image_file);
}

void ImageCache::resize(unsigned int new_size)
{
    return m_imp->resize(new_size);
}

unsigned int ImageCache::size()
{
    return m_imp->size();
}

void ImageCache::clear()
{
    m_imp->clear();
}

ImageCache::ImageCache()
{
    m_imp = new ImageCacheImp(gContext->
                              GetNumSetting("mythvideo.ImageCacheSize", 50));
}

ImageCache::~ImageCache()
{
    delete m_imp;
}
