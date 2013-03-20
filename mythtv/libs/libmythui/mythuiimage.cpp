
#include "mythuiimage.h"

// C
#include <cstdlib>
#include <time.h>

// POSIX
#include <stdint.h>

// QT
#include <QFile>
#include <QDir>
#include <QDomDocument>
#include <QImageReader>
#include <QReadWriteLock>
#include <QRunnable>
#include <QEvent>
#include <QCoreApplication>

// libmythbase
#include "mythlogging.h"

// Mythui
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythscreentype.h"

class ImageLoadThread;

#define LOC      QString("MythUIImage(0x%1): ").arg((uint64_t)this,0,16)

/////////////////////////////////////////////////////

ImageProperties::ImageProperties()
{
    Init();
}

ImageProperties::ImageProperties(const ImageProperties& other)
{
    Init();
    Copy(other);
}

ImageProperties &ImageProperties::operator=(const ImageProperties &other)
{
    Copy(other);

    return *this;
}

ImageProperties::~ImageProperties()
{
    if (maskImage)
        maskImage->DecrRef();
}

void ImageProperties::Init()
{
    filename = QString();
    cropRect = MythRect(0, 0, 0, 0);
    forceSize = QSize(0, 0);
    preserveAspect = false;
    isGreyscale = false;
    isReflected = false;
    isMasked = false;
    isOriented = false;
    reflectAxis = ReflectVertical;
    reflectScale = 100;
    reflectLength = 100;
    reflectShear = 0;
    reflectSpacing = 0;
    orientation = 1;
    maskImage = NULL;
}

void ImageProperties::Copy(const ImageProperties &other)
{
    filename = other.filename;
    filename.detach();

    cropRect = other.cropRect;
    forceSize = other.forceSize;

    preserveAspect = other.preserveAspect;
    isGreyscale = other.isGreyscale;
    isReflected = other.isReflected;
    isMasked = other.isMasked;
    isOriented = other.isOriented;

    reflectAxis = other.reflectAxis;
    reflectScale = other.reflectScale;
    reflectLength = other.reflectLength;
    reflectShear = other.reflectShear;
    reflectSpacing = other.reflectSpacing;
    orientation = other.orientation;

    SetMaskImage(other.maskImage);
}

void ImageProperties::SetMaskImage(MythImage *image)
{
    if (image)
        image->IncrRef();
    if (maskImage)
        maskImage->DecrRef();

    isMasked = false;
    maskImage = image;
    isMasked = maskImage;
}

/*!
 * \class ImageLoader
 */
class ImageLoader
{
  public:
    ImageLoader() { };
   ~ImageLoader() { };

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
        if (!filename.startsWith("myth://") &&
            (extension == "gif" ||
             extension == "apng" ||
             extension == "mng"))
            return true;

