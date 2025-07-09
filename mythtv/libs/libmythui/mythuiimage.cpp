
#include "mythuiimage.h"

// C++
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <algorithm>

// QT
#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QEvent>
#include <QFile>
#include <QImageReader>
#include <QReadWriteLock>
#include <QRunnable>

// libmythbase
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#ifdef _MSC_VER
#  include "libmythbase/compat.h"   // random
#endif

// Mythui
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythscreentype.h"

class ImageLoadThread;

#define LOC      QString("MythUIImage(0x%1): ").arg((uint64_t)this,0,16)

/////////////////////////////////////////////////////

ImageProperties::ImageProperties(const ImageProperties& other)
{
    Copy(other);
}

ImageProperties &ImageProperties::operator=(const ImageProperties &other)
{
    if (this == &other)
        return *this;

    Copy(other);

    return *this;
}

ImageProperties::~ImageProperties()
{
    if (m_maskImage)
        m_maskImage->DecrRef();
}

// The m_maskImage field is assigned in the call to SetMaskImage().
//
// cppcheck-suppress operatorEqVarError
void ImageProperties::Copy(const ImageProperties &other)
{
    m_filename = other.m_filename;

    m_cropRect = other.m_cropRect;
    m_forceSize = other.m_forceSize;

    m_preserveAspect = other.m_preserveAspect;
    m_isGreyscale = other.m_isGreyscale;
    m_isReflected = other.m_isReflected;
    m_isOriented = other.m_isOriented;

    m_reflectAxis = other.m_reflectAxis;
    m_reflectScale = other.m_reflectScale;
    m_reflectLength = other.m_reflectLength;
    m_reflectShear = other.m_reflectShear;
    m_reflectSpacing = other.m_reflectSpacing;
    m_orientation = other.m_orientation;

    m_isThemeImage = other.m_isThemeImage;

    SetMaskImage(other.m_maskImage);
    m_isMasked = other.m_isMasked;
    m_maskImageFilename = other.m_maskImageFilename;
}

void ImageProperties::SetMaskImage(MythImage *image)
{
    if (image)
        image->IncrRef();
    if (m_maskImage)
        m_maskImage->DecrRef();

    m_maskImage = image;
    m_isMasked = m_maskImage;
}

/*!
 * \class ImageLoader
 */
class ImageLoader
{
  public:
    ImageLoader() = default;
   ~ImageLoader() = default;

    static QHash<QString, const MythUIImage *> m_loadingImages;
    static QMutex                        m_loadingImagesLock;
    static QWaitCondition                m_loadingImagesCond;

    static bool PreLoad(const QString &cacheKey, const MythUIImage *uitype)
    {
        m_loadingImagesLock.lock();

        // Check to see if the image is being loaded by us in another thread
        if ((m_loadingImages.contains(cacheKey)) &&
            (m_loadingImages[cacheKey] == uitype))
        {
            LOG(VB_GUI | VB_FILE, LOG_DEBUG,
                QString("ImageLoader::PreLoad(%1), this "
                        "file is already being loaded by this same MythUIImage "
                        "in another thread.").arg(cacheKey));
            m_loadingImagesLock.unlock();
            return false;
        }

        // Check to see if the exact same image is being loaded anywhere else
        while (m_loadingImages.contains(cacheKey))
            m_loadingImagesCond.wait(&m_loadingImagesLock);

        m_loadingImages[cacheKey] = uitype;
        m_loadingImagesLock.unlock();

        return true;
    }

    static void PostLoad(const QString &cacheKey)
    {
        m_loadingImagesLock.lock();
        m_loadingImages.remove(cacheKey);
        m_loadingImagesCond.wakeAll();
        m_loadingImagesLock.unlock();
    }

    static bool SupportsAnimation(const QString &filename)
    {
        QString extension = filename.section('.', -1);
        return !filename.startsWith("myth://") &&
            (extension == "gif" ||
             extension == "apng" ||
             extension == "mng");
    }

    /**
    *  \brief Generates a unique identifying string for this image which is used
    *         as a key in the image cache.
    */
    static QString GenImageLabel(const ImageProperties &imProps)
    {
        QString imagelabel;
        QString s_Attrib;

        if (imProps.m_isMasked)
            s_Attrib = "masked";

        if (imProps.m_isReflected)
            s_Attrib += "reflected";

        if (imProps.m_isGreyscale)
            s_Attrib += "greyscale";

        if (imProps.m_isOriented)
        {
            s_Attrib += "orientation";
            s_Attrib += QString("%1").arg(imProps.m_orientation);
        }

        int w = -1;
        int h = -1;
        if (!imProps.m_forceSize.isNull())
        {
            if (imProps.m_forceSize.width() != -1)
                w = imProps.m_forceSize.width();

            if (imProps.m_forceSize.height() != -1)
                h = imProps.m_forceSize.height();
        }


        imagelabel  = QString("%1-%2-%3x%4.png")
                    .arg(imProps.m_filename,
                         s_Attrib)
                    .arg(w)
                    .arg(h);
        imagelabel.replace('/', '-');
#ifdef Q_OS_ANDROID
        imagelabel.replace(':', '-');
#endif

        return imagelabel;
    }

