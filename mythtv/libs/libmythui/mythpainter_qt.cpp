
// QT headers
#include <QPainter>
#include <QPixmap>

// MythUI headers
#include "mythpainter_qt.h"
#include "mythfontproperties.h"
#include "mythmainwindow.h"

// MythDB headers
#include "compat.h"
#include "mythverbose.h"

class MythQtImage : public MythImage
{
  public:
    MythQtImage(MythPainter *parent) : MythImage(parent),
                m_Pixmap(NULL), m_bRegenPixmap(false) { }
   ~MythQtImage() { }

    void SetChanged(bool change = true);
    QPixmap *GetPixmap(void) { return m_Pixmap; }

    bool NeedsRegen(void) { return m_bRegenPixmap; }
    void RegeneratePixmap(void);

  protected:
    QPixmap *m_Pixmap;
    bool m_bRegenPixmap;
};

void MythQtImage::SetChanged(bool change)
{
    if (change)
        m_bRegenPixmap = true;

    MythImage::SetChanged(change);
}

void MythQtImage::RegeneratePixmap(void)
{
    // We allocate the pixmap here so it is done in the UI
    // thread since QPixmap uses non-reentrant X calls.
    if (!m_Pixmap)
        m_Pixmap = new QPixmap;

    if (m_Pixmap)
    {
        *m_Pixmap = QPixmap::fromImage(*((QImage *)this));
        m_bRegenPixmap = false;
    }
}

MythQtPainter::MythQtPainter() :
    MythPainter(),
    painter(0)
{
}

MythQtPainter::~MythQtPainter()
{
}

void MythQtPainter::Begin(QPaintDevice *parent)
{
    if (!parent)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: No parent widget defined for "
                              "QT Painter, bailing");
        return;
    }

    MythPainter::Begin(parent);

    painter = new QPainter(parent);
    clipRegion = QRegion(QRect(0, 0, 0, 0));

    QMutexLocker locker(&m_imageDeleteLock);
    while (!m_imageDeleteList.empty())
    {
        QPixmap *pm = m_imageDeleteList.front();
        m_imageDeleteList.pop_front();
        delete pm;
    }
}

void MythQtPainter::End(void)
{
    painter->end();
    delete painter;

    MythPainter::End();
}

void MythQtPainter::SetClipRect(const QRect &clipRect)
{
    painter->setClipRect(clipRect);
    if (!clipRect.isEmpty())
    {
        painter->setClipping(true);
        if (clipRegion.isEmpty())
            clipRegion = QRegion(clipRect);
        else
            clipRegion = clipRegion.unite(clipRect);
    }
    else
        painter->setClipping(false);
}

void MythQtPainter::DrawImage(const QRect &r, MythImage *im,
                              const QRect &src, int alpha)
{
    if (!painter)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: DrawImage called with no painter");
        return;
    }

    MythQtImage *qim = reinterpret_cast<MythQtImage *>(im);

    if (qim->NeedsRegen())
        qim->RegeneratePixmap();

    painter->setOpacity(static_cast<float>(alpha) / 255.0);
    painter->drawPixmap(r.topLeft(), *(qim->GetPixmap()), src);
    painter->setOpacity(1.0);
}

void MythQtPainter::DrawText(const QRect &r, const QString &msg,
                             int flags, const MythFontProperties &font,
                             int alpha, const QRect &boundRect)
{
    if (!painter)
    {
        VERBOSE(VB_IMPORTANT, "FATAL ERROR: DrawText called with no painter");
        return;
    }

    (void)alpha;

    painter->setOpacity(static_cast<float>(alpha) / 255.0);
    
    painter->setFont(font.face());

    if (font.hasShadow())
    {
        QPoint shadowOffset;
        QColor shadowColor;
        int shadowAlpha;

        font.GetShadow(shadowOffset, shadowColor, shadowAlpha);

        shadowColor.setAlpha(shadowAlpha);

        QRect a = r;
        a.translate(shadowOffset.x(), shadowOffset.y());

        painter->setPen(shadowColor);
        painter->drawText(a, flags, msg);
    }

    if (font.hasOutline() && alpha > 128)
    {
        QColor outlineColor;
        int outlineSize, outlineAlpha;

        font.GetOutline(outlineColor, outlineSize, outlineAlpha);

        if (GetMythMainWindow()->GetUIScreenRect().height() > 700)
            outlineSize = 1;

        painter->setPen(outlineColor);

        QRect a = r;
        a.translate(0 - outlineSize, 0 - outlineSize);
        painter->drawText(a, flags, msg);

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(1, 0);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(0, 1);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(-1, 0);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.translate(0, -1);
            painter->drawText(a, flags, msg);
        }
    }

    painter->setPen(QPen(font.GetBrush(), 0));
    painter->drawText(r, flags, msg);
    painter->setOpacity(1.0);
}

void MythQtPainter::DrawRect(const QRect &area,
                             bool drawFill, const QColor &fillColor, 
                             bool drawLine, int lineWidth, const QColor &lineColor)
{
    if (drawLine)
        painter->setPen(QPen(lineColor, lineWidth));
    else
        painter->setPen(QPen(Qt::NoPen));

    if (drawFill)
        painter->setBrush(QBrush(fillColor));
    else
        painter->setBrush(QBrush(Qt::NoBrush));

    painter->drawRect(area);

    painter->setBrush(QBrush(Qt::NoBrush));
}

void MythQtPainter::DrawRoundRect(const QRect &area, int radius, 
                                  bool drawFill, const QColor &fillColor, 
                                  bool drawLine, int lineWidth, const QColor &lineColor)
{
    painter->setRenderHint(QPainter::Antialiasing);

    if (drawLine)
        painter->setPen(QPen(lineColor, lineWidth));
    else
        painter->setPen(QPen(Qt::NoPen));

    if (drawFill)
        painter->setBrush(QBrush(fillColor));
    else
        painter->setBrush(QBrush(Qt::NoBrush));

    if ((area.width() / 2) < radius)
        radius = area.width() / 2;

    if ((area.height() / 2) < radius)
        radius = area.height() / 2;

    QRectF r(area);
    if (lineWidth > 0)
        r.adjust(lineWidth / 2, lineWidth / 2, -lineWidth / 2, -lineWidth / 2);

    painter->drawRoundedRect(r, (qreal)radius, (qreal)radius);

    painter->setRenderHint(QPainter::Antialiasing, false);

    painter->setBrush(QBrush(Qt::NoBrush));
}

MythImage *MythQtPainter::GetFormatImage()
{
    return new MythQtImage(this);
}

void MythQtPainter::DeleteFormatImage(MythImage *im)
{
    MythQtImage *qim = static_cast<MythQtImage *>(im);

    QMutexLocker locker(&m_imageDeleteLock);
    if (qim->GetPixmap())
        m_imageDeleteList.push_back(qim->GetPixmap());
}

