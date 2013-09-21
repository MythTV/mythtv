#include <QFontMetrics>

#include "mythlogging.h"
#include "mythfontproperties.h"
#include "mythuisimpletext.h"
#include "mythuishape.h"
#include "mythuiimage.h"
#include "mythpainter.h"
#include "subtitlescreen.h"

#define LOC      QString("Subtitles: ")
#define LOC_WARN QString("Subtitles Warning: ")

// Class SubWrapper is used to attach caching structures to MythUIType
// objects in the SubtitleScreen's m_ChildrenList.  If these objects
// inherit from SubWrapper, they can carry relevant information and an
// external caching object is not required.
//
// The following caching information is represented here:
//   1. Original m_Area, used for OptimiseDisplayedArea().
//   2. Which cc708 window it belongs to, used for Clear708Cache().
//   3. Subtitle expire time, primarily for AV subtitles.
class SubWrapper
{
protected:
    SubWrapper(const MythRect &rect,
               long long expireTime,
               int whichImageCache = -1) :
        m_swOrigArea(rect),
        m_swWhichImageCache(whichImageCache),
        m_swExpireTime(expireTime)
    {
    }
public:
    long long GetExpireTime(void) const { return m_swExpireTime; }
    MythRect GetOrigArea(void) const { return m_swOrigArea; }
    int GetWhichImageCache(void) const { return m_swWhichImageCache; }
protected:
    // Returns true if the object was deleted.
    const MythRect m_swOrigArea;
    const int m_swWhichImageCache; // cc708 only; -1 = none
    const long long m_swExpireTime; // avsubs only; -1 = none
};

class SubSimpleText : public MythUISimpleText, public SubWrapper
{
public:
    SubSimpleText(const QString &text, const MythFontProperties &font,
                  const QRect &rect, Qt::Alignment align,
                  MythUIType *parent, const QString &name,
                  int whichImageCache, long long expireTime) :
        MythUISimpleText(text, font, rect, align, parent, name),
        SubWrapper(rect, expireTime, whichImageCache) {}
};

class SubShape : public MythUIShape, public SubWrapper
{
public:
    SubShape(MythUIType *parent, const QString &name, const MythRect &area,
             int whichImageCache, long long expireTime) :
        MythUIShape(parent, name),
        SubWrapper(area, expireTime, whichImageCache) {}
};

class SubImage : public MythUIImage, public SubWrapper
{
public:
    SubImage(MythUIType *parent, const QString &name, const MythRect &area,
             long long expireTime) :
        MythUIImage(parent, name),
        SubWrapper(area, expireTime) {}
    MythImage *GetImage(void) { return m_Images[0]; }
};

////////////////////////////////////////////////////////////////////////////

// Class SubtitleFormat manages fonts and backgrounds for subtitles.
//
// Formatting is specified by the theme in the new file
// osd_subtitle.xml, in the osd_subtitle window.  Subtitle types are
// text, teletext, 608, 708_0, 708_1, ..., 708_7.  Each subtitle type
// has a fontdef component for the text and a shape component for the
// background.  By default, the attributes of a subtitle type come
// from either the provider (e.g. font color, italics) or the system
// default (e.g. font family).  These attributes can be overridden by
// the xml file.
//
// The fontdef name and the background shape name are both simply the
// name of the subtitle type.  The fontdef and shape should ultimately
// inherit from the special value "provider", otherwise all provider
// values will be ignored.
//
// The following example forces .srt subtitles to be rendered in
// yellow text, FreeSans font, black shadow, with a translucent black
// background.  The fontdef and shape names are "text" since the
// subtitles come from a .srt file.  Note that in this example, color
// formatting controls in the .srt file will be ignored due to the
// explicit setting of yellow text.
//
// <?xml version="1.0" encoding="utf-8"?>
// <!DOCTYPE mythuitheme SYSTEM "http://www.mythtv.org/schema/mythuitheme.dtd">
// <mythuitheme>
//   <window name="osd_subtitle">
//     <fontdef name="text" face="FreeSans" from="provider">
//       <color>#FFFF00</color>
//       <shadowoffset>2,2</shadowoffset>
//       <shadowcolor>#000000</shadowcolor>
//     </fontdef>
//     <shape name="text" from="provider">
//       <fill color="#000000" alpha="100" />
//     </shape>
//   </window>
// </mythuitheme>
//
// All elements/attributes of fontdef and shape can be used.  Note
// however that area and position are explicitly ignored, as these are
// dictated by the subtitle layout.
//
// This is implemented with almost no MythUI changes.  Two copies of a
// "provider" object are created, with default/representative provider
// attributes.  One copy is then "complemented" to have a different
// value for each attribute that a provider might change.  The
// osd_subtitle.xml file is loaded twice, once with respect to each
// provider object, and the desired fontdef or shape is looked up.
// The two fontdefs or shapes are compared attribute by attribute, and
// each attribute that is different is an attribute that the provider
// may modify, whereas each identical attribute represents one that is
// fixed by the theme.

class SubtitleFormat
{
public:
    SubtitleFormat(void) {}
    ~SubtitleFormat(void);
    MythFontProperties *GetFont(const QString &family,
                                const CC708CharacterAttribute &attr,
                                int pixelSize, int zoom, int stretch);
    SubShape *GetBackground(MythUIType *parent, const QString &name,
                            const QString &family,
                            const CC708CharacterAttribute &attr,
                            const MythRect &area,
                            int whichImageCache,
                            int start, int duration);
    static QString MakePrefix(const QString &family,
                              const CC708CharacterAttribute &attr);
private:
    void Load(const QString &family,
              const CC708CharacterAttribute &attr);
    bool IsUnlocked(const QString &prefix, const QString &property) const
    {
        return m_changeMap[prefix].contains(property);
    }
    static void CreateProviderDefault(const QString &family,
                                      const CC708CharacterAttribute &attr,
                                      MythUIType *parent,
                                      bool isComplement,
                                      MythFontProperties **font,
                                      MythUIShape **bg);
    static void Complement(MythFontProperties *font, MythUIShape *bg);
    static QSet<QString> Diff(const QString &family,
                              const CC708CharacterAttribute &attr,
                              MythFontProperties *font1,
                              MythFontProperties *font2,
                              MythUIShape *bg1,
                              MythUIShape *bg2);

    QHash<QString, MythFontProperties *> m_fontMap;
    QHash<QString, MythUIShape *>       m_shapeMap;
    QHash<QString, QSet<QString> >     m_changeMap;
    // The following m_*Map fields are the original values from the
    // m_fontMap, which are preserved because the text font zoom
    // factor affects the actual values stored in the font.
    QHash<QString, int>             m_pixelSizeMap;
    QHash<QString, int>           m_outlineSizeMap;
    QHash<QString, QPoint>       m_shadowOffsetMap;
    QVector<MythUIType *> m_cleanup;
};

static const QString kSubProvider("provider");
static const QString kSubFileName  ("osd_subtitle.xml");
static const QString kSubWindowName("osd_subtitle");
static const QString kSubFamily608     ("608");
static const QString kSubFamily708     ("708");
static const QString kSubFamilyText    ("text");
static const QString kSubFamilyAV      ("AV");
static const QString kSubFamilyTeletext("teletext");

static const QString kSubAttrItalics  ("italics");
static const QString kSubAttrBold     ("bold");
static const QString kSubAttrUnderline("underline");
static const QString kSubAttrPixelsize("pixelsize");
static const QString kSubAttrColor    ("color");
static const QString kSubAttrBGfill   ("bgfill");
static const QString kSubAttrShadow      ("shadow"); // unused
static const QString kSubAttrShadowoffset("shadowoffset");
static const QString kSubAttrShadowcolor ("shadowcolor");
static const QString kSubAttrShadowalpha ("shadowalpha");
static const QString kSubAttrOutline     ("outline"); // unused
static const QString kSubAttrOutlinecolor("outlinecolor");
static const QString kSubAttrOutlinesize ("outlinesize");
static const QString kSubAttrOutlinealpha("outlinealpha");

static QString srtColorString(QColor color)
{
    return QString("#%1%2%3")
        .arg(color.red(),   2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(),  2, 16, QLatin1Char('0'));
}

static QString fontToString(MythFontProperties *f)
{
    QString result;
    result = QString("face=%1 pixelsize=%2 color=%3 "
                     "italics=%4 weight=%5 underline=%6")
        .arg(f->GetFace()->family())
        .arg(f->GetFace()->pixelSize())
        .arg(srtColorString(f->color()))
        .arg(f->GetFace()->italic())
        .arg(f->GetFace()->weight())
        .arg(f->GetFace()->underline());
    QPoint offset;
    QColor color;
    int alpha;
    int size;
    f->GetShadow(offset, color, alpha);
    result += QString(" shadow=%1 shadowoffset=%2 "
                      "shadowcolor=%3 shadowalpha=%4")
        .arg(f->hasShadow())
        .arg(QString("(%1,%2)").arg(offset.x()).arg(offset.y()))
        .arg(srtColorString(color))
        .arg(alpha);
    f->GetOutline(color, size, alpha);
    result += QString(" outline=%1 outlinecolor=%2 "
                      "outlinesize=%3 outlinealpha=%4")
        .arg(f->hasOutline())
        .arg(srtColorString(color))
        .arg(size)
        .arg(alpha);
    return result;
}

