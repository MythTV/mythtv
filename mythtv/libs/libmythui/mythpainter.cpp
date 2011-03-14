#include <stdint.h>

// QT headers
#include <QRect>
#include <QPainter>

// libmythbase headers
#include "mythverbose.h"

// libmythui headers
#include "mythfontproperties.h"
#include "mythimage.h"

// Own header
#include "mythpainter.h"

MythPainter::MythPainter()
  : m_Parent(0), m_HardwareCacheSize(0), m_SoftwareCacheSize(0),
    m_showBorders(false), m_showNames(false)
{
    SetMaximumCacheSizes(96, 96);
}

MythPainter::~MythPainter(void)
{
    QMutexLocker locker(&m_allocationLock);
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

void MythPainter::DrawText(const QRect &r, const QString &msg,
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

void MythPainter::DrawRect(const QRect &area, const QBrush &fillBrush,
                           const QPen &linePen, int alpha)
{
    MythImage *im = GetImageFromRect(area, 0, 0, fillBrush, linePen);
    if (im)
        DrawImage(area.x(), area.y(), im, alpha);
}

void MythPainter::DrawRoundRect(const QRect &area, int cornerRadius,
                                const QBrush &fillBrush, const QPen &linePen,
                                int alpha)
{
    MythImage *im = GetImageFromRect(area, cornerRadius, 0, fillBrush, linePen);
    if (im)
        DrawImage(area.x(), area.y(), im, alpha);
}

void MythPainter::DrawEllipse(const QRect &area, const QBrush &fillBrush,
                              const QPen &linePen, int alpha)
{
    MythImage *im = GetImageFromRect(area, 0, 1, fillBrush, linePen);
    if (im)
        DrawImage(area.x(), area.y(), im, alpha);
}

void MythPainter::DrawTextPriv(MythImage *im, const QString &msg, int flags,
                               const QRect &r, const MythFontProperties &font)
{
    if (!im)
        return;

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
    QFont tmpfont = font.face();
    tmpfont.setStyleStrategy(QFont::OpenGLCompatible);
    tmp.setFont(tmpfont);

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

    tmp.setPen(QPen(font.GetBrush(), 0));
    tmp.drawText(drawOffset.x(), drawOffset.y(), r.width(), r.height(),
                 flags, msg);
    tmp.end();
    im->Assign(pm);
}

void MythPainter::DrawRectPriv(MythImage *im, const QRect &area, int radius,
                               int ellipse,
                               const QBrush &fillBrush, const QPen &linePen)
{
    if (!im)
        return;

    QImage image(QSize(area.width(), area.height()), QImage::Format_ARGB32);
    image.fill(0x00000000);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(linePen);
    painter.setBrush(fillBrush);

    if ((area.width() / 2) < radius)
        radius = area.width() / 2;

    if ((area.height() / 2) < radius)
        radius = area.height() / 2;

    int lineWidth = linePen.width();
    QRect r(lineWidth, lineWidth,
            area.width() - (lineWidth * 2), area.height() - (lineWidth * 2));

    if (ellipse)
        painter.drawEllipse(r);
    else if (radius == 0)
        painter.drawRect(r);
    else
        painter.drawRoundedRect(r, (qreal)radius, qreal(radius));

    painter.end();
    im->Assign(image);
}

MythImage *MythPainter::GetImageFromString(const QString &msg,
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
    if (im)
    {
        m_SoftwareCacheSize += im->byteCount();
        DrawTextPriv(im, msg, flags, r, font);
        m_StringToImageMap[incoming] = im;
        m_StringExpireList.push_back(incoming);
        ExpireImages(m_MaxSoftwareCacheSize);
    }
    return im;
}

MythImage* MythPainter::GetImageFromRect(const QRect &area, int radius,
                                         int ellipse,
                                         const QBrush &fillBrush,
                                         const QPen &linePen)
{
    if (area.width() <= 0 || area.height() <= 0)
        return NULL;

    uint64_t hash1 = ((0xfff & (uint64_t)area.left())) +
                     ((0xfff & (uint64_t)area.top())        << 12) +
                     ((0xfff & (uint64_t)area.width())      << 24) +
                     ((0xfff & (uint64_t)area.height())     << 36) +
                     ((0xfff & (uint64_t)fillBrush.style()) << 48) +
                     ((0xf   & (uint64_t)linePen.width())   << 60);
    uint64_t hash2 = ((0xfff & (uint64_t)radius)) +
                     ((0xfff & (uint64_t)linePen.style()) << 12) +
                     ((0xfff & (uint64_t)ellipse)         << 24);
    uint64_t hash3 = ((0xffffffff & (uint64_t)linePen.color().rgba())) +
                     ((0xffffffff & (uint64_t)fillBrush.color().rgba()) << 32);

    QString incoming("R");
    if (fillBrush.style() == Qt::LinearGradientPattern && fillBrush.gradient())
    {
        const QLinearGradient *gradient = static_cast<const QLinearGradient*>(fillBrush.gradient());
        if (gradient)
        {
            incoming = QString::number(
                             ((0xfff & (uint64_t)gradient->start().x())) +
                             ((0xfff & (uint64_t)gradient->start().y()) << 12) +
                             ((0xfff & (uint64_t)gradient->finalStop().x()) << 24) +
                             ((0xfff & (uint64_t)gradient->finalStop().y()) << 36));
            QGradientStops stops = gradient->stops();
            for (int i = 0; i < stops.size(); i++)
            {
                incoming += QString::number(
                             ((0xfff * (uint64_t)(stops[i].first * 100))) +
                             ((uint64_t)stops[i].second.rgba() << 12));
            }
        }
    }

    incoming += QString::number(hash1) + QString::number(hash2) +
                QString::number(hash3);

    if (m_StringToImageMap.contains(incoming))
    {
        m_StringExpireList.remove(incoming);
        m_StringExpireList.push_back(incoming);
        return m_StringToImageMap[incoming];
    }

    MythImage *im = GetFormatImage();
    if (im)
    {
        m_SoftwareCacheSize += im->byteCount();
        DrawRectPriv(im, area, radius, ellipse, fillBrush, linePen);
        m_StringToImageMap[incoming] = im;
        m_StringExpireList.push_back(incoming);
        ExpireImages(m_MaxSoftwareCacheSize);
    }
    return im;
}

MythImage *MythPainter::GetFormatImage()
{
    m_allocationLock.lock();
    MythImage *result = GetFormatImagePriv();
    m_allocatedImages.append(result);
    m_allocationLock.unlock();
    return result;
}

void MythPainter::DeleteFormatImage(MythImage *im)
{
    m_allocationLock.lock();
    DeleteFormatImagePriv(im);

    while (m_allocatedImages.contains(im))
        m_allocatedImages.removeOne(im);
    m_allocationLock.unlock();
}

void MythPainter::CheckFormatImage(MythImage *im)
{
    if (im && !im->GetParent())
    {
        m_allocationLock.lock();
        m_allocatedImages.append(im);
        im->SetParent(this);
        m_allocationLock.unlock();
    }
}

void MythPainter::ExpireImages(int max)
{
    if (m_StringExpireList.size() < 1)
        return;

    while (m_SoftwareCacheSize > max)
    {
        QString oldmsg = m_StringExpireList.front();
        m_StringExpireList.pop_front();

        MythImage *oldim = NULL;
        if (m_StringToImageMap.contains(oldmsg))
            oldim = m_StringToImageMap[oldmsg];

        m_StringToImageMap.remove(oldmsg);

        if (oldim)
        {
            m_SoftwareCacheSize -= oldim->byteCount();
            oldim->DownRef();
        }
    }
}

// the following assume graphics hardware operates natively at 32bpp
void MythPainter::SetMaximumCacheSizes(int hardware, int software)
{
    m_MaxHardwareCacheSize = 1024 * 1024 * hardware;
    m_MaxSoftwareCacheSize = 1024 * 1024 * software;
    VERBOSE(VB_GUI, QString("MythPainter cache sizes: Hardware %1Mb, Software %2Mb")
        .arg(hardware).arg(software));
}
