#include "mythcontext.h"
#include "mythfontproperties.h"
#include "mythmainwindow.h"

//FIXME remove, old crap
#include "uitypes.h"

MythFontProperties::MythFontProperties() :
    m_color(QColor(Qt::white)), m_hasShadow(false), m_shadowAlpha(255),
    m_hasOutline(false), m_outlineAlpha(255), m_bFreeze(false) 
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
    if (m_bFreeze)
        return;

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

void MythFontProperties::Freeze(void)
{
    m_bFreeze = true;
}

void MythFontProperties::Unfreeze(void)
{
    m_bFreeze = false;
    CalcHash();
}

MythFontProperties *MythFontProperties::ParseFromXml(QDomElement &element,
                                                     bool addToGlobal)
{
    // Crappy, but cached.  Move to GlobalFontMap?
    QString fontSizeType = gContext->GetSetting("ThemeFontSizeType", "default");

    bool fromBase = false;
    MythFontProperties *newFont = new MythFontProperties();
    newFont->Freeze();

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Font needs a name");
        return NULL;
    }

    if (addToGlobal && GetGlobalFontMap()->Contains(name))
    {
        VERBOSE(VB_IMPORTANT, QString("Error, already have a "
                                      "global font called: %1").arg(name));
        return NULL;
    }

    QString base = element.attribute("base", "");
    if (!base.isNull() && !base.isEmpty())
    {
        MythFontProperties *tmp = GetGlobalFontMap()->GetFont(base);
        if (!tmp)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Specified base font '%1' does not "
                            "exist for font %2").arg(base).arg(name));
            return NULL;
        }

        *newFont = *tmp;
        fromBase = true;
    }

    int size, sizeSmall, sizeBig;
    size = sizeSmall = sizeBig = -1;

    QString face = element.attribute("face", "");
    if (face.isNull() || face.isEmpty())
    {
        if (!fromBase)
        {
            VERBOSE(VB_IMPORTANT, "Font needs a face");
            return NULL;
        }
    }
    else
        newFont->m_face.setFamily(face);

    QString hint = element.attribute("stylehint", "");
    if (!hint.isNull() && !hint.isEmpty())
    {
        newFont->m_face.setStyleHint((QFont::StyleHint)hint.toInt());
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "size")
            {
                size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:small")
            {
                sizeSmall = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:big")
            {
                sizeBig = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                newFont->m_color = QColor(getFirstText(info));
            }
            else if (info.tagName() == "dropcolor" ||
                     info.tagName() == "shadowcolor")
            {
                newFont->m_shadowColor = QColor(getFirstText(info));
            }
            else if (info.tagName() == "shadow" ||
                     info.tagName() == "shadowoffset")
            {
                newFont->m_hasShadow = true;
                newFont->m_shadowOffset = parsePoint(info);
            }
            else if (info.tagName() == "shadowalpha")
            {
                newFont->m_shadowAlpha = getFirstText(info).toInt();
            }
            else if (info.tagName() == "outlinecolor")
            {
                newFont->m_outlineColor = QColor(getFirstText(info));
            }
            else if (info.tagName() == "outlinesize")
            {
                newFont->m_hasOutline = true;
                newFont->m_outlineSize = getFirstText(info).toInt();
            }
            else if (info.tagName() == "outlinealpha")
            {
                newFont->m_outlineAlpha = getFirstText(info).toInt();
            }
            else if (info.tagName() == "bold")
            {
                newFont->m_face.setBold(parseBool(info));
            }
            else if (info.tagName() == "italics")
            {
                newFont->m_face.setItalic(parseBool(info));
            }
            else if (info.tagName() == "underline")
            {
                newFont->m_face.setUnderline(parseBool(info));
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown tag %1 in font")
                                              .arg(info.tagName()));
                return NULL;
            }
        }
    }

    if (sizeSmall > 0 && fontSizeType == "small")
    {
        size = sizeSmall;
    }
    else if (sizeBig > 0 && fontSizeType == "big")
    {
        size = sizeBig;
    }

    if (size <= 0 && !fromBase)
    {
        VERBOSE(VB_IMPORTANT, "Error, font size must be > 0");
        return NULL;
    }
    else if (size > 0)
        newFont->m_face.setPointSize(GetMythMainWindow()->NormalizeFontSize(size));    

    newFont->Unfreeze();

    if (addToGlobal)
    {
        GetGlobalFontMap()->AddFont(name, newFont);
    }

    return newFont;
}

static GlobalFontMap *gFontMap = NULL;

// FIXME: remove
extern QMap<QString, fontProp> globalFontMap;

MythFontProperties *GlobalFontMap::GetFont(const QString &text)
{
    if (text.isEmpty())
        return NULL;

    if (m_globalFontMap.contains(text))
        return &(m_globalFontMap[text]);
    return NULL;
}

bool GlobalFontMap::AddFont(const QString &text, MythFontProperties *font)
{
    if (!font || text.isEmpty())
        return false;

    if (m_globalFontMap.contains(text))
    {
        VERBOSE(VB_IMPORTANT, QString("Already have a font: %1").arg(text));
        return false;
    }

    m_globalFontMap[text] = *font;

    {
        /* FIXME backwards compat, remove */
        fontProp oldf;

        oldf.face = font->m_face;
        oldf.color = font->m_color;
        if (font->m_hasShadow)
        {
            oldf.dropColor = font->m_shadowColor;
            oldf.shadowOffset = font->m_shadowOffset;
        }

        globalFontMap[text] = oldf;
    }

    return true;
}

bool GlobalFontMap::Contains(const QString &text)
{
    return m_globalFontMap.contains(text);
}

void GlobalFontMap::Clear(void)
{
    m_globalFontMap.clear();

    //FIXME: remove
    globalFontMap.clear();
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

