#include <algorithm>
#include <complex>
#include <cstdint>

// QT headers
#include <QRect>
#include <QPainter>
#include <QPainterPath>

// libmythbase headers
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

// libmythui headers
#include "mythfontproperties.h"
#include "mythimage.h"
#include "mythuianimation.h"    // UIEffects

// Own header
#include "mythpainter.h"

MythPainter::MythPainter()
{
    SetMaximumCacheSizes(64, 48);
}

void MythPainter::Teardown(void)
{
    ExpireImages(0);

    QMutexLocker locker(&m_allocationLock);

    if (!m_allocatedImages.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("MythPainter: %1 images not yet de-allocated.")
            .arg(m_allocatedImages.size()));
    }

    for (auto *image : qAsConst(m_allocatedImages))
        image->SetParent(nullptr);
    m_allocatedImages.clear();
}

void MythPainter::SetClipRect(const QRect /*clipRect*/)
{
}

void MythPainter::SetClipRegion(const QRegion & /*clipRegion*/)
{
}

void MythPainter::Clear(QPaintDevice */*device*/, const QRegion &/*region*/)
{
}

void MythPainter::DrawImage(int x, int y, MythImage *im, int alpha)
{
    if (!im)
    {
        LOG(VB_GENERAL, LOG_ERR,
                    "Null image pointer passed to MythPainter::DrawImage()");
        return;
    }
    QRect dest = QRect(x, y, im->width(), im->height());
    QRect src = im->rect();
    DrawImage(dest, im, src, alpha);
}

void MythPainter::DrawImage(const QPoint topLeft, MythImage *im, int alpha)
{
    DrawImage(topLeft.x(), topLeft.y(), im, alpha);
}

void MythPainter::DrawText(const QRect r, const QString &msg,
                           int flags, const MythFontProperties &font,
                           int alpha, const QRect boundRect)
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
    im->DecrRef();
}

void MythPainter::DrawTextLayout(const QRect canvasRect,
                                 const LayoutVector & layouts,
                                 const FormatVector & formats,
                                 const MythFontProperties & font, int alpha,
                                 const QRect destRect)
{
    if (canvasRect.isNull())
        return;

    QRect      canvas(canvasRect);
    QRect      dest(destRect);

    MythImage *im = GetImageFromTextLayout(layouts, formats, font,
                                           canvas, dest);
    if (!im)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythPainter::DrawTextLayout: "
                                             "Unable to create image."));
        return;
    }
    if (im->isNull())
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("MythPainter::DrawTextLayout: "
                                           "Rendered image is null."));
        im->DecrRef();
        return;
    }

    QRect srcRect(0, 0, dest.width(), dest.height());
    DrawImage(dest, im, srcRect, alpha);

    im->DecrRef();
}

void MythPainter::DrawRect(const QRect area, const QBrush &fillBrush,
                           const QPen &linePen, int alpha)
{
    MythImage *im = GetImageFromRect(area, 0, 0, fillBrush, linePen);
    if (im)
    {
        DrawImage(area.x(), area.y(), im, alpha);
        im->DecrRef();
    }
}

void MythPainter::DrawRoundRect(const QRect area, int cornerRadius,
                                const QBrush &fillBrush, const QPen &linePen,
                                int alpha)
{
    MythImage *im = GetImageFromRect(area, cornerRadius, 0, fillBrush, linePen);
    if (im)
    {
        DrawImage(area.x(), area.y(), im, alpha);
        im->DecrRef();
    }
}

void MythPainter::DrawEllipse(const QRect area, const QBrush &fillBrush,
                              const QPen &linePen, int alpha)
{
    MythImage *im = GetImageFromRect(area, 0, 1, fillBrush, linePen);
    if (im)
    {
        DrawImage(area.x(), area.y(), im, alpha);
        im->DecrRef();
    }
}

