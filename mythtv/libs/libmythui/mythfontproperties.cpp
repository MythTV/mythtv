#include "mythcontext.h"

#include "mythfontproperties.h"

MythFontProperties::MythFontProperties() :
    m_color(QColor(Qt::white)), m_hasShadow(false), m_shadowAlpha(255),
    m_hasOutline(false), m_outlineAlpha(255) 
{ 
    CalcHash();
}

void MythFontProperties::SetFace(const QFont &face)
{
    m_face = face;
    CalcHash();
}

void MythFontProperties::SetColor(const QColor &color)
{
    m_color = color;
    CalcHash();
}

void MythFontProperties::SetShadow(bool on, const QPoint &offset, 
                                   const QColor &color, int alpha)
{
    m_hasShadow = on;
    m_shadowOffset = offset;
    m_shadowColor = color;
    m_shadowAlpha = alpha;
    CalcHash();
}

void MythFontProperties::SetOutline(bool on, const QColor &color, 
                                    int size, int alpha)
{
    m_hasOutline = on;
    m_outlineColor = color;
    m_outlineSize = size;
    m_outlineAlpha = alpha;
    CalcHash();
}

void MythFontProperties::GetShadow(QPoint &offset, QColor &color, int &alpha) const
{
    offset = m_shadowOffset;
    color = m_shadowColor;
    alpha = m_shadowAlpha;
}

void MythFontProperties::GetOutline(QColor &color, int &size, int &alpha) const
{
    color = m_outlineColor;
    size = m_outlineSize;
    alpha = m_outlineAlpha;
}

void MythFontProperties::CalcHash(void)
{
    m_hash = QString("%1%2%3%4").arg(m_face.toString())
                 .arg(m_color.name()).arg(m_hasShadow).arg(m_hasOutline);

    if (m_hasShadow)
        m_hash += QString("%1%2%3%4").arg(m_shadowOffset.x())
                 .arg(m_shadowOffset.y()).arg(m_shadowColor.name())
                 .arg(m_shadowAlpha);

    if (m_hasOutline)
        m_hash += QString("%1%2%3").arg(m_outlineColor.name())
                 .arg(m_outlineSize).arg(m_outlineAlpha);
}

static GlobalFontMap *gFontMap = NULL;

MythFontProperties *GlobalFontMap::GetFont(const QString &text)
{
    if (text.isEmpty())
        return NULL;

    if (m_globalFontMap.contains(text))
        return &(m_globalFontMap[text]);
    return NULL;
}

bool GlobalFontMap::AddFont(const QString &text, MythFontProperties *fontProp)
{
    if (!fontProp || text.isEmpty())
        return false;

    if (m_globalFontMap.contains(text))
    {
        VERBOSE(VB_IMPORTANT, QString("Already have a font: %1").arg(text));
        return false;
    }

    m_globalFontMap[text] = *fontProp;
    return true;
}

void GlobalFontMap::Clear(void)
{
    m_globalFontMap.clear();
}

GlobalFontMap *GlobalFontMap::GetGlobalFontMap(void)
{
    if (!gFontMap)
        gFontMap = new GlobalFontMap();
    return gFontMap;
}

GlobalFontMap *GetGlobalFontMap(void)
{
    return GlobalFontMap::GetGlobalFontMap();
}

