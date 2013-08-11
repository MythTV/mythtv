#include <QPainter>
#include <QImage>

#include "mythpainter_yuva.h"
#include "mythfontproperties.h"
#include "mythlogging.h"

#define MAX_FONT_CACHE 32

QColor inline rgb_to_yuv(const QColor &original);

MythYUVAPainter::~MythYUVAPainter()
{
    Teardown();
    foreach(MythFontProperties* font, m_convertedFonts)
        delete font;
}

void MythYUVAPainter::DrawImage(const QRect &dest, MythImage *im,
                                const QRect &src, int alpha)
{
    if (im->format() != QImage::Format_ARGB32)
    {
        QImage converted = im->convertToFormat(QImage::Format_ARGB32);
        im->Assign(converted);
    }

    im->ConvertToYUV();
    MythQImagePainter::DrawImage(dest, im, src, alpha);
}

void MythYUVAPainter::DrawText(const QRect &dest, const QString &msg, int flags,
                               const MythFontProperties &font, int alpha,
                               const QRect &boundRect)
{
    MythFontProperties *converted = GetConvertedFont(font);
    if (converted)
    {
        // We pull an image here, in the hopes that when DrawText
        // pulls an image this will still be in the cache and have
        // the right properties.
        MythImage *im = GetImageFromString(msg, flags, dest, *converted);
        if (im)
        {
            im->SetToYUV();
            im->DecrRef();
            im = NULL;
        }

        MythQImagePainter::DrawText(dest, msg, flags, *converted,
                                    alpha, boundRect);
    }
}

void MythYUVAPainter::DrawRect(const QRect &area, const QBrush &fillBrush,
                               const QPen &linePen, int alpha)
{
    QBrush brush(fillBrush);

    switch (fillBrush.style())
    {
    case Qt::LinearGradientPattern:
    case Qt::RadialGradientPattern:
    case Qt::ConicalGradientPattern:
        {
        QGradient gradient = *fillBrush.gradient();
        QGradientStops stops = gradient.stops();
        for (QGradientStops::iterator it = stops.begin(); it != stops.end(); ++it)
        {
            it->second = rgb_to_yuv(it->second);
            it->second.setAlpha(alpha);
        }
        gradient.setStops(stops);
        brush = gradient;
        }
        break;
    default:
        brush.setColor(rgb_to_yuv(brush.color()));
        break;
    }

    QPen pen(linePen);
    pen.setColor(rgb_to_yuv(pen.color()));

    // We pull an image here, in the hopes that when DrawRect
    // pulls an image this will still be in the cache and have
    // the right properties.
    MythImage *im = GetImageFromRect(area, 0, 0, brush, pen);
    if (im)
    {
        im->SetToYUV();
        im->DecrRef();
        im = NULL;
    }

    MythQImagePainter::DrawRect(area, brush, pen, alpha);
}

void MythYUVAPainter::DrawRoundRect(const QRect &area, int cornerRadius,
                                    const QBrush &fillBrush, const QPen &linePen,
                                    int alpha)
{
    QBrush brush(fillBrush);
    brush.setColor(rgb_to_yuv(brush.color()));
    QPen pen(linePen);
    pen.setColor(rgb_to_yuv(pen.color()));

    // We pull an image here, in the hopes that when DrawRect
    // pulls an image this will still be in the cache and have
    // the right properties.
    MythImage *im = GetImageFromRect(area, cornerRadius, 0, brush, pen);
    if (im)
    {
        im->SetToYUV();
        im->DecrRef();
        im = NULL;
    }

    MythQImagePainter::DrawRoundRect(area, cornerRadius, brush, pen, alpha);
}

void MythYUVAPainter::DrawEllipse(const QRect &area, const QBrush &fillBrush,
                                  const QPen &linePen, int alpha)
{
    QBrush brush(fillBrush);
    brush.setColor(rgb_to_yuv(brush.color()));
    QPen pen(linePen);
    pen.setColor(rgb_to_yuv(pen.color()));

    // We pull an image here, in the hopes that when DrawRect
    // pulls an image this will still be in the cache and have
    // the right properties.
    MythImage *im = GetImageFromRect(area, 0, 1, brush, pen);
    if (im)
    {
        im->SetToYUV();
        im->DecrRef();
        im = NULL;
    }

    MythQImagePainter::DrawEllipse(area, brush, pen, alpha);
}

MythFontProperties* MythYUVAPainter::GetConvertedFont(const MythFontProperties &font)
{
    QString original = font.GetHash();

    if (m_convertedFonts.contains(original))
    {
        m_expireList.remove(original);
        m_expireList.push_back(original);
    }
    else
    {
        QColor yuv_color;
        MythFontProperties *new_font = new MythFontProperties();
        yuv_color = rgb_to_yuv(font.color());
        new_font->SetFace(font.face());
        new_font->SetColor(yuv_color);

        if (font.hasShadow())
        {
            QPoint offset;
            QColor color;
            int alpha;
            font.GetShadow(offset, color, alpha);
            yuv_color = rgb_to_yuv(color);
            new_font->SetShadow(true, offset, yuv_color, alpha);
        }

        if (font.hasOutline())
        {
            QColor color;
            int size, alpha;
            font.GetOutline(color, size, alpha);
            yuv_color = rgb_to_yuv(color);
            new_font->SetOutline(true, yuv_color, size, alpha);
        }

        m_convertedFonts.insert(original, new_font);
        m_expireList.push_back(original);

        if (m_convertedFonts.size() > MAX_FONT_CACHE)
        {
            QString expire = m_expireList.front();
            m_expireList.pop_front();
            if (m_convertedFonts.contains(expire))
            {
                delete m_convertedFonts.value(expire);
                m_convertedFonts.remove(expire);
            }
        }
    }

    return m_convertedFonts.value(original);
}

#define SCALEBITS 8
#define ONE_HALF (1 << (SCALEBITS - 1))
#define FIX(x)   ((int) ((x) * (1L<<SCALEBITS) /*+ 0.5*/))

QColor inline rgb_to_yuv(const QColor &original)
{
    int r = original.red();
    int g = original.green();
    int b = original.blue();
    int a = original.alpha();

    int y = (FIX(0.299) * r + FIX(0.587) * g +
             FIX(0.114) * b + ONE_HALF) >> SCALEBITS;
    int u = ((- FIX(0.169) * r - FIX(0.331) * g +
             FIX(0.499) * b + ONE_HALF) >> SCALEBITS) + 128;
    int v = ((FIX(0.499) * r - FIX(0.418) * g -
             FIX(0.0813) * b + ONE_HALF) >> SCALEBITS) + 128;

    return QColor(y, u, v, a);
}