        return false;
    }

    /**
    *  \brief Generates a unique identifying string for this image which is used
    *         as a key in the image cache.
    */
    static QString GenImageLabel(const ImageProperties &imProps)
    {
        QString imagelabel;
        QString s_Attrib;

        if (imProps.isMasked)
            s_Attrib = "masked";

        if (imProps.isReflected)
            s_Attrib += "reflected";

        if (imProps.isGreyscale)
            s_Attrib += "greyscale";

        if (imProps.isOriented)
        {
            s_Attrib += "orientation";
            s_Attrib += QString("%1").arg(imProps.orientation);
        }

        int w = -1;
        int h = -1;
        if (!imProps.forceSize.isNull())
        {
            if (imProps.forceSize.width() != -1)
                w = imProps.forceSize.width();

            if (imProps.forceSize.height() != -1)
                h = imProps.forceSize.height();
        }


        imagelabel  = QString("%1-%2-%3x%4.png")
                    .arg(imProps.filename)
                    .arg(s_Attrib)
                    .arg(w)
                    .arg(h);
        imagelabel.replace('/', '-');

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
                                MythImageReader *imageReader = NULL)
    {
        QString cacheKey = GenImageLabel(imProps);
        if (!PreLoad(cacheKey, parent))
        {
            aborted = true;
            return NULL;
        }

        QString filename = imProps.filename;
        MythImage *image = NULL;

        bool bForceResize = false;
        bool bFoundInCache = false;

        int w = -1;
        int h = -1;

        if (!imProps.forceSize.isNull())
        {
            if (imProps.forceSize.width() != -1)
                w = imProps.forceSize.width();

            if (imProps.forceSize.height() != -1)
                h = imProps.forceSize.height();

            bForceResize = true;
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

            if (imProps.isReflected)
                image->setIsReflected(true);

            if (imProps.isOriented)
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
                image = NULL;
            }
        }

        if (image && image->isNull())
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO,
                QString("ImageLoader::LoadImage(%1) Image is NULL")
                                                    .arg(filename));

            image->DecrRef();
            image = NULL;
        }

        if (image && !bFoundInCache)
        {
            if (bForceResize)
                image->Resize(QSize(w, h), imProps.preserveAspect);

            if (imProps.isMasked)
            {
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
                image->setAlphaChannel(mask.alphaChannel());
            }

            if (imProps.isReflected)
                image->Reflect(imProps.reflectAxis, imProps.reflectShear,
                               imProps.reflectScale, imProps.reflectLength,
                               imProps.reflectSpacing);

            if (imProps.isGreyscale)
                image->ToGreyscale();

            if (imProps.isOriented)
                image->Orientation(imProps.orientation);

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
                                              ImageProperties imProps,
                                              ImageCacheMode cacheMode,
                                               // Included only to check address, could be
                                               // replaced by generating a unique value for
                                               // each MythUIImage object?
                                              const MythUIImage *parent,
                                              bool &aborted)
    {
        QString filename = QString("frame-%1-") + imProps.filename;
        QString frameFilename;
        int imageCount = 1;

        MythImageReader *imageReader = new MythImageReader(imProps.filename);

        AnimationFrames *images = new AnimationFrames();

        while (imageReader->canRead() && !aborted)
        {
            frameFilename = filename.arg(imageCount);

            ImageProperties frameProps = imProps;
            frameProps.filename = frameFilename;

            MythImage *im = LoadImage(painter, frameProps, cacheMode, parent,
                                      aborted, imageReader);

            if (!im)
                aborted = true;

            images->append(AnimationFrame(im, imageReader->nextImageDelay()));
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
                   const QString &basefile, const QString &filename,
                   int number, bool aborted)
        : QEvent(kEventType),
          m_parent(parent), m_image(image), m_basefile(basefile),
          m_filename(filename), m_number(number),
          m_images(NULL), m_aborted(aborted) { }

    ImageLoadEvent(const MythUIImage *parent, AnimationFrames *frames,
                   const QString &basefile,
                   const QString &filename, bool aborted)
        : QEvent(kEventType),
          m_parent(parent), m_image(NULL), m_basefile(basefile),
          m_filename(filename), m_number(0),
          m_images(frames), m_aborted(aborted) { }

    const MythUIImage *GetParent() const    { return m_parent; }
    MythImage *GetImage() const       { return m_image; }
    const QString GetBasefile() const { return m_basefile; }
    const QString GetFilename() const { return m_filename; }
    int GetNumber() const             { return m_number; }
    AnimationFrames *GetAnimationFrames() const { return m_images; }
    bool GetAbortState() const        { return m_aborted; }

    static Type kEventType;

  private:
    const MythUIImage     *m_parent;
    MythImage       *m_image;
    QString          m_basefile;
    QString          m_filename;
    int              m_number;

    // Animated Images
    AnimationFrames  *m_images;

    // Image Load
    bool             m_aborted;
};

QEvent::Type ImageLoadEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

/*!
* \class ImageLoadThread
*/
class ImageLoadThread : public QRunnable
{
  public:
    ImageLoadThread(const MythUIImage *parent, MythPainter *painter,
                    const ImageProperties &imProps, const QString &basefile,
                    int number, ImageCacheMode mode) :
        m_parent(parent), m_painter(painter), m_imageProperties(imProps),
        m_basefile(basefile), m_number(number), m_cacheMode(mode)
    {
    }

