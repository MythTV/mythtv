
// Own header
#include "mythimage.h"

// C++ headers
#include <stdint.h>

// QT headers
#include <QImageReader>
#include <QPainter>
#include <QMatrix>
#include <QNetworkReply>
#include <QRgb>

// Mythdb headers
#include "mythdownloadmanager.h"
#include "mythlogging.h"

// Myth headers
#include "remotefile.h"

// MythUI headers
#include "mythuihelper.h"
#include "mythmainwindow.h"

MythUIHelper *MythImage::s_ui = NULL;

MythImage::MythImage(MythPainter *parent, const char *name) :
    ReferenceCounter(name)
{
    if (!parent)
        LOG(VB_GENERAL, LOG_ERR, "Image created without parent!");

    m_Parent = parent;

    m_Changed = false;

    m_isGradient = false;
    m_gradBegin = QColor("#000000");
    m_gradEnd = QColor("#FFFFFF");
    m_gradAlpha = 255;
    m_gradDirection = FillTopToBottom;

    m_isOriented = false;
    m_isReflected = false;
    m_isYUV = false;

    m_imageId = 0;

    m_FileName = "";

    m_cached = false;
    if (!s_ui)
        s_ui = GetMythUI();
}

MythImage::~MythImage()
{
    if (m_Parent)
        m_Parent->DeleteFormatImage(this);
}

int MythImage::IncrRef(void)
{
    int cnt = ReferenceCounter::IncrRef();
    if ((2 == cnt) && s_ui && m_cached)
        s_ui->ExcludeFromCacheSize(this);
    return cnt;
}

int MythImage::DecrRef(void)
{
    bool cached = m_cached;
    int cnt = ReferenceCounter::DecrRef();
    if (cached)
    {
        if (s_ui && (1 == cnt))
            s_ui->IncludeInCacheSize(this);

        if (0 == cnt)
        {
            LOG(VB_GENERAL, LOG_INFO,
                "Image should be removed from cache prior to deletion.");
        }
    }
    return cnt;
}

void MythImage::SetIsInCache(bool bCached)
{
    IncrRef();
    m_cached = bCached;
    DecrRef();
}

void MythImage::Assign(const QImage &img)
{
    if (m_cached)
    {
        SetIsInCache(false);
        *(static_cast<QImage*>(this)) = img;
        SetIsInCache(m_cached);
    }
    else
    {
        *(static_cast<QImage*>(this)) = img;
    }
    SetChanged();
}

void MythImage::Assign(const QPixmap &pix)
{
    Assign(pix.toImage());
}

/**
 * Changes the orientation angle of the image according to
 * the exif rotation values. The image will be rotated accordingly.
 * @brief MythImage::Orientation
 * @param orientation
 */
void MythImage::Orientation(int orientation)
{
    if (m_isOriented)
        return;

    QMatrix matrix;
    switch (orientation)
    {
    case 1: // If the image is in its original state
        break;

    case 2: // The image is horizontally flipped
        Assign(mirrored(true, false));
        break;

    case 3: // The image is rotated 180°
        matrix.rotate(180);
        Assign(transformed(matrix, Qt::SmoothTransformation));
        break;

    case 4: // The image is vertically flipped
        Assign(mirrored(false, true));
        break;

    case 5: // The image is transposed (rotated 90° CW flipped horizontally)
        matrix.rotate(90);
        Assign(transformed(matrix, Qt::SmoothTransformation));
        Assign(mirrored(true, false));
        break;

    case 6: // The image is rotated 90° CCW
        matrix.rotate(270);
        Assign(transformed(matrix, Qt::SmoothTransformation));
        break;

    case 7: // The image is transversed  (rotated 90° CW and flipped vertically)
        matrix.rotate(90);
        Assign(transformed(matrix, Qt::SmoothTransformation));
        Assign(mirrored(false, true));
        break;

    case 8: // The image is rotated 90° CW
        matrix.rotate(90);
        Assign(transformed(matrix, Qt::SmoothTransformation));
        break;

    default:
        break;
    }

    m_isOriented = true;
}

void MythImage::Resize(const QSize &newSize, bool preserveAspect)
{
    if ((size() == newSize) && !isNull())
        return;

    if (m_isGradient)
    {
        *(static_cast<QImage *> (this)) = QImage(newSize, QImage::Format_ARGB32);
        MakeGradient(*this, m_gradBegin, m_gradEnd, m_gradAlpha, m_gradDirection);
        SetChanged();
    }
    else
    {
        Qt::AspectRatioMode mode = Qt::IgnoreAspectRatio;
        if (preserveAspect)
            mode = Qt::KeepAspectRatio;

        Assign(scaled(newSize, mode, Qt::SmoothTransformation));
    }
}