    static MythImage *LoadImage(MythPainter *painter,
                                 // Must be a copy for thread safety
                                ImageProperties imProps,
                                ImageCacheMode cacheMode,
                                 // Included only to check address, could be
                                 // replaced by generating a unique value for
                                 // each MythUIImage object?
                                const MythUIImage *parent,
                                bool &aborted,
                                MythImageReader *imageReader = nullptr)
    {
        QString cacheKey = GenImageLabel(imProps);
        if (!PreLoad(cacheKey, parent))
        {
            aborted = true;
            return nullptr;
        }

        QString filename = imProps.m_filename;
        MythImage *image = nullptr;

        bool bResize = false;
        bool bFoundInCache = false;

        int w = -1;
        int h = -1;

        if (!imProps.m_forceSize.isNull())
        {
            if (imProps.m_forceSize.width() != -1)
                w = imProps.m_forceSize.width();

            if (imProps.m_forceSize.height() != -1)
                h = imProps.m_forceSize.height();

            bResize = true;
        }

        if (!imageReader)
        {
            image = GetMythUI()->LoadCacheImage(filename, cacheKey,
                                                painter, cacheMode);
        }

        if (image)
        {
            if (VERBOSE_LEVEL_CHECK(VB_GUI | VB_FILE, LOG_INFO))
            {
                image->IncrRef();
                int cnt = image->DecrRef();
                LOG(VB_GUI | VB_FILE, LOG_INFO,
                    QString("ImageLoader::LoadImage(%1) Found in cache, "
                            "RefCount = %2")
                    .arg(cacheKey).arg(cnt));
            }

            if (imProps.m_isReflected)
                image->setIsReflected(true);

            if (imProps.m_isOriented)
                image->setIsOriented(true);

            bFoundInCache = true;
        }
        else
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO,
                QString("ImageLoader::LoadImage(%1) NOT Found in cache. "
                        "Loading Directly").arg(cacheKey));

            image = painter->GetFormatImage();
            bool ok = false;

            if (imageReader)
                ok = image->Load(imageReader);
            else
                ok = image->Load(filename);

            if (!ok)
            {
                image->DecrRef();
                image = nullptr;
            }
        }

        if (image && image->isNull())
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO,
                QString("ImageLoader::LoadImage(%1) Image is NULL")
                                                    .arg(filename));

            image->DecrRef();
            image = nullptr;
        }

        if (image && !bFoundInCache)
        {
            if (imProps.m_isReflected)
            {
                image->Reflect(imProps.m_reflectAxis, imProps.m_reflectShear,
                               imProps.m_reflectScale, imProps.m_reflectLength,
                               imProps.m_reflectSpacing);
            }

            if (imProps.m_isGreyscale)
                image->ToGreyscale();

            if (imProps.m_isOriented)
                image->Orientation(imProps.m_orientation);

            // Even if an explicit size wasn't defined this image may still need
            // to be scaled because of a difference between the theme resolution
            // and the screen resolution. We want to avoid scaling twice.
            if (!bResize && imProps.m_isThemeImage)
            {
                float wmult = NAN; // Width multipler
                float hmult = NAN; // Height multipler
                GetMythMainWindow()->GetScalingFactors(wmult, hmult);
                if (wmult != 1.0F || hmult != 1.0F)
                {
                    w = image->size().width() * wmult;
                    h = image->size().height() * hmult;
                    bResize = true;
                }
            }

            if (bResize)
                image->Resize(QSize(w, h), imProps.m_preserveAspect);

            if (imProps.m_isMasked)
            {
                MythImage *newMaskImage = painter->GetFormatImage();
                if (newMaskImage->Load(imProps.GetMaskImageFilename()))
                {
                    float wmult = NAN; // Width multipler
                    float hmult = NAN; // Height multipler
                    GetMythMainWindow()->GetScalingFactors(wmult, hmult);
                    if (wmult != 1.0F || hmult != 1.0F)
                    {
                        int width = newMaskImage->size().width() * wmult;
                        int height = newMaskImage->size().height() * hmult;
                        newMaskImage->Resize(QSize(width, height));
                    }

                    imProps.SetMaskImage(newMaskImage);
                }
                else
                {
                    imProps.SetMaskImage(nullptr);
                }
                newMaskImage->DecrRef();

                QRect imageArea = image->rect();
                QRect maskArea = imProps.GetMaskImageRect();

                // Crop the mask to the image
                int x = 0;
                int y = 0;

                if (maskArea.width() > imageArea.width())
                    x = (maskArea.width() - imageArea.width()) / 2;

                if (maskArea.height() > imageArea.height())
                    y = (maskArea.height() - imageArea.height()) / 2;

                if (x > 0 || y > 0)
                    imageArea.translate(x, y);

                QImage mask = imProps.GetMaskImageSubset(imageArea);
                image->setAlphaChannel(mask.convertToFormat(QImage::Format_Alpha8));
            }

            if (!imageReader)
                GetMythUI()->CacheImage(cacheKey, image);
        }

        if (image)
            image->SetChanged();

        PostLoad(cacheKey);

        return image;
    }

    static AnimationFrames *LoadAnimatedImage(MythPainter *painter,
                                               // Must be a copy for thread safety
                                              const ImageProperties& imProps,
                                              ImageCacheMode cacheMode,
                                               // Included only to check address, could be
                                               // replaced by generating a unique value for
                                               // each MythUIImage object?
                                              const MythUIImage *parent,
                                              bool &aborted)
    {
        QString filename = QString("frame-%1-") + imProps.m_filename;
        QString frameFilename;
        int imageCount = 1;

        auto *imageReader = new MythImageReader(imProps.m_filename);
        auto *images = new AnimationFrames();

        while (imageReader->canRead() && !aborted)
        {
            frameFilename = filename.arg(imageCount);

            ImageProperties frameProps = imProps;
            frameProps.m_filename = frameFilename;

            MythImage *im = LoadImage(painter, frameProps, cacheMode, parent,
                                      aborted, imageReader);

            if (!im)
                aborted = true;

            images->append(AnimationFrame(im, std::chrono::milliseconds(imageReader->nextImageDelay())));
            imageCount++;
        }

        delete imageReader;

        return images;
    }

};

