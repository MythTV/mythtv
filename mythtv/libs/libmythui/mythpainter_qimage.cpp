// QT headers
#include <QPainter>
#include <QImage>

// MythUI headers
#include "mythpainter_qimage.h"
#include "mythmainwindow.h"

// MythDB headers
#include "compat.h"
#include "mythlogging.h"

MythQImagePainter::~MythQImagePainter()
{
    Teardown();
}

void MythQImagePainter::Begin(QPaintDevice *parent)
{
    if (!parent)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "FATAL ERROR: No parent widget defined for QT Painter, bailing");
        return;
    }

    MythPainter::Begin(parent);
    m_painter       = new QPainter(parent);
    m_copy          = true;
    m_paintedRegion = QRegion();
    m_painter->setCompositionMode(QPainter::CompositionMode_Source);
    m_clipRegion = QRegion();
    SetClipRect(QRect());
}

void MythQImagePainter::CheckPaintMode(const QRect &area)
{
    if (!m_painter)
        return;

    bool intersects = false;

    if (m_paintedRegion.isEmpty())
    {
        intersects = false;
        m_paintedRegion = QRegion(area);
    }
    else
    {
        intersects = m_paintedRegion.intersects(area);
        m_paintedRegion = m_paintedRegion.united(area);
    }

    if (intersects && m_copy)
    {
        m_copy = false;
        m_painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    else if (!intersects && !m_copy)
    {
        m_copy = true;
        m_painter->setCompositionMode(QPainter::CompositionMode_Source);
    }
}

void MythQImagePainter::End(void)
{
    if (!m_painter)
        return;

    m_painter->end();
    delete m_painter;

    MythPainter::End();
}

void MythQImagePainter::SetClipRect(const QRect &clipRect)
{
    if (!m_painter)
        return;

    if (!clipRect.isEmpty())
    {
        m_painter->setClipping(true);
        if (m_clipRegion.isEmpty())
            m_clipRegion = QRegion(clipRect);
        else
            m_clipRegion = m_clipRegion.united(clipRect);
        m_painter->setClipRegion(m_clipRegion);
    }
    else
        m_painter->setClipping(false);
}

void MythQImagePainter::SetClipRegion(const QRegion &region)
{
    if (!m_painter)
        return;

    if (!region.isEmpty())
    {
        m_painter->setClipping(true);
        m_clipRegion = region;
        m_painter->setClipRegion(m_clipRegion);
    }
    else
        m_painter->setClipping(false);
}

void MythQImagePainter::Clear(QPaintDevice *device, const QRegion &region)
{
    if (!device || region.isEmpty())
        return;

    auto *dev = dynamic_cast<QImage*>(device);
    if (!dev)
        return;

    int img_width  = dev->size().width();
    int img_height = dev->size().height();

#if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    QVector<QRect> rects = region.rects();
    for (int i = 0; i < rects.size(); i++)
    {
        const QRect& r = rects[i];
#else
    for (const QRect& r : region)
    {
#endif
        if (r.top() > img_height || r.left() > img_width)
            continue;

        int bottom = std::min(r.top() + r.height(), img_height);
        int bwidth = std::min(r.left() + r.width(), img_width);
        bwidth = (bwidth - r.left()) << 2;

        for (int row = r.top(); row < bottom; row++)
            memset(dev->scanLine(row) + (r.left() << 2), 0, bwidth);
    }
}

void MythQImagePainter::DrawImage(const QRect &r, MythImage *im,
                                  const QRect &src, int alpha)
{
    if (!m_painter)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "FATAL ERROR: DrawImage called with no painter");
        return;
    }

    (void)alpha;

    CheckPaintMode(QRect(r.topLeft(), src.size()));
    m_painter->setOpacity(static_cast<float>(alpha) / 255.0F);
    m_painter->drawImage(r.topLeft(), (QImage)(*im), src);
    m_painter->setOpacity(1.0);
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
