// QT headers
#include <QPainter>
#include <QImage>

// MythUI headers
#include "mythpainter_qimage.h"
#include "mythfontproperties.h"
#include "mythmainwindow.h"

// MythDB headers
#include "compat.h"
#include "mythverbose.h"

#define MAX_CACHE_ITEMS 256

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
    MythImage *im = GetImageFromString(msg, flags, r, font);
    if (!im)
        return;

    QRect destRect(boundRect);
    QRect srcRect(0,0,r.width(),r.height());
    if (!boundRect.isEmpty() && boundRect != r)
    {
        int x = 0;
        int y = 0;
        int width = boundRect.width();
        int height = boundRect.height();

        if (boundRect.x() > r.x())
        {
            x = boundRect.x()-r.x();
        }
        else if (r.x() > boundRect.x())
        {
            destRect.setX(r.x());
            width = (boundRect.x() + boundRect.width()) - r.x();
        }

        if (boundRect.y() > r.y())
        {
            y = boundRect.y()-r.y();
        }
        else if (r.y() > boundRect.y())
        {
            destRect.setY(r.y());
            height = (boundRect.y() + boundRect.height()) - r.y();
        }

        if (width <= 0 || height <= 0)
            return;

        srcRect.setRect(x,y,width,height);
    }

    DrawImage(destRect, im, srcRect, alpha);
}

void MythQImagePainter::DrawRect(const QRect &area, bool drawFill,
                                 const QColor &fillColor, bool drawLine,
                                 int lineWidth, const QColor &lineColor)
{
    if (!painter)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: DrawRect called with no painter");
        return;
    }

    if (drawLine)
        painter->setPen(QPen(lineColor, lineWidth));
    else
        painter->setPen(QPen(Qt::NoPen));

    if (drawFill)
        painter->setBrush(QBrush(fillColor));
    else
        painter->setBrush(QBrush(Qt::NoBrush));

    CheckPaintMode(area);
    painter->drawRect(area);
    painter->setBrush(QBrush(Qt::NoBrush));
}

void MythQImagePainter::DrawRoundRect(const QRect &area, int radius,
                                      bool drawFill, const QColor &fillColor,
                                      bool drawLine, int lineWidth,
                                      const QColor &lineColor)
{
    MythImage *im = GetImageFromRect(area.size(), radius, drawFill, fillColor,
                                     drawLine, lineWidth, lineColor);
    if (im)
        DrawImage(area, im, QRect(0, 0, area.width(), area.height()), 255);
}

MythImage* MythQImagePainter::GetImageFromRect(const QSize &size, int radius,
                                               bool drawFill,
                                               const QColor &fillColor,
                                               bool drawLine,
                                               int lineWidth,
                                               const QColor &lineColor)
{
    if (size.width() <= 0 || size.height() <= 0)
        return NULL;

    QString incoming = QString("RECT") + QString::number(size.width()) +
                       QString::number(size.height()) +
                       QString::number(radius) + QString::number(drawFill) +
                       QString::number(fillColor.rgba()) +
                       QString::number(drawLine) + QString::number(lineWidth) +
                       QString::number(lineColor.rgba());

    if (m_StringToImageMap.contains(incoming))
    {
        m_StringExpireList.remove(incoming);
        m_StringExpireList.push_back(incoming);
        return m_StringToImageMap[incoming];
    }

    QImage image(QSize(size.width(), size.height()), QImage::Format_ARGB32);
    image.fill(0x00000000);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    if (drawLine)
        painter.setPen(QPen(lineColor, lineWidth));
    else
        painter.setPen(QPen(Qt::NoPen));

    if (drawFill)
        painter.setBrush(QBrush(fillColor));
    else
        painter.setBrush(QBrush(Qt::NoBrush));

    if ((size.width() / 2) < radius)
        radius = size.width() / 2;

    if ((size.height() / 2) < radius)
        radius = size.height() / 2;

    QRect r(lineWidth / 2, lineWidth / 2, size.width() - lineWidth,
            size.height() - lineWidth);
    painter.drawRoundedRect(r, (qreal)radius, qreal(radius));
    painter.end();

    MythImage *im = GetFormatImage();
    im->Assign(image);
    m_StringToImageMap[incoming] = im;
    m_StringExpireList.push_back(incoming);
    ExpireImages(MAX_CACHE_ITEMS);
    return im;
}

MythImage *MythQImagePainter::GetImageFromString(const QString &msg,
                                                 int flags, const QRect &r,
                                                 const MythFontProperties &font)
{
    QString incoming = font.GetHash() + QString::number(r.width()) +
                       QString::number(r.height()) +
                       QString::number(flags) +
                       QString::number(font.color().rgba()) + msg;

    if (m_StringToImageMap.contains(incoming))
    {
        m_StringExpireList.remove(incoming);
        m_StringExpireList.push_back(incoming);
        return m_StringToImageMap[incoming];
    }

    MythImage *im = GetFormatImage();

    QPoint drawOffset;
    font.GetOffset(drawOffset);

    QImage pm(r.size(), QImage::Format_ARGB32);
    QColor fillcolor = font.color();
    if (font.hasOutline())
    {
        QColor outlineColor;
        int outlineSize, outlineAlpha;

        font.GetOutline(outlineColor, outlineSize, outlineAlpha);

        fillcolor = outlineColor;
    }
    fillcolor.setAlpha(0);
    pm.fill(fillcolor.rgba());

    QPainter tmp(&pm);
    tmp.setFont(font.face());

    if (font.hasShadow())
    {
        QPoint shadowOffset;
        QColor shadowColor;
        int shadowAlpha;

        font.GetShadow(shadowOffset, shadowColor, shadowAlpha);

        QRect a = QRect(0, 0, r.width(), r.height());
        a.translate(shadowOffset.x() + drawOffset.x(),
                    shadowOffset.y() + drawOffset.y());

        shadowColor.setAlpha(shadowAlpha);
        tmp.setPen(shadowColor);
        tmp.drawText(a, flags, msg);
    }

    if (font.hasOutline())
    {
        QColor outlineColor;
        int outlineSize, outlineAlpha;

        font.GetOutline(outlineColor, outlineSize, outlineAlpha);

        /* FIXME: use outlineAlpha */
        int outalpha = 16;

        QRect a = QRect(0, 0, r.width(), r.height());
        a.translate(-outlineSize + drawOffset.x(),
                    -outlineSize + drawOffset.y());

        outlineColor.setAlpha(outalpha);
        tmp.setPen(outlineColor);
        tmp.drawText(a, flags, msg);

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(1, 0);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(0, 1);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(-1, 0);
            tmp.drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(0, -1);
            tmp.drawText(a, flags, msg);
        }
    }

    tmp.setPen(font.color());
    tmp.drawText(drawOffset.x(), drawOffset.y(), r.width(), r.height(),
                 flags, msg);

    tmp.end();

    im->Assign(pm);

    m_StringToImageMap[incoming] = im;
    m_StringExpireList.push_back(incoming);
    ExpireImages(MAX_CACHE_ITEMS);
    return im;
}

void MythQImagePainter::ExpireImages(uint max)
{
    while (m_StringExpireList.size() > max)
    {
        QString oldmsg = m_StringExpireList.front();
        m_StringExpireList.pop_front();
        if (m_StringToImageMap.contains(oldmsg))
        {
            m_StringToImageMap[oldmsg]->DownRef();
            m_StringToImageMap.remove(oldmsg);
        }
    }
}