QHash<QString, const MythUIImage *> ImageLoader::m_loadingImages;
QMutex                              ImageLoader::m_loadingImagesLock;
QWaitCondition                      ImageLoader::m_loadingImagesCond;

/*!
 * \class ImageLoadEvent
 */
class ImageLoadEvent : public QEvent
{
  public:
    ImageLoadEvent(const MythUIImage *parent, MythImage *image,
                   QString basefile, QString filename,
                   int number, bool aborted)
        : QEvent(kEventType),
          m_parent(parent), m_image(image), m_basefile(std::move(basefile)),
          m_filename(std::move(filename)), m_number(number),
          m_aborted(aborted) { }

    ImageLoadEvent(const MythUIImage *parent, AnimationFrames *frames,
                   QString basefile,
                   QString filename, bool aborted)
        : QEvent(kEventType),
          m_parent(parent), m_basefile(std::move(basefile)),
          m_filename(std::move(filename)),
          m_images(frames), m_aborted(aborted) { }

    const MythUIImage *GetParent() const    { return m_parent; }
    MythImage *GetImage() const       { return m_image; }
    QString GetBasefile() const { return m_basefile; }
    QString GetFilename() const { return m_filename; }
    int GetNumber() const             { return m_number; }
    AnimationFrames *GetAnimationFrames() const { return m_images; }
    bool GetAbortState() const        { return m_aborted; }

    static const Type kEventType;

  private:
    const MythUIImage *m_parent   {nullptr};
    MythImage         *m_image    {nullptr};
    QString            m_basefile;
    QString            m_filename;
    int                m_number   {0};

    // Animated Images
    AnimationFrames   *m_images   {nullptr};

    // Image Load
    bool               m_aborted;
};

const QEvent::Type ImageLoadEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

/*!
* \class ImageLoadThread
*/
class ImageLoadThread : public QRunnable
{
  public:
    ImageLoadThread(MythUIImage *parent, MythPainter *painter,
                    const ImageProperties &imProps, QString basefile,
                    int number, ImageCacheMode mode) :
        m_parent(parent), m_painter(painter), m_imageProperties(imProps),
        m_basefile(std::move(basefile)), m_number(number), m_cacheMode(mode)
    {
    }

    void run() override // QRunnable
    {
        bool aborted = false;
        QString filename =  m_imageProperties.m_filename;

        // NOTE Do NOT use MythImageReader::supportsAnimation here, it defeats
        // the point of caching remote images
        if (ImageLoader::SupportsAnimation(filename))
        {
             AnimationFrames *frames =
                 ImageLoader::LoadAnimatedImage(m_painter,
                                                m_imageProperties,
                                                m_cacheMode, m_parent,
                                                aborted);

             if (frames && frames->count() > 1)
             {
                auto *le = new ImageLoadEvent(m_parent, frames, m_basefile,
                                              m_imageProperties.m_filename,
                                              aborted);
                QCoreApplication::postEvent(m_parent, le);

                return;
             }
             delete frames;
        }

        MythImage *image = ImageLoader::LoadImage(m_painter,
                                                    m_imageProperties,
                                                    m_cacheMode, m_parent,
                                                    aborted);

        auto *le = new ImageLoadEvent(m_parent, image, m_basefile,
                                      m_imageProperties.m_filename,
                                      m_number, aborted);
        QCoreApplication::postEvent(m_parent, le);
    }

private:
    MythUIImage       *m_parent  {nullptr};
    MythPainter       *m_painter {nullptr};
    ImageProperties m_imageProperties;
    QString         m_basefile;
    int             m_number;
    ImageCacheMode  m_cacheMode;
};

/////////////////////////////////////////////////////////////////
class MythUIImagePrivate
{
public:
    explicit MythUIImagePrivate(MythUIImage *p)
        : m_parent(p) { }
    ~MythUIImagePrivate() = default;

    MythUIImage *m_parent       {nullptr};

    QReadWriteLock m_updateLock {QReadWriteLock::Recursive};
};

/////////////////////////////////////////////////////////////////

