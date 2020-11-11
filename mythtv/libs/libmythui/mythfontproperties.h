#ifndef MYTHFONTPROPERTIES_H_
#define MYTHFONTPROPERTIES_H_

#include <QFont>
#include <QColor>
#include <QBrush>
#include <QPoint>
#include <QMap>
#include <QMutex>

#include "xmlparsebase.h"

class MUI_PUBLIC MythFontProperties: public XMLParseBase
{
  public:
    MythFontProperties(void);

    QFont* GetFace(void) { return &m_face; }

    void SetFace(const QFont &face);
    void SetColor(const QColor &color);
    void SetShadow(bool on, QPoint offset, const QColor &color, int alpha);
    void SetOutline(bool on, const QColor &color, int size, int alpha);

    QFont face(void) const;
    QColor color(void) const { return m_brush.color(); }
    QBrush GetBrush(void) const { return m_brush; }

    bool hasShadow(void) const { return m_hasShadow; }
    void GetShadow(QPoint &offset, QColor &color, int &alpha) const;

    bool hasOutline(void) const { return m_hasOutline; }
    void GetOutline(QColor &color, int &size, int &alpha) const;

    QString GetHash(void) const { return m_hash; }

    static MythFontProperties *ParseFromXml(
        const QString &filename, const QDomElement &element,
        MythUIType *parent = nullptr, bool addToGlobal = false,
        bool showWarnings = true);

    void SetRelativeSize(float rSize) { m_relativeSize = rSize; }
    float GetRelativeSize(void) const { return m_relativeSize; }
    void SetPixelSize(float size);
    void SetPointSize(uint points);
    void Rescale(void);
    void Rescale(int height);
    void AdjustStretch(int stretch);

    static void Zoom(void);
    static void SetZoom(uint zoom_percent);

  private:
    void Freeze(void); // no hash updates
    void Unfreeze(void);

    void CalcHash(void);

    QFont   m_face;
    QBrush  m_brush         {Qt::white};

    bool    m_hasShadow     {false};
    QPoint  m_shadowOffset;
    QColor  m_shadowColor;
    int     m_shadowAlpha   {255};

    bool    m_hasOutline    {false};
    QColor  m_outlineColor;
    int     m_outlineSize   {0};
    int     m_outlineAlpha  {255};

    float   m_relativeSize  {0.5F};

    QString m_hash;

    bool    m_bFreeze       {false};

    int     m_stretch       {100};

    static QMutex s_zoomLock;
    static uint   s_zoomPercent;

    friend class FontMap;
};

class MUI_PUBLIC FontMap
{
  public:
    FontMap() = default;

    MythFontProperties *GetFont(const QString &text);
    bool AddFont(const QString &text, MythFontProperties *fontProp);
    bool Contains(const QString &text);

    void Clear(void);
    void Rescale(int height = 0);

    static FontMap *GetGlobalFontMap(void);

  private:
    QMap<QString, MythFontProperties> m_fontMap;
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
