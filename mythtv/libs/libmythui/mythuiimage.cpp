#include <cstdlib>

#include <QFile>
#include <QDir>
#include <QDomDocument>
#include <QImageReader>

#include "mythverbose.h"

#include "mythuiimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythscreentype.h"

MythUIImage::MythUIImage(const QString &filepattern,
                         int low, int high, int delayms,
                         MythUIType *parent, const QString &name)
           : MythUIType(parent, name)
{
    m_Filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;

    m_Delay = delayms;

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

    Init();
}

MythUIImage::MythUIImage(MythUIType *parent, const QString &name)
           : MythUIType(parent, name)
{
    m_LowNum = 0;
    m_HighNum = 0;
    m_Delay = -1;

    Init();
}

MythUIImage::~MythUIImage()
{
    Clear();
    if (m_maskImage)
        m_maskImage->DownRef();
}

/**
 *  \brief Remove all images from the widget
 */
void MythUIImage::Clear(void)
{
    while (!m_Images.isEmpty())
    {
        m_Images.back()->DownRef();
        m_Images.pop_back();
    }
}

/**
 *  \brief Reset the image back to the default defined in the theme
 */
void MythUIImage::Reset(void)
{
    if (m_Filename != m_OrigFilename)
    {
        m_Filename = m_OrigFilename;
        Load();
    }
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
}

/**
 *  \brief Set the image filename, does not load the image. See Load()
 */
void MythUIImage::SetFilename(const QString &filename)
{
    m_Filename = filename;
}

/**
 *  \brief Set the image filename pattern and integer range for an animated
 *         image, does not load the image. See Load()
 */
void MythUIImage::SetFilepattern(const QString &filepattern, int low,
                                 int high)
{
    m_Filename = filepattern;
    m_LowNum = low;
    m_HighNum = high;
}

/**
 *  \brief Set the integer range for an animated image pattern
 */
void MythUIImage::SetImageCount(int low, int high)
{
    m_LowNum = low;
    m_HighNum = high;
}

/**
 *  \brief Set the delay between each image in an animation
 */
void MythUIImage::SetDelay(int delayms)
{
    m_Delay = delayms;
    m_LastDisplay = QTime::currentTime();
    m_CurPos = 0;
}

/**
 *  \brief Assign a MythImage to the widget. Discouraged, use SetFilename()
 *         instead.
 */
void MythUIImage::SetImage(MythImage *img)
{
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

    m_Images.push_back(img);
    m_CurPos = 0;
    SetRedraw();
}

/**
 *  \brief Assign a set of MythImages to the widget for animation.
 *         Discouraged, use SetFilePattern() instead.
 */
void MythUIImage::SetImages(QVector<MythImage *> &images)
{
    Clear();

    // HACK: This is just a workaround for the buttonlist inheritance problem
    //       until the image cache is completed.
    if (m_gradient)
    {
        Load();
        return;
    }

    QSize aSize = m_Area.size();

    QVector<MythImage *>::iterator it;
    for (it = images.begin(); it != images.end(); ++it)
    {
        MythImage *im = (*it);
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

        m_Images.push_back(im);

        aSize = aSize.expandedTo(im->size());
    }

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

    m_ForceSize = size;

    if (size.isEmpty())
        return;

    QSize aSize = m_Area.size();

    QVector<MythImage *>::iterator it;
    for (it = m_Images.begin(); it != m_Images.end(); ++it)
    {
        MythImage *im = (*it);

        int w = (m_ForceSize.width() <= 0) ? im->width() : m_ForceSize.width();
        int h = (m_ForceSize.height() <= 0) ? im->height() : m_ForceSize.height();

        im->Resize(QSize(w, h), m_preserveAspect);
        aSize = aSize.expandedTo(im->size());
    }

    SetSize(m_ForceSize);
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
    m_cropRect = rect;
    SetRedraw();
}

QString MythUIImage::GenImageLabel(const QString &filename, int w, int h)
{
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
    imagelabel.replace("/","-");

    return imagelabel;
}

QString MythUIImage::GenImageLabel(int w, int h)
{
    return GenImageLabel(m_Filename, w, h);
}

/**
 *  \brief Load the image(s)
 */