MythFontProperties *
SubtitleFormat::GetFont(const QString &family,
                        const CC708CharacterAttribute &attr,
                        int pixelSize, int zoom, int stretch)
{
    int origPixelSize = pixelSize;
    float scale = zoom / 100.0;
    if ((attr.pen_size & 0x3) == k708AttrSizeSmall)
        scale = scale * 32 / 42;
    else if ((attr.pen_size & 0x3) == k708AttrSizeLarge)
        scale = scale * 42 / 32;

    QString prefix = MakePrefix(family, attr);
    if (!m_fontMap.contains(prefix))
        Load(family, attr);
    MythFontProperties *result = m_fontMap[prefix];

    // Apply the scaling factor to pixelSize even if the theme
    // explicitly sets pixelSize.
    if (!IsUnlocked(prefix, kSubAttrPixelsize))
        pixelSize = m_pixelSizeMap[prefix];
    pixelSize *= scale;
    result->GetFace()->setPixelSize(pixelSize);

    result->GetFace()->setStretch(stretch);
    if (IsUnlocked(prefix, kSubAttrItalics))
        result->GetFace()->setItalic(attr.italics);
    if (IsUnlocked(prefix, kSubAttrUnderline))
        result->GetFace()->setUnderline(attr.underline);
    if (IsUnlocked(prefix, kSubAttrBold))
        result->GetFace()->setBold(attr.boldface);
    if (IsUnlocked(prefix, kSubAttrColor))
        result->SetColor(attr.GetFGColor());

    MythPoint offset;
    QColor color;
    int alpha;
    bool shadow = result->hasShadow();
    result->GetShadow(offset, color, alpha);
    if (IsUnlocked(prefix, kSubAttrShadowcolor))
        color = attr.GetEdgeColor();
    if (IsUnlocked(prefix, kSubAttrShadowalpha))
        alpha = attr.GetFGAlpha();
    if (IsUnlocked(prefix, kSubAttrShadowoffset))
    {
        int off = scale * pixelSize / 20 + 0.5;
        offset = QPoint(off, off);
        if (attr.edge_type == k708AttrEdgeLeftDropShadow)
        {
            shadow = true;
            offset.setX(-off);
        }
        else if (attr.edge_type == k708AttrEdgeRightDropShadow)
            shadow = true;
        else
            shadow = false;
    }
    else
    {
        offset = m_shadowOffsetMap[prefix];
        offset.NormPoint();
        offset.setX(offset.x() * scale + 0.5);
        offset.setY(offset.y() * scale + 0.5);
    }
    result->SetShadow(shadow, offset, color, alpha);

    int off;
    bool outline = result->hasOutline();
    result->GetOutline(color, off, alpha);
    if (IsUnlocked(prefix, kSubAttrOutlinecolor))
        color = attr.GetEdgeColor();
    if (IsUnlocked(prefix, kSubAttrOutlinealpha))
        alpha = attr.GetFGAlpha();
    if (IsUnlocked(prefix, kSubAttrOutlinesize))
    {
        if (attr.edge_type == k708AttrEdgeUniform ||
            attr.edge_type == k708AttrEdgeRaised  ||
            attr.edge_type == k708AttrEdgeDepressed)
        {
            outline = true;
            off = scale * pixelSize / 20 + 0.5;
        }
        else
            outline = false;
    }
    else
    {
        off = m_outlineSizeMap[prefix];
        MythPoint point(off, off);
        point.NormPoint();
        off = point.x() * scale + 0.5;
    }
    result->SetOutline(outline, color, off, alpha);

    LOG(VB_VBI, LOG_DEBUG,
        QString("GetFont(family=%1, prefix=%2, orig pixelSize=%3, "
                "new pixelSize=%4 zoom=%5) = %6")
        .arg(family).arg(prefix).arg(origPixelSize).arg(pixelSize)
        .arg(zoom).arg(fontToString(result)));
    return result;
}

SubtitleFormat::~SubtitleFormat(void)
{
    for (int i = 0; i < m_cleanup.size(); ++i)
    {
        m_cleanup[i]->DeleteAllChildren();
        m_cleanup[i]->deleteLater();
        m_cleanup[i] = NULL; // just to be safe
    }
}

QString SubtitleFormat::MakePrefix(const QString &family,
                                   const CC708CharacterAttribute &attr)
{
    if (family == kSubFamily708)
        return family + "_" + QString::number(attr.font_tag & 0x7);
    else
        return family;
}

void SubtitleFormat::CreateProviderDefault(const QString &family,
                                           const CC708CharacterAttribute &attr,
                                           MythUIType *parent,
                                           bool isComplement,
                                           MythFontProperties **returnFont,
                                           MythUIShape **returnBg)
{
    MythFontProperties *font = new MythFontProperties();
    MythUIShape *bg = new MythUIShape(parent, kSubProvider);
    if (family == kSubFamily608)
    {
        font->GetFace()->setFamily("FreeMono");
        bg->SetFillBrush(QBrush(Qt::black));
    }
    else if (family == kSubFamily708)
    {
        static const char *cc708Fonts[] = {
            "FreeMono",        // default
            "FreeMono",        // mono serif
            "Droid Serif",     // prop serif
            "Droid Sans Mono", // mono sans
            "Droid Sans",      // prop sans
            "Purisa",          // casual
            "TeX Gyre Chorus", // cursive
            "Droid Serif"      // small caps, QFont::SmallCaps will be applied
        };
        font->GetFace()->setFamily(cc708Fonts[attr.font_tag & 0x7]);
    }
    else if (family == kSubFamilyText)
    {
        font->GetFace()->setFamily("Droid Sans");
        bg->SetFillBrush(QBrush(Qt::black));
    }
    else if (family == kSubFamilyTeletext)
    {
        font->GetFace()->setFamily("FreeMono");
    }
    font->GetFace()->setPixelSize(10);

    if (isComplement)
        Complement(font, bg);
    parent->AddFont(kSubProvider, font);

    *returnFont = font;
    *returnBg = bg;
}

static QColor differentColor(const QColor &color)
{
    return color == Qt::white ? Qt::black : Qt::white;
}

// Change everything (with respect to the == operator) that the
// provider might define or override.
void SubtitleFormat::Complement(MythFontProperties *font, MythUIShape *bg)
{
    QPoint offset;
    QColor color;
    int alpha;
    int size;
    QFont *face = font->GetFace();
    face->setItalic(!face->italic());
    face->setPixelSize(face->pixelSize() + 1);
    face->setUnderline(!face->underline());
    face->setWeight((face->weight() + 1) % 32);
    font->SetColor(differentColor(font->color()));

    font->GetShadow(offset, color, alpha);
    offset.setX(offset.x() + 1);
    font->SetShadow(!font->hasShadow(), offset, differentColor(color),
                    255 - alpha);

    font->GetOutline(color, size, alpha);
    font->SetOutline(!font->hasOutline(), differentColor(color),
                     size + 1, 255 - alpha);

    bg->SetFillBrush(bg->m_fillBrush == Qt::NoBrush ?
                     Qt::SolidPattern : Qt::NoBrush);
}

void SubtitleFormat::Load(const QString &family,
                          const CC708CharacterAttribute &attr)
{
    // Widgets for the actual values
    MythUIType *baseParent = new MythUIType(NULL, "base");
    m_cleanup += baseParent;
    MythFontProperties *providerBaseFont;
    MythUIShape *providerBaseShape;
    CreateProviderDefault(family, attr, baseParent, false,
                          &providerBaseFont, &providerBaseShape);

    // Widgets for the "negative" values
    MythUIType *negParent = new MythUIType(NULL, "base");
    m_cleanup += negParent;
    MythFontProperties *negFont;
    MythUIShape *negBG;
    CreateProviderDefault(family, attr, negParent, true, &negFont, &negBG);

    bool posResult =
        XMLParseBase::LoadWindowFromXML(kSubFileName, kSubWindowName,
                                        baseParent);
    bool negResult =
        XMLParseBase::LoadWindowFromXML(kSubFileName, kSubWindowName,
                                        negParent);
    if (!posResult || !negResult)
        LOG(VB_VBI, LOG_INFO,
            QString("Couldn't load theme file %1").arg(kSubFileName));
    QString prefix = MakePrefix(family, attr);
    MythFontProperties *resultFont = baseParent->GetFont(prefix);
    if (!resultFont)
        resultFont = providerBaseFont;
    MythUIShape *resultBG =
        dynamic_cast<MythUIShape *>(baseParent->GetChild(prefix));
    if (!resultBG)
        resultBG = providerBaseShape;
    MythFontProperties *testFont = negParent->GetFont(prefix);
    if (!testFont)
        testFont = negFont;
    MythUIShape *testBG =
        dynamic_cast<MythUIShape *>(negParent->GetChild(prefix));
    if (!testBG)
        testBG = negBG;
    if (family == kSubFamily708 &&
        (attr.font_tag & 0x7) == k708AttrFontSmallCaps)
        resultFont->GetFace()->setCapitalization(QFont::SmallCaps);
    m_fontMap[prefix] = resultFont;
    m_shapeMap[prefix] = resultBG;
    LOG(VB_VBI, LOG_DEBUG,
        QString("providerBaseFont = %1").arg(fontToString(providerBaseFont)));
    LOG(VB_VBI, LOG_DEBUG,
        QString("negFont = %1").arg(fontToString(negFont)));
    LOG(VB_VBI, LOG_DEBUG,
        QString("resultFont = %1").arg(fontToString(resultFont)));
    LOG(VB_VBI, LOG_DEBUG,
        QString("testFont = %1").arg(fontToString(testFont)));
    m_changeMap[prefix] = Diff(family, attr, resultFont, testFont,
                               resultBG, testBG);
    QPoint offset;
    QColor color;
    int alpha;
    int size;
    resultFont->GetShadow(offset, color, alpha);
    resultFont->GetOutline(color, size, alpha);
    m_outlineSizeMap[prefix] = size;
    m_shadowOffsetMap[prefix] = offset;
    m_pixelSizeMap[prefix] = resultFont->GetFace()->pixelSize();

    delete negFont;
}

QSet<QString> SubtitleFormat::Diff(const QString &family,
                                   const CC708CharacterAttribute &attr,
                                   MythFontProperties *font1,
                                   MythFontProperties *font2,
                                   MythUIShape *bg1,
                                   MythUIShape *bg2)
{
    bool is708 = (family == kSubFamily708);
    QSet<QString> result;
    QFont *face1 = font1->GetFace();
    QFont *face2 = font2->GetFace();
    if (face1->italic() != face2->italic())
        result << kSubAttrItalics;
    if (face1->weight() != face2->weight())
        result << kSubAttrBold;
    if (face1->underline() != face2->underline())
        result << kSubAttrUnderline;
    if (face1->pixelSize() != face2->pixelSize())
        result << kSubAttrPixelsize;
    if (font1->color() != font2->color())
        result << kSubAttrColor;
    if (is708 && font1->hasShadow() != font2->hasShadow())
    {
        result << kSubAttrShadow;
        QPoint offset1, offset2;
        QColor color1, color2;
        int alpha1, alpha2;
        font1->GetShadow(offset1, color1, alpha1);
        font2->GetShadow(offset2, color2, alpha2);
        if (offset1 != offset2)
            result << kSubAttrShadowoffset;
        if (color1 != color2)
            result << kSubAttrShadowcolor;
        if (alpha1 != alpha2)
            result << kSubAttrShadowalpha;
    }
    if (is708 && font1->hasOutline() != font2->hasOutline())
    {
        result << kSubAttrOutline;
        QColor color1, color2;
        int size1, size2;
        int alpha1, alpha2;
        font1->GetOutline(color1, size1, alpha1);
        font2->GetOutline(color2, size2, alpha2);
        if (color1 != color2)
            result << kSubAttrOutlinecolor;
        if (size1 != size2)
            result << kSubAttrOutlinesize;
        if (alpha1 != alpha2)
            result << kSubAttrOutlinealpha;
    }
    if (bg1->m_fillBrush != bg2->m_fillBrush)
        result << kSubAttrBGfill;

    QString values = "";
    QSet<QString>::const_iterator i = result.constBegin();
    for (; i != result.constEnd(); ++i)
        values += " " + (*i);
    LOG(VB_VBI, LOG_INFO,
        QString("Subtitle family %1 allows provider to change:%2")
        .arg(MakePrefix(family, attr)).arg(values));

    return result;
}

