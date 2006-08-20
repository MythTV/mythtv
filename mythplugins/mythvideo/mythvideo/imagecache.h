#ifndef IMAGECACHE_H_
#define IMAGECACHE_H_

class QPixmap;
class ImageCacheImp;
class ImageCache
{
  public:
    static ImageCache &getImageCache();

  public:
    const QPixmap *load(const QString &image_file);
    const QPixmap *load(const QString &image_file, int width, int height,
                        QImage::ScaleMode scale);
    const QPixmap *load(const QString &image_file, QPixmap *pre_loaded);
    bool hitTest(const QString &image_file) const;
    void resize(unsigned int new_size);
    unsigned int size();
    void clear();

  private:
    ImageCache();
    ~ImageCache();

  private:
    ImageCacheImp *m_imp;
};

#endif // IMAGECACHE_H_
