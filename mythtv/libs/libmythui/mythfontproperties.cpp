
#include "mythfontproperties.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QFontInfo>
#include <QRect>

#include "mythlogging.h"
#include "mythdb.h"

#include "mythuihelper.h"
#include "mythmainwindow.h"
#include "xmlparsebase.h"

#define LOC      QString("MythFontProperties: ")

MythFontProperties::MythFontProperties() :
    m_brush(QColor(Qt::white)), m_hasShadow(false), m_shadowAlpha(255),
    m_hasOutline(false), m_outlineAlpha(255), m_relativeSize(0.05f),
    m_bFreeze(false), m_stretch(100)
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
    m_brush.setColor(color);
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

void MythFontProperties::GetOffset(QPoint &offset) const
{
    offset = m_drawingOffset;
}

void MythFontProperties::CalcHash(void)
{
    if (m_bFreeze)
        return;

    m_hash = QString("%1%2%3%4").arg(m_face.toString())
                                .arg(m_brush.color().name())
                                .arg(m_hasShadow)
                                .arg(m_hasOutline);

    if (m_hasShadow)
        m_hash += QString("%1%2%3%4").arg(m_shadowOffset.x())
                 .arg(m_shadowOffset.y()).arg(m_shadowColor.name())
                 .arg(m_shadowAlpha);

    if (m_hasOutline)
        m_hash += QString("%1%2%3").arg(m_outlineColor.name())
                 .arg(m_outlineSize).arg(m_outlineAlpha);

    /* Note: drawingOffset is NOT used by the MythUIText class anymore.
     * MythuiText draws the base text where theme told it to, and
     * then expands the area (as needed) to draw the outline and/or shadow
     * around it.
     *
     * drawingOffset is only maintained for use by MythUISimpleText.
     *
     * 2012-01-25 JPP
     */
    m_drawingOffset = QPoint(0, 0);

    if (m_hasOutline)
    {
        m_drawingOffset = QPoint(m_outlineSize, m_outlineSize);
    }

    if (m_hasShadow && !m_hasOutline)
    {
        if (m_shadowOffset.x() < 0)
            m_drawingOffset.setX(-m_shadowOffset.x());
        if (m_shadowOffset.y() < 0)
            m_drawingOffset.setY(-m_shadowOffset.y());
    }
    if (m_hasShadow && m_hasOutline)
    {
        if (m_shadowOffset.x() < 0 && m_shadowOffset.x() < -m_outlineSize)
            m_drawingOffset.setX(-m_shadowOffset.x());
        if (m_shadowOffset.y() < 0 && m_shadowOffset.y() < -m_outlineSize)
            m_drawingOffset.setY(-m_shadowOffset.y());
    }
}

void MythFontProperties::Rescale(int height)
{
    m_face.setPixelSize(m_relativeSize * height);
}

void MythFontProperties::Rescale(void)
{
    QRect rect = GetMythMainWindow()->GetUIScreenRect();
    Rescale(rect.height());
}

void MythFontProperties::AdjustStretch(int stretch)
{
    int newStretch = (int)(((float)m_stretch * ((float)stretch / 100.0f)) + 0.5f);

    if (newStretch <= 0)
        newStretch = 1;

    m_face.setStretch(newStretch);
}

void MythFontProperties::SetPixelSize(float size)
{
    QSize baseSize = GetMythUI()->GetBaseSize();
    m_relativeSize = size / (float)(baseSize.height());
    m_face.setPixelSize(GetMythMainWindow()->NormY((int)(size + 0.5f)));
}

