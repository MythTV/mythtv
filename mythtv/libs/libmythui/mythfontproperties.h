#ifndef MYTHFONTPROPERTIES_H_
#define MYTHFONTPROPERTIES_H_

#include <qfont.h>
#include <qcolor.h>
#include <qpoint.h>
#include <qmap.h>

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

class GlobalFontMap
{
  // singleton
  protected:
    GlobalFontMap() {}

  public:
    MythFontProperties *GetFont(const QString &text);
    bool AddFont(const QString &text, MythFontProperties *fontProp);

    void Clear(void);

    static GlobalFontMap *GetGlobalFontMap(void);

  private:
    QMap<QString, MythFontProperties> m_globalFontMap;
};

GlobalFontMap *GetGlobalFontMap(void);

#endif