MythUIImage::MythUIImage(const QString &filepattern,
                         int low, int high, std::chrono::milliseconds delay,
                         MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_delay(delay),
      m_lowNum(low),
      m_highNum(high),
      d(new MythUIImagePrivate(this))
{
    m_imageProperties.m_filename = filepattern;

    m_enableInitiator = true;

    emit DependChanged(false);
}

MythUIImage::MythUIImage(const QString &filename, MythUIType *parent,
                         const QString &name)
    : MythUIType(parent, name),
      m_origFilename(filename),
      m_delay(-1ms),
      d(new MythUIImagePrivate(this))
{
    m_imageProperties.m_filename = filename;

    m_enableInitiator = true;

    emit DependChanged(false);
}

MythUIImage::MythUIImage(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_delay(-1ms),
      d(new MythUIImagePrivate(this))
{
    m_enableInitiator = true;
}

MythUIImage::~MythUIImage()
{
    // Wait until all image loading threads are complete or bad things
    // may happen if this MythUIImage disappears when a queued thread
    // needs it.
    if (m_runningThreads > 0)
    {
        GetMythUI()->GetImageThreadPool()->waitForDone();
    }

    Clear();

    delete d;
}

/**
 *  \brief Remove all images from the widget
 */
void MythUIImage::Clear(void)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    QMutexLocker locker(&m_imagesLock);

    for (auto it = m_images.begin();
         it != m_images.end();
         it = m_images.erase(it))
    {
        if (*it)
            (*it)->DecrRef();
    }

    m_delays.clear();

    if (m_animatedImage)
    {
        m_lowNum = 0;
        m_highNum = 0;
        m_animatedImage = false;
    }
}

/**
 *  \brief Reset the image back to the default defined in the theme
 */
void MythUIImage::Reset(void)
{
    d->m_updateLock.lockForWrite();

    SetMinArea(MythRect());

    if (m_imageProperties.m_filename != m_origFilename)
    {
        m_imageProperties.m_isThemeImage = true;
        m_imageProperties.m_filename = m_origFilename;

        if (m_animatedImage)
        {
            m_lowNum = 0;
            m_highNum = 0;
            m_animatedImage = false;
        }
        emit DependChanged(true);

        d->m_updateLock.unlock();
        Load();
    }
    else
    {
        d->m_updateLock.unlock();
    }

    MythUIType::Reset();
}

/**
 *  \brief Set the image filename, does not load the image. See Load()
 */
void MythUIImage::SetFilename(const QString &filename)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    m_imageProperties.m_isThemeImage = false;
    m_imageProperties.m_filename = filename;
    if (filename == m_origFilename)
        emit DependChanged(true);
    else
        emit DependChanged(false);
}

/**
 *  \brief Set the image filename pattern and integer range for an animated
 *         image, does not load the image. See Load()
 */
void MythUIImage::SetFilepattern(const QString &filepattern, int low,
                                 int high)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    m_imageProperties.m_isThemeImage = false;
    m_imageProperties.m_filename = filepattern;
    m_lowNum = low;
    m_highNum = high;
    if (filepattern == m_origFilename)
        emit DependChanged(true);
    else
        emit DependChanged(false);
}

/**
 *  \brief Set the integer range for an animated image pattern
 */
void MythUIImage::SetImageCount(int low, int high)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    m_lowNum = low;
    m_highNum = high;
}

/**
 *  \brief Set the delay between each image in an animation
 */
void MythUIImage::SetDelay(std::chrono::milliseconds delay)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    m_delay = delay;
    m_lastDisplay = QTime::currentTime();
    m_curPos = 0;
}

/**
 *  \brief Sets the delays between each image in an animation
 */
void MythUIImage::SetDelays(const QVector<std::chrono::milliseconds>& delays)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    QMutexLocker imageLocker(&m_imagesLock);

    for (std::chrono::milliseconds delay : std::as_const(delays))
        m_delays[m_delays.size()] = delay;

    if (m_delay == -1ms)
        m_delay = m_delays[0];

    m_lastDisplay = QTime::currentTime();
    m_curPos = 0;
}

/**
 *  \brief Assign a MythImage to the widget. Use is strongly discouraged, use
 *         SetFilename() instead.
 */
void MythUIImage::SetImage(MythImage *img)
{
    d->m_updateLock.lockForWrite();

    if (!img)
    {
        d->m_updateLock.unlock();
        Reset();
        return;
    }

    m_imageProperties.m_isThemeImage = false;
    m_imageProperties.m_filename = img->GetFileName();

    img->IncrRef();

    QSize forceSize = m_imageProperties.m_forceSize;
    if (!forceSize.isNull())
    {
        int w = (forceSize.width() <= 0) ? img->width() : forceSize.width();
        int h = (forceSize.height() <= 0) ? img->height() : forceSize.height();
        img->Resize(QSize(w, h), m_imageProperties.m_preserveAspect);
    }

    if (m_imageProperties.m_isReflected && !img->IsReflected())
    {
        img->Reflect(m_imageProperties.m_reflectAxis,
                     m_imageProperties.m_reflectShear,
                     m_imageProperties.m_reflectScale,
                     m_imageProperties.m_reflectLength,
                     m_imageProperties.m_reflectSpacing);
    }

    if (m_imageProperties.m_isGreyscale && !img->isGrayscale())
        img->ToGreyscale();

    Clear();
    m_delay = -1ms;

    if (m_imageProperties.m_isOriented && !img->IsOriented())
        img->Orientation(m_imageProperties.m_orientation);

    if (m_imageProperties.m_forceSize.isNull())
        SetSize(img->size());

    m_imagesLock.lock();
    m_images[0] = img;
    m_delays.clear();
    m_imagesLock.unlock();

    m_curPos = 0;
    m_initiator = m_enableInitiator;
    SetRedraw();

    d->m_updateLock.unlock();
}

/**
 *  \brief Assign a set of MythImages to the widget for animation.
 *         Use is strongly discouraged, use SetFilepattern() instead.
 *
 */