void MythFontProperties::SetPointSize(uint points)
{
    float pixels = (float)points / 72.0 * 100.0;
    SetPixelSize(pixels);
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

MythFontProperties *MythFontProperties::ParseFromXml(
    const QString &filename,
    const QDomElement &element,
    MythUIType *parent,
    bool addToGlobal,
    bool showWarnings)
{
    // Crappy, but cached.  Move to GlobalFontMap?

    bool fromBase = false;
    MythFontProperties *newFont = new MythFontProperties();
    newFont->Freeze();

    if (element.tagName() == "font")
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("File %1: Use of 'font' is deprecated in favour of "
                    "'fontdef'") .arg(filename));

    QString name = element.attribute("name", "");
    if (name.isEmpty())
    {
        VERBOSE_XML(VB_GENERAL, LOG_ERR,
                    filename, element, "Font requires a name");
        delete newFont;
        return NULL;
    }

    QString base = element.attribute("from", "");

    if (!base.isEmpty())
    {
        MythFontProperties *tmp = NULL;

        if (parent)
            tmp = parent->GetFont(base);

        if (!tmp)
            tmp = GetGlobalFontMap()->GetFont(base);

        if (!tmp)
        {
            VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                QString("Specified base font '%1' does not exist.").arg(base));

            delete newFont;
            return NULL;
        }

        *newFont = *tmp;
        fromBase = true;
    }

    int size, pixelsize;
    size = pixelsize = -1;

    QString face = element.attribute("face", "");
    if (face.isEmpty())
    {
        if (!fromBase)
        {
            VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                        "Font needs a face");
            delete newFont;
            return NULL;
        }
    }
    else
    {
        newFont->m_face.setFamily(face);
    }

    if (addToGlobal && GetGlobalFontMap()->Contains(name))
    {
        MythFontProperties *tmp = GetGlobalFontMap()->GetFont(name);
        if (showWarnings)
        {
            VERBOSE_XML(VB_GENERAL, LOG_WARNING, filename, element,
                QString("Attempting to define '%1'\n\t\t\t"
                        "with face '%2', but it already "
                        "exists with face '%3'")
                .arg(name).arg(QFontInfo(newFont->m_face).family())
                .arg((tmp) ? QFontInfo(tmp->m_face).family() : "ERROR"));
        }
        delete newFont;
        return NULL;
    }

    QString hint = element.attribute("stylehint", "");
    if (!hint.isEmpty())
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
            else if (info.tagName() == "pixelsize")
            {
                pixelsize = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                newFont->m_brush = QBrush(QColor(getFirstText(info)));
            }
            else if (info.tagName() == "gradient")
            {
                newFont->m_brush = parseGradient(info);
            }
            else if (info.tagName() == "shadowcolor")
            {
                newFont->m_shadowColor = QColor(getFirstText(info));
            }
            else if (info.tagName() == "shadowoffset")
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
            else if (info.tagName() == "italics")
            {
                newFont->m_face.setItalic(parseBool(info));
            }
            else if (info.tagName() == "letterspacing")
            {
                newFont->m_face.setLetterSpacing(QFont::AbsoluteSpacing,
                                              getFirstText(info).toInt());
            }
            else if (info.tagName() == "wordspacing")
            {
                newFont->m_face.setWordSpacing(getFirstText(info).toInt());
            }
            else if (info.tagName() == "decoration")
            {
                QString dec = getFirstText(info).toLower();
                QStringList values = dec.split(',');

                QStringList::Iterator it;
                for ( it = values.begin(); it != values.end(); ++it )
                {
                    if (*it == "underline")
                        newFont->m_face.setUnderline(true);
                    else if (*it == "overline")
                        newFont->m_face.setOverline(true);
                    else if (*it == "strikeout")
                        newFont->m_face.setStrikeOut(true);
                }
            }
            else if (info.tagName() == "weight")
            {
                QString weight = getFirstText(info).toLower();

                if (weight == "ultralight" ||
                    weight == "1")
                    newFont->m_face.setWeight(1);
                else if (weight == "light" ||
                         weight == "2")
                    newFont->m_face.setWeight(QFont::Light);
                else if (weight == "normal" ||
                         weight == "3")
                    newFont->m_face.setWeight(QFont::Normal);
                else if (weight == "demibold" ||
                         weight == "4")
                    newFont->m_face.setWeight(QFont::DemiBold);
                else if (weight == "bold" ||
                         weight == "5")
                    newFont->m_face.setWeight(QFont::Bold);
                else if (weight == "black" ||
                         weight == "6")
                    newFont->m_face.setWeight(QFont::Black);
                else if (weight == "ultrablack" ||
                         weight == "7")
                    newFont->m_face.setWeight(99);
                else
                    newFont->m_face.setWeight(QFont::Normal);
            }
            else if (info.tagName() == "stretch")
            {
                QString stretch = getFirstText(info).toLower();

                if (stretch == "ultracondensed" ||
                    stretch == "1")
                    newFont->m_stretch = QFont::UltraCondensed;
                else if (stretch == "extracondensed" ||
                         stretch == "2")
                    newFont->m_stretch = QFont::ExtraCondensed;
                else if (stretch == "condensed" ||
                         stretch == "3")
                    newFont->m_stretch = QFont::Condensed;
                else if (stretch == "semicondensed" ||
                         stretch == "4")
                    newFont->m_stretch = QFont::SemiCondensed;
                else if (stretch == "unstretched" ||
                         stretch == "5")
                    newFont->m_stretch = QFont::Unstretched;
                else if (stretch == "semiexpanded" ||
                         stretch == "6")
                    newFont->m_stretch = QFont::SemiExpanded;
                else if (stretch == "expanded" ||
                         stretch == "7")
                    newFont->m_stretch = QFont::Expanded;
                else if (stretch == "extraexpanded" ||
                         stretch == "8")
                    newFont->m_stretch = QFont::ExtraExpanded;
                else if (stretch == "ultraexpanded" ||
                         stretch == "9")
                    newFont->m_stretch = QFont::UltraExpanded;
                else
                    newFont->m_stretch = QFont::Unstretched;

                newFont->m_face.setStretch(newFont->m_stretch);
            }
            else
            {
                VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, info,
                            QString("Unknown tag in font '%1'").arg(name));
                delete newFont;
                return NULL;
            }
        }
    }

    if (size <= 0 && pixelsize <= 0 && !fromBase)
    {
        VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                    "Font size must be greater than 0.");
        delete newFont;
        return NULL;
    }
    else if (pixelsize > 0)
    {
        newFont->SetPixelSize(pixelsize);
    }
    else if (size > 0)
    {
        newFont->SetPointSize(size);
    }

    newFont->Unfreeze();

    QFontInfo fi(newFont->m_face);
    if (newFont->m_face.family() != fi.family())
    {
        VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                    QString("Failed to load '%1', got '%2' instead")
            .arg(newFont->m_face.family()).arg(fi.family()));
    }
    else
    {
        VERBOSE_XML(VB_GUI, LOG_DEBUG, filename, element,
                    QString("loaded '%1'").arg(fi.family()));
    }

    if (addToGlobal)
        GetGlobalFontMap()->AddFont(name, newFont);

    return newFont;
}

