#include <cassert>
#include <stdint.h>

// QT headers
#include <QPainter>
#include <QMatrix>

#include "mythimage.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"

MythImage::MythImage(MythPainter *parent)
{
    assert(parent);

    m_Parent = parent;
    m_Changed = false;

    m_RefCount = 0;

    m_isGradient = false;
    m_gradBegin = QColor("#000000");
    m_gradEnd = QColor("#FFFFFF");
    m_gradAlpha = 255;
    m_gradDirection = FillTopToBottom;

    m_isReflected = false;

    m_imageId = 0;

    m_FileName = "";
}

MythImage::~MythImage()
{
    m_Parent->DeleteFormatImage(this);
}

// these technically should be locked, but all deletion should be happening in the UI thread, and nowhere else.
void MythImage::UpRef(void)
{
    m_RefCount++;
}

bool MythImage::DownRef(void)
{
    m_RefCount--;
    if (m_RefCount < 0)
    {
        delete this;
        return true;
    }
    return false;
}

void MythImage::Assign(const QImage &img)
{
    *(QImage *)this = img;
    SetChanged();
}

void MythImage::Assign(const QPixmap &pix)
{
    Assign(pix.toImage());
}

void MythImage::Resize(const QSize &newSize, bool preserveAspect)
{
    if (size() == newSize)
        return;

    if (m_isGradient)
    {
        *(QImage *)this = QImage(newSize, QImage::Format_ARGB32);
        MakeGradient(*this, m_gradBegin, m_gradEnd, m_gradAlpha, m_gradDirection);
        SetChanged();
    }
    else
    {
        Qt::AspectRatioMode mode = Qt::IgnoreAspectRatio;
        if (preserveAspect)
            mode = Qt::KeepAspectRatio;

        Assign(scaled(newSize, mode));
    }
}

void MythImage::Reflect(ReflectAxis axis, int shear, int scale, int length)
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

    QImage alphaChannel(mirrorImage.size(), QImage::Format_RGB32);
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
        newsize = QSize(mirrorImage.width(), height()+mirrorImage.height());
    else if (axis == ReflectHorizontal)
        newsize = QSize(width()+mirrorImage.width(), mirrorImage.height());

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

        newpainter.drawImage(0, height(), mirrorImage);
    }
    else if (axis == ReflectHorizontal)
    {
        if (shear < 0)
            newpainter.drawImage(0, mirrorImage.height()-height(),
                                 copy(0,0,width(),height()));
        else
            newpainter.drawImage(0, 0, copy(0,0,width(),height()));

        newpainter.drawImage(width(), 0, mirrorImage);
    }

    newpainter.end();

    Assign(temp);

    m_isReflected = true;
}

MythImage *MythImage::FromQImage(QImage **img)
{
    if (!img || !*img)
        return NULL;

    MythImage *ret = GetMythPainter()->GetFormatImage();
    ret->Assign(**img);
    delete *img;
    *img = NULL;
    return ret;
}

// FIXME: Get rid of LoadScaleImage
bool MythImage::Load(const QString &filename)
{
    QImage *im = GetMythUI()->LoadScaleImage(filename);
    SetFileName(filename);
    if (im)
    {
        Assign(*im);
        delete im;
        return true;
    }

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
        // Draw boundry rect
        QColor black(0, 0, 0, alpha);
        painter.setPen(black);
        QPen pen = painter.pen();
        pen.setWidth(1);
        painter.drawRect(image.rect());
    }
    painter.end();
}

MythImage *MythImage::Gradient(const QSize & size, const QColor &begin,
                               const QColor &end, uint alpha,
                               FillDirection direction)
{
    QImage img(size.width(), size.height(), QImage::Format_ARGB32);

    MakeGradient(img, begin, end, alpha, true, direction);

    MythImage *ret = GetMythPainter()->GetFormatImage();
    ret->Assign(img);
    ret->m_isGradient = true;
    ret->m_gradBegin = begin;
    ret->m_gradEnd = end;
    ret->m_gradAlpha = alpha;
    ret->m_gradDirection = direction;
    return ret;
}