void MythUIImage::SetImages(QVector<MythImage *> *images)
{
    Clear();

    QWriteLocker updateLocker(&d->m_updateLock);
    QSize aSize = GetFullArea().size();

    m_imageProperties.m_isThemeImage = false;

    for (auto *im : std::as_const(*images))
    {
        if (!im)
        {
            QMutexLocker locker(&m_imagesLock);
            m_images[m_images.size()] = im;
            continue;
        }

        im->IncrRef();


        QSize forceSize = m_imageProperties.m_forceSize;
        if (!forceSize.isNull())
        {
            int w = (forceSize.width() <= 0) ? im->width() : forceSize.width();
            int h = (forceSize.height() <= 0) ? im->height() : forceSize.height();
            im->Resize(QSize(w, h), m_imageProperties.m_preserveAspect);
        }

        if (m_imageProperties.m_isReflected && !im->IsReflected())
        {
            im->Reflect(m_imageProperties.m_reflectAxis,
                        m_imageProperties.m_reflectShear,
                        m_imageProperties.m_reflectScale,
                        m_imageProperties.m_reflectLength,
                        m_imageProperties.m_reflectSpacing);
        }

        if (m_imageProperties.m_isGreyscale && !im->isGrayscale())
            im->ToGreyscale();

        if (m_imageProperties.m_isOriented && !im->IsOriented())
            im->Orientation(m_imageProperties.m_orientation);

        m_imagesLock.lock();
        m_images[m_images.size()] = im;
        m_imagesLock.unlock();

        aSize = aSize.expandedTo(im->size());
    }

    SetImageCount(1, m_images.size());

    if (m_imageProperties.m_forceSize.isNull())
        SetSize(aSize);

    MythRect rect(GetFullArea());
    rect.setSize(aSize);
    SetMinArea(rect);

    m_curPos = 0;
    m_animatedImage = true;
    m_initiator = m_enableInitiator;
    SetRedraw();
}

void MythUIImage::SetAnimationFrames(const AnimationFrames& frames)
{
    QVector<std::chrono::milliseconds> delays;
    QVector<MythImage *> images;

    for (const auto & frame : std::as_const(frames))
    {
        images.append(frame.first);
        delays.append(frame.second);
    }

    if (!images.empty())
    {
        SetImages(&images);

        if (m_delay < 0ms  && !delays.empty())
            SetDelays(delays);
    }
    else
    {
        Reset();
    }
}

/**
 *  \brief Force the dimensions of the widget and image to the given size.
 */
void MythUIImage::ForceSize(const QSize size)
{
    if (m_imageProperties.m_forceSize == size)
        return;

    d->m_updateLock.lockForWrite();
    m_imageProperties.m_forceSize = size;
    d->m_updateLock.unlock();

    if (size.isEmpty())
        return;

    SetSize(m_imageProperties.m_forceSize);

    Load();
}

/**
 *  \brief Saves the exif orientation value of the first image in the widget
 */
void MythUIImage::SetOrientation(int orientation)
{
    m_imageProperties.m_isOriented = true;
    m_imageProperties.m_orientation = orientation;
}

/**
 *  \brief Set the size of the widget
 */
void MythUIImage::SetSize(int width, int height)
{
    SetSize(QSize(width, height));
}

/**
 *  \brief Set the size of the widget
 */
void MythUIImage::SetSize(const QSize size)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    MythUIType::SetSize(size);
    m_needLoad = true;
}

/**
 *  \brief Crop the image using the given rectangle, useful for removing
 *         unsightly edges from imported images or zoom effects
 */
void MythUIImage::SetCropRect(int x, int y, int width, int height)
{
    SetCropRect(MythRect(x, y, width, height));
}

/**
 *  \brief Crop the image using the given rectangle, useful for removing
 *         unsightly edges from imported images or zoom effects
 */
void MythUIImage::SetCropRect(const MythRect &rect)
{
    QWriteLocker updateLocker(&d->m_updateLock);
    m_imageProperties.m_cropRect = rect;
    SetRedraw();
}

/**
 *  \brief Load the image(s), wraps ImageLoader::LoadImage()
 */