SubShape *
SubtitleFormat::GetBackground(MythUIType *parent, const QString &name,
                              const QString &family,
                              const CC708CharacterAttribute &attr,
                              const MythRect &area,
                              int whichImageCache,
                              int start,
                              int duration)
{
    QString prefix = MakePrefix(family, attr);
    if (!m_shapeMap.contains(prefix))
        Load(family, attr);
    if (m_shapeMap[prefix]->GetAlpha() == 0)
        return NULL;
    SubShape *result = new SubShape(parent, name, area, whichImageCache,
                                    start + duration);
    result->CopyFrom(m_shapeMap[prefix]);
    if (family == kSubFamily708)
    {
        if (IsUnlocked(prefix, kSubAttrBGfill))
        {
            result->SetFillBrush(QBrush(attr.GetBGColor()));
        }
    }
    else if (family == kSubFamilyTeletext)
    {
        // add code here when teletextscreen.cpp is refactored
    }
    LOG(VB_VBI, LOG_DEBUG,
        QString("GetBackground(prefix=%1) = "
                "{type=%2 alpha=%3 brushstyle=%4 brushcolor=%5}")
        .arg(prefix).arg(result->m_type).arg(result->GetAlpha())
        .arg(result->m_fillBrush.style())
        .arg(srtColorString(result->m_fillBrush.color())));
    return result;
}

////////////////////////////////////////////////////////////////////////////

QSize FormattedTextChunk::CalcSize(float layoutSpacing) const
{
    return parent->CalcTextSize(text, m_format, layoutSpacing);
}

void FormattedTextChunk::CalcPadding(bool isFirst, bool isLast,
                                     int &left, int &right) const
{
    parent->CalcPadding(m_format, isFirst, isLast, left, right);
}

bool FormattedTextChunk::Split(FormattedTextChunk &newChunk)
{
    LOG(VB_VBI, LOG_INFO,
        QString("Attempting to split chunk '%1'").arg(text));
    int lastSpace = text.lastIndexOf(' ', -2); // -2 to ignore trailing space
    if (lastSpace < 0)
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Failed to split chunk '%1'").arg(text));
        return false;
    }
    newChunk.parent = parent;
    newChunk.m_format = m_format;
    newChunk.text = text.mid(lastSpace + 1).trimmed() + ' ';
    text = text.left(lastSpace).trimmed();
    LOG(VB_VBI, LOG_INFO,
        QString("Split chunk into '%1' + '%2'").arg(text).arg(newChunk.text));
    return true;
}

QString FormattedTextChunk::ToLogString(void) const
{
    QString str("");
    str += QString("fg=%1.%2 ")
        .arg(srtColorString(m_format.GetFGColor()))
        .arg(m_format.GetFGAlpha());
    str += QString("bg=%1.%2 ")
        .arg(srtColorString(m_format.GetBGColor()))
        .arg(m_format.GetBGAlpha());
    str += QString("edge=%1.%2 ")
        .arg(srtColorString(m_format.GetEdgeColor()))
        .arg(m_format.edge_type);
    str += QString("off=%1 pensize=%2 ")
        .arg(m_format.offset)
        .arg(m_format.pen_size);
    str += QString("it=%1 ul=%2 bf=%3 ")
        .arg(m_format.italics)
        .arg(m_format.underline)
        .arg(m_format.boldface);
    str += QString("font=%1 ").arg(m_format.font_tag);
    str += QString(" text='%1'").arg(text);
    return str;
}

bool FormattedTextChunk::PreRender(bool isFirst, bool isLast,
                                   int &x, int y, int height)
{
    textFont = parent->GetFont(m_format);
    if (!textFont)
        return false;
    QFontMetrics font(*(textFont->GetFace()));
    // If the chunk starts with whitespace, the leading whitespace
    // ultimately gets lost due to the text.trimmed() operation in the
    // MythUISimpleText constructor.  To compensate, we manually
    // indent the chunk accordingly.
    int count = 0;
    while (count < text.length() && text.at(count) == ' ')
    {
        ++count;
    }
    int x_adjust = count * font.width(" ");
    int leftPadding, rightPadding;
    CalcPadding(isFirst, isLast, leftPadding, rightPadding);
    // Account for extra padding before the first chunk.
    if (isFirst)
        x += leftPadding;
    QSize chunk_sz = CalcSize();
    QRect bgrect(x - leftPadding, y,
                 chunk_sz.width() + leftPadding + rightPadding,
                 height);
    // Don't draw a background behind leading spaces.
    if (isFirst)
        bgrect.setLeft(bgrect.left() + x_adjust);
    bgShapeName = QString("subbg%1x%2@%3,%4")
        .arg(chunk_sz.width()).arg(height).arg(x).arg(y);
    bgShapeRect = bgrect;
    // Shift to the right to account for leading spaces that
    // are removed by the MythUISimpleText constructor.  Also
    // add in padding at the end to avoid clipping.
    textName = QString("subtxt%1x%2@%3,%4")
        .arg(chunk_sz.width()).arg(height).arg(x).arg(y);
    textRect = QRect(x + x_adjust, y,
                     chunk_sz.width() - x_adjust + rightPadding, height);

    x += chunk_sz.width();
    return true;
}

QSize FormattedTextLine::CalcSize(float layoutSpacing) const
{
    int height = 0, width = 0;
    int leftPadding = 0, rightPadding = 0;
    QList<FormattedTextChunk>::const_iterator it;
    bool isFirst = true;
    for (it = chunks.constBegin(); it != chunks.constEnd(); ++it)
    {
        bool isLast = (it + 1 == chunks.constEnd());
        QSize tmp = (*it).CalcSize(layoutSpacing);
        height = max(height, tmp.height());
        width += tmp.width();
        (*it).CalcPadding(isFirst, isLast, leftPadding, rightPadding);
        if (it == chunks.constBegin())
            width += leftPadding;
        isFirst = false;
    }
    return QSize(width + rightPadding, height);
}

FormattedTextSubtitle::FormattedTextSubtitle(const QString &base,
                                             const QRect &safearea,
                                             uint64_t start, uint64_t duration,
                                             SubtitleScreen *p) :
    m_base(base), m_safeArea(safearea),
    m_start(start), m_duration(duration), m_subScreen(p)
{
    // make cppcheck happy
    m_xAnchorPoint = 0;
    m_yAnchorPoint = 0;
    m_xAnchor = 0;
    m_yAnchor = 0;
}

FormattedTextSubtitle::FormattedTextSubtitle(void) :
    m_start(0), m_duration(0), m_subScreen(NULL)
{
    // make cppcheck happy
    m_xAnchorPoint = 0;
    m_yAnchorPoint = 0;
    m_xAnchor = 0;
    m_yAnchor = 0;
}

// Normal font height is designed to be 1/20 of safe area height, with
// extra blank space between lines to make 17 lines within the safe
// area.
static const float LINE_SPACING = (20.0 / 17.0);
static const float PAD_WIDTH  = 0.5;
static const float PAD_HEIGHT = 0.04;

// Resolves any TBD x_indent and y_indent values in FormattedTextLine
// objects.  Calculates m_bounds.  Prunes most leading and all
// trailing whitespace from each line so that displaying with a black
// background doesn't look clumsy.
void FormattedTextSubtitle::Layout(void)
{
    // Calculate dimensions of bounding rectangle
    int anchor_width = 0;
    int anchor_height = 0;
    for (int i = 0; i < m_lines.size(); i++)
    {
        QSize sz = m_lines[i].CalcSize(LINE_SPACING);
        anchor_width = max(anchor_width, sz.width());
        anchor_height += sz.height();
    }

    // Adjust the anchor point according to actual width and height
    int anchor_x = m_xAnchor;
    int anchor_y = m_yAnchor;
    if (m_xAnchorPoint == 1)
        anchor_x -= anchor_width / 2;
    else if (m_xAnchorPoint == 2)
        anchor_x -= anchor_width;
    if (m_yAnchorPoint == 1)
        anchor_y -= anchor_height / 2;
    else if (m_yAnchorPoint == 2)
        anchor_y -= anchor_height;

    // Shift the anchor point back into the safe area if necessary/possible.
    anchor_y = max(0, min(anchor_y, m_safeArea.height() - anchor_height));
    anchor_x = max(0, min(anchor_x, m_safeArea.width() - anchor_width));

    m_bounds = QRect(anchor_x, anchor_y, anchor_width, anchor_height);

    // Fill in missing coordinates
    int y = anchor_y;
    for (int i = 0; i < m_lines.size(); i++)
    {
        if (m_lines[i].x_indent < 0)
            m_lines[i].x_indent = anchor_x;
        if (m_lines[i].y_indent < 0)
            m_lines[i].y_indent = y;
        y += m_lines[i].CalcSize(LINE_SPACING).height();
        // Prune leading all-whitespace chunks.
        while (!m_lines[i].chunks.isEmpty() &&
               m_lines[i].chunks.first().text.trimmed().isEmpty())
        {
            m_lines[i].x_indent +=
                m_lines[i].chunks.first().CalcSize().width();
            m_lines[i].chunks.removeFirst();
        }
        // Prune trailing all-whitespace chunks.
        while (!m_lines[i].chunks.isEmpty() &&
               m_lines[i].chunks.last().text.trimmed().isEmpty())
        {
            m_lines[i].chunks.removeLast();
        }
        // Trim trailing whitespace from last chunk.  (Trimming
        // leading whitespace from all chunks is handled in the Draw()
        // routine.)
        if (!m_lines[i].chunks.isEmpty())
        {
            QString *str = &m_lines[i].chunks.last().text;
            int idx = str->length() - 1;
            while (idx >= 0 && str->at(idx) == ' ')
                --idx;
            str->truncate(idx + 1);
        }
    }
}

void FormattedTextSubtitle::PreRender(void)
{
    for (int i = 0; i < m_lines.size(); i++)
    {
        int x = m_lines[i].x_indent;
        int y = m_lines[i].y_indent;
        int height = m_lines[i].CalcSize().height();
        QList<FormattedTextChunk>::iterator chunk;
        bool isFirst = true;
        for (chunk = m_lines[i].chunks.begin();
             chunk != m_lines[i].chunks.end();
             ++chunk)
        {
            bool isLast = (chunk + 1 == m_lines[i].chunks.end());
            (*chunk).PreRender(isFirst, isLast, x, y, height);
            isFirst = false;
        }
    }
}