    void run()
    {
        bool aborted = false;
        QString filename =  m_imageProperties.filename;

        // NOTE Do NOT use MythImageReader::supportsAnimation here, it defeats
        // the point of caching remote images
        if (ImageLoader::SupportsAnimation(filename))
        {
             AnimationFrames *frames;

             frames = ImageLoader::LoadAnimatedImage(m_painter,
                                                     m_imageProperties,
                                                     m_cacheMode, m_parent,
                                                     aborted);

             ImageLoadEvent *le = new ImageLoadEvent(m_parent, frames,
                                                     m_basefile,
                                                     m_imageProperties.filename,
                                                     aborted);
             QCoreApplication::postEvent(const_cast<MythUIImage*>(m_parent), le);
        }
        else
        {
            MythImage *image = ImageLoader::LoadImage(m_painter,
                                                      m_imageProperties,
                                                      m_cacheMode, m_parent,
                                                      aborted);

            ImageLoadEvent *le = new ImageLoadEvent(m_parent, image, m_basefile,
                                                    m_imageProperties.filename,
                                                    m_number, aborted);
            QCoreApplication::postEvent(const_cast<MythUIImage*>(m_parent), le);
        }
    }

private:
    const MythUIImage    *m_parent;
    MythPainter       *m_painter;
    ImageProperties m_imageProperties;
    QString         m_basefile;
    int             m_number;
    ImageCacheMode  m_cacheMode;
};

/////////////////////////////////////////////////////////////////
class MythUIImagePrivate
{
public:
    MythUIImagePrivate(MythUIImage *p)
        : m_parent(p),            m_UpdateLock(QReadWriteLock::Recursive)
    { };
    ~MythUIImagePrivate() {};

    MythUIImage *m_parent;

    QReadWriteLock m_UpdateLock;
};

/////////////////////////////////////////////////////////////////

MythUIImage::MythUIImage(const QString &filepattern,
                         int low, int high, int delayms,
                         MythUIType *parent, const QString &name)
    : MythUIType(parent, name)
{
    m_imageProperties.filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;

    m_Delay = delayms;
    m_EnableInitiator = true;

    d = new MythUIImagePrivate(this);
    emit DependChanged(false);
    Init();
}

MythUIImage::MythUIImage(const QString &filename, MythUIType *parent,
                         const QString &name)
    : MythUIType(parent, name)
{
    m_imageProperties.filename = filename;
    m_OrigFilename = filename;

    m_LowNum = 0;
    m_HighNum = 0;
    m_Delay = -1;
    m_EnableInitiator = true;

    d = new MythUIImagePrivate(this);
    emit DependChanged(false);
    Init();
}

MythUIImage::MythUIImage(MythUIType *parent, const QString &name)
    : MythUIType(parent, name)
{
    m_LowNum = 0;
    m_HighNum = 0;
    m_Delay = -1;
    m_EnableInitiator = true;

    d = new MythUIImagePrivate(this);

    Init();
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
    QWriteLocker updateLocker(&d->m_UpdateLock);
    QMutexLocker locker(&m_ImagesLock);

    while (!m_Images.isEmpty())
    {
        QHash<int, MythImage *>::iterator it = m_Images.begin();

        if (*it)
            (*it)->DecrRef();

        m_Images.remove(it.key());
    }

    m_Delays.clear();

    if (m_animatedImage)
    {
        m_LowNum = 0;
        m_HighNum = 0;
        m_animatedImage = false;
    }
}

/**
 *  \brief Reset the image back to the default defined in the theme
 */
void MythUIImage::Reset(void)
{
    d->m_UpdateLock.lockForWrite();

    SetMinArea(MythRect());

    if (m_imageProperties.filename != m_OrigFilename)
    {
        m_imageProperties.filename = m_OrigFilename;

        if (m_animatedImage)
        {
            m_LowNum = 0;
            m_HighNum = 0;
            m_animatedImage = false;
        }
        emit DependChanged(true);

        d->m_UpdateLock.unlock();
        Load();
    }
    else
        d->m_UpdateLock.unlock();

    MythUIType::Reset();
}

/**
 *  \brief Initialises the class
 */
void MythUIImage::Init(void)
{
    m_CurPos = 0;
    m_LastDisplay = QTime::currentTime();

    m_NeedLoad = false;

    m_animationCycle = kCycleStart;
    m_animationReverse = false;
    m_animatedImage = false;

    m_runningThreads = 0;
}

/**
 *  \brief Set the image filename, does not load the image. See Load()
 */
void MythUIImage::SetFilename(const QString &filename)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);
    m_imageProperties.filename = filename;
    if (filename == m_OrigFilename)
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
    QWriteLocker updateLocker(&d->m_UpdateLock);
    m_imageProperties.filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;
    if (filepattern == m_OrigFilename)
        emit DependChanged(true);
    else
        emit DependChanged(false);
}

/**
 *  \brief Set the integer range for an animated image pattern
 */
void MythUIImage::SetImageCount(int low, int high)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);
    m_LowNum = low;
    m_HighNum = high;
}

