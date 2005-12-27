#ifndef MYTHFONTPROPERTIES_H_
#define MYTHFONTPROPERTIES_H_

#include <qfont.h>
#include <qcolor.h>

struct MythFontProperties
{
  public:

    MythFontProperties() :
        color(QColor(Qt::white)),
        hasShadow(false),
        shadowAlpha(255),
        hasOutline(false),
        outlineAlpha(255) { }

    QFont   face;
    QColor  color;

    bool    hasShadow;
    QPoint  shadowOffset;
    QColor  shadowColor;
    int     shadowAlpha;

    bool    hasOutline;
    QColor  outlineColor;
    int     outlineSize;
    int     outlineAlpha;
};

#endif