void MythImage::Reflect(ReflectAxis axis, int shear, int scale, int length,
                        int spacing)
{
    if (m_isReflected)
        return;

    QImage mirrorImage;
    FillDirection fillDirection = FillTopToBottom;
    if (axis == ReflectVertical)
    {
        mirrorImage = mirrored(false,true);
        if (length < 100)
        {
            int height = (int)((float)mirrorImage.height() * (float)length/100);
            mirrorImage = mirrorImage.copy(0,0,mirrorImage.width(),height);
        }
        fillDirection = FillTopToBottom;
    }
    else if (axis == ReflectHorizontal)
    {
        mirrorImage = mirrored(true,false);
        if (length < 100)
        {
            int width = (int)((float)mirrorImage.width() * (float)length/100);
            mirrorImage = mirrorImage.copy(0,0,width,mirrorImage.height());
        }
        fillDirection = FillLeftToRight;
    }

    QImage alphaChannel(mirrorImage.size(), QImage::Format_ARGB32);
    MakeGradient(alphaChannel, QColor("#AAAAAA"), QColor("#000000"), 255,
                 false, fillDirection);
    mirrorImage.setAlphaChannel(alphaChannel);

    QMatrix shearMatrix;
    if (axis == ReflectVertical)
    {
        shearMatrix.scale(1,(float)scale/100);
        shearMatrix.shear((float)shear/100,0);
    }
    else if (axis == ReflectHorizontal)
    {
        shearMatrix.scale((float)scale/100,1);
        shearMatrix.shear(0,(float)shear/100);
    }

    mirrorImage = mirrorImage.transformed(shearMatrix, Qt::SmoothTransformation);

    QSize newsize;
    if (axis == ReflectVertical)
        newsize = QSize(mirrorImage.width(), height()+spacing+mirrorImage.height());
    else if (axis == ReflectHorizontal)
        newsize = QSize(width()+spacing+mirrorImage.width(), mirrorImage.height());

    QImage temp(newsize, QImage::Format_ARGB32);
    temp.fill(Qt::transparent);

    QPainter newpainter(&temp);
    newpainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (axis == ReflectVertical)
    {
        if (shear < 0)
            newpainter.drawImage(mirrorImage.width()-width(), 0,
                                 copy(0,0,width(),height()));
        else
            newpainter.drawImage(0, 0, copy(0,0,width(),height()));

        newpainter.drawImage(0, height()+spacing, mirrorImage);
    }
    else if (axis == ReflectHorizontal)
    {
        if (shear < 0)
            newpainter.drawImage(0, mirrorImage.height()-height(),
                                 copy(0,0,width(),height()));
        else
            newpainter.drawImage(0, 0, copy(0,0,width(),height()));

        newpainter.drawImage(width()+spacing, 0, mirrorImage);
    }

    newpainter.end();

    Assign(temp);

    m_isReflected = true;
}

void MythImage::ToGreyscale()
{
    if (isGrayscale())
        return;

    for (int y = 0; y < height(); ++y)
    {
        for (int x = 0; x < width(); ++x)
        {
            QRgb oldPixel = pixel(x, y);
            int greyVal = qGray(oldPixel);
            setPixel(x, y, qRgba(greyVal, greyVal, greyVal, qAlpha(oldPixel)));
        }
    }
}

bool MythImage::Load(MythImageReader *reader)
{
    if (!reader || !reader->canRead())
        return false;

    QImage *im = new QImage;

    if (im && reader->read(im))
    {
        Assign(*im);
        delete im;
        return true;
    }

    if (im)
        delete im;

    return false;
}

bool MythImage::Load(const QString &filename)
{
    if (filename.isEmpty())
        return false;

    QImage *im = NULL;
    if (filename.startsWith("myth://"))
    {
        // Attempting a file transfer on a file that doesn't exist throws
        // a lot of noisy warnings on the backend and frontend, so to avoid
        // that first check it's there
        if (RemoteFile::Exists(filename))
        {
            RemoteFile *rf = new RemoteFile(filename, false, false, 0);

            QByteArray data;
            bool ret = rf->SaveAs(data);

            delete rf;

            if (ret)
            {
                im = new QImage();
                im->loadFromData(data);
            }
        }
#if 0
        else
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythImage::Load failed to load remote image %1")
                    .arg(filename));
#endif

    }
    else if ((filename.startsWith("http://")) ||
                (filename.startsWith("https://")) ||
                (filename.startsWith("ftp://")))
    {
        QByteArray data;
        if (GetMythDownloadManager()->download(filename, &data))
        {
            im = new QImage();
            im->loadFromData(data);
        }
    }
    else
    {
        QString path = filename;
        if (path.startsWith('/') ||
            GetMythUI()->FindThemeFile(path))
            im = new QImage(path);
    }

    if (im && im->isNull())
    {
        delete im;
        im = NULL;
    }

    SetFileName(filename);
    if (im)
    {
        Assign(*im);
        delete im;
        return true;
    }
    else
        LOG(VB_GUI, LOG_WARNING, QString("MythImage::Load(%1) failed").arg(filename));

    return false;
}