/**
 *  \brief Set the delay between each image in an animation
 */
void MythUIImage::SetDelay(int delayms)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);
    m_Delay = delayms;
    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;
}

/**
 *  \brief Sets the delays between each image in an animation
 */
void MythUIImage::SetDelays(QVector<int> delays)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);
    QMutexLocker imageLocker(&m_ImagesLock);
    QVector<int>::iterator it;

    for (it = delays.begin(); it != delays.end(); ++it)
        m_Delays[m_Delays.size()] = *it;

    if (m_Delay == -1)
        m_Delay = m_Delays[0];

    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;
}

/**
 *  \brief Assign a MythImage to the widget. Use is strongly discouraged, use
 *         SetFilename() instead.
 */
void MythUIImage::SetImage(MythImage *img)
{
    d->m_UpdateLock.lockForWrite();

    if (!img)
    {
        d->m_UpdateLock.unlock();
        Reset();
        return;
    }

    m_imageProperties.filename = img->GetFileName();

    img->IncrRef();

    QSize forceSize = m_imageProperties.forceSize;
    if (!forceSize.isNull())
    {
        int w = (forceSize.width() <= 0) ? img->width() : forceSize.width();
        int h = (forceSize.height() <= 0) ? img->height() : forceSize.height();
        img->Resize(QSize(w, h), m_imageProperties.preserveAspect);
    }

    if (m_imageProperties.isReflected && !img->IsReflected())
        img->Reflect(m_imageProperties.reflectAxis,
                     m_imageProperties.reflectShear,
                     m_imageProperties.reflectScale,
                     m_imageProperties.reflectLength,
                     m_imageProperties.reflectSpacing);

    if (m_imageProperties.isGreyscale && !img->isGrayscale())
        img->ToGreyscale();

    Clear();
    m_Delay = -1;

    if (m_imageProperties.isOriented && !img->IsOriented() &&
        (m_imageProperties.orientation >= 1 &&
         m_imageProperties.orientation <= 8))
        img->Orientation(m_imageProperties.orientation);

    if (m_imageProperties.forceSize.isNull())
        SetSize(img->size());

    m_ImagesLock.lock();
    m_Images[0] = img;
    m_Delays.clear();
    m_ImagesLock.unlock();

    m_CurPos = 0;
    m_Initiator = m_EnableInitiator;
    SetRedraw();

    d->m_UpdateLock.unlock();
}

/**
 *  \brief Assign a set of MythImages to the widget for animation.
 *         Use is strongly discouraged, use SetFilepattern() instead.
 *
 */
void MythUIImage::SetImages(QVector<MythImage *> *images)
{
    Clear();

    QWriteLocker updateLocker(&d->m_UpdateLock);
    QSize aSize = GetFullArea().size();

    QVector<MythImage *>::iterator it;

    for (it = images->begin(); it != images->end(); ++it)
    {
        MythImage *im = (*it);

        if (!im)
        {
            QMutexLocker locker(&m_ImagesLock);
            m_Images[m_Images.size()] = im;
            continue;
        }

        im->IncrRef();


        QSize forceSize = m_imageProperties.forceSize;
        if (!forceSize.isNull())
        {
            int w = (forceSize.width() <= 0) ? im->width() : forceSize.width();
            int h = (forceSize.height() <= 0) ? im->height() : forceSize.height();
            im->Resize(QSize(w, h), m_imageProperties.preserveAspect);
        }

        if (m_imageProperties.isReflected && !im->IsReflected())
            im->Reflect(m_imageProperties.reflectAxis,
                        m_imageProperties.reflectShear,
                        m_imageProperties.reflectScale,
                        m_imageProperties.reflectLength,
                        m_imageProperties.reflectSpacing);

        if (m_imageProperties.isGreyscale && !im->isGrayscale())
            im->ToGreyscale();

        if (m_imageProperties.isOriented && !im->IsOriented() &&
            (m_imageProperties.orientation >= 1 &&
             m_imageProperties.orientation <= 8))
            im->Orientation(m_imageProperties.orientation);

        m_ImagesLock.lock();
        m_Images[m_Images.size()] = im;
        m_ImagesLock.unlock();

        aSize = aSize.expandedTo(im->size());
    }

    SetImageCount(1, m_Images.size());

    if (m_imageProperties.forceSize.isNull())
        SetSize(aSize);

    MythRect rect(GetFullArea());
    rect.setSize(aSize);
    SetMinArea(rect);

    m_CurPos = 0;
    m_animatedImage = true;
    m_Initiator = m_EnableInitiator;
    SetRedraw();
}