// Returns true if anything new was drawn, false if not.  The caller
// should call SubtitleScreen::OptimiseDisplayedArea() if true is
// returned.
void FormattedTextSubtitle::Draw(void)
{
    QList<MythUIType *> textList, shapeList;
    for (int i = 0; i < m_lines.size(); i++)
    {
        QList<FormattedTextChunk>::const_iterator chunk;
        for (chunk = m_lines[i].chunks.constBegin();
             chunk != m_lines[i].chunks.constEnd();
             ++chunk)
        {
            MythFontProperties *mythfont =
                m_subScreen->GetFont((*chunk).m_format);
            if (!mythfont)
                continue;
            // Note: NULL is passed as the parent argument to the
            // MythUI constructors so that we can control the drawing
            // order of the children.  In particular, background
            // shapes should be added/drawn first, and text drawn on
            // top.
            SubSimpleText *text =
                new SubSimpleText((*chunk).text, *mythfont,
                                  (*chunk).textRect,
                                  Qt::AlignLeft|Qt::AlignTop,
                                  /*m_subScreen*/NULL,
                                  (*chunk).textName, CacheNum(),
                                  m_start + m_duration);
            textList += text;
            MythUIShape *bgshape = m_subScreen->GetSubtitleFormat()->
                GetBackground(/*m_subScreen*/NULL,
                              (*chunk).bgShapeName,
                              m_base, (*chunk).m_format,
                              MythRect((*chunk).bgShapeRect), CacheNum(),
                              m_start, m_duration);
            if (bgshape)
            {
                bgshape->SetArea(MythRect((*chunk).bgShapeRect));
                shapeList += bgshape;
            }
        }
    }
    while (!shapeList.isEmpty())
        m_subScreen->AddChild(shapeList.takeFirst());
    while (!textList.isEmpty())
        m_subScreen->AddChild(textList.takeFirst());
}

QStringList FormattedTextSubtitle::ToSRT(void) const
{
    QStringList result;
    for (int i = 0; i < m_lines.size(); i++)
    {
        QString line;
        if (m_lines[i].orig_x > 0)
            line.fill(' ', m_lines[i].orig_x);
        QList<FormattedTextChunk>::const_iterator chunk;
        for (chunk = m_lines[i].chunks.constBegin();
             chunk != m_lines[i].chunks.constEnd();
             ++chunk)
        {
            const QString &text = (*chunk).text;
            const CC708CharacterAttribute &attr = (*chunk).m_format;
            bool isBlank = !attr.underline && text.trimmed().isEmpty();
            if (!isBlank)
            {
                if (attr.boldface)
                    line += "<b>";
                if (attr.italics)
                    line += "<i>";
                if (attr.underline)
                    line += "<u>";
                if (attr.GetFGColor() != Qt::white)
                    line += QString("<font color=\"%1\">")
                        .arg(srtColorString(attr.GetFGColor()));
            }
            line += text;
            if (!isBlank)
            {
                if (attr.GetFGColor() != Qt::white)
                    line += QString("</font>");
                if (attr.underline)
                    line += "</u>";
                if (attr.italics)
                    line += "</i>";
                if (attr.boldface)
                    line += "</b>";
            }
        }
        if (!line.trimmed().isEmpty())
            result += line;
    }
    return result;
}

void FormattedTextSubtitleSRT::Init(const QStringList &subs)
{
    // Does a simplistic parsing of HTML tags from the strings.
    // Nesting is not implemented.  Stray whitespace may cause
    // legitimate tags to be ignored.  All other HTML tags are
    // stripped and ignored.
    //
    // <i> - enable italics
    // </i> - disable italics
    // <b> - enable boldface
    // </b> - disable boldface
    // <u> - enable underline
    // </u> - disable underline
    // <font color="#xxyyzz"> - change font color
    // </font> - reset font color to white

    int pixelSize = m_safeArea.height() / 20;
    if (m_subScreen)
        m_subScreen->SetFontSize(pixelSize);
    m_xAnchorPoint = 1; // center
    m_yAnchorPoint = 2; // bottom
    m_xAnchor = m_safeArea.width() / 2;
    m_yAnchor = m_safeArea.height();

    bool isItalic = false;
    bool isBold = false;
    bool isUnderline = false;
    QColor color(Qt::white);
    QRegExp htmlTag("</?.+>");
    QString htmlPrefix("<font color=\""), htmlSuffix("\">");
    htmlTag.setMinimal(true);
    foreach (QString subtitle, subs)
    {
        FormattedTextLine line;
        QString text(subtitle);
        while (!text.isEmpty())
        {
            int pos = text.indexOf(htmlTag);
            if (pos != 0) // don't add a zero-length string
            {
                CC708CharacterAttribute attr(isItalic, isBold, isUnderline,
                                             color);
                FormattedTextChunk chunk(text.left(pos), attr, m_subScreen);
                line.chunks += chunk;
                text = (pos < 0 ? "" : text.mid(pos));
                LOG(VB_VBI, LOG_INFO, QString("Adding SRT chunk: %1")
                    .arg(chunk.ToLogString()));
            }
            if (pos >= 0)
            {
                int htmlLen = htmlTag.matchedLength();
                QString html = text.left(htmlLen).toLower();
                text = text.mid(htmlLen);
                if (html == "<i>")
                    isItalic = true;
                else if (html == "</i>")
                    isItalic = false;
                else if (html.startsWith(htmlPrefix) &&
                         html.endsWith(htmlSuffix))
                {
                    int colorLen = html.length() -
                        (htmlPrefix.length() + htmlSuffix.length());
                    QString colorString(
                        html.mid(htmlPrefix.length(), colorLen));
                    QColor newColor(colorString);
                    if (newColor.isValid())
                    {
                        color = newColor;
                    }
                    else
                    {
                        LOG(VB_VBI, LOG_INFO,
                            QString("Ignoring invalid SRT color specification: "
                                    "'%1'").arg(colorString));
                    }
                }
                else if (html == "</font>")
                    color = Qt::white;
                else if (html == "<b>")
                    isBold = true;
                else if (html == "</b>")
                    isBold = false;
                else if (html == "<u>")
                    isUnderline = true;
                else if (html == "</u>")
                    isUnderline = false;
                else
                {
                    LOG(VB_VBI, LOG_INFO,
                        QString("Ignoring unknown SRT formatting: '%1'")
                        .arg(html));
                }

                LOG(VB_VBI, LOG_INFO,
                    QString("SRT formatting change '%1', "
                            "new ital=%2 bold=%3 uline=%4 color=%5)")
                    .arg(html).arg(isItalic).arg(isBold).arg(isUnderline)
                    .arg(srtColorString(color)));
            }
        }
        m_lines += line;
    }
}

void FormattedTextSubtitleSRT::WrapLongLines(void)
{
    int maxWidth = m_safeArea.width();
    for (int i = 0; i < m_lines.size(); i++)
    {
        int width = m_lines[i].CalcSize().width();
        // Move entire chunks to the next line as necessary.  Leave at
        // least one chunk on the current line.
        while (width > maxWidth && m_lines[i].chunks.size() > 1)
        {
            width -= m_lines[i].chunks.back().CalcSize().width();
            // Make sure there's a next line to wrap into.
            if (m_lines.size() == i + 1)
                m_lines += FormattedTextLine(m_lines[i].x_indent,
                                             m_lines[i].y_indent);
            m_lines[i+1].chunks.prepend(m_lines[i].chunks.takeLast());
            LOG(VB_VBI, LOG_INFO,
                QString("Wrapping chunk to next line: '%1'")
                .arg(m_lines[i+1].chunks[0].text));
        }
        // Split the last chunk until width is small enough or until
        // no more splits are possible.
        bool isSplitPossible = true;
        while (width > maxWidth && isSplitPossible)
        {
            FormattedTextChunk newChunk;
            isSplitPossible = m_lines[i].chunks.back().Split(newChunk);
            if (isSplitPossible)
            {
                // Make sure there's a next line to split into.
                if (m_lines.size() == i + 1)
                    m_lines += FormattedTextLine(m_lines[i].x_indent,
                                                 m_lines[i].y_indent);
                m_lines[i+1].chunks.prepend(newChunk);
                width = m_lines[i].CalcSize().width();
            }
        }
    }
}

/// Extract everything from the text buffer up until the next format
/// control character.  Return that substring, and remove it from the
/// input string.  Bogus control characters are left unchanged.  If the
/// buffer starts with a valid control character, the output parameters
/// are corresondingly updated (and the control character is stripped).
static QString extract_cc608(QString &text, int &color,
                             bool &isItalic, bool &isUnderline)
{
    QString result;
    QString orig(text);

    // Handle an initial control sequence.
    if (text.length() >= 1 && text[0] >= 0x7000)
    {
        int op = text[0].unicode() - 0x7000;
        isUnderline = (op & 0x1);
        switch (op & ~1)
        {
        case 0x0e:
            // color unchanged
            isItalic = true;
            break;
        case 0x1e:
            color = op >> 1;
            isItalic = true;
            break;
        case 0x20:
            // color unchanged
            // italics unchanged
            break;
        default:
            color = (op & 0xf) >> 1;
            isItalic = false;
            break;
        }
        text = text.mid(1);
    }

    // Copy the string into the result, up to the next control
    // character.
    int nextControl = text.indexOf(QRegExp("[\\x7000-\\x7fff]"));
    if (nextControl < 0)
    {
        result = text;
        text = QString::null;
    }
    else
    {
        result = text.left(nextControl);
        // Print the space character before handling the next control
        // character, otherwise the space character will be lost due
        // to the text.trimmed() operation in the MythUISimpleText
        // constructor, combined with the left-justification of
        // captions.
        if (text[nextControl] < (0x7000 + 0x10))
            result += " ";
        text = text.mid(nextControl);
    }

    return result;
}

// Adjusts the Y coordinates to avoid overlap, which could happen as a
// result of a large text zoom factor.  Then, if the total height
// exceeds the safe area, compresses each piece of vertical blank
// space proportionally to make it fit.
void FormattedTextSubtitle608::Layout(void)
{
    int i;
    int totalHeight = 0;
    int totalSpace = 0;
    int firstY = 0;
    int prevY = 0;
    QVector<int> heights(m_lines.size());
    QVector<int> spaceBefore(m_lines.size());
    // Calculate totalHeight and totalSpace
    for (i = 0; i < m_lines.size(); i++)
    {
        m_lines[i].y_indent = max(m_lines[i].y_indent, prevY); // avoid overlap
        int y = m_lines[i].y_indent;
        if (i == 0)
            firstY = prevY = y;
        int height = m_lines[i].CalcSize().height();
        heights[i] = height;
        spaceBefore[i] = y - prevY;
        totalSpace += (y - prevY);
        prevY = y + height;
        totalHeight += height;
    }
    int safeHeight = m_safeArea.height();
    int overage = min(totalHeight - safeHeight, totalSpace);

    // Recalculate Y coordinates, applying the shrink factor to space
    // between each line.
    if (overage > 0 && totalSpace > 0)
    {
        float shrink = (totalSpace - overage) / (float)totalSpace;
        prevY = firstY;
        for (i = 0; i < m_lines.size(); i++)
        {
            m_lines[i].y_indent = prevY + spaceBefore[i] * shrink;
            prevY = m_lines[i].y_indent + heights[i];
        }
    }

    // Shift Y coordinates back up into the safe area.
    int shift = min(firstY, max(0, prevY - safeHeight));
    for (i = 0; i < m_lines.size(); i++)
        m_lines[i].y_indent -= shift;

    FormattedTextSubtitle::Layout();
}

