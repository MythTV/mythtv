#include <cassert>

#include <qpainter.h>
#include <qapplication.h>

#include "mythimage.h"
#include "mythmainwindow.h"
#include "mythcontext.h"

MythImage::MythImage(MythPainter *parent)
{
    assert(parent);

    m_Parent = parent;
    m_Changed = false;

    m_RefCount = 0;

    m_isGradient = false;
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
    Assign(pix.convertToImage());
}

void MythImage::Resize(const QSize &newSize)
{
    if (m_isGradient)
    {
        *(QImage *)this = QImage(newSize, 32);
        MakeGradient(*this, m_gradBegin, m_gradEnd, m_gradAlpha);
        SetChanged();
    }
    else
    {
        Assign(smoothScale(newSize));
    }
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
    QImage *im = gContext->LoadScaleImage(filename);
    if (im)
    {
        Assign(*im);
        delete im;
        return true;
    }

    return false;
}

void MythImage::MakeGradient(QImage &image, const QColor &begin, const QColor &end, int alpha)
{
    image.setAlphaBuffer(alpha < 255);

    // calculate how much to change the colour by at each step
    float rstep = float(end.red() - begin.red()) /
                  float(image.height());
    float gstep = float(end.green() - begin.green()) / 
                  float(image.height());
    float bstep = float(end.blue() - begin.blue()) / 
                  float(image.height());

    uint32_t black = qRgba(0, 0, 0, alpha);

    uint32_t *ptr = (uint32_t *)image.scanLine(0);
    for (int x = 0; x < image.width(); x++, ptr++)
         *ptr = black;

    for (int y = 1; y < image.height() - 1; y++)
    {
        int r = (int)(begin.red()   + (y * rstep));
        int g = (int)(begin.green() + (y * gstep));
        int b = (int)(begin.blue()  + (y * bstep));
        uint32_t color = qRgba(r, g, b, alpha);
        ptr = (uint32_t *)image.scanLine(y);

        *ptr = black; ptr++;
        for (int x = 0; x < image.width() - 2; x++, ptr++)
            *ptr = color;
        *ptr = black;
    }

    ptr = (uint32_t *)image.scanLine(image.height() - 1);
    for (int x = 0; x < image.width(); x++, ptr++)
         *ptr = black;
}

MythImage *MythImage::Gradient(const QSize & size, const QColor &begin,
                               const QColor &end, uint alpha)
{
    QImage img(size.width(), size.height(), 32);

    MakeGradient(img, begin, end, alpha);

    MythImage *ret = GetMythPainter()->GetFormatImage();
    ret->Assign(img);
    ret->m_isGradient = true;
    ret->m_gradBegin = begin;
    ret->m_gradEnd = end;
    ret->m_gradAlpha = alpha;
    return ret;
}