void MythUIImage::SetAnimationFrames(AnimationFrames frames)
{
    QVector<int> delays;
    QVector<MythImage *> images;

    AnimationFrames::iterator it;

    for (it = frames.begin(); it != frames.end(); ++it)
    {
        images.append((*it).first);
        delays.append((*it).second);
    }

    if (images.size())
    {
        SetImages(&images);

        if (m_Delay < 0  && delays.size())
            SetDelays(delays);
    }
    else
        Reset();
}

/**
 *  \brief Force the dimensions of the widget and image to the given size.
 */
void MythUIImage::ForceSize(const QSize &size)
{
    if (m_imageProperties.forceSize == size)
        return;

    d->m_UpdateLock.lockForWrite();
    m_imageProperties.forceSize = size;
    d->m_UpdateLock.unlock();

    if (size.isEmpty())
        return;

    SetSize(m_imageProperties.forceSize);

    Load();
    return;
}

/**
 *  \brief Saves the exif orientation value of the first image in the widget
 */
void MythUIImage::SetOrientation(int orientation)
{
    m_imageProperties.isOriented = true;
    m_imageProperties.orientation = orientation;
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
void MythUIImage::SetSize(const QSize &size)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);
    MythUIType::SetSize(size);
    m_NeedLoad = true;
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
    QWriteLocker updateLocker(&d->m_UpdateLock);
    m_imageProperties.cropRect = rect;
    SetRedraw();
}

/**
 *  \brief Load the image(s), wraps ImageLoader::LoadImage()
 */
bool MythUIImage::Load(bool allowLoadInBackground, bool forceStat)
{
    d->m_UpdateLock.lockForRead();

    m_Initiator = m_EnableInitiator;

    QString bFilename = m_imageProperties.filename;
    bFilename.detach();

    d->m_UpdateLock.unlock();

    QString filename = bFilename;

    if (bFilename.isEmpty())
    {
        Clear();
        SetMinArea(MythRect());
        SetRedraw();

        return false;
    }

    if (getenv("DISABLETHREADEDMYTHUIIMAGE"))
        allowLoadInBackground = false;

    // Don't clear the widget before we need to, otherwise it causes
    // unsightly flashing. We exclude animations for now since that requires a
    // deeper fix
    bool isAnimation = (m_HighNum != m_LowNum) || m_animatedImage;

    if (isAnimation)
        Clear();

    QString imagelabel;

    int j = 0;

    for (int i = m_LowNum; i <= m_HighNum && !m_animatedImage; i++)
    {
        if (!m_animatedImage && m_HighNum != m_LowNum &&
            bFilename.contains("%1"))
            filename = bFilename.arg(i);

        ImageProperties imProps = m_imageProperties;
        imProps.filename = filename;
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

        if (!isAnimation && !GetMythUI()->IsImageInCache(imagelabel))
            Clear();

        if (do_background_load)
        {
            SetMinArea(MythRect());
            LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC +
                QString("Load(), spawning thread to load '%1'").arg(filename));
                
            m_runningThreads++;
            ImageLoadThread *bImgThread;
            bImgThread = new ImageLoadThread(this, GetPainter(),
                                             imProps,
                                             bFilename, i,
                                             static_cast<ImageCacheMode>(cacheMode2));
            GetMythUI()->GetImageThreadPool()->start(bImgThread, "ImageLoad");
        }
        else
        {
            // Perform a blocking load
            LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC +
                QString("Load(), loading '%1' in foreground").arg(filename));
            bool aborted = false;

            if (ImageLoader::SupportsAnimation(filename))
            {
                AnimationFrames *myFrames;

                myFrames = ImageLoader::LoadAnimatedImage(GetPainter(), imProps,
                                        static_cast<ImageCacheMode>(cacheMode2),
                                        this, aborted);

                // TODO We might want to handle an abort here more gracefully
                if (aborted)
                    LOG(VB_GUI, LOG_DEBUG, QString("Aborted loading animated"
                                                   "image %1 in foreground")
                                                                .arg(filename));

                SetAnimationFrames(*myFrames);

                delete myFrames;
            }
            else
            {
                MythImage *image = NULL;

                image = ImageLoader::LoadImage(GetPainter(),
                                               imProps,
                                               static_cast<ImageCacheMode>(cacheMode2),
                                               this, aborted);

                // TODO We might want to handle an abort here more gracefully
                if (aborted)
                    LOG(VB_GUI, LOG_DEBUG, QString("Aborted loading animated"
                                                   "image %1 in foreground")
                                                                .arg(filename));

                if (image)
                {
                    if (m_imageProperties.forceSize.isNull())
                        SetSize(image->size());

                    MythRect rect(GetFullArea());
                    rect.setSize(image->size());
                    SetMinArea(rect);

                    m_ImagesLock.lock();
                    m_Images[j] = image;
                    m_ImagesLock.unlock();

                    SetRedraw();
                    d->m_UpdateLock.lockForWrite();
                    m_LastDisplay = QTime::currentTime();
                    d->m_UpdateLock.unlock();
                }
                else
                {
                    Reset();

                    m_ImagesLock.lock();
                    m_Images[j] = NULL;
                    m_ImagesLock.unlock();
                }
            }
        }

        ++j;
    }

    return true;
}

