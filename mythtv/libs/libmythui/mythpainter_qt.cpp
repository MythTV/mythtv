#include <cassert>
#include <qpainter.h>
#include <qpixmap.h>
#include "mythpainter_qt.h"
#include "mythfontproperties.h"

class MythQtImage : public MythImage
{
  public:
    MythQtImage(MythPainter *parent) : MythImage(parent) { }

    void SetChanged(bool change = true);
    QPixmap *GetPixmap(void) { return &m_Pixmap; }

  protected:
    QPixmap m_Pixmap;
};

void MythQtImage::SetChanged(bool change)
{
    if (change)
        m_Pixmap.convertFromImage(*((QImage *)this));

    MythImage::SetChanged(change);
}

MythQtPainter::MythQtPainter()
             : MythPainter()
{
}

MythQtPainter::~MythQtPainter()
{
}

void MythQtPainter::Begin(QWidget *parent)
{
    assert(parent);

    MythPainter::Begin(parent);

    // Oddly enough, caching these makes drawing slower.
    mainPainter = new QPainter(parent);
    drawPixmap = new QPixmap(parent->size());
    painter = new QPainter(drawPixmap);

    clipRegion = QRegion(QRect(0, 0, 0, 0));
}

void MythQtPainter::End(void)
{
    painter->end();

    if (!clipRegion.isEmpty() && !clipRegion.isNull())
    {
        QMemArray<QRect> rects = clipRegion.rects();

        for (unsigned int i = 0; i < rects.size(); i++)
        {
            QRect rect = rects[i];

            if (rect.width() == 0 || rect.height() == 0)
                continue;

            mainPainter->drawPixmap(rect.topLeft(), *drawPixmap, rect);
        }
    }
    else
        mainPainter->drawPixmap(0, 0, *drawPixmap);

    mainPainter->end();

    delete painter;
    delete drawPixmap;
    delete mainPainter;

    MythPainter::End();
}

void MythQtPainter::SetClipRect(const QRect &clipRect)
{
    if (clipRect.size() == drawPixmap->size())
        return;

    painter->setClipRect(clipRect);
    if (clipRect != QRect())
    {
        painter->setClipping(true);
        if (clipRegion.isNull() || clipRegion.isEmpty())
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
    assert(painter);
    (void)alpha;

    MythQtImage *qim = reinterpret_cast<MythQtImage *>(im);

    painter->drawPixmap(r.topLeft(), *(qim->GetPixmap()), src);
}

void MythQtPainter::DrawText(const QRect &r, const QString &msg,
                             int flags, const MythFontProperties &font,
                             int alpha)
{
    assert(painter);
    (void)alpha;

    painter->setFont(font.face());

    if (font.hasShadow())
    {
        QPoint shadowOffset;
        QColor shadowColor;
        int shadowAlpha;

        font.GetShadow(shadowOffset, shadowColor, shadowAlpha);

        QRect a = r;
        a.moveBy(shadowOffset.x(), shadowOffset.y());

        painter->setPen(shadowColor);
        painter->drawText(a, flags, msg);
    }

    if (font.hasOutline() && alpha > 128)
    {
        QColor outlineColor;
        int outlineSize, outlineAlpha;

        font.GetOutline(outlineColor, outlineSize, outlineAlpha);

        painter->setPen(outlineColor);

        QRect a = r;
        a.moveBy(0 - outlineSize, 0 - outlineSize);
        painter->drawText(a, flags, msg);

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(1, 0);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(0, 1);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(-1, 0);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - outlineSize + 1); i <= outlineSize; i++)
        {
            a.moveBy(0, -1);
            painter->drawText(a, flags, msg);
        }
    }

    painter->setPen(font.color());
    painter->drawText(r, flags, msg);
}

MythImage *MythQtPainter::GetFormatImage()
{
    return new MythQtImage(this);
}

void MythQtPainter::DeleteFormatImage(MythImage* /* im */)
{
}

