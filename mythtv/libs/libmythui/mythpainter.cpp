#include "mythpainter.h"
#include "mythfontproperties.h"
#include <cassert>
#include "mythimage.h"

void MythPainter::SetClipRect(const QRect &)
{
}

void MythPainter::DrawImage(int x, int y, MythImage *im, int alpha)
{
    assert(im);
    QRect dest = QRect(x, y, im->width(), im->height());
    QRect src = im->rect();
    DrawImage(dest, im, src, alpha);
}

void MythPainter::DrawImage(const QPoint &topLeft, MythImage *im, int alpha)
{
    DrawImage(topLeft.x(), topLeft.y(), im, alpha);
}