void FormattedTextSubtitle608::Init(const vector<CC608Text*> &buffers)
{
    static const QColor clr[8] =
    {
        Qt::white,   Qt::green,   Qt::blue,    Qt::cyan,
        Qt::red,     Qt::yellow,  Qt::magenta, Qt::white,
    };

    if (buffers.empty())
        return;
    vector<CC608Text*>::const_iterator i = buffers.begin();
    int xscale = 36;
    int yscale = 17;
    int pixelSize = m_safeArea.height() / (yscale * LINE_SPACING);
    int fontwidth = 0;
    int xmid = 0;
    int zoom = 100;
    if (m_subScreen)
    {
        zoom = m_subScreen->GetZoom();
        m_subScreen->SetFontSize(pixelSize);
        CC708CharacterAttribute def_attr(false, false, false, clr[0]);
        QFont *font = m_subScreen->GetFont(def_attr)->GetFace();
        QFontMetrics fm(*font);
        fontwidth = fm.averageCharWidth();
        xmid = m_safeArea.width() / 2;
        // Disable centering for zoom factor >= 100%
        if (zoom >= 100)
            xscale = m_safeArea.width() / fontwidth;
    }

    for (; i != buffers.end(); ++i)
    {
        CC608Text *cc = (*i);
        int color = 0;
        bool isItalic = false, isUnderline = false;
        const bool isBold = false;
        QString text(cc->text);

        int orig_x = cc->x;
        // position as if we use a fixed size font
        // - font size already has zoom factor applied

        int x;
        if (xmid)
            // center horizontally
            x = xmid + (orig_x - xscale / 2) * fontwidth;
        else
            // fallback
            x = (orig_x + 3) * m_safeArea.width() / xscale;

        int orig_y = cc->y;
        int y;
        if (orig_y < yscale / 2)
            // top half -- anchor up
            y = (orig_y * m_safeArea.height() * zoom / (yscale * 100));
        else
            // bottom half -- anchor down
            y = m_safeArea.height() -
                ((yscale - orig_y - 0.5) * m_safeArea.height() * zoom /
                 (yscale * 100));

        FormattedTextLine line(x, y, orig_x, orig_y);
        while (!text.isNull())
        {
            QString captionText =
                extract_cc608(text, color, isItalic, isUnderline);
            CC708CharacterAttribute attr(isItalic, isBold, isUnderline,
                                         clr[min(max(0, color), 7)]);
            FormattedTextChunk chunk(captionText, attr, m_subScreen);
            line.chunks += chunk;
            LOG(VB_VBI, LOG_INFO,
                QString("Adding cc608 chunk (%1,%2): %3")
                .arg(cc->x).arg(cc->y).arg(chunk.ToLogString()));
        }
        m_lines += line;
    }
}

void FormattedTextSubtitle708::Draw(void)
{
    // Draw the window background after calculating bounding rectangle
    // in Layout()
    if (m_bgFillAlpha) // TODO border?
    {
        QBrush fill(m_bgFillColor, Qt::SolidPattern);
        SubShape *shape =
            new SubShape(m_subScreen, QString("cc708bg%1").arg(m_num),
                         MythRect(m_bounds), m_num, m_start + m_duration);
        shape->SetFillBrush(fill);
        shape->SetArea(MythRect(m_bounds));
    }
    // The background is drawn first so that it appears behind
    // everything else.
    FormattedTextSubtitle::Draw();
}

void FormattedTextSubtitle708::Init(const CC708Window &win,
                                    const vector<CC708String*> &list,
                                    float aspect)
{
    LOG(VB_VBI, LOG_DEBUG,LOC +
        QString("Display Win %1, Anchor_id %2, x_anch %3, y_anch %4, "
                "relative %5")
            .arg(m_num).arg(win.anchor_point).arg(win.anchor_horizontal)
            .arg(win.anchor_vertical).arg(win.relative_pos));
    int pixelSize = m_safeArea.height() / 20;
    if (m_subScreen)
        m_subScreen->SetFontSize(pixelSize);

    float xrange  = win.relative_pos ? 100.0f :
                    (aspect > 1.4f) ? 210.0f : 160.0f;
    float yrange  = win.relative_pos ? 100.0f : 75.0f;
    float xmult   = (float)m_safeArea.width() / xrange;
    float ymult   = (float)m_safeArea.height() / yrange;
    uint anchor_x = (uint)(xmult * (float)win.anchor_horizontal);
    uint anchor_y = (uint)(ymult * (float)win.anchor_vertical);
    m_xAnchorPoint = win.anchor_point % 3;
    m_yAnchorPoint = win.anchor_point / 3;
    m_xAnchor = anchor_x;
    m_yAnchor = anchor_y;

    for (uint i = 0; i < list.size(); i++)
    {
        if (list[i]->y >= (uint)m_lines.size())
            m_lines.resize(list[i]->y + 1);
        FormattedTextChunk chunk(list[i]->str, list[i]->attr, m_subScreen);
        m_lines[list[i]->y].chunks += chunk;
        LOG(VB_VBI, LOG_INFO, QString("Adding cc708 chunk: win %1 row %2: %3")
            .arg(m_num).arg(list[i]->y).arg(chunk.ToLogString()));
    }
}

////////////////////////////////////////////////////////////////////////////

SubtitleScreen::SubtitleScreen(MythPlayer *player, const char * name,
                               int fontStretch) :
    MythScreenType((MythScreenType*)NULL, name),
    m_player(player),  m_subreader(NULL),   m_608reader(NULL),
    m_708reader(NULL), m_safeArea(QRect()),
    m_removeHTML(QRegExp("</?.+>")),        m_subtitleType(kDisplayNone),
    m_fontSize(0),
    m_textFontZoom(100), m_textFontZoomPrev(100),
    m_textFontDelayMs(0), m_textFontDelayMsPrev(0),
    m_refreshModified(false), m_refreshDeleted(false),
    m_fontStretch(fontStretch),
    m_format(new SubtitleFormat)
{
    m_removeHTML.setMinimal(true);

#ifdef USING_LIBASS
    m_assLibrary   = NULL;
    m_assRenderer  = NULL;
    m_assTrackNum  = -1;
    m_assTrack     = NULL;
    m_assFontCount = 0;
#endif
}

SubtitleScreen::~SubtitleScreen(void)
{
    ClearAllSubtitles();
    delete m_format;
#ifdef USING_LIBASS
    CleanupAssLibrary();
#endif
}

void SubtitleScreen::EnableSubtitles(int type, bool forced_only)
{
    if (forced_only)
    {
        SetElementDeleted();
        SetVisible(true);
        SetArea(MythRect());
        return;
    }

    m_subtitleType = type;
    if (m_subreader)
    {
        m_subreader->EnableAVSubtitles(kDisplayAVSubtitle == m_subtitleType);
        m_subreader->EnableTextSubtitles(kDisplayTextSubtitle == m_subtitleType);
        m_subreader->EnableRawTextSubtitles(kDisplayRawTextSubtitle == m_subtitleType);
    }
    if (m_608reader)
        m_608reader->SetEnabled(kDisplayCC608 == m_subtitleType);
    if (m_708reader)
        m_708reader->SetEnabled(kDisplayCC708 == m_subtitleType);
    ClearAllSubtitles();
    SetVisible(m_subtitleType != kDisplayNone);
    SetArea(MythRect());
    switch (m_subtitleType)
    {
    case kDisplayTextSubtitle:
    case kDisplayRawTextSubtitle:
        m_family = kSubFamilyText;
        m_textFontZoom  = gCoreContext->GetNumSetting("OSDCC708TextZoom", 100);
        break;
    case kDisplayCC608:
        m_family = kSubFamily608;
        m_textFontZoom  = gCoreContext->GetNumSetting("OSDCC708TextZoom", 100);
        break;
    case kDisplayCC708:
        m_family = kSubFamily708;
        m_textFontZoom  = gCoreContext->GetNumSetting("OSDCC708TextZoom", 100);
        break;
    case kDisplayAVSubtitle:
        m_family = kSubFamilyAV;
        m_textFontZoom = gCoreContext->GetNumSetting("OSDAVSubZoom", 100);
        break;
    }
    m_textFontZoomPrev = m_textFontZoom;
    m_textFontDelayMsPrev = m_textFontDelayMs;
}

void SubtitleScreen::DisableForcedSubtitles(void)
{
    if (kDisplayNone != m_subtitleType)
        return;
    ClearAllSubtitles();
    SetVisible(false);
    SetArea(MythRect());
}

void SubtitleScreen::ClearAllSubtitles(void)
{
    ClearNonDisplayedSubtitles();
    ClearDisplayedSubtitles();
#ifdef USING_LIBASS
    if (m_assTrack)
        ass_flush_events(m_assTrack);
#endif
}

void SubtitleScreen::ClearNonDisplayedSubtitles(void)
{
    if (m_subreader && (kDisplayAVSubtitle == m_subtitleType))
        m_subreader->ClearAVSubtitles();
    if (m_subreader && (kDisplayRawTextSubtitle == m_subtitleType))
        m_subreader->ClearRawTextSubtitles();
    if (m_608reader && (kDisplayCC608 == m_subtitleType))
        m_608reader->ClearBuffers(true, true);
    if (m_708reader && (kDisplayCC708 == m_subtitleType))
        m_708reader->ClearBuffers();
}

void SubtitleScreen::ClearDisplayedSubtitles(void)
{
    SetElementDeleted();
    DeleteAllChildren();
}