bool MythUIImage::Load(void)
{
    Clear();

//     if (!IsVisible(true))
//         return false;

    if (m_Filename.isEmpty() && (!m_gradient))
        return false;

    QSize newSize;

    VERBOSE(VB_FILE, QString("MythUIImage::Load (%1) Object %2").arg(m_Filename).arg(objectName()));

    for (int i = m_LowNum; i <= m_HighNum; i++)
    {
        MythImage *image = NULL;
        bool bNeedLoad = false;

        if (m_gradient)
        {
            QSize gradsize;
            if (!m_Area.isEmpty())
                gradsize = m_Area.size();
            else
                gradsize = QSize(10, 10);

            image = MythImage::Gradient(gradsize, m_gradientStart,
                                        m_gradientEnd, m_gradientAlpha,
                                        m_gradientDirection);
            image->UpRef();
        }
        else
            bNeedLoad = true;

        int w = -1;
        int h = -1;

        bool bForceResize = false;
        bool bFoundInCache = false;

        QString imagelabel;

        if (!m_ForceSize.isNull())
        {
            if (m_ForceSize.width() != -1)
                w = m_ForceSize.width();

            if (m_ForceSize.height() != -1)
                h = m_ForceSize.height();

            bForceResize = true;
        }

        if (bNeedLoad)
        {

            QString filename = m_Filename;
            if (m_HighNum >= 1)
                filename = QString(m_Filename).arg(i);

            imagelabel = GenImageLabel(filename, w, h);

            image = GetMythUI()->LoadCacheImage(filename, imagelabel);
            if (image)
            {
                image->UpRef();
                VERBOSE(VB_FILE, QString("MythUIImage::Load found in cache :%1:").arg(imagelabel));
                if (m_isReflected)
                    image->setIsReflected(true);

                bFoundInCache = true;
            }
            else
            {
                VERBOSE(VB_FILE, QString("MythUIImage::Load Not Found in "
                                         "cache. Loading Directly :%1:")
                                         .arg(filename));
                image = GetMythPainter()->GetFormatImage();
                image->UpRef();
                if (!image->Load(filename))
                {
                    VERBOSE(VB_FILE, QString("MythUIImage::Load Could not load :%1:").arg(filename));
                    image->DownRef();
                    SetRedraw();
                    return false;
                }
            }
        }
        else
            imagelabel = GenImageLabel(w,h);

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
                QImage mask = m_maskImage->copy(imageArea);
                image->setAlphaChannel(mask.alphaChannel());
            }

            if (m_isReflected)
                image->Reflect(m_reflectAxis, m_reflectShear, m_reflectScale,
                            m_reflectLength, m_reflectSpacing);

            if (m_isGreyscale)
                image->ToGreyscale();

            // Save scaled copy to cache
            if (bNeedLoad)
                GetMythUI()->CacheImage(imagelabel, image);
        }

        if (!bForceResize)
        {
            newSize = newSize.expandedTo(image->size());
            SetSize(newSize);
        }

        if (image->isNull())
        {
            VERBOSE(VB_FILE, QString("MythUIImage::Load Image is NULL :%1:").arg(m_Filename));
            image->DownRef();
        }
        else
        {
            image->SetChanged();
            m_Images.push_back(image);
        }
    }

    m_LastDisplay = QTime::currentTime();
    SetRedraw();

    return true;
}

/**
 *  \copydoc MythUIType::Pulse()
 */
void MythUIImage::Pulse(void)
{
    if (m_Delay > 0 &&
        abs(m_LastDisplay.msecsTo(QTime::currentTime())) > m_Delay)
    {
        m_CurPos++;
        if (m_CurPos >= (uint)m_Images.size())
            m_CurPos = 0;

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
    if (m_Images.size() > 0)
    {
        if (m_CurPos > (uint)m_Images.size())
            m_CurPos = 0;

        QRect area = m_Area.toQRect();
        area.translate(xoffset, yoffset);

        int alpha = CalcAlpha(alphaMod);

        MythImage *currentImage = m_Images[m_CurPos];
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
        m_cropRect.CalculateArea(m_Area);
        if (!m_cropRect.isEmpty())
            srcRect = m_cropRect.toQRect();
        else
            srcRect = currentImageArea;

        p->DrawImage(area, currentImage, srcRect, alpha);
    }
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIImage::ParseElement(QDomElement &element)
{
    if (element.tagName() == "filename")
    {
        m_OrigFilename = m_Filename = getFirstText(element);
        if (m_Filename.endsWith("/"))
        {
            QDir imageDir(m_Filename);
            if (!imageDir.exists())
            {
                QString themeDir = GetMythUI()->GetThemeDir() + "/";
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
            QString randFile = QString("%1%2").arg(m_Filename)
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
    }
    else if (element.tagName() == "gradient")
    {
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
        m_Delay = getFirstText(element).toInt();
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

        m_maskImage = GetMythPainter()->GetFormatImage();
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
        return MythUIType::ParseElement(element);

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
    MythUIImage *im = dynamic_cast<MythUIImage *>(base);
    if (!im)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
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

    //SetImages(im->m_Images);

    MythUIType::CopyFrom(base);

    m_NeedLoad = im->m_NeedLoad;

    if (m_NeedLoad)
        Load();
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIImage::CreateCopy(MythUIType *parent)
{
    MythUIImage *im = new MythUIImage(parent, objectName());
    im->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUIImage::Finalize(void)
{
    if (m_NeedLoad)
        Load();

    MythUIType::Finalize();
}

void MythUIImage::LoadNow(void)
{
    if (m_NeedLoad)
        return;

    m_NeedLoad = true;
    Load();

    MythUIType::LoadNow();
}