void MythPainter::DrawTextPriv(MythImage *im, const QString &msg, int flags,
                               const QRect r, const MythFontProperties &font)
{
    if (!im)
        return;

    QColor outlineColor;
    int outlineSize = 0;
    int outlineAlpha = 255;
    if (font.hasOutline())
        font.GetOutline(outlineColor, outlineSize, outlineAlpha);

    QPoint shadowOffset(0, 0);
    QColor shadowColor;
    int shadowAlpha = 255;
    if (font.hasShadow())
        font.GetShadow(shadowOffset, shadowColor, shadowAlpha);

    QFontMetrics fm(font.face());
    int totalHeight = fm.height() + outlineSize +
        std::max(outlineSize, std::abs(shadowOffset.y()));

    // initialPaddingX is the number of pixels from the left of the
    // input QRect to the left of the actual text.  It is always 0
    // because we don't add padding to the text rectangle.
    int initialPaddingX = 0;

    // initialPaddingY is the number of pixels from the top of the
    // input QRect to the top of the actual text.  It may be nonzero
    // because of extra vertical padding.
    int initialPaddingY = (r.height() - totalHeight) / 2;
    // Hack.  Normally we vertically center the text due to some
    // (solvable) issues in the SubtitleScreen code - the text rect
    // and the background rect are both created with PAD_WIDTH extra
    // padding, and to honor Qt::AlignTop, the text rect needs to be
    // without padding.  This doesn't work for Qt::TextWordWrap, since
    // the first line will be vertically centered with subsequence
    // lines below.  So if Qt::TextWordWrap is set, we do top
    // alignment.
    if (flags & Qt::TextWordWrap)
        initialPaddingY = 0;

    // textOffsetX is the number of pixels from r.left() to the left
    // edge of the core text.  This assumes that flags contains
    // Qt::AlignLeft.
    int textOffsetX =
        initialPaddingX + std::max(outlineSize, -shadowOffset.x());

    // textOffsetY is the number of pixels from r.top() to the top
    // edge of the core text.  This assumes that flags contains
    // Qt::AlignTop.
    int textOffsetY =
        initialPaddingY + std::max(outlineSize, -shadowOffset.y());

    QImage pm(r.size(), QImage::Format_ARGB32);
    QColor fillcolor = font.color();
    if (font.hasOutline())
        fillcolor = outlineColor;
    fillcolor.setAlpha(0);
    pm.fill(fillcolor.rgba());

    QPainter tmp(&pm);
    QFont tmpfont = font.face();
#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
    tmpfont.setStyleStrategy(QFont::OpenGLCompatible);
#endif
    tmp.setFont(tmpfont);

    QPainterPath path;
    if (font.hasOutline())
        path.addText(0, 0, tmpfont, msg);

    if (font.hasShadow())
    {
        QRect a = QRect(0, 0, r.width(), r.height());
        a.translate(shadowOffset.x() + textOffsetX,
                    shadowOffset.y() + textOffsetY);

        shadowColor.setAlpha(shadowAlpha);
        tmp.setPen(shadowColor);
        tmp.drawText(a, flags, msg);
    }

    if (font.hasOutline())
    {
        // QPainter::drawText() treats the Y coordinate as the top of
        // the text (when Qt::AlignTop is used).  However,
        // QPainterPath::addText() treats the Y coordinate as the base
        // line of the text.  To translate from the top to the base
        // line, we need to add QFontMetrics::ascent().
        int adjX = 0;
        int adjY = fm.ascent();

        outlineColor.setAlpha(outlineAlpha);
        tmp.setPen(outlineColor);

        path.translate(adjX + textOffsetX, adjY + textOffsetY);
        QPen pen = tmp.pen();
        pen.setWidth(outlineSize * 2 + 1);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        tmp.setPen(pen);
        tmp.drawPath(path);

        path.translate(outlineSize, outlineSize);
    }

    tmp.setPen(QPen(font.GetBrush(), 0));
    tmp.setBrush(font.GetBrush());
    tmp.drawText(textOffsetX, textOffsetY, r.width(), r.height(),
                 flags, msg);
    tmp.end();
    im->Assign(pm);
}

void MythPainter::DrawRectPriv(MythImage *im, const QRect area, int radius,
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
                                           int flags, const QRect r,
                                           const MythFontProperties &font)
{
    QString incoming = font.GetHash() + QString::number(r.width()) +
                       QString::number(r.height()) +
                       QString::number(flags) +
                       QString::number(font.color().rgba()) + msg;

    MythImage *im = nullptr;
    if (m_stringToImageMap.contains(incoming))
    {
        m_stringExpireList.remove(incoming);
        m_stringExpireList.push_back(incoming);
        im = m_stringToImageMap[incoming];
        if (im)
            im->IncrRef();
    }
    else
    {
        im = GetFormatImage();
        im->SetFileName(QString("GetImageFromString: %1").arg(msg));
        DrawTextPriv(im, msg, flags, r, font);

        im->IncrRef();
        m_softwareCacheSize += im->GetSize();
        m_stringToImageMap[incoming] = im;
        m_stringExpireList.push_back(incoming);
        ExpireImages(m_maxSoftwareCacheSize);
    }
    return im;
}