void SubtitleScreen::DisplayDVDButton(AVSubtitle* dvdButton, QRect &buttonPos)
{
    if (!dvdButton || !m_player)
        return;

    VideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    DeleteAllChildren();
    SetElementDeleted();

    float tmp = 0.0;
    QRect dummy;
    vo->GetOSDBounds(dummy, m_safeArea, tmp, tmp, tmp);

    AVSubtitleRect *hl_button = dvdButton->rects[0];
    uint h = hl_button->h;
    uint w = hl_button->w;
    QRect rect = QRect(hl_button->x, hl_button->y, w, h);
    QImage bg_image(hl_button->pict.data[0], w, h, w, QImage::Format_Indexed8);
    uint32_t *bgpalette = (uint32_t *)(hl_button->pict.data[1]);

    QVector<uint32_t> bg_palette(4);
    for (int i = 0; i < 4; i++)
        bg_palette[i] = bgpalette[i];
    bg_image.setColorTable(bg_palette);

    // copy button region of background image
    const QRect fg_rect(buttonPos.translated(-hl_button->x, -hl_button->y));
    QImage fg_image = bg_image.copy(fg_rect);
    QVector<uint32_t> fg_palette(4);
    uint32_t *fgpalette = (uint32_t *)(dvdButton->rects[1]->pict.data[1]);
    if (fgpalette)
    {
        for (int i = 0; i < 4; i++)
            fg_palette[i] = fgpalette[i];
        fg_image.setColorTable(fg_palette);
    }

    bg_image = bg_image.convertToFormat(QImage::Format_ARGB32);
    fg_image = fg_image.convertToFormat(QImage::Format_ARGB32);

    // set pixel of highlight area to highlight color
    for (int x=fg_rect.x(); x < fg_rect.x()+fg_rect.width(); ++x)
    {
        if ((x < 0) || (x > hl_button->w))
            continue;
        for (int y=fg_rect.y(); y < fg_rect.y()+fg_rect.height(); ++y)
        {
            if ((y < 0) || (y > hl_button->h))
                continue;
            bg_image.setPixel(x, y, fg_image.pixel(x-fg_rect.x(),y-fg_rect.y()));
        }
    }

    AddScaledImage(bg_image, rect);
}

void SubtitleScreen::SetZoom(int percent)
{
    m_textFontZoom = percent;
    if (m_family == kSubFamilyAV)
        gCoreContext->SaveSetting("OSDAVSubZoom", percent);
    else
        gCoreContext->SaveSetting("OSDCC708TextZoom", percent);
}

int SubtitleScreen::GetZoom(void) const
{
    return m_textFontZoom;
}

void SubtitleScreen::SetDelay(int ms)
{
    m_textFontDelayMs = ms;
}

int SubtitleScreen::GetDelay(void) const
{
    return m_textFontDelayMs;
}

void SubtitleScreen::Clear708Cache(uint64_t mask)
{
    QList<MythUIType *> list = m_ChildrenList;
    QList<MythUIType *>::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        MythUIType *child = *it;
        SubWrapper *wrapper = dynamic_cast<SubWrapper *>(child);
        if (wrapper)
        {
            int whichImageCache = wrapper->GetWhichImageCache();
            if (whichImageCache != -1 && (mask & (1ul << whichImageCache)))
            {
                SetElementDeleted();
                DeleteChild(child);
            }
        }
    }
}

// SetElementAdded() should be called after a new element is added to
// the subtitle screen.
void SubtitleScreen::SetElementAdded(void)
{
    m_refreshModified = true;
}

// SetElementResized() should be called after a subtitle screen
// element's size is changed.
void SubtitleScreen::SetElementResized(void)
{
    SetElementAdded();
}

// SetElementAdded() should be called *before* an element is deleted
// from the subtitle screen.
void SubtitleScreen::SetElementDeleted(void)
{
    if (!m_refreshDeleted)
        SetRedraw();
    m_refreshDeleted = true;
}

// The QFontMetrics class does not account for the MythFontProperties
// shadow and offset properties.  This method calculates the
// additional padding to the right and below that is needed for proper
// bounding box computation.
static QSize CalcShadowOffsetPadding(MythFontProperties *mythfont)
{
    QColor color;
    int alpha;
    int outlineSize = 0;
    int shadowWidth = 0, shadowHeight = 0;
    if (mythfont->hasOutline())
    {
        mythfont->GetOutline(color, outlineSize, alpha);
        MythPoint outline(outlineSize, outlineSize);
        outline.NormPoint();
        outlineSize = outline.x();
    }
    if (mythfont->hasShadow())
    {
        MythPoint shadowOffset;
        mythfont->GetShadow(shadowOffset, color, alpha);
        shadowOffset.NormPoint();
        shadowWidth = abs(shadowOffset.x());
        shadowHeight = abs(shadowOffset.y());
        // Shadow and outline overlap, so don't just add them.
        shadowWidth = max(shadowWidth, outlineSize);
        shadowHeight = max(shadowHeight, outlineSize);
    }
    return QSize(shadowWidth + outlineSize, shadowHeight + outlineSize);
}

QSize SubtitleScreen::CalcTextSize(const QString &text,
                                   const CC708CharacterAttribute &format,
                                   float layoutSpacing) const
{
    MythFontProperties *mythfont = GetFont(format);
    QFont *font = mythfont->GetFace();
    QFontMetrics fm(*font);
    int width = fm.width(text);
    int height = fm.height() * (1 + PAD_HEIGHT);
    if (layoutSpacing > 0 && !text.trimmed().isEmpty())
        height = max(height, (int)(font->pixelSize() * layoutSpacing));
    height += CalcShadowOffsetPadding(mythfont).height();
    return QSize(width, height);
}

// Padding calculation is different depending on whether the padding
// is on the left side or the right side of the text.  Padding on the
// right needs to add the shadow and offset padding.
void SubtitleScreen::CalcPadding(const CC708CharacterAttribute &format,
                                 bool isFirst, bool isLast,
                                 int &left, int &right) const
{
    MythFontProperties *mythfont = GetFont(format);
    QFont *font = mythfont->GetFace();
    QFontMetrics fm(*font);
    int basicPadding = fm.maxWidth() * PAD_WIDTH;
    left = isFirst ? basicPadding : 0;
    right = CalcShadowOffsetPadding(mythfont).width() +
        (isLast ? basicPadding : 0);
}

MythFontProperties *
SubtitleScreen::GetFont(const CC708CharacterAttribute &attr) const
{
    return m_format->GetFont(m_family, attr, m_fontSize,
                             m_textFontZoom, m_fontStretch);
}

QString SubtitleScreen::GetTeletextFontName(void)
{
    SubtitleFormat format;
    CC708CharacterAttribute attr(false, false, false, Qt::white);
    MythFontProperties *mythfont =
        format.GetFont(kSubFamilyTeletext, attr,
                       /*pixelsize*/20, /*zoom*/100, /*stretch*/100);
    return mythfont->face().family();
}

bool SubtitleScreen::Create(void)
{
    if (!m_player)
        return false;

    m_subreader = m_player->GetSubReader();
    m_608reader = m_player->GetCC608Reader();
    m_708reader = m_player->GetCC708Reader();
    if (!m_subreader)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to get subtitle reader.");
    if (!m_608reader)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to get CEA-608 reader.");
    if (!m_708reader)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to get CEA-708 reader.");

    return true;
}

void SubtitleScreen::Pulse(void)
{
    QList<MythUIType *>::iterator it, itNext;

    VideoOutput    *videoOut = m_player->GetVideoOutput();
    VideoFrame *currentFrame = videoOut ? videoOut->GetLastShownFrame() : NULL;
    long long now = currentFrame ? currentFrame->timecode : LLONG_MAX;
    bool needRescale = (m_textFontZoom != m_textFontZoomPrev);

    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); it = itNext)
    {
        itNext = it + 1;
        MythUIType *child = *it;
        SubWrapper *wrapper = dynamic_cast<SubWrapper *>(child);
        if (!wrapper)
            continue;

        // Expire the subtitle object if needed.
        long long expireTime = wrapper->GetExpireTime();
        if (expireTime > 0 && expireTime < now)
        {
            DeleteChild(child);
            SetElementDeleted();
            continue;
        }

        // Rescale the AV subtitle image if the zoom changed.
        if (expireTime > 0 && needRescale)
        {
            SubImage *image = dynamic_cast<SubImage *>(child);
            if (image)
            {
                double factor = m_textFontZoom / (double)m_textFontZoomPrev;
                QSize size = image->GetImage()->size();
                size *= factor;
                image->GetImage()->Resize(size);
                SetElementResized();
            }
        }
    }

    DisplayAVSubtitles(); // allow forced subtitles to work

    if (kDisplayTextSubtitle == m_subtitleType)
        DisplayTextSubtitles();
    else if (kDisplayCC608 == m_subtitleType)
        DisplayCC608Subtitles();
    else if (kDisplayCC708 == m_subtitleType)
        DisplayCC708Subtitles();
    else if (kDisplayRawTextSubtitle == m_subtitleType)
        DisplayRawTextSubtitles();
    while (!m_qInited.isEmpty())
    {
        FormattedTextSubtitle *fsub = m_qInited.takeFirst();
        fsub->WrapLongLines();
        fsub->Layout();
        fsub->PreRender();
        fsub->Draw();
        SetElementAdded();
    }

    OptimiseDisplayedArea();
    MythScreenType::Pulse();
    m_textFontZoomPrev = m_textFontZoom;
    m_textFontDelayMsPrev = m_textFontDelayMs;
    ResetElementState();
}

void SubtitleScreen::ResetElementState(void)
{
    m_refreshModified = false;
    m_refreshDeleted = false;
}

void SubtitleScreen::OptimiseDisplayedArea(void)
{
    if (!m_refreshModified)
        return;

    QRegion visible;
    QListIterator<MythUIType *> i(m_ChildrenList);
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        SubWrapper *wrapper = dynamic_cast<SubWrapper *>(img);
        if (wrapper && img->IsVisible())
            visible = visible.united(wrapper->GetOrigArea());
    }

    if (visible.isEmpty())
        return;

    QRect bounding  = visible.boundingRect();
    bounding = bounding.translated(m_safeArea.topLeft());
    bounding = m_safeArea.intersected(bounding);
    int left = m_safeArea.left() - bounding.left();
    int top  = m_safeArea.top()  - bounding.top();
    SetArea(MythRect(bounding));

    i.toFront();
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        SubWrapper *wrapper = dynamic_cast<SubWrapper *>(img);
        if (wrapper && img->IsVisible())
            img->SetArea(wrapper->GetOrigArea().translated(left, top));
    }
}

