
// Own header
#include "mythpainter.h"

// QT headers
#include <QRect>

// libmythdb headers
#include "mythverbose.h"

// libmythui headers
#include "mythfontproperties.h"
#include "mythimage.h"

int MythPainter::m_MaxCacheSize = 1024 * 1024 * 64;

MythPainter::~MythPainter(void)
{
    if (m_allocatedImages.isEmpty())
        return;

    VERBOSE(VB_GENERAL, QString("MythPainter: %1 images not yet de-allocated.")
        .arg(m_allocatedImages.size()));
    while (!m_allocatedImages.isEmpty())
        m_allocatedImages.takeLast()->SetParent(NULL);
}

void MythPainter::SetClipRect(const QRect &)
{
}

void MythPainter::SetClipRegion(const QRegion &)
{
}

void MythPainter::Clear(QPaintDevice *device, const QRegion &region)
{
}

void MythPainter::DrawImage(int x, int y, MythImage *im, int alpha)
{
    if (!im)
    {
        VERBOSE(VB_IMPORTANT,
                    "Null image pointer passed to MythPainter::DrawImage()");
        return;
    }
    QRect dest = QRect(x, y, im->width(), im->height());
    QRect src = im->rect();
    DrawImage(dest, im, src, alpha);
}

void MythPainter::DrawImage(const QPoint &topLeft, MythImage *im, int alpha)
{
    DrawImage(topLeft.x(), topLeft.y(), im, alpha);
}

MythImage *MythPainter::GetFormatImage()
{
    MythImage *result = new MythImage(this);
    m_allocatedImages.append(result);
    return result;
}

void MythPainter::DeleteFormatImage(MythImage *im)
{
    DeleteFormatImagePriv(im);

    while (m_allocatedImages.contains(im))
        m_allocatedImages.removeOne(im);
}

// the following assume graphics hardware operates natively at 32bpp
void MythPainter::IncreaseCacheSize(QSize size)
{
    m_CacheSize += size.width() * size.height() * 4;
}

void MythPainter::DecreaseCacheSize(QSize size)
{
    m_CacheSize -= size.width() * size.height() * 4;
}