MythImage *MythPainter::GetImageFromTextLayout(const LayoutVector &layouts,
                                               const FormatVector &formats,
                                               const MythFontProperties &font,
                                               QRect &canvas, QRect &dest)
{
    QString incoming = QString::number(canvas.x()) +
                       QString::number(canvas.y()) +
                       QString::number(canvas.width()) +
                       QString::number(canvas.height()) +
                       QString::number(dest.width()) +
                       QString::number(dest.height()) +
                       font.GetHash();

    for (auto *layout : qAsConst(layouts))
        incoming += layout->text();

    MythImage *im = nullptr;
    if (m_stringToImageMap.contains(incoming))
    {
        m_stringExpireList.remove(incoming);
        m_stringExpireList.push_back(incoming);
        im = m_stringToImageMap[incoming];
        if (im)
            im->IncrRef();
    }
    else
    {
        im = GetFormatImage();
        im->SetFileName("GetImageFromTextLayout");

        QImage pm(canvas.size(), QImage::Format_ARGB32_Premultiplied);
        pm.fill(0);

        QPainter painter(&pm);
        if (!painter.isActive())
        {
            LOG(VB_GENERAL, LOG_ERR, "MythPainter::GetImageFromTextLayout: "
                "Invalid canvas.");
            return im;
        }

        QRect    clip;
        clip.setSize(canvas.size());

        QFont tmpfont = font.face();
#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
        tmpfont.setStyleStrategy(QFont::OpenGLCompatible);
#endif
        painter.setFont(tmpfont);
        painter.setRenderHint(QPainter::Antialiasing);

        if (font.hasShadow())
        {
            QRect  shadowRect;
            QPoint shadowOffset;
            QColor shadowColor;
            int    shadowAlpha = 255;

            font.GetShadow(shadowOffset, shadowColor, shadowAlpha);
            shadowColor.setAlpha(shadowAlpha);

            MythPoint  shadow(shadowOffset);
            shadow.NormPoint(); // scale it to screen resolution

            shadowRect = canvas;
            shadowRect.translate(shadow.x(), shadow.y());

            painter.setPen(shadowColor);
            for (auto *layout : qAsConst(layouts))
                layout->draw(&painter, shadowRect.topLeft(), formats, clip);
        }

        painter.setPen(QPen(font.GetBrush(), 0));
        for (auto *layout : qAsConst(layouts))
        {
            layout->draw(&painter, canvas.topLeft(),
                           layout->formats(), clip);
        }
        painter.end();

        pm.setOffset(canvas.topLeft());
        im->Assign(pm.copy(0, 0, dest.width(), dest.height()));

        im->IncrRef();
        m_softwareCacheSize += im->GetSize();
        m_stringToImageMap[incoming] = im;
        m_stringExpireList.push_back(incoming);
        ExpireImages(m_maxSoftwareCacheSize);
    }
    return im;
}

