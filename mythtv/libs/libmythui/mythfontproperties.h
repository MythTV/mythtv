#ifndef MYTHFONTPROPERTIES_H_
#define MYTHFONTPROPERTIES_H_

#include <qfont.h>
#include <qcolor.h>
#include <qpoint.h>
#include <qmap.h>

#include "xmlparsebase.h"

class MythFontProperties: public XMLParseBase
{
  public:
    MythFontProperties();

    void SetFace(const QFont &face);
    void SetColor(const QColor &color);
    void SetShadow(bool on, const QPoint &offset, const QColor &color, int alpha);
    void SetOutline(bool on, const QColor &color, int size, int alpha);

    QFont face(void) const { return m_face; }
    QColor color(void) const { return m_color; }

    bool hasShadow(void) const { return m_hasShadow; }
    void GetShadow(QPoint &offset, QColor &color, int &alpha) const;

    bool hasOutline(void) const { return m_hasOutline; }
    void GetOutline(QColor &color, int &size, int &alpha) const;

    QString GetHash(void) const { return m_hash; }

    static MythFontProperties *ParseFromXml(QDomElement &element,
                                            bool addToGlobal = false);

  private:
    void Freeze(void); // no hash updates
    void Unfreeze(void);

    void CalcHash(void);

    QFont   m_face;
    QColor  m_color;

    bool    m_hasShadow;
    QPoint  m_shadowOffset;
    QColor  m_shadowColor;
    int     m_shadowAlpha;

    bool    m_hasOutline;
    QColor  m_outlineColor;
    int     m_outlineSize;
    int     m_outlineAlpha;

    QString m_hash;

    bool    m_bFreeze;

    friend class FontMap;
};

class FontMap
{
  public:
    FontMap() {}

    MythFontProperties *GetFont(const QString &text);
    bool AddFont(const QString &text, MythFontProperties *fontProp);
    bool Contains(const QString &text);

    void Clear(void);

    static FontMap *GetGlobalFontMap(void);

  private:
    QMap<QString, MythFontProperties> m_FontMap;
};

FontMap *GetGlobalFontMap(void);

#endif