void SubtitleScreen::DisplayAVSubtitles(void)
{
    if (!m_player || !m_subreader)
        return;

    AVSubtitles* subs = m_subreader->GetAVSubtitles();
    QMutexLocker lock(&(subs->lock));
    if (subs->buffers.empty() && (kDisplayAVSubtitle != m_subtitleType))
        return;

    VideoOutput    *videoOut = m_player->GetVideoOutput();
    VideoFrame *currentFrame = videoOut ? videoOut->GetLastShownFrame() : NULL;

    if (!currentFrame || !videoOut)
        return;

    float tmp = 0.0;
    QRect dummy;
    videoOut->GetOSDBounds(dummy, m_safeArea, tmp, tmp, tmp);

    while (!subs->buffers.empty())
    {
        const AVSubtitle subtitle = subs->buffers.front();
        if (subtitle.start_display_time > currentFrame->timecode)
            break;

        long long displayfor = subtitle.end_display_time -
                               subtitle.start_display_time;
        if (displayfor == 0)
            displayfor = 60000;
        displayfor = (displayfor < 50) ? 50 : displayfor;
        long long late = currentFrame->timecode -
                         subtitle.start_display_time;

        ClearDisplayedSubtitles();
        subs->buffers.pop_front();
        for (std::size_t i = 0; i < subtitle.num_rects; ++i)
        {
            AVSubtitleRect* rect = subtitle.rects[i];

            bool displaysub = true;
            if (subs->buffers.size() > 0 &&
                subs->buffers.front().end_display_time <
                currentFrame->timecode)
            {
                displaysub = false;
            }

            if (displaysub && rect->type == SUBTITLE_BITMAP)
            {
                QRect display(rect->display_x, rect->display_y,
                              rect->display_w, rect->display_h);

                // XSUB and some DVD/DVB subs are based on the original video
                // size before the video was converted. We need to guess the
                // original size and allow for the difference

                int right  = rect->x + rect->w;
                int bottom = rect->y + rect->h;
                if (subs->fixPosition || (currentFrame->height < bottom) ||
                    (currentFrame->width  < right) ||
                    !display.width() || !display.height())
                {
                    int sd_height = 576;
                    if ((m_player->GetFrameRate() > 26.0f ||
                         m_player->GetFrameRate() < 24.0f) && bottom <= 480)
                        sd_height = 480;
                    int height = ((currentFrame->height <= sd_height) &&
                                  (bottom <= sd_height)) ? sd_height :
                                 ((currentFrame->height <= 720) && bottom <= 720)
                                   ? 720 : 1080;
                    int width  = ((currentFrame->width  <= 720) &&
                                  (right <= 720)) ? 720 :
                                 ((currentFrame->width  <= 1280) &&
                                  (right <= 1280)) ? 1280 : 1920;
                    display = QRect(0, 0, width, height);
                }

                // split into upper/lower to allow zooming
                QRect bbox;
                int uh = display.height() / 2 - rect->y;
                int lh;
                long long displayuntil = currentFrame->timecode + displayfor;
                if (uh > 0)
                {
                    bbox = QRect(0, 0, rect->w, uh);
                    uh = DisplayScaledAVSubtitles(rect, bbox, true, display,
                                                  subtitle.forced,
                                                  QString("avsub%1t").arg(i),
                                                  displayuntil, late);
                }
                else
                    uh = 0;
                lh = rect->h - uh;
                if (lh > 0)
                {
                    bbox = QRect(0, uh, rect->w, lh);
                    DisplayScaledAVSubtitles(rect, bbox, false, display,
                                             subtitle.forced,
                                             QString("avsub%1b").arg(i),
                                             displayuntil, late);
                }
            }
#ifdef USING_LIBASS
            else if (displaysub && rect->type == SUBTITLE_ASS)
            {
                InitialiseAssTrack(m_player->GetDecoder()->GetTrack(kTrackTypeSubtitle));
                AddAssEvent(rect->ass);
            }
#endif
        }
        m_subreader->FreeAVSubtitle(subtitle);
    }
#ifdef USING_LIBASS
    RenderAssTrack(currentFrame->timecode);
#endif
}

int SubtitleScreen::DisplayScaledAVSubtitles(const AVSubtitleRect *rect,
                                             QRect &bbox, bool top,
                                             QRect &display, int forced,
                                             QString imagename,
                                             long long displayuntil,
                                             long long late)
{
    // split image vertically if it spans middle of display
    // - split point is empty line nearest the middle
    // crop image to reduce scaling time
    int xmin, xmax, ymin, ymax;
    int ylast, ysplit;
    bool prev_empty = false;

    // initialize to opposite edges
    xmin = bbox.right();
    xmax = bbox.left();
    ymin = bbox.bottom();
    ymax = bbox.top();
    ylast = bbox.top();
    ysplit = bbox.bottom();

    // find bounds of active image
    for (int y = bbox.top(); y <= bbox.bottom(); ++y)
    {
        if (y >= rect->h)
        {
            // end of image
            if (!prev_empty)
                ylast = y;
            break;
        }

        bool empty = true;
        for (int x = bbox.left(); x <= bbox.right(); ++x)
        {
            const uint8_t color =
                rect->pict.data[0][y * rect->pict.linesize[0] + x];
            const uint32_t pixel = *((uint32_t *)rect->pict.data[1] + color);
            if (pixel & 0xff000000)
            {
                empty = false;
                if (x < xmin)
                    xmin = x;
                if (x > xmax)
                    xmax = x;
            }
        }

        if (!empty)
        {
            if (y < ymin)
                ymin = y;
            if (y > ymax)
                ymax = y;
        }
        else if (!prev_empty)
        {
            // remember uppermost empty line
            ylast = y;
        }
        prev_empty = empty;
    }

    if (ymax <= ymin)
        return 0;

    if (top)
    {
        if (ylast < ymin)
            // no empty lines
            return 0;

        if (ymax == bbox.bottom())
        {
            ymax = ylast;
            ysplit = ylast;
        }
    }

    // set new bounds
    bbox.setLeft(xmin);
    bbox.setRight(xmax);
    bbox.setTop(ymin);
    bbox.setBottom(ymax);

    // copy active region
    // AVSubtitleRect's image data's not guaranteed to be 4 byte
    // aligned.

    QRect orig_rect(bbox.left(), bbox.top(), bbox.width(), bbox.height());

    QImage qImage(bbox.width(), bbox.height(), QImage::Format_ARGB32);
    for (int y = 0; y < bbox.height(); ++y)
    {
        int ysrc = y + bbox.top();
        for (int x = 0; x < bbox.width(); ++x)
        {
            int xsrc = x + bbox.left();
            const uint8_t color =
                rect->pict.data[0][ysrc * rect->pict.linesize[0] + xsrc];
            const uint32_t pixel = *((uint32_t *)rect->pict.data[1] + color);
            qImage.setPixel(x, y, pixel);
        }
    }

    // translate to absolute coordinates
    bbox.translate(rect->x, rect->y);

    // scale and move according to zoom factor
    bbox.setWidth(bbox.width() * m_textFontZoom / 100);
    bbox.setHeight(bbox.height() * m_textFontZoom / 100);

    VideoOutput *videoOut = m_player->GetVideoOutput();
    QRect scaled = videoOut->GetImageRect(bbox, &display);

    if (scaled.size() != orig_rect.size())
        qImage = qImage.scaled(scaled.width(), scaled.height(),
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    int hsize = m_safeArea.width();
    int vsize = m_safeArea.height();

    scaled.moveLeft(((100 - m_textFontZoom) * hsize / 2 +
                     m_textFontZoom * scaled.left()) /
                    100);
    if (top)
        // anchor up
        scaled.moveTop(scaled.top() * m_textFontZoom / 100);
    else
        // anchor down
        scaled.moveTop(((100 - m_textFontZoom) * vsize +
                        m_textFontZoom * scaled.top()) /
                       100);


    MythPainter *osd_painter = videoOut->GetOSDPainter();
    MythImage *image = NULL;
    if (osd_painter)
       image = osd_painter->GetFormatImage();

    SubImage *uiimage = NULL;
    if (image)
    {
        image->Assign(qImage);
        uiimage = new SubImage(this, imagename,
                               MythRect(scaled), displayuntil);
        if (uiimage)
        {
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(scaled));
            SetElementAdded();
        }
        image->DecrRef();
        image = NULL;
    }
    if (uiimage)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Display %1AV sub until %2ms")
           .arg(forced ? "FORCED " : "")
           .arg(displayuntil));
        if (late > 50)
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("AV Sub was %1ms late").arg(late));
    }

    return (ysplit + 1);
}

void SubtitleScreen::DisplayTextSubtitles(void)
{
    if (!m_player || !m_subreader)
        return;

    bool changed = (m_textFontZoom != m_textFontZoomPrev);
    changed |= (m_textFontDelayMs != m_textFontDelayMsPrev);
    VideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;
    m_safeArea = vo->GetSafeRect();

    VideoFrame *currentFrame = vo->GetLastShownFrame();
    if (!currentFrame)
        return;

    TextSubtitles *subs = m_subreader->GetTextSubtitles();
    subs->Lock();
    uint64_t playPos = 0;
    int playPosAdj = m_textFontDelayMs;
    if (subs->IsFrameBasedTiming())
    {
        // frame based subtitles get out of synch after running mythcommflag
        // for the file, i.e., the following number is wrong and does not
        // match the subtitle frame numbers:
        playPos = currentFrame->frameNumber;
        playPosAdj /= m_player->GetFrameRate();
    }
    else
    {
        // Use timecodes for time based SRT subtitles. Feeding this into
        // NormalizeVideoTimecode() should adjust for non-zero start times
        // and wraps. For MPEG, wraps will occur just once every 26.5 hours
        // and other formats less frequently so this should be sufficient.
        // Note: timecodes should now always be valid even in the case
        // when a frame doesn't have a valid timestamp. If an exception is
        // found where this is not true then we need to use the frameNumber
        // when timecode is not defined by uncommenting the following lines.
        //if (currentFrame->timecode == 0)
        //    playPos = (uint64_t)
        //        ((currentFrame->frameNumber / video_frame_rate) * 1000);
        //else
        //playPos = m_player->GetDecoder()->NormalizeVideoTimecode(currentFrame->timecode);
        //
        // Change of plans.  Just ask the player how many milliseconds
        // have been played so far.
        playPos = m_player->GetSecondsPlayed(false, 1);
    }
    playPos -= playPosAdj;
    if (playPos != 0)
        changed |= subs->HasSubtitleChanged(playPos);
    if (!changed)
    {
        subs->Unlock();
        return;
    }

    SetElementDeleted();
    DeleteAllChildren();

    if (playPos == 0)
    {
        subs->Unlock();
        return;
    }

    QStringList rawsubs = subs->GetSubtitles(playPos);
    if (rawsubs.empty())
    {
        subs->Unlock();
        return;
    }

    subs->Unlock();
    DrawTextSubtitles(rawsubs, 0, 0);
}

void SubtitleScreen::DisplayRawTextSubtitles(void)
{
    if (!m_player || !m_subreader)
        return;

    uint64_t duration;
    QStringList subs = m_subreader->GetRawTextSubtitles(duration);
    if (subs.empty())
        return;

    VideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    VideoFrame *currentFrame = vo->GetLastShownFrame();
    if (!currentFrame)
        return;

    m_safeArea = vo->GetSafeRect();

    // delete old subs that may still be on screen
    DeleteAllChildren();
    SetElementDeleted();
    DrawTextSubtitles(subs, currentFrame->timecode, duration);
}

