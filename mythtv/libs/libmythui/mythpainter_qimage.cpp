// QT headers
#include <QPainter>
#include <QImage>

// MythUI headers
#include "mythpainter_qimage.h"
#include "mythmainwindow.h"

// MythDB headers
#include "compat.h"
#include "mythverbose.h"

MythQImagePainter::MythQImagePainter() :
    MythPainter(), painter(NULL), copy(false)
{
}

MythQImagePainter::~MythQImagePainter()
{
    ExpireImages();
}

void MythQImagePainter::Begin(QPaintDevice *parent)
{
    if (!parent)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: No parent widget defined for "
                              "QT Painter, bailing");
        return;
    }

    MythPainter::Begin(parent);
    painter       = new QPainter(parent);
    copy          = true;
    paintedRegion = QRegion();
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    clipRegion = QRegion();
    SetClipRect(QRect());
}

void MythQImagePainter::CheckPaintMode(const QRect &area)
{
    if (!painter)
        return;

    bool intersects;

    if (paintedRegion.isEmpty())
    {
        intersects = false;
        paintedRegion = QRegion(area);
    }
    else
    {
        intersects = paintedRegion.intersects(area);
        paintedRegion = paintedRegion.united(area);
    }

    if (intersects && copy)
    {
        copy = false;
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    else if (!intersects && !copy)
    {
        copy = true;
        painter->setCompositionMode(QPainter::CompositionMode_Source);
    }
}

void MythQImagePainter::End(void)
{
    if (!painter)
        return;

    painter->end();
    delete painter;

    MythPainter::End();
}

void MythQImagePainter::SetClipRect(const QRect &clipRect)
{
    if (!painter)
        return;

    if (!clipRect.isEmpty())
    {
        painter->setClipping(true);
        if (clipRegion.isEmpty())
            clipRegion = QRegion(clipRect);
        else
            clipRegion = clipRegion.united(clipRect);
        painter->setClipRegion(clipRegion);
    }
    else
        painter->setClipping(false);
}

void MythQImagePainter::SetClipRegion(const QRegion &region)
{
    if (!painter)
        return;

    if (!region.isEmpty())
    {
        painter->setClipping(true);
        clipRegion = region;
        painter->setClipRegion(clipRegion);
    }
    else
        painter->setClipping(false);
}

void MythQImagePainter::Clear(QPaintDevice *device, const QRegion &region)
{
    if (!device || region.isEmpty())
        return;

    QImage *dev = dynamic_cast<QImage*>(device);
    if (!dev)
        return;

    int img_width  = dev->size().width();
    int img_height = dev->size().height();

    QVector<QRect> rects = region.rects();
    for (int i = 0; i < rects.size(); i++)
    {
        if (rects[i].top() > img_height || rects[i].left() > img_width)
            continue;

        int bottom = std::min(rects[i].top() + rects[i].height(), img_height);
        int bwidth = std::min(rects[i].left() + rects[i].width(), img_width);
        bwidth = (bwidth - rects[i].left()) << 2;

        for (int row = rects[i].top(); row < bottom; row++)
            memset(dev->scanLine(row) + (rects[i].left() << 2), 0, bwidth);
    }
}

void MythQImagePainter::DrawImage(const QRect &r, MythImage *im,
                                  const QRect &src, int alpha)
{
    if (!painter)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: DrawImage called with no painter");
        return;
    }

    (void)alpha;

    CheckPaintMode(QRect(r.topLeft(), src.size()));
    painter->setOpacity(static_cast<float>(alpha) / 255.0);
    painter->drawImage(r.topLeft(), (QImage)(*im), src);
    painter->setOpacity(1.0);
}

void MythQImagePainter::DrawText(const QRect &r, const QString &msg,
                                 int flags, const MythFontProperties &font,
                                 int alpha, const QRect &boundRect)
{
    MythPainter::DrawText(r, msg, flags, font, alpha, boundRect);
}

void MythQImagePainter::DrawRect(const QRect &area, const QBrush &fillBrush,
                                 const QPen &linePen, int alpha)
{
    MythPainter::DrawRect(area, fillBrush, linePen, alpha);
}

void MythQImagePainter::DrawRoundRect(const QRect &area, int cornerRadius,
                                      const QBrush &fillBrush,
                                      const QPen &linePen, int alpha)
{
    MythPainter::DrawRoundRect(area, cornerRadius, fillBrush, linePen, alpha);
}

void MythQImagePainter::DrawEllipse(const QRect &area, const QBrush &fillBrush,
                                    const QPen &linePen, int alpha)
{
    MythPainter::DrawEllipse(area, fillBrush, linePen, alpha);
}
