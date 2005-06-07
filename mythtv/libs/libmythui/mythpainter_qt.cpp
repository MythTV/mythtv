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

    mainPainter = new QPainter(parent);

    drawPixmap = new QPixmap(parent->size());
    painter = new QPainter(drawPixmap);
}

void MythQtPainter::End(void)
{
    painter->end();

    mainPainter->drawPixmap(0, 0, *drawPixmap);
    mainPainter->end();

    delete painter;
    delete drawPixmap;
    delete mainPainter;

    MythPainter::End();
}

void MythQtPainter::SetClipRect(const QRect &clipRect)
{
    painter->setClipRect(clipRect);
    mainPainter->setClipRect(clipRect);
    if (clipRect != QRect())
    {
        painter->setClipping(true);
        mainPainter->setClipping(true);
    }
    else
    {
        painter->setClipping(false);
        mainPainter->setClipping(false);
    }
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

    painter->setFont(font.face);

    if (font.hasShadow)
    {
        QRect a = r;
        a.moveBy(font.shadowOffset.x(), font.shadowOffset.y());

        painter->setPen(font.shadowColor);
        painter->drawText(a, flags, msg);
    }

    if (font.hasOutline && alpha > 128)
    {
        painter->setPen(font.outlineColor);

        QRect a = r;
        a.moveBy(0 - font.outlineSize, 0 - font.outlineSize);
        painter->drawText(a, flags, msg);

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(1, 0);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(0, 1);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(-1, 0);
            painter->drawText(a, flags, msg);
        }

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(0, -1);
            painter->drawText(a, flags, msg);
        }
    }

    painter->setPen(font.color);
    painter->drawText(r, flags, msg);
}

MythImage *MythQtPainter::GetFormatImage()
{
    return new MythQtImage(this);
}

void MythQtPainter::DeleteFormatImage(MythImage* /* im */)
{
}