void SubtitleScreen::DrawTextSubtitles(const QStringList &subs,
                                       uint64_t start, uint64_t duration)
{
    FormattedTextSubtitleSRT *fsub =
        new FormattedTextSubtitleSRT(m_family, m_safeArea, start, duration,
                                     this, subs);
    m_qInited.append(fsub);
}

void SubtitleScreen::DisplayCC608Subtitles(void)
{
    if (!m_608reader)
        return;

    bool changed = (m_textFontZoom != m_textFontZoomPrev);

    if (!m_player || !m_player->GetVideoOutput())
        return;
    m_safeArea = m_player->GetVideoOutput()->GetSafeRect();

    CC608Buffer* textlist = m_608reader->GetOutputText(changed);
    if (!changed)
        return;

    SetElementDeleted();
    DeleteAllChildren();

    if (!textlist)
        return;

    QMutexLocker locker(&textlist->lock);

    if (textlist->buffers.empty())
        return;

    FormattedTextSubtitle608 *fsub =
        new FormattedTextSubtitle608(textlist->buffers, m_family,
                                     m_safeArea, this/*, m_textFontZoom*/);
    m_qInited.append(fsub);
}

void SubtitleScreen::DisplayCC708Subtitles(void)
{
    if (!m_708reader || !m_player || !m_player->GetVideoOutput())
        return;

    CC708Service *cc708service = m_708reader->GetCurrentService();
    float video_aspect = 1.77777f;
    bool changed = false;
    video_aspect = m_player->GetVideoAspect();
    QRect oldsafe = m_safeArea;
    m_safeArea = m_player->GetVideoOutput()->GetSafeRect();
    changed = (oldsafe != m_safeArea || m_textFontZoom != m_textFontZoomPrev);
    if (changed)
    {
        for (int i = 0; i < k708MaxWindows; i++)
            cc708service->windows[i].SetChanged();
    }

    uint64_t clearMask = 0;
    QList<FormattedTextSubtitle *> addList;
    for (int i = 0; i < k708MaxWindows; i++)
    {
        CC708Window &win = cc708service->windows[i];
        if (win.GetExists() && win.GetVisible() && !win.GetChanged())
            continue;
        if (!win.GetChanged())
            continue;

        clearMask |= (1ul << i);
        win.ResetChanged();
        if (!win.GetExists() || !win.GetVisible())
            continue;

        QMutexLocker locker(&win.lock);
        vector<CC708String*> list = win.GetStrings();
        if (!list.empty())
        {
            FormattedTextSubtitle708 *fsub =
                new FormattedTextSubtitle708(win, i, list, m_family,
                                             m_safeArea,
                                             this, video_aspect);
            addList.append(fsub);
        }
        win.DisposeStrings(list);
    }
    if (clearMask)
    {
        Clear708Cache(clearMask);
    }
    if (addList.size())
        m_qInited.append(addList);
}

void SubtitleScreen::AddScaledImage(QImage &img, QRect &pos)
{
    VideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    QRect scaled = vo->GetImageRect(pos);
    if (scaled.size() != pos.size())
    {
        img = img.scaled(scaled.width(), scaled.height(),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    MythPainter *osd_painter = vo->GetOSDPainter();
    MythImage* image = NULL;
    if (osd_painter)
         image = osd_painter->GetFormatImage();

    if (image)
    {
        image->Assign(img);
        MythUIImage *uiimage = new SubImage(this, "dvd_button", MythRect(scaled), 0);
        if (uiimage)
        {
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(scaled));
            SetElementAdded();
        }
        image->DecrRef();
    }
}

#ifdef USING_LIBASS
static void myth_libass_log(int level, const char *fmt, va_list vl, void *ctx)
{
    static QString full_line("libass:");
    static const int msg_len = 255;
    static QMutex string_lock;
    uint64_t verbose_mask = VB_GENERAL;
    LogLevel_t verbose_level = LOG_INFO;

    switch (level)
    {
        case 0: //MSGL_FATAL
            verbose_level = LOG_EMERG;
            break;
        case 1: //MSGL_ERR
            verbose_level = LOG_ERR;
            break;
        case 2: //MSGL_WARN
            verbose_level = LOG_WARNING;
            break;
        case 4: //MSGL_INFO
            verbose_level = LOG_INFO;
            break;
        case 6: //MSGL_V
        case 7: //MSGL_DBG2
            verbose_level = LOG_DEBUG;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_mask, verbose_level))
        return;

    string_lock.lock();

    char str[msg_len+1];
    int bytes = vsnprintf(str, msg_len+1, fmt, vl);
    // check for truncated messages and fix them
    if (bytes > msg_len)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("libASS log output truncated %1 of %2 bytes written")
                .arg(msg_len).arg(bytes));
        str[msg_len-1] = '\n';
    }

    full_line += QString(str);
    if (full_line.endsWith("\n"))
    {
        LOG(verbose_mask, verbose_level, full_line.trimmed());
        full_line.truncate(0);
    }
    string_lock.unlock();
}

bool SubtitleScreen::InitialiseAssLibrary(void)
{
    if (m_assLibrary && m_assRenderer)
        return true;

    if (!m_assLibrary)
    {
        m_assLibrary = ass_library_init();
        if (!m_assLibrary)
            return false;

        ass_set_message_cb(m_assLibrary, myth_libass_log, NULL);
        ass_set_extract_fonts(m_assLibrary, true);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialised libass object.");
    }

    LoadAssFonts();

    if (!m_assRenderer)
    {
        m_assRenderer = ass_renderer_init(m_assLibrary);
        if (!m_assRenderer)
            return false;

        ass_set_fonts(m_assRenderer, NULL, "sans-serif", 1, NULL, 1);
        ass_set_hinting(m_assRenderer, ASS_HINTING_LIGHT);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialised libass renderer.");
    }

    return true;
}

void SubtitleScreen::LoadAssFonts(void)
{
    if (!m_assLibrary || !m_player)
        return;

    uint count = m_player->GetDecoder()->GetTrackCount(kTrackTypeAttachment);
    if (m_assFontCount == count)
        return;

    ass_clear_fonts(m_assLibrary);
    m_assFontCount = 0;

    // TODO these need checking and/or reinitialising after a stream change
    for (uint i = 0; i < count; ++i)
    {
        QByteArray filename;
        QByteArray font;
        m_player->GetDecoder()->GetAttachmentData(i, filename, font);
        ass_add_font(m_assLibrary, filename.data(), font.data(), font.size());
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Retrieved font '%1'")
            .arg(filename.constData()));
        m_assFontCount++;
    }
}

void SubtitleScreen::CleanupAssLibrary(void)
{
    CleanupAssTrack();

    if (m_assRenderer)
        ass_renderer_done(m_assRenderer);
    m_assRenderer = NULL;

    if (m_assLibrary)
    {
        ass_clear_fonts(m_assLibrary);
        m_assFontCount = 0;
        ass_library_done(m_assLibrary);
    }
    m_assLibrary = NULL;
}

void SubtitleScreen::InitialiseAssTrack(int tracknum)
{
    if (!InitialiseAssLibrary() || !m_player)
        return;

    if (tracknum == m_assTrackNum && m_assTrack)
        return;

    LoadAssFonts();
    CleanupAssTrack();
    m_assTrack = ass_new_track(m_assLibrary);
    m_assTrackNum = tracknum;

    QByteArray header = m_player->GetDecoder()->GetSubHeader(tracknum);
    if (!header.isNull())
        ass_process_codec_private(m_assTrack, header.data(), header.size());

    m_safeArea = m_player->GetVideoOutput()->GetMHEGBounds();
    ResizeAssRenderer();
}

void SubtitleScreen::CleanupAssTrack(void)
{
    if (m_assTrack)
        ass_free_track(m_assTrack);
    m_assTrack = NULL;
}

void SubtitleScreen::AddAssEvent(char *event)
{
    if (m_assTrack && event)
        ass_process_data(m_assTrack, event, strlen(event));
}

void SubtitleScreen::ResizeAssRenderer(void)
{
    // TODO this probably won't work properly for anamorphic content and XVideo
    ass_set_frame_size(m_assRenderer, m_safeArea.width(), m_safeArea.height());
    ass_set_margins(m_assRenderer, 0, 0, 0, 0);
    ass_set_use_margins(m_assRenderer, true);
    ass_set_font_scale(m_assRenderer, 1.0);
}

void SubtitleScreen::RenderAssTrack(uint64_t timecode)
{
    if (!m_player || !m_assRenderer || !m_assTrack)
        return;

    VideoOutput *vo = m_player->GetVideoOutput();
    if (!vo )
        return;

    QRect oldscreen = m_safeArea;
    m_safeArea = vo->GetMHEGBounds();
    if (oldscreen != m_safeArea)
        ResizeAssRenderer();

    int changed = 0;
    ASS_Image *images = ass_render_frame(m_assRenderer, m_assTrack,
                                         timecode, &changed);
    if (!changed)
        return;

    MythPainter *osd_painter = vo->GetOSDPainter();
    if (!osd_painter)
        return;

    int count = 0;
    SetElementDeleted();
    DeleteAllChildren();
    while (images)
    {
        if (images->w == 0 || images->h == 0)
        {
            images = images->next;
            continue;
        }

        uint8_t alpha = images->color & 0xFF;
        uint8_t blue = images->color >> 8 & 0xFF;
        uint8_t green = images->color >> 16 & 0xFF;
        uint8_t red = images->color >> 24 & 0xFF;

        if (alpha == 255)
        {
            images = images->next;
            continue;
        }

        QSize img_size(images->w, images->h);
        QRect img_rect(images->dst_x,images->dst_y,
                       images->w, images->h);
        QImage qImage(img_size, QImage::Format_ARGB32);
        qImage.fill(0x00000000);

        unsigned char *src = images->bitmap;
        for (int y = 0; y < images->h; ++y)
        {
            for (int x = 0; x < images->w; ++x)
            {
                uint8_t value = src[x];
                if (value)
                {
                    uint32_t pixel = (value * (255 - alpha) / 255 << 24) |
                                     (red << 16) | (green << 8) | blue;
                    qImage.setPixel(x, y, pixel);
                }
            }
            src += images->stride;
        }

        MythImage* image = NULL;
        SubImage *uiimage = NULL;

        if (osd_painter)
            image = osd_painter->GetFormatImage();

        if (image)
        {
            image->Assign(qImage);
            QString name = QString("asssub%1").arg(count);
            uiimage = new SubImage(this, name, MythRect(img_rect), 0);
            if (uiimage)
            {
                uiimage->SetImage(image);
                uiimage->SetArea(MythRect(img_rect));
                SetElementAdded();
            }
            image->DecrRef();
        }
        images = images->next;
        count++;
    }
}
#endif // USING_LIBASS
