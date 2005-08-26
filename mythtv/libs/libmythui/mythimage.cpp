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

/* note, Qt 4 can draw into an image, so this gets much simpler then */
MythImage *MythImage::Gradient(const QSize & size, const QColor &begin,
                               const QColor &end, uint alpha)
{
    QImage img(size.width(), size.height(), 32);
    img.setAlphaBuffer(true);

    for (int y = 0; y < img.height(); y++)
    {
        for (int x = 0; x < img.width(); x++)
        {
            uint *p = (uint *)img.scanLine(y) + x;
            *p = qRgba(0, 0, 0, alpha);
        }
    }

    // calculate how much to change the colour by at each step
    float rstep = float(end.red() - begin.red()) /
                  float(img.height());
    float gstep = float(end.green() - begin.green()) / 
                  float(img.height());
    float bstep = float(end.blue() - begin.blue()) / 
                  float(img.height());

    qApp->lock();

    /* create the painter */
    QPixmap pix = QPixmap(img);
    QPainter p(&pix);

    float r = begin.red();
    float g = begin.green();
    float b = begin.blue();

    for (int y = 0; y < img.height(); y++) 
    {
        QColor c((int)r, (int)g, (int)b);
        p.setPen(c);
        p.drawLine(0, y, img.width(), y);
        r += rstep;
        g += gstep;
        b += bstep;
    }
    p.setPen(Qt::black);
    p.drawLine(0, 0, 0, img.height() - 1);
    p.drawLine(0, 0, img.width() - 1, 0);
    p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
    p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
    p.end();

    qApp->unlock();

    MythImage *ret = GetMythPainter()->GetFormatImage();
    ret->Assign(pix);
    return ret;
}
