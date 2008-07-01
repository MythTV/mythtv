#include <cassert>
#include <QPainter>
#include <QPixmap>

#include "mythpainter_qt.h"
#include "mythfontproperties.h"
#include "mythmainwindow.h"
#include "compat.h"

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
        m_Pixmap = QPixmap::fromImage(*((QImage *)this));

    MythImage::SetChanged(change);
}

MythQtPainter::MythQtPainter() :
    MythPainter(),
    painter(0)
{
}

MythQtPainter::~MythQtPainter()
{
}

void MythQtPainter::Begin(QWidget *parent)
{
    assert(parent);

    MythPainter::Begin(parent);

    painter = new QPainter(parent);
    clipRegion = QRegion(QRect(0, 0, 0, 0));
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
    assert(painter);
    (void)alpha;

    MythQtImage *qim = reinterpret_cast<MythQtImage *>(im);

    painter->drawPixmap(r.topLeft(), *(qim->GetPixmap()), src);
}

void MythQtPainter::DrawText(const QRect &r, const QString &msg,
                             int flags, const MythFontProperties &font,
                             int alpha, const QRect &boundRect)
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

