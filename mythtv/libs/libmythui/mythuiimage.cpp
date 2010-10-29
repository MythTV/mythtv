
#include "mythuiimage.h"

// C
#include <cstdlib>

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

// Libmythdb
#include "mythverbose.h"

// Mythui
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythscreentype.h"

class ImageLoadThread;

#define LOC      QString("MythUIImage(0x%1): ").arg((uintptr_t)this,0,16)
#define LOC_ERR  QString("MythUIImage(0x%1) Error: ").arg((uintptr_t)this,0,16)
#define LOC_WARN QString("MythUIImage(0x%1) Warning: ") \
                     .arg((uintptr_t)this,0,16)

/*!
 * \class ImageLoadEvent
 */
class ImageLoadEvent : public QEvent
{
  public:
    ImageLoadEvent(MythUIImage *parent, MythImage *image,
                   const QString &basefile, const QString &filename,
                   int number)
                 : QEvent(kEventType),
                   m_parent(parent), m_image(image), m_basefile(basefile),
                   m_filename(filename), m_number(number) { }

    MythUIImage *GetParent() const { return m_parent; }
    MythImage *GetImage() const { return m_image; }
    const QString GetBasefile() const { return m_basefile; }
    const QString GetFilename() const { return m_filename; }
    const int GetNumber() const { return m_number; }

    static Type kEventType;

  private:
    MythUIImage     *m_parent;
    MythImage       *m_image;
    QString          m_basefile;
    QString          m_filename;
    int              m_number;
};

QEvent::Type ImageLoadEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

/*!
* \class ImageLoadThread
*/
class ImageLoadThread : public QRunnable
{
  public:
    ImageLoadThread(MythUIImage *parent, const QString &basefile,
                    const QString &filename, int number,
                    QSize forceSize, ImageCacheMode mode) :
        m_parent(parent), m_basefile(basefile),
        m_filename(filename), m_number(number),
        m_ForceSize(forceSize), m_cacheMode(mode)
    {
        m_basefile.detach();
        m_filename.detach();
    }

    void run()
    {
        QString tmpFilename;
        if ((m_filename.startsWith("/")) ||
            (m_filename.startsWith("http://")) ||
            (m_filename.startsWith("https://")) ||
            (m_filename.startsWith("ftp://")))
            tmpFilename = m_filename;

        MythImageReader imageReader(tmpFilename);

        if (imageReader.supportsAnimation())
        {
            m_parent->LoadAnimatedImage(
                imageReader, m_filename, m_ForceSize, m_cacheMode);
        }
        else
        {
            MythImage *image = m_parent->LoadImage(
                imageReader, m_filename, m_ForceSize, m_cacheMode);
            ImageLoadEvent *le = new ImageLoadEvent(m_parent, image, m_basefile,
                                                    m_filename, m_number);
            QCoreApplication::postEvent(m_parent, le);
        }
    }

  private:
    MythUIImage *m_parent;
    QString      m_basefile;
    QString      m_filename;
    int          m_number;
    QSize        m_ForceSize;
    ImageCacheMode m_cacheMode;
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

QHash<QString, MythUIImage*> MythUIImage::m_loadingImages;
QMutex                       MythUIImage::m_loadingImagesLock;
QWaitCondition               MythUIImage::m_loadingImagesCond;

MythUIImage::MythUIImage(const QString &filepattern,
                         int low, int high, int delayms,
                         MythUIType *parent, const QString &name)
           : MythUIType(parent, name)
{
    m_Filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;

    m_Delay = delayms;

    d = new MythUIImagePrivate(this);

    Init();
}

MythUIImage::MythUIImage(const QString &filename, MythUIType *parent,
                         const QString &name)
           : MythUIType(parent, name)
{
    m_Filename = filename;
    m_OrigFilename = filename;

    m_LowNum = 0;
    m_HighNum = 0;
    m_Delay = -1;

    d = new MythUIImagePrivate(this);

    Init();
}

MythUIImage::MythUIImage(MythUIType *parent, const QString &name)
           : MythUIType(parent, name)
{
    m_LowNum = 0;
    m_HighNum = 0;
    m_Delay = -1;

    d = new MythUIImagePrivate(this);

    Init();
}

MythUIImage::~MythUIImage()
{
    // Wait until all image loading threads are complete or bad things
    // may happen if this MythUIImage disappears when a queued thread
    // needs it.
    GetMythUI()->GetImageThreadPool()->waitForDone();

    Clear();
    if (m_maskImage)
        m_maskImage->DownRef();

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
        QHash<int, MythImage*>::iterator it = m_Images.begin();
        if (*it)
            (*it)->DownRef();
        m_Images.remove(it.key());
    }
    m_Delays.clear();
}