bool MythUIImage::Load(bool allowLoadInBackground, bool forceStat)
{
    d->m_updateLock.lockForRead();

    m_initiator = m_enableInitiator;

    QString bFilename = m_imageProperties.m_filename;

    d->m_updateLock.unlock();

    QString filename = bFilename;

    if (bFilename.isEmpty())
    {
        Clear();
        SetMinArea(MythRect());
        SetRedraw();

        return false;
    }

    if (qEnvironmentVariableIsSet("DISABLETHREADEDMYTHUIIMAGE"))
        allowLoadInBackground = false;

    // Don't clear the widget before we need to, otherwise it causes
    // unsightly flashing. We exclude animations for now since that requires a
    // deeper fix
    bool isAnimation = (m_highNum != m_lowNum) || m_animatedImage;

    if (isAnimation)
        Clear();

    bool complete = true;

    QString imagelabel;

    int j = 0;

    for (int i = m_lowNum; i <= m_highNum && !m_animatedImage; i++)
    {
        if (!m_animatedImage && m_highNum != m_lowNum &&
            bFilename.contains("%1"))
            filename = bFilename.arg(i);

        ImageProperties imProps = m_imageProperties;
        imProps.m_filename = filename;
        imagelabel = ImageLoader::GenImageLabel(imProps);

        // Only load in the background if allowed and the image is
        // not already in our mem cache
        int cacheMode = kCacheIgnoreDisk;

        if (forceStat)
            cacheMode |= (int)kCacheForceStat;

        int cacheMode2 = kCacheNormal;

        if (forceStat)
            cacheMode2 |= (int)kCacheForceStat;

        bool do_background_load = false;
        if (allowLoadInBackground)
        {
            MythImage *img = GetMythUI()->LoadCacheImage(
                filename, imagelabel, GetPainter(),
                static_cast<ImageCacheMode>(cacheMode));
            if (img)
                img->DecrRef();
            else
                do_background_load = true;
        }

        if (do_background_load)
        {
            SetMinArea(MythRect());
            LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC +
                QString("Load(), spawning thread to load '%1'").arg(filename));

            m_runningThreads++;
            auto *bImgThread = new ImageLoadThread(this, GetPainter(),
                                    imProps, bFilename, i,
                                    static_cast<ImageCacheMode>(cacheMode2));
            GetMythUI()->GetImageThreadPool()->start(bImgThread, "ImageLoad");
        }
        else
        {
            if (!isAnimation && !GetMythUI()->IsImageInCache(imagelabel))
                Clear();

            // Perform a blocking load
            LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC +
                QString("Load(), loading '%1' in foreground").arg(filename));
            bool aborted = false;

            if (ImageLoader::SupportsAnimation(filename))
            {
                AnimationFrames *myFrames =
                    ImageLoader::LoadAnimatedImage(GetPainter(), imProps,
                                        static_cast<ImageCacheMode>(cacheMode2),
                                        this, aborted);

                // TODO We might want to handle an abort here more gracefully
                if (aborted)
                {
                    LOG(VB_GUI, LOG_DEBUG, QString("Aborted loading animated"
                                                   "image %1 in foreground")
                                                                .arg(filename));
                }

                SetAnimationFrames(*myFrames);

                delete myFrames;
            }
            else
            {
                MythImage *image = nullptr;

                image = ImageLoader::LoadImage(GetPainter(),
                                               imProps,
                                               static_cast<ImageCacheMode>(cacheMode2),
                                               this, aborted);

                // TODO We might want to handle an abort here more gracefully
                if (aborted)
                {
                    LOG(VB_GUI, LOG_DEBUG, QString("Aborted loading animated"
                                                   "image %1 in foreground")
                                                                .arg(filename));
                }

                if (image)
                {
                    if (m_imageProperties.m_forceSize.isNull())
                        SetSize(image->size());

                    MythRect rect(GetFullArea());
                    rect.setSize(image->size());
                    SetMinArea(rect);

                    m_imagesLock.lock();
                    m_images[j] = image;
                    m_imagesLock.unlock();

                    SetRedraw();
                    d->m_updateLock.lockForWrite();
                    m_lastDisplay = QTime::currentTime();
                    d->m_updateLock.unlock();
                }
                else
                {
                    Reset();

                    m_imagesLock.lock();
                    m_images[j] = nullptr;
                    m_imagesLock.unlock();
                }
            }
        }

        ++j;

        // Load is complete if no image is loading in background
        complete &= !do_background_load;
    }

    if (complete)
        emit LoadComplete();

    return true;
}

/**
 *  \copydoc MythUIType::Pulse()
 */
void MythUIImage::Pulse(void)
{
    d->m_updateLock.lockForWrite();

    auto delay = -1ms;

    if (m_delays.contains(m_curPos))
        delay = m_delays[m_curPos];
    else if (m_delay > 0ms)
        delay = m_delay;

    if (delay > 0ms &&
        abs(m_lastDisplay.msecsTo(QTime::currentTime())) > delay.count())
    {
        if (m_showingRandomImage)
        {
            FindRandomImage();
            d->m_updateLock.unlock();
            Load();
            d->m_updateLock.lockForWrite();
        }
        else
        {
            m_imagesLock.lock();

            if (m_animationCycle == kCycleStart)
            {
                ++m_curPos;

                if (m_curPos >= (uint)m_images.size())
                    m_curPos = 0;
            }
            else if (m_animationCycle == kCycleReverse)
            {
                if ((m_curPos + 1) >= (uint)m_images.size())
                {
                    m_animationReverse = true;
                }
                else if (m_curPos == 0)
                {
                    m_animationReverse = false;
                }

                if (m_animationReverse)
                    --m_curPos;
                else
                    ++m_curPos;
            }

            m_imagesLock.unlock();

            SetRedraw();
        }

        m_lastDisplay = QTime::currentTime();
    }

    MythUIType::Pulse();

    d->m_updateLock.unlock();
}

/**
 *  \copydoc MythUIType::DrawSelf()
 */