void MythImage::MakeGradient(QImage &image, const QColor &begin,
                             const QColor &end, int alpha, bool drawBoundary,
                             FillDirection direction)
{
    // Gradient fill colours
    QColor startColor = begin;
    QColor endColor = end;
    startColor.setAlpha(alpha);
    endColor.setAlpha(alpha);

    // Define Gradient
    QPoint pointA(0,0);
    QPoint pointB;
    if (direction == FillTopToBottom)
    {
        pointB = QPoint(0,image.height());
    }
    else if (direction == FillLeftToRight)
    {
        pointB = QPoint(image.width(),0);
    }

    QLinearGradient gradient(pointA, pointB);
    gradient.setColorAt(0, startColor);
    gradient.setColorAt(1, endColor);

    // Draw Gradient
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(0, 0, image.width(), image.height(), gradient);

    if (drawBoundary)
    {
        // Draw boundary rect
        QColor black(0, 0, 0, alpha);
        painter.setPen(black);
        QPen pen = painter.pen();
        pen.setWidth(1);
        painter.drawRect(image.rect());
    }
    painter.end();
}

MythImage *MythImage::Gradient(MythPainter *painter,
                               const QSize & size, const QColor &begin,
                               const QColor &end, uint alpha,
                               FillDirection direction)
{
    QImage img(size.width(), size.height(), QImage::Format_ARGB32);

    MakeGradient(img, begin, end, alpha, true, direction);

    MythImage *ret = painter->GetFormatImage();
    ret->Assign(img);
    ret->m_isGradient = true;
    ret->m_gradBegin = begin;
    ret->m_gradEnd = end;
    ret->m_gradAlpha = alpha;
    ret->m_gradDirection = direction;
    return ret;
}

#define SCALEBITS 8
#define ONE_HALF (1 << (SCALEBITS - 1))
#define FIX(x)   ((int) ((x) * (1L<<SCALEBITS) /*+ 0.5*/))

void MythImage::ConvertToYUV(void)
{
    if (m_isYUV)
        return;

    m_isYUV = true;

    int r, r1, g, g1, b, b1, a;
    for (int i = 0; i < height(); i ++)
    {
        QRgb *data = (QRgb*)scanLine(i);
        for (int j = 0; j < width(); j++)
        {
            r = qRed(data[j]);
            g = qGreen(data[j]);
            b = qBlue(data[j]);
            a = qAlpha(data[j]);

            r1 = (FIX(0.299) * r + FIX(0.587) * g +
                  FIX(0.114) * b + ONE_HALF) >> SCALEBITS;
            g1 = ((- FIX(0.169) * r - FIX(0.331) * g +
                  FIX(0.499) * b + ONE_HALF) >> SCALEBITS) + 128;
            b1 = ((FIX(0.499) * r - FIX(0.418) * g -
                  FIX(0.0813) * b + ONE_HALF) >> SCALEBITS) + 128;

            data[j] = qRgba(r1, g1, b1, a);
        }
    }
}

MythImageReader::MythImageReader(const QString &fileName)
  : QImageReader(),
    m_fileName(fileName), m_networkReply(NULL)
{
    if ((m_fileName.startsWith("http://")) ||
        (m_fileName.startsWith("https://")) ||
        (m_fileName.startsWith("ftp://")))
    {
        m_networkReply = GetMythDownloadManager()->download(m_fileName);
        if (m_networkReply)
            setDevice(m_networkReply);
    }
    else if (!m_fileName.isEmpty())
    {
        if (!m_fileName.startsWith("/") && !QFile::exists(m_fileName))
        {
            QString tmpFile = GetMythUI()->GetThemeDir() + '/' + m_fileName;
            if (QFile::exists(tmpFile))
                m_fileName = tmpFile;
        }
        setFileName(m_fileName);
    }
}

MythImageReader::~MythImageReader()
{
    if (m_networkReply)
    {
        setDevice(NULL);
        m_networkReply->deleteLater();
        m_networkReply = NULL;
    }
}