/**
 *  \brief Reset the image back to the default defined in the theme
 */
void MythUIImage::Reset(void)
{
    d->m_UpdateLock.lockForWrite();
    if (m_Filename != m_OrigFilename)
    {
        m_Filename = m_OrigFilename;
        m_animatedImage = false;
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
    m_cropRect = MythRect(0,0,0,0);
    m_ForceSize = QSize(0,0);

    m_CurPos = 0;
    m_LastDisplay = QTime::currentTime();

    m_NeedLoad = false;

    m_isReflected = false;
    m_reflectShear = 0;
    m_reflectScale = m_reflectLength = 100;
    m_reflectAxis = ReflectVertical;
    m_reflectSpacing = 0;

    m_gradient = false;
    m_gradientStart = QColor("#505050");
    m_gradientEnd = QColor("#000000");
    m_gradientAlpha = 100;
    m_gradientDirection = FillTopToBottom;

    m_isMasked = false;
    m_maskImage = NULL;

    m_isGreyscale = false;

    m_preserveAspect = false;
    
    m_animationCycle = kCycleStart;
    m_animationReverse = false;
    m_animatedImage = false;
}

/**
 *  \brief Set the image filename, does not load the image. See Load()
 */
void MythUIImage::SetFilename(const QString &filename)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);
    m_Filename = filename;
}

/**
 *  \brief Set the image filename pattern and integer range for an animated
 *         image, does not load the image. See Load()
 */
void MythUIImage::SetFilepattern(const QString &filepattern, int low,
                                 int high)
{
    QWriteLocker updateLocker(&d->m_UpdateLock);
    m_Filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;
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
    QWriteLocker updateLocker(&d->m_UpdateLock);
    if (!img)
    {
        Reset();
        return;
    }

    m_Filename = img->GetFileName();
    Clear();
    m_Delay = -1;

    img->UpRef();

    if (!m_ForceSize.isNull())
    {
        int w = (m_ForceSize.width() <= 0) ? img->width() : m_ForceSize.width();
        int h = (m_ForceSize.height() <= 0) ? img->height() : m_ForceSize.height();
        img->Resize(QSize(w, h), m_preserveAspect);
    }

    if (m_isReflected && !img->IsReflected())
        img->Reflect(m_reflectAxis, m_reflectShear, m_reflectScale,
                        m_reflectLength, m_reflectSpacing);

    if (m_isGreyscale && !img->isGrayscale())
        img->ToGreyscale();

    if (m_ForceSize.isNull())
        SetSize(img->size());

    m_ImagesLock.lock();
    m_Images[0] = img;
    m_Delays.clear();
    m_ImagesLock.unlock();

    m_CurPos = 0;
    SetRedraw();
}

/**
 *  \brief Assign a set of MythImages to the widget for animation.
 *         Use is strongly discouraged, use SetFilepattern() instead.
 *
 */
void MythUIImage::SetImages(QVector<MythImage *> &images)
{
    Clear();

    // HACK: This is just a workaround for the buttonlist inheritance problem
    //       until the image cache is completed.
    d->m_UpdateLock.lockForRead();
    if (m_gradient)
    {
        d->m_UpdateLock.unlock();
        Load();
        return;
    }
    d->m_UpdateLock.unlock();

    QWriteLocker updateLocker(&d->m_UpdateLock);
    QSize aSize = GetArea().size();

    QVector<MythImage *>::iterator it;
    for (it = images.begin(); it != images.end(); ++it)
    {
        MythImage *im = (*it);
        if (!im)
        {
            QMutexLocker locker(&m_ImagesLock);
            m_Images[m_Images.size()] = im;
            continue;
        }

        im->UpRef();

        if (!m_ForceSize.isNull())
        {
            int w = (m_ForceSize.width() <= 0) ? im->width() : m_ForceSize.width();
            int h = (m_ForceSize.height() <= 0) ? im->height() : m_ForceSize.height();

            im->Resize(QSize(w, h), m_preserveAspect);
        }

        if (m_isReflected && !im->IsReflected())
            im->Reflect(m_reflectAxis, m_reflectShear, m_reflectScale,
                         m_reflectLength, m_reflectSpacing);

        if (m_isGreyscale && !im->isGrayscale())
            im->ToGreyscale();

        m_ImagesLock.lock();
        m_Images[m_Images.size()] = im;
        m_ImagesLock.unlock();

        aSize = aSize.expandedTo(im->size());
    }

    SetImageCount(1, m_Images.size());

    if (m_ForceSize.isNull())
        SetSize(aSize);

    m_CurPos = 0;
    SetRedraw();
}