/**
 *  \copydoc MythUIType::Pulse()
 */
void MythUIImage::Pulse(void)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);

    int delay = -1;

    if (m_Delays.contains(m_CurPos))
        delay = m_Delays[m_CurPos];
    else if (m_Delay > 0)
        delay = m_Delay;

    if (delay > 0 &&
        abs(m_LastDisplay.msecsTo(QTime::currentTime())) > delay)
    {
        m_ImagesLock.lock();

        if (m_animationCycle == kCycleStart)
        {
            ++m_CurPos;

            if (m_CurPos >= (uint)m_Images.size())
                m_CurPos = 0;
        }
        else if (m_animationCycle == kCycleReverse)
        {
            if ((m_CurPos + 1) >= (uint)m_Images.size())
            {
                m_animationReverse = true;
            }
            else if (m_CurPos == 0)
            {
                m_animationReverse = false;
            }

            if (m_animationReverse)
                --m_CurPos;
            else
                ++m_CurPos;
        }

        m_ImagesLock.unlock();

        SetRedraw();
        m_LastDisplay = QTime::currentTime();
    }

    MythUIType::Pulse();
}

/**
 *  \copydoc MythUIType::DrawSelf()
 */
void MythUIImage::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                           int alphaMod, QRect clipRect)
{
    m_ImagesLock.lock();

    if (m_Images.size() > 0)
    {
        d->m_UpdateLock.lockForWrite();

        if (m_CurPos >= (uint)m_Images.size())
            m_CurPos = 0;

        if (!m_Images[m_CurPos])
        {
            unsigned int origPos = m_CurPos;
            m_CurPos++;

            while (!m_Images[m_CurPos] && m_CurPos != origPos)
            {
                m_CurPos++;

                if (m_CurPos >= (uint)m_Images.size())
                    m_CurPos = 0;
            }
        }

        QRect area = GetArea().toQRect();
        area.translate(xoffset, yoffset);

        int alpha = CalcAlpha(alphaMod);

        MythImage *currentImage = m_Images[m_CurPos];

        if (currentImage)
            currentImage->IncrRef();

        m_ImagesLock.unlock();
        d->m_UpdateLock.unlock();

        if (!currentImage)
            return;

        d->m_UpdateLock.lockForRead();

        QRect currentImageArea = currentImage->rect();

        if (!m_imageProperties.forceSize.isNull())
            area.setSize(area.size().expandedTo(currentImage->size()));

        // Centre image in available space
        int x = 0;
        int y = 0;

        if (area.width() > currentImageArea.width())
            x = (area.width() - currentImageArea.width()) / 2;

        if (area.height() > currentImageArea.height())
            y = (area.height() - currentImageArea.height()) / 2;

        if (x > 0 || y > 0)
            area.translate(x, y);

        QRect srcRect;
        m_imageProperties.cropRect.CalculateArea(GetFullArea());

        if (!m_imageProperties.cropRect.isEmpty())
            srcRect = m_imageProperties.cropRect.toQRect();
        else
            srcRect = currentImageArea;

        p->DrawImage(area, currentImage, srcRect, alpha);
        currentImage->DecrRef();
        d->m_UpdateLock.unlock();
    }
    else
        m_ImagesLock.unlock();
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIImage::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);

    if (element.tagName() == "filename")
    {
        m_OrigFilename = m_imageProperties.filename = getFirstText(element);

        if (m_imageProperties.filename.endsWith('/'))
        {
            QDir imageDir(m_imageProperties.filename);

            if (!imageDir.exists())
            {
                QString themeDir = GetMythUI()->GetThemeDir() + '/';
                imageDir = themeDir + m_imageProperties.filename;
            }

            QStringList imageTypes;

            QList< QByteArray > exts = QImageReader::supportedImageFormats();
            QList< QByteArray >::Iterator it = exts.begin();

            for (; it != exts.end(); ++it)
            {
                imageTypes.append(QString("*.").append(*it));
            }

            imageDir.setNameFilters(imageTypes);

            QStringList imageList = imageDir.entryList();
            QString randFile;

            if (imageList.size())
                randFile = QString("%1%2").arg(m_imageProperties.filename)
                           .arg(imageList.takeAt(random() % imageList.size()));

            m_OrigFilename = m_imageProperties.filename = randFile;
        }
    }
    else if (element.tagName() == "filepattern")
    {
        m_OrigFilename = m_imageProperties.filename = getFirstText(element);
        QString tmp = element.attribute("low");

        if (!tmp.isEmpty())
            m_LowNum = tmp.toInt();

        tmp = element.attribute("high");

        if (!tmp.isEmpty())
            m_HighNum = tmp.toInt();

        tmp = element.attribute("cycle", "start");

        if (tmp == "reverse")
            m_animationCycle = kCycleReverse;
    }
    else if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
        m_imageProperties.forceSize = m_Area.size();
    }
    else if (element.tagName() == "preserveaspect")
        m_imageProperties.preserveAspect = parseBool(element);
    else if (element.tagName() == "crop")
        m_imageProperties.cropRect = parseRect(element);
    else if (element.tagName() == "delay")
    {
        QString value = getFirstText(element);

        if (value.contains(","))
        {
            QVector<int> delays;
            QStringList tokens = value.split(",");
            QStringList::iterator it = tokens.begin();

            for (; it != tokens.end(); ++it)
            {
                if ((*it).isEmpty())
                {
                    if (delays.size())
                        delays.append(delays[delays.size()-1]);
                    else
                        delays.append(0); // Default 0ms delay before first image
                }
                else
                {
                    delays.append((*it).toInt());
                }
            }

            if (delays.size())
            {
                m_Delay = delays[0];
                SetDelays(delays);
            }
        }
        else
        {
            m_Delay = value.toInt();
        }
    }
    else if (element.tagName() == "reflection")
    {
        m_imageProperties.isReflected = true;
        QString tmp = element.attribute("axis");

        if (!tmp.isEmpty())
        {
            if (tmp.toLower() == "horizontal")
                m_imageProperties.reflectAxis = ReflectHorizontal;
            else
                m_imageProperties.reflectAxis = ReflectVertical;
        }

        tmp = element.attribute("shear");

        if (!tmp.isEmpty())
            m_imageProperties.reflectShear = tmp.toInt();

        tmp = element.attribute("scale");

        if (!tmp.isEmpty())
            m_imageProperties.reflectScale = tmp.toInt();

        tmp = element.attribute("length");

        if (!tmp.isEmpty())
            m_imageProperties.reflectLength = tmp.toInt();

        tmp = element.attribute("spacing");

        if (!tmp.isEmpty())
            m_imageProperties.reflectSpacing = tmp.toInt();
    }
    else if (element.tagName() == "mask")
    {
        QString maskfile = getFirstText(element);

        MythImage *newMaskImage = GetPainter()->GetFormatImage();
        if (newMaskImage->Load(maskfile))
            m_imageProperties.SetMaskImage(newMaskImage);
        else
            m_imageProperties.SetMaskImage(NULL);
        newMaskImage->DecrRef();
    }
    else if (element.tagName() == "grayscale" ||
             element.tagName() == "greyscale")
    {
        m_imageProperties.isGreyscale = parseBool(element);
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    m_NeedLoad = true;

    if (m_Parent && m_Parent->IsDeferredLoading(true))
        m_NeedLoad = false;

    return true;
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIImage::CopyFrom(MythUIType *base)
{
    d->m_UpdateLock.lockForWrite();
    MythUIImage *im = dynamic_cast<MythUIImage *>(base);

    if (!im)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2) ERROR, bad parsing '%3' (%4)")
            .arg(objectName()).arg(GetXMLLocation())
            .arg(base->objectName()).arg(base->GetXMLLocation()));
        d->m_UpdateLock.unlock();
        return;
    }

    m_OrigFilename = im->m_OrigFilename;

    m_Delay = im->m_Delay;
    m_LowNum = im->m_LowNum;
    m_HighNum = im->m_HighNum;

    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;

    m_imageProperties = im->m_imageProperties;

    m_animationCycle = im->m_animationCycle;
    m_animatedImage = im->m_animatedImage;

    MythUIType::CopyFrom(base);

    // We need to update forceSize in case the parent area has changed
    // however we only want to set forceSize if it was previously in use
    if (!m_imageProperties.forceSize.isNull())
        m_imageProperties.forceSize = m_Area.size();

    m_NeedLoad = im->m_NeedLoad;

    d->m_UpdateLock.unlock();

    d->m_UpdateLock.lockForRead();

    if (m_NeedLoad)
    {
        d->m_UpdateLock.unlock();
        Load();
    }
    else
        d->m_UpdateLock.unlock();
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIImage::CreateCopy(MythUIType *parent)
{
    QReadLocker updateLocker(&d->m_UpdateLock);
    MythUIImage *im = new MythUIImage(parent, objectName());
    im->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUIImage::Finalize(void)
{
    d->m_UpdateLock.lockForRead();

    if (m_NeedLoad)
    {
        d->m_UpdateLock.unlock();
        Load();
    }
    else
        d->m_UpdateLock.unlock();

    MythUIType::Finalize();
}

/**
 *  \copydoc MythUIType::LoadNow()
 */
void MythUIImage::LoadNow(void)
{
    d->m_UpdateLock.lockForWrite();

    if (m_NeedLoad)
    {
        d->m_UpdateLock.unlock();
        return;
    }

    m_NeedLoad = true;
    d->m_UpdateLock.unlock();

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
        MythImage *image = NULL;
        AnimationFrames *animationFrames = NULL;
        int number = 0;
        QString filename;
        bool aborted;

        ImageLoadEvent *le = static_cast<ImageLoadEvent *>(event);

        if (le->GetParent() != this)
            return;

        image  = le->GetImage();
        number = le->GetNumber();
        filename = le->GetFilename();
        animationFrames = le->GetAnimationFrames();
        aborted = le->GetAbortState();

        m_runningThreads--;

        d->m_UpdateLock.lockForRead();

        // 1) We aborted loading the image for some reason (e.g. two requests
        //    for same image)
        // 2) Filename changed since we started this image, so abort to avoid
        // rendering two different images in quick succession which causes
        // unsightly flickering
        if (aborted ||
            (le->GetBasefile() != m_imageProperties.filename))
        {
            d->m_UpdateLock.unlock();

            if (aborted)
                LOG(VB_GUI, LOG_DEBUG, QString("Aborted loading image %1")
                                                                .arg(filename));

            if (image)
                image->DecrRef();

            if (animationFrames)
            {
                AnimationFrames::iterator it;

                for (it = animationFrames->begin(); it != animationFrames->end();
                     ++it)
                {
                    MythImage *im = (*it).first;
                    if (im)
                        im->DecrRef();
                }

                delete animationFrames;
            }

            return;
        }

        d->m_UpdateLock.unlock();

        if (animationFrames)
        {
            SetAnimationFrames(*animationFrames);

            delete animationFrames;

            return;
        }

        if (image)
        {
            // We don't clear until we have the new image ready to display to
            // avoid unsightly flashing. This isn't currently supported for
            // animations.
            if ((m_HighNum == m_LowNum) && !m_animatedImage)
                Clear();

            d->m_UpdateLock.lockForWrite();

            if (m_imageProperties.forceSize.isNull())
                SetSize(image->size());

            MythRect rect(GetFullArea());
            rect.setSize(image->size());
            SetMinArea(rect);

            d->m_UpdateLock.unlock();

            m_ImagesLock.lock();

            if (m_Images[number])
            {
                // If we got to this point, it means this same MythUIImage
                // was told to reload the same image, so we use the newest
                // copy of the image.
                m_Images[number]->DecrRef(); // delete the original
            }

            m_Images[number] = image;
            m_ImagesLock.unlock();

            SetRedraw();

            d->m_UpdateLock.lockForWrite();
            m_LastDisplay = QTime::currentTime();
            d->m_UpdateLock.unlock();

            return;
        }

        // No Images were loaded, so trigger Reset to default
        Reset();
    }


}