void MythUIImage::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                           int alphaMod, QRect clipRect)
{
    m_imagesLock.lock();

    if (!m_images.empty())
    {
        d->m_updateLock.lockForWrite();

        if (m_curPos >= (uint)m_images.size())
            m_curPos = 0;

        if (!m_images[m_curPos])
        {
            unsigned int origPos = m_curPos;
            m_curPos++;

            while (!m_images[m_curPos] && m_curPos != origPos)
            {
                m_curPos++;

                if (m_curPos >= (uint)m_images.size())
                    m_curPos = 0;
            }
        }

        QRect area = GetArea().toQRect();
        area.translate(xoffset, yoffset);

        int alpha = CalcAlpha(alphaMod);

        MythImage *currentImage = m_images[m_curPos];

        if (currentImage)
            currentImage->IncrRef();

        m_imagesLock.unlock();
        d->m_updateLock.unlock();

        if (!currentImage)
            return;

        d->m_updateLock.lockForRead();

        QRect currentImageArea = currentImage->rect();

        if (!m_imageProperties.m_forceSize.isNull())
            area.setSize(area.size().expandedTo(currentImage->size()));

        // Centre image in available space, accounting for zoom
        int x = 0;
        int y = 0;
        QRect visibleImage = m_effects.GetExtent(currentImageArea.size());

        if (area.width() > visibleImage.width())
            x = (area.width() / 2) + visibleImage.topLeft().x();

        if (area.height() > visibleImage.height())
            y = (area.height() / 2) + visibleImage.topLeft().y();

        if ((x > 0 || y > 0))
            area.translate(x, y);

        QRect srcRect;
        m_imageProperties.m_cropRect.CalculateArea(GetFullArea());

        if (!m_imageProperties.m_cropRect.isEmpty())
            srcRect = m_imageProperties.m_cropRect.toQRect();
        else
            srcRect = currentImageArea;

        p->SetClipRect(clipRect);
        p->DrawImage(area, currentImage, srcRect, alpha);
        currentImage->DecrRef();
        d->m_updateLock.unlock();
    }
    else
    {
        m_imagesLock.unlock();
    }
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIImage::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    QWriteLocker updateLocker(&d->m_updateLock);

    if (element.tagName() == "filename")
    {
        m_imageProperties.m_isThemeImage = true; // This is an image distributed with the theme
        m_origFilename = m_imageProperties.m_filename = getFirstText(element);

        if (m_imageProperties.m_filename.endsWith('/'))
        {
            m_showingRandomImage = true;
            m_imageDirectory = m_imageProperties.m_filename;

            FindRandomImage();
        }
    }
    else if (element.tagName() == "filepattern")
    {
        m_imageProperties.m_isThemeImage = true; // This is an image distributed with the theme
        m_origFilename = m_imageProperties.m_filename = getFirstText(element);
        QString tmp = element.attribute("low");

        if (!tmp.isEmpty())
            m_lowNum = tmp.toInt();

        tmp = element.attribute("high");

        if (!tmp.isEmpty())
            m_highNum = tmp.toInt();

        tmp = element.attribute("cycle", "start");

        if (tmp == "reverse")
            m_animationCycle = kCycleReverse;
    }
    else if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
        m_imageProperties.m_forceSize = m_area.size();
    }
    else if (element.tagName() == "preserveaspect")
    {
        m_imageProperties.m_preserveAspect = parseBool(element);
    }
    else if (element.tagName() == "crop")
    {
        m_imageProperties.m_cropRect = parseRect(element);
    }
    else if (element.tagName() == "delay")
    {
        QString value = getFirstText(element);

        if (value.contains(","))
        {
            QVector<std::chrono::milliseconds> delays;
            QStringList tokens = value.split(",");
            for (const auto & token : std::as_const(tokens))
            {
                if (token.isEmpty())
                {
                    if (!delays.empty())
                        delays.append(delays[delays.size()-1]);
                    else
                        delays.append(0ms); // Default delay before first image
                }
                else
                {
                    delays.append(std::chrono::milliseconds(token.toInt()));
                }
            }

            if (!delays.empty())
            {
                m_delay = delays[0];
                SetDelays(delays);
            }
        }
        else
        {
            m_delay = std::chrono::milliseconds(value.toInt());
        }
    }
    else if (element.tagName() == "reflection")
    {
        m_imageProperties.m_isReflected = true;
        QString tmp = element.attribute("axis");

        if (!tmp.isEmpty())
        {
            if (tmp.toLower() == "horizontal")
                m_imageProperties.m_reflectAxis = ReflectAxis::Horizontal;
            else
                m_imageProperties.m_reflectAxis = ReflectAxis::Vertical;
        }

        tmp = element.attribute("shear");

        if (!tmp.isEmpty())
            m_imageProperties.m_reflectShear = tmp.toInt();

        tmp = element.attribute("scale");

        if (!tmp.isEmpty())
            m_imageProperties.m_reflectScale = tmp.toInt();

        tmp = element.attribute("length");

        if (!tmp.isEmpty())
            m_imageProperties.m_reflectLength = tmp.toInt();

        tmp = element.attribute("spacing");

        if (!tmp.isEmpty())
            m_imageProperties.m_reflectSpacing = tmp.toInt();
    }
    else if (element.tagName() == "mask")
    {
        m_imageProperties.SetMaskImageFilename(getFirstText(element));
        m_imageProperties.m_isMasked = true;
    }
    else if (element.tagName() == "grayscale" ||
             element.tagName() == "greyscale")
    {
        m_imageProperties.m_isGreyscale = parseBool(element);
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    m_needLoad = true;

    if (m_parent && m_parent->IsDeferredLoading(true))
        m_needLoad = false;

    return true;
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIImage::CopyFrom(MythUIType *base)
{
    d->m_updateLock.lockForWrite();
    auto *im = dynamic_cast<MythUIImage *>(base);
    if (!im)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2) ERROR, bad parsing '%3' (%4)")
            .arg(objectName(), GetXMLLocation(),
                 base->objectName(), base->GetXMLLocation()));
        d->m_updateLock.unlock();
        return;
    }

    m_origFilename = im->m_origFilename;

    m_delay = im->m_delay;
    m_lowNum = im->m_lowNum;
    m_highNum = im->m_highNum;

    m_lastDisplay = QTime::currentTime();
    m_curPos = 0;

    m_imageProperties = im->m_imageProperties;

    m_animationCycle = im->m_animationCycle;
    m_animatedImage = im->m_animatedImage;

    m_showingRandomImage = im->m_showingRandomImage;
    m_imageDirectory = im->m_imageDirectory;

    MythUIType::CopyFrom(base);

    // We need to update forceSize in case the parent area has changed
    // however we only want to set forceSize if it was previously in use
    if (!m_imageProperties.m_forceSize.isNull())
        m_imageProperties.m_forceSize = m_area.size();

    m_needLoad = im->m_needLoad;

    d->m_updateLock.unlock();

    d->m_updateLock.lockForRead();

    if (m_needLoad)
    {
        d->m_updateLock.unlock();
        Load();
    }
    else
    {
        d->m_updateLock.unlock();
    }
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIImage::CreateCopy(MythUIType *parent)
{
    QReadLocker updateLocker(&d->m_updateLock);
    auto *im = new MythUIImage(parent, objectName());
    im->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUIImage::Finalize(void)
{
    d->m_updateLock.lockForRead();

    if (m_needLoad)
    {
        d->m_updateLock.unlock();
        Load();
    }
    else
    {
        d->m_updateLock.unlock();
    }

    MythUIType::Finalize();
}

/**
 *  \copydoc MythUIType::LoadNow()
 */
void MythUIImage::LoadNow(void)
{
    d->m_updateLock.lockForWrite();

    if (m_needLoad)
    {
        d->m_updateLock.unlock();
        return;
    }

    m_needLoad = true;
    d->m_updateLock.unlock();

    Load(false);

    MythUIType::LoadNow();
}

/**
*  \copydoc MythUIType::customEvent()
*/
void MythUIImage::customEvent(QEvent *event)
{
    if (event->type() == ImageLoadEvent::kEventType)
    {
        auto * le = dynamic_cast<ImageLoadEvent *>(event);
        if (!le || le->GetParent() != this)
            return;

        MythImage *image                 = le->GetImage();
        int number                       = le->GetNumber();
        QString filename                 = le->GetFilename();
        AnimationFrames *animationFrames = le->GetAnimationFrames();
        bool aborted                     = le->GetAbortState();

        m_runningThreads--;

        d->m_updateLock.lockForRead();
        QString propFilename = m_imageProperties.m_filename;
        d->m_updateLock.unlock();

        // 1) We aborted loading the image for some reason (e.g. two requests
        //    for same image)
        // 2) Filename changed since we started this image, so abort to avoid
        // rendering two different images in quick succession which causes
        // unsightly flickering
        if (aborted || (le->GetBasefile() != propFilename))
        {
            if (aborted)
                LOG(VB_GUI, LOG_DEBUG, QString("Aborted loading image %1")
                                                                .arg(filename));

            if (image)
                image->DecrRef();

            if (animationFrames)
            {
                for (const auto & frame : std::as_const(*animationFrames))
                {
                    MythImage *im = frame.first;
                    if (im)
                        im->DecrRef();
                }

                delete animationFrames;
            }
        }
        else if (animationFrames)
        {
            SetAnimationFrames(*animationFrames);

            delete animationFrames;
        }
        else if (image)
        {
            // We don't clear until we have the new image ready to display to
            // avoid unsightly flashing. This isn't currently supported for
            // animations.
            if ((m_highNum == m_lowNum) && !m_animatedImage)
                Clear();

            d->m_updateLock.lockForWrite();

            if (m_imageProperties.m_forceSize.isNull())
                SetSize(image->size());

            MythRect rect(GetFullArea());
            rect.setSize(image->size());
            SetMinArea(rect);

            d->m_updateLock.unlock();

            m_imagesLock.lock();

            if (m_images[number])
            {
                // If we got to this point, it means this same MythUIImage
                // was told to reload the same image, so we use the newest
                // copy of the image.
                m_images[number]->DecrRef(); // delete the original
            }

            m_images[number] = image;
            m_imagesLock.unlock();

            SetRedraw();

            d->m_updateLock.lockForWrite();
            m_lastDisplay = QTime::currentTime();
            d->m_updateLock.unlock();
        }
        else
        {
            // No Images were loaded, so trigger Reset to default
            Reset();
        }

        // NOLINTNEXTLINE(readability-misleading-indentation)
        emit LoadComplete();
    }
}

void MythUIImage::FindRandomImage(void)
{
    QString randFile;

    // find and save the list of available images
    if (m_imageList.isEmpty())
    {
        QDir imageDir(m_imageDirectory);

        if (!imageDir.exists())
        {
            QString themeDir = GetMythUI()->GetThemeDir() + '/';
            imageDir.setPath(themeDir + m_imageDirectory);
        }

        QStringList imageTypes;

        QList< QByteArray > exts = QImageReader::supportedImageFormats();
        for (const auto & ext : std::as_const(exts))
        {
            imageTypes.append(QString("*.").append(ext));
        }

        imageDir.setNameFilters(imageTypes);

        m_imageList = imageDir.entryList();

        if (m_imageList.empty())
        {
            m_origFilename = m_imageProperties.m_filename = randFile;
            return;
        }

        // randomly shuffle the images
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(m_imageList.begin(), m_imageList.end(), g);
        m_imageListIndex = 0;
        randFile = QString("%1%2").arg(m_imageDirectory, m_imageList.at(m_imageListIndex));
    }
    else
    {
        if (!m_imageList.empty())
        {
            m_imageListIndex++;

            // if we are at the last image in the list re-shuffle the list and start from the beginning
            if (m_imageListIndex == m_imageList.size())
            {
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(m_imageList.begin(), m_imageList.end(), g);
                m_imageListIndex = 0;
            }

            // make sure we don't show the same image again in the unlikely event the re-shuffle shows the same image again 
            if (m_imageList.at(m_imageListIndex) == m_origFilename && m_imageList.size() > 1)
                m_imageListIndex++;

            randFile = QString("%1%2").arg(m_imageDirectory, m_imageList.at(m_imageListIndex));
        }
    }

    m_origFilename = m_imageProperties.m_filename = randFile;
}