/**
 *  \brief Force the dimensions of the widget and image to the given size.
 */
void MythUIImage::ForceSize(const QSize &size)
{
    if (m_ForceSize == size)
        return;

    d->m_UpdateLock.lockForWrite();
    m_ForceSize = size;
    d->m_UpdateLock.unlock();

    if (size.isEmpty())
        return;

    SetSize(m_ForceSize);

    Load();
    return;
}

/**
 *  \brief Set the size of the widget
 */
void MythUIImage::SetSize(int width, int height)
{
    SetSize(QSize(width,height));
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
    m_cropRect = rect;
    SetRedraw();
}

/**
 *  \brief Generates a unique identifying string for this image which is used
 *         as a key in the image cache.
 */
QString MythUIImage::GenImageLabel(const QString &filename, int w, int h) const
{
    QReadLocker updateLocker(&d->m_UpdateLock);
    QString imagelabel;
    QString s_Attrib;

    if (m_isMasked)
        s_Attrib = "masked";

    if (m_isReflected)
        s_Attrib += "reflected";

    if (m_isGreyscale)
        s_Attrib += "greyscale";

    imagelabel  = QString("%1-%2-%3x%4.png")
                          .arg(filename)
                          .arg(s_Attrib)
                          .arg(w)
                          .arg(h);
    imagelabel.replace('/','-');

    return imagelabel;
}

/**
 *  \brief Generates a unique identifying string for this image which is used
 *         as a key in the image cache.
 */
QString MythUIImage::GenImageLabel(int w, int h) const
{
    QReadLocker updateLocker(&d->m_UpdateLock);
    return GenImageLabel(m_Filename, w, h);
}

/**
 *  \brief Load the image(s), wraps LoadImage()
 */
