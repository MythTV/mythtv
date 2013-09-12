#ifndef MYTHFONTPROPERTIES_H_
#define MYTHFONTPROPERTIES_H_

#include <QFont>
#include <QColor>
#include <QBrush>
#include <QPoint>
#include <QMap>

#include "xmlparsebase.h"
#include "mythmainwindow.h"

class MUI_PUBLIC MythFontProperties: public XMLParseBase
{
  public:
    MythFontProperties();

    QFont* GetFace(void) { return &m_face; }

    void SetFace(const QFont &face);
    void SetColor(const QColor &color);
    void SetShadow(bool on, const QPoint &offset, const QColor &color, int alpha);
    void SetOutline(bool on, const QColor &color, int size, int alpha);

    QFont face(void) const { return m_face; }
    QColor color(void) const { return m_brush.color(); }
    QBrush GetBrush(void) const { return m_brush; }

    bool hasShadow(void) const { return m_hasShadow; }
    void GetShadow(QPoint &offset, QColor &color, int &alpha) const;

    bool hasOutline(void) const { return m_hasOutline; }
    void GetOutline(QColor &color, int &size, int &alpha) const;

    QString GetHash(void) const { return m_hash; }

    static MythFontProperties *ParseFromXml(
        const QString &filename, const QDomElement &element,
        MythUIType *parent = NULL, bool addToGlobal = false,
        bool showWarnings = true);

    void SetRelativeSize(float rSize) { m_relativeSize = rSize; }
    float GetRelativeSize(void) const { return m_relativeSize; }
    void SetPixelSize(float size);
    void SetPointSize(uint size);
    void Rescale(void);
    void Rescale(int height);
    void AdjustStretch(int stretch);

  private:
    void Freeze(void); // no hash updates
    void Unfreeze(void);

    void CalcHash(void);

    QFont   m_face;
    QBrush  m_brush;

    bool    m_hasShadow;
    QPoint  m_shadowOffset;
    QColor  m_shadowColor;
    int     m_shadowAlpha;

    bool    m_hasOutline;
    QColor  m_outlineColor;
    int     m_outlineSize;
    int     m_outlineAlpha;

    float   m_relativeSize;

    QString m_hash;

    bool    m_bFreeze;

    int     m_stretch;

    friend class FontMap;
};

class MUI_PUBLIC FontMap
{
  public:
    FontMap() {}

    MythFontProperties *GetFont(const QString &text);
    bool AddFont(const QString &text, MythFontProperties *fontProp);
    bool Contains(const QString &text);

    void Clear(void);
    void Rescale(int height = 0);

    static FontMap *GetGlobalFontMap(void);

  private:
    QMap<QString, MythFontProperties> m_FontMap;
};

MUI_PUBLIC FontMap *GetGlobalFontMap(void);


// FIXME: remove legacy crap
struct fontProp {
    QFont face;
    QPoint shadowOffset;
    QColor color;
    QColor dropColor;
};
extern MUI_PUBLIC QMap<QString, fontProp> globalFontMap;

#endif