static FontMap *gFontMap = NULL;

// FIXME: remove
QMap<QString, fontProp> globalFontMap;

MythFontProperties *FontMap::GetFont(const QString &text)
{
    if (text.isEmpty())
        return NULL;

    if (m_FontMap.contains(text))
        return &(m_FontMap[text]);
    return NULL;
}

bool FontMap::AddFont(const QString &text, MythFontProperties *font)
{
    if (!font || text.isEmpty())
        return false;

    if (m_FontMap.contains(text))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Already have a font: %1").arg(text));
        return false;
    }

    m_FontMap[text] = *font;

    {
        /* FIXME backwards compat, remove */
        fontProp oldf;

        oldf.face = font->m_face;
        oldf.color = font->m_brush.color();
        if (font->m_hasShadow)
        {
            oldf.dropColor = font->m_shadowColor;
            oldf.shadowOffset = font->m_shadowOffset;
        }

        globalFontMap[text] = oldf;
    }

    return true;
}

bool FontMap::Contains(const QString &text)
{
    return m_FontMap.contains(text);
}

void FontMap::Clear(void)
{
    m_FontMap.clear();

    //FIXME: remove
    globalFontMap.clear();
}

void FontMap::Rescale(int height)
{
    if (height <= 0)
    {
        QRect rect = GetMythMainWindow()->GetUIScreenRect();
        height = rect.height();
    }

    QMap<QString, MythFontProperties>::iterator it;
    for (it = m_FontMap.begin(); it != m_FontMap.end(); ++it)
    {
        (*it).Rescale(height);
    }
}

FontMap *FontMap::GetGlobalFontMap(void)
{
    if (!gFontMap)
        gFontMap = new FontMap();
    return gFontMap;
}

FontMap *GetGlobalFontMap(void)
{
    return FontMap::GetGlobalFontMap();
}