bool MythUIImage::Load(bool allowLoadInBackground, bool forceStat)
{
    d->m_UpdateLock.lockForRead();

    QSize bForceSize = m_ForceSize;
    QString bFilename = m_Filename;
    bFilename.detach();

    d->m_UpdateLock.unlock();

    QString filename = bFilename;

    if (bFilename.isEmpty() && (!m_gradient))
    {
        Clear();
        SetRedraw();

        return false;
    }

    Clear();

//    SetRedraw();

//    if (!IsVisible(true))
//        return false;

    int w = -1;
    int h = -1;

    if (!bForceSize.isNull())
    {
        if (bForceSize.width() != -1)
            w = bForceSize.width();

        if (bForceSize.height() != -1)
            h = bForceSize.height();
    }

    if (m_gradient)
    {
        int gradWidth = 10;
        int gradHeight = 10;

        if (!m_Area.isEmpty())
        {
            gradWidth  = GetArea().size().width();
            gradHeight = GetArea().size().height();
        }
        else if ((w > 0) && (h > 0))
        {
            gradWidth = w;
            gradHeight = h;
        }

        int sr, sg, sb;
        m_gradientStart.getRgb(&sr, &sg, &sb);

        int er, eg, eb;
        m_gradientEnd.getRgb(&er, &eg, &eb);

        d->m_UpdateLock.lockForWrite();
        m_Filename =
            QString("gradient-%1x%2-%3-%4-%5-%6-%7-%8-%9-%10")
                    .arg(gradWidth).arg(gradHeight)
                    .arg(sr).arg(sg).arg(sb).arg(er).arg(eg).arg(eb)
                    .arg(m_gradientAlpha).arg(m_gradientDirection);
        bFilename = m_Filename;
        bFilename.detach();
        d->m_UpdateLock.unlock();

        filename = bFilename;

        VERBOSE(VB_GUI|VB_FILE|VB_EXTRA, LOC +
                QString("Auto-generated gradient filename of '%1'")
                .arg(bFilename));
    }

    QString imagelabel;

    int j = 0;
    for (int i = m_LowNum; i <= m_HighNum && !m_animatedImage; i++)
    {
        if (!m_animatedImage && m_HighNum >= 1)
            filename = bFilename.arg(i);

        imagelabel = GenImageLabel(filename, w, h);

        // Only load in the background if allowed and the image is
        // not already in our mem cache
        ImageCacheMode cacheMode = kCacheCheckMemoryOnly;
        if (m_gradient)
            cacheMode = kCacheIgnoreDisk;
        else if (forceStat)
            cacheMode = (ImageCacheMode)
                ((int)kCacheCheckMemoryOnly | (int)kCacheForceStat);
        
        ImageCacheMode cacheMode2 = (!forceStat) ? kCacheNormal :
            (ImageCacheMode) ((int)kCacheNormal | (int)kCacheForceStat);


        if ((allowLoadInBackground) &&
            (!GetMythUI()->LoadCacheImage(filename, imagelabel,
                                          GetPainter(), cacheMode)) &&
            (!getenv("DISABLETHREADEDMYTHUIIMAGE")))
        {
            VERBOSE(VB_GUI|VB_FILE|VB_EXTRA, LOC + QString(
                        "Load(), spawning thread to load '%1'").arg(filename));
            ImageLoadThread *bImgThread = new ImageLoadThread(
                this, bFilename, filename, i, bForceSize, cacheMode2);
            GetMythUI()->GetImageThreadPool()->start(bImgThread);
        }
        else
        {
            // Perform a blocking load
            VERBOSE(VB_GUI|VB_FILE|VB_EXTRA, LOC + QString(
                        "Load(), loading '%1' in foreground").arg(filename));
            QString tmpFilename;
            if ((filename.startsWith("/")) ||
                (filename.startsWith("http://")) ||
                (filename.startsWith("https://")) ||
                (filename.startsWith("ftp://")))
                tmpFilename = filename;

            MythImageReader imageReader(tmpFilename);

            if (imageReader.supportsAnimation())
            {
                LoadAnimatedImage(
                    imageReader, filename, bForceSize, cacheMode2);
            }
            else
            {
                MythImage *image = LoadImage(
                    imageReader, filename, bForceSize, cacheMode2);
                if (image)
                {
                    if (bForceSize.isNull())
                        SetSize(image->size());

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
*  \brief Load an image
*/
MythImage *MythUIImage::LoadImage(
    MythImageReader &imageReader, const QString &imFile,
    QSize bForceSize, int cacheMode)
{
    QString filename = imFile;

    m_loadingImagesLock.lock();

    // Check to see if the image is being loaded by us in another thread
    if ((m_loadingImages.contains(filename)) &&
        (m_loadingImages[filename] == this))
    {
        VERBOSE(VB_GUI|VB_FILE|VB_EXTRA, LOC + QString(
                    "MythUIImage::LoadImage(%1), this "
                    "file is already being loaded by this same MythUIImage in "
                    "another thread.").arg(filename));
        m_loadingImagesLock.unlock();
        return NULL;
    }

    // Check to see if the exact same image is being loaded anywhere else
    while (m_loadingImages.contains(filename))
        m_loadingImagesCond.wait(&m_loadingImagesLock);

    m_loadingImages[filename] = this;
    m_loadingImagesLock.unlock();

    VERBOSE(VB_GUI|VB_FILE, LOC + QString("LoadImage(%2) Object %3")
            .arg(filename).arg(objectName()));

    MythImage *image = NULL;

    bool bForceResize = false;
    bool bFoundInCache = false;

    QString imagelabel;

    int w = -1;
    int h = -1;

    if (!bForceSize.isNull())
    {
        if (bForceSize.width() != -1)
            w = bForceSize.width();

        if (bForceSize.height() != -1)
            h = bForceSize.height();

        bForceResize = true;
    }

    imagelabel = GenImageLabel(filename, w, h);

    if (!imageReader.supportsAnimation())
    {
        image = GetMythUI()->LoadCacheImage(
            filename, imagelabel, GetPainter(), (ImageCacheMode) cacheMode);
    }

    if (image)
    {
        image->UpRef();

        VERBOSE(VB_GUI|VB_FILE, LOC +
                QString("LoadImage found in cache :%1: RefCount = %2")
                .arg(imagelabel).arg(image->RefCount()));

        if (m_isReflected)
            image->setIsReflected(true);

        bFoundInCache = true;
    }
    else
    {
        if (m_gradient)
        {
            VERBOSE(VB_GUI|VB_FILE, LOC +
                    QString("LoadImage Not Found in cache. "
                            "Creating Gradient :%1:").arg(filename));

            QSize gradsize;
            if (!m_Area.isEmpty())
                gradsize = GetArea().size();
            else
                gradsize = QSize(10, 10);

            image = MythImage::Gradient(GetPainter(), gradsize, m_gradientStart,
                                        m_gradientEnd, m_gradientAlpha,
                                        m_gradientDirection);
            image->UpRef();
        }
        else
        {
            VERBOSE(VB_GUI|VB_FILE, LOC +
                    QString("LoadImage Not Found in cache. "
                            "Loading Directly :%1:").arg(filename));

            image = GetPainter()->GetFormatImage();
            image->UpRef();
            bool ok = false;
            if (imageReader.supportsAnimation())
                ok = image->Load(imageReader);
            else
                ok = image->Load(filename);

            if (!ok)
            {
                image->DownRef();

                m_loadingImagesLock.lock();
                m_loadingImages.remove(filename);
                m_loadingImagesCond.wakeAll();
                m_loadingImagesLock.unlock();

                return NULL;
            }
        }
    }

    if (!bFoundInCache)
    {
        if (bForceResize)
            image->Resize(QSize(w, h), m_preserveAspect);

        if (m_isMasked)
        {
            QRect imageArea = image->rect();
            QRect maskArea = m_maskImage->rect();

            // Crop the mask to the image
            int x = 0;
            int y = 0;
            if (maskArea.width() > imageArea.width())
                x = (maskArea.width() - imageArea.width()) / 2;
            if (maskArea.height() > imageArea.height())
                y = (maskArea.height() - imageArea.height()) / 2;

            if (x > 0 || y > 0)
                imageArea.translate(x,y);
            d->m_UpdateLock.lockForWrite();
            QImage mask = m_maskImage->copy(imageArea);
            d->m_UpdateLock.unlock();
            image->setAlphaChannel(mask.alphaChannel());
        }

        if (m_isReflected)
            image->Reflect(m_reflectAxis, m_reflectShear, m_reflectScale,
                        m_reflectLength, m_reflectSpacing);

        if (m_isGreyscale)
            image->ToGreyscale();

        if (!imageReader.supportsAnimation())
            GetMythUI()->CacheImage(imagelabel, image);
    }

    if (image->isNull())
    {
        VERBOSE(VB_GUI|VB_FILE, LOC + QString("LoadImage Image is NULL :%1:")
                .arg(filename));

        image->DownRef();
        Reset();

        m_loadingImagesLock.lock();
        m_loadingImages.remove(filename);
        m_loadingImagesCond.wakeAll();
        m_loadingImagesLock.unlock();

        return NULL;
    }

    image->SetChanged();

    m_loadingImagesLock.lock();
    m_loadingImages.remove(filename);
    m_loadingImagesCond.wakeAll();
    m_loadingImagesLock.unlock();

    return image;
}

/**
*  \brief Load an animated image
*/
bool MythUIImage::LoadAnimatedImage(
    MythImageReader &imageReader, const QString &imFile,
    QSize bForceSize, int cacheMode)
{
    bool result = false;
    m_loadingImagesLock.lock();

    // Check to see if the image is being loaded by us in another thread
    if ((m_loadingImages.contains(imFile)) &&
        (m_loadingImages[imFile] == this))
    {
        VERBOSE(VB_GUI|VB_FILE|VB_EXTRA, LOC + QString(
                    "MythUIImage::LoadAnimatedImage(%1), this "
                    "file is already being loaded by this same MythUIImage in "
                    "another thread.").arg(imFile));
        m_loadingImagesLock.unlock();
        return result;
    }

    // Check to see if the exact same image is being loaded anywhere else
    while (m_loadingImages.contains(imFile))
        m_loadingImagesCond.wait(&m_loadingImagesLock);

    m_loadingImages[imFile] = this;
    m_loadingImagesLock.unlock();

    QString filename = QString("frame-%1-") + imFile;
    QString frameFilename;
    QVector<MythImage *> images;
    QVector<int> delays;
    int imageCount = 1;
    QString imageLabel;

    int w = -1;
    int h = -1;

    if (!bForceSize.isNull())
    {
        if (bForceSize.width() != -1)
            w = bForceSize.width();

        if (bForceSize.height() != -1)
            h = bForceSize.height();
    }

    while (imageReader.canRead())
    {
        frameFilename = filename.arg(imageCount);
        imageLabel = GenImageLabel(frameFilename, w, h);
        MythImage *im = LoadImage(
            imageReader, frameFilename,
            bForceSize, (ImageCacheMode) cacheMode);

        if (!im)
            break;

        images.append(im);
        delays.append(imageReader.nextImageDelay());
        imageCount++;
    }

    if (images.size())
    {
        m_animatedImage = true;
        SetImages(images);
        if ((m_Delay == -1) &&
            (imageReader.supportsAnimation()) &&
            (delays.size()))
        {
            SetDelays(delays);
        }
        result = true;
    }

    m_loadingImagesLock.lock();
    m_loadingImages.remove(imFile);
    m_loadingImagesCond.wakeAll();
    m_loadingImagesLock.unlock();

    return result;
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
            currentImage->UpRef();
        m_ImagesLock.unlock();
        d->m_UpdateLock.unlock();

        if (!currentImage)
            return;

        d->m_UpdateLock.lockForRead();

        QRect currentImageArea = currentImage->rect();

        if (!m_ForceSize.isNull())
            area.setSize(area.size().expandedTo(currentImage->size()));

        // Centre image in available space
        int x = 0;
        int y = 0;
        if (area.width() > currentImageArea.width())
            x = (area.width() - currentImageArea.width()) / 2;
        if (area.height() > currentImageArea.height())
            y = (area.height() - currentImageArea.height()) / 2;

        if (x > 0 || y > 0)
            area.translate(x,y);

        QRect srcRect;
        m_cropRect.CalculateArea(GetArea());
        if (!m_cropRect.isEmpty())
            srcRect = m_cropRect.toQRect();
        else
            srcRect = currentImageArea;

        p->DrawImage(area, currentImage, srcRect, alpha);
        currentImage->DownRef();
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
        m_OrigFilename = m_Filename = getFirstText(element);
        if (m_Filename.endsWith('/'))
        {
            QDir imageDir(m_Filename);
            if (!imageDir.exists())
            {
                QString themeDir = GetMythUI()->GetThemeDir() + '/';
                imageDir = themeDir + m_Filename;
            }
            QStringList imageTypes;

            QList< QByteArray > exts = QImageReader::supportedImageFormats();
            QList< QByteArray >::Iterator it = exts.begin();
            for (;it != exts.end();++it)
            {
                imageTypes.append( QString("*.").append(*it) );
            }

            imageDir.setNameFilters(imageTypes);

            QStringList imageList = imageDir.entryList();
            srand(time(NULL));
            QString randFile;
            if (imageList.size())
                randFile = QString("%1%2").arg(m_Filename)
                           .arg(imageList.takeAt(rand() % imageList.size()));
            m_OrigFilename = m_Filename = randFile;
        }
    }
    else if (element.tagName() == "filepattern")
    {
        m_OrigFilename = m_Filename = getFirstText(element);
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
    else if (element.tagName() == "gradient")
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Use of gradient in an imagetype is "
                            "deprecated, see shape gradients instead.");
        m_gradient = true;
        m_gradientStart = QColor(element.attribute("start", "#505050"));
        m_gradientEnd = QColor(element.attribute("end", "#000000"));
        m_gradientAlpha = element.attribute("alpha", "100").toInt();
        QString direction = element.attribute("direction", "vertical");
        if (direction == "vertical")
            m_gradientDirection = FillTopToBottom;
        else
            m_gradientDirection = FillLeftToRight;
    }
    else if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
        m_ForceSize = m_Area.size();
    }
    else if (element.tagName() == "preserveaspect")
        m_preserveAspect = parseBool(element);
    else if (element.tagName() == "crop")
        m_cropRect = parseRect(element);
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
        m_isReflected = true;
        QString tmp = element.attribute("axis");
        if (!tmp.isEmpty())
        {
            if (tmp.toLower() == "horizontal")
                m_reflectAxis = ReflectHorizontal;
            else
                m_reflectAxis = ReflectVertical;
        }
        tmp = element.attribute("shear");
        if (!tmp.isEmpty())
            m_reflectShear = tmp.toInt();
        tmp = element.attribute("scale");
        if (!tmp.isEmpty())
            m_reflectScale = tmp.toInt();
        tmp = element.attribute("length");
        if (!tmp.isEmpty())
            m_reflectLength = tmp.toInt();
        tmp = element.attribute("spacing");
        if (!tmp.isEmpty())
            m_reflectSpacing = tmp.toInt();
    }
    else if (element.tagName() == "mask")
    {
        QString maskfile = getFirstText(element);
        if (m_maskImage)
        {
            m_maskImage->DownRef();
            m_maskImage = NULL;
        }

        m_maskImage = GetPainter()->GetFormatImage();
        m_maskImage->UpRef();
        if (m_maskImage->Load(maskfile))
            m_isMasked = true;
        else
        {
            m_maskImage->DownRef();
            m_maskImage = NULL;
            m_isMasked = false;
        }
    }
    else if (element.tagName() == "grayscale" ||
             element.tagName() == "greyscale")
    {
        m_isGreyscale = parseBool(element);
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "bad parsing");
        d->m_UpdateLock.unlock();
        return;
    }

    m_Filename = im->m_Filename;
    m_OrigFilename = im->m_OrigFilename;

    m_cropRect = im->m_cropRect;
    m_ForceSize = im->m_ForceSize;

    m_Delay = im->m_Delay;
    m_LowNum = im->m_LowNum;
    m_HighNum = im->m_HighNum;

    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;

    m_isReflected = im->m_isReflected;
    m_reflectAxis = im->m_reflectAxis;
    m_reflectShear = im->m_reflectShear;
    m_reflectScale = im->m_reflectScale;
    m_reflectLength = im->m_reflectLength;
    m_reflectSpacing = im->m_reflectSpacing;

    m_isMasked = im->m_isMasked;
    m_maskImage = im->m_maskImage;
    if (m_maskImage)
        m_maskImage->UpRef();

    m_gradient = im->m_gradient;
    m_gradientStart = im->m_gradientStart;
    m_gradientEnd = im->m_gradientEnd;
    m_gradientAlpha = im->m_gradientAlpha;
    m_gradientDirection = im->m_gradientDirection;

    m_preserveAspect = im->m_preserveAspect;

    m_isGreyscale = im->m_isGreyscale;

    m_animationCycle = im->m_animationCycle;
    m_animatedImage = im->m_animatedImage;

    //SetImages(im->m_Images);

    MythUIType::CopyFrom(base);

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
        ImageLoadEvent *le = dynamic_cast<ImageLoadEvent*>(event);

        if (le->GetParent() != this)
            return;

        MythImage *image = le->GetImage();

        if (!image)
            return;

        d->m_UpdateLock.lockForRead();
        if (le->GetBasefile() != m_Filename)
        {
            d->m_UpdateLock.unlock();
#if 0
            VERBOSE(VB_GUI|VB_FILE|VB_EXTRA, LOC +
                    QString("customEvent(): Expecting '%2', got '%3'")
                    .arg(m_Filename).arg(le->GetBasefile()));
#endif
            image->DownRef();
            return;
        }
        d->m_UpdateLock.unlock();

        QString filename = le->GetFilename();
        int number = le->GetNumber();

        d->m_UpdateLock.lockForWrite();
        if (m_ForceSize.isNull())
            SetSize(image->size());
        d->m_UpdateLock.unlock();

        m_ImagesLock.lock();
        if (m_Images[number])
        {
            // If we got to this point, it means this same MythUIImage
            // was told to reload the same image, so we use the newest
            // copy of the image.
            m_Images[number]->DownRef(); // delete the original
        }
        m_Images[number] = image;
        m_ImagesLock.unlock();

        SetRedraw();

        d->m_UpdateLock.lockForWrite();
        m_LastDisplay = QTime::currentTime();
        d->m_UpdateLock.unlock();
    }
}