MythImage* MythPainter::GetImageFromRect(const QRect area, int radius,
                                         int ellipse,
                                         const QBrush &fillBrush,
                                         const QPen &linePen)
{
    if (area.width() <= 0 || area.height() <= 0)
        return nullptr;

    uint64_t hash1 = ((0xfff & (uint64_t)area.width())) +
                     ((0xfff & (uint64_t)area.height())     << 12) +
                     ((0xff  & (uint64_t)fillBrush.style()) << 24) +
                     ((0xff  & (uint64_t)linePen.width())   << 32) +
                     ((0xff  & (uint64_t)radius)            << 40) +
                     ((0xff  & (uint64_t)linePen.style())   << 48) +
                     ((0xff  & (uint64_t)ellipse)           << 56);
    uint64_t hash2 = ((0xffffffff & (uint64_t)linePen.color().rgba())) +
                     ((0xffffffff & (uint64_t)fillBrush.color().rgba()) << 32);

    QString incoming("R");
    if (fillBrush.style() == Qt::LinearGradientPattern && fillBrush.gradient())
    {
        // The Q*Gradient classes are not polymorohic, and therefore
        // dynamic_cast can't be used here.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        const auto *gradient = static_cast<const QLinearGradient*>(fillBrush.gradient());
        if (gradient)
        {
            incoming = QString::number(
                             ((0xfff & (uint64_t)gradient->start().x())) +
                             ((0xfff & (uint64_t)gradient->start().y()) << 12) +
                             ((0xfff & (uint64_t)gradient->finalStop().x()) << 24) +
                             ((0xfff & (uint64_t)gradient->finalStop().y()) << 36));
            QGradientStops stops = gradient->stops();
            for (const auto & stop : qAsConst(stops))
            {
                incoming += QString::number(
                             ((0xfff * (uint64_t)(stop.first * 100))) +
                             ((uint64_t)stop.second.rgba() << 12));
            }
        }
    }

    incoming += QString::number(hash1) + QString::number(hash2);

    MythImage *im = nullptr;
    if (m_stringToImageMap.contains(incoming))
    {
        m_stringExpireList.remove(incoming);
        m_stringExpireList.push_back(incoming);
        im = m_stringToImageMap[incoming];
        if (im)
            im->IncrRef();
    }
    else
    {
        im = GetFormatImage();
        im->SetFileName("GetImageFromRect");
        DrawRectPriv(im, area, radius, ellipse, fillBrush, linePen);

        im->IncrRef();
        m_softwareCacheSize += im->GetSize();
        m_stringToImageMap[incoming] = im;
        m_stringExpireList.push_back(incoming);
        ExpireImages(m_maxSoftwareCacheSize);
    }
    return im;
}

MythImage *MythPainter::GetFormatImage(void)
{
    QMutexLocker locker(&m_allocationLock);
    MythImage *result = GetFormatImagePriv();
    result->SetFileName("GetFormatImage");
    m_allocatedImages.insert(result);
    return result;
}

void MythPainter::DeleteFormatImage(MythImage *im)
{
    QMutexLocker locker(&m_allocationLock);
    DeleteFormatImagePriv(im);
    m_allocatedImages.remove(im);
}

void MythPainter::CheckFormatImage(MythImage *im)
{
    if (im && !im->GetParent())
    {
        QMutexLocker locker(&m_allocationLock);
        m_allocatedImages.insert(im);
        im->SetParent(this);
    }
}

void MythPainter::ExpireImages(int64_t max)
{
    bool recompute = false;
    while (!m_stringExpireList.empty())
    {
        if (m_softwareCacheSize < max)
            break;

        QString oldmsg = m_stringExpireList.front();
        m_stringExpireList.pop_front();

        QMap<QString, MythImage*>::iterator it =
            m_stringToImageMap.find(oldmsg);
        if (it == m_stringToImageMap.end())
        {
            recompute = true;
            continue;
        }
        MythImage *oldim = *it;
        it = m_stringToImageMap.erase(it);

        if (oldim)
        {
            m_softwareCacheSize -= oldim->GetSize();
            if (m_softwareCacheSize < 0)
            {
                m_softwareCacheSize = 0;
                recompute = true;
            }
            oldim->DecrRef();
        }
    }
    if (recompute)
    {
        m_softwareCacheSize = 0;
        for (auto *img : qAsConst(m_stringToImageMap))
            m_softwareCacheSize += img->GetSize();
    }
}

// the following assume graphics hardware operates natively at 32bpp
void MythPainter::SetMaximumCacheSizes(int hardware, int software)
{
    static constexpr int64_t kOneMeg = 1LL * 1024 * 1024;
    m_maxHardwareCacheSize = kOneMeg * hardware;
    m_maxSoftwareCacheSize = kOneMeg * software;

    bool err = false;
    if (m_maxHardwareCacheSize < 0)
    {
        m_maxHardwareCacheSize = 0;
        err = true;
    }
    if (m_maxSoftwareCacheSize < 0)
    {
        m_maxSoftwareCacheSize = kOneMeg * 48;
        err = true;
    }

    LOG((err) ? VB_GENERAL : VB_GUI, (err) ? LOG_ERR : LOG_INFO,
        QString("MythPainter cache sizes: Hardware %1MB, Software %2MB")
        .arg(m_maxHardwareCacheSize / kOneMeg)
        .arg(m_maxSoftwareCacheSize / kOneMeg));
}
