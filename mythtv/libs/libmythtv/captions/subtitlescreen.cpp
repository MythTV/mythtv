#include <algorithm>

#include <QFontMetrics>
#include <QRegularExpression>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythfontproperties.h"
#include "libmythui/mythpainter.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuishape.h"
#include "libmythui/mythuisimpletext.h"

#include "captions/subtitlescreen.h"

#define LOC      QString("Subtitles: ")
#define LOC_WARN QString("Subtitles Warning: ")

#ifdef DEBUG_SUBTITLES
static QString toString(MythVideoFrame const * const frame)
{
    std::chrono::milliseconds time = frame->m_timecode;
    return QString("%1(%2)")
        .arg(MythDate::formatTime(time,"HH:mm:ss.zzz"))
        .arg(time.count());
}
#endif

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
               std::chrono::milliseconds expireTime,
               int whichImageCache = -1) :
        m_swOrigArea(rect),
        m_swWhichImageCache(whichImageCache),
        m_swExpireTime(expireTime)
    {
    }
public:
    std::chrono::milliseconds GetExpireTime(void) const { return m_swExpireTime; }
    MythRect GetOrigArea(void) const { return m_swOrigArea; }
    int GetWhichImageCache(void) const { return m_swWhichImageCache; }
protected:
    // Returns true if the object was deleted.
    const MythRect m_swOrigArea;
    const int m_swWhichImageCache; // cc708 only; -1 = none
    const std::chrono::milliseconds m_swExpireTime; // avsubs only; -1 = none
};

class SubSimpleText : public MythUISimpleText, public SubWrapper
{
public:
    SubSimpleText(const QString &text, const MythFontProperties &font,
                  QRect rect, Qt::Alignment align,
                  MythUIType *parent, const QString &name,
                  int whichImageCache, std::chrono::milliseconds expireTime) :
        MythUISimpleText(text, font, rect, align, parent, name),
        SubWrapper(MythRect(rect), expireTime, whichImageCache) {}
};

class SubShape : public MythUIShape, public SubWrapper
{
public:
    SubShape(MythUIType *parent, const QString &name, const MythRect &area,
             int whichImageCache, std::chrono::milliseconds expireTime) :
        MythUIShape(parent, name),
        SubWrapper(area, expireTime, whichImageCache) {}
};

class SubImage : public MythUIImage, public SubWrapper
{
public:
    SubImage(MythUIType *parent, const QString &name, const MythRect &area,
             std::chrono::milliseconds expireTime) :
        MythUIImage(parent, name),
        SubWrapper(area, expireTime) {}
    MythImage *GetImage(void) { return m_images[0]; }
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
    SubtitleFormat(void) = default;
    ~SubtitleFormat(void);
    MythFontProperties *GetFont(const QString &family,
                                const CC708CharacterAttribute &attr,
                                int pixelSize, int zoom, int stretch);
    SubShape *GetBackground(MythUIType *parent, const QString &name,
                            const QString &family,
                            const CC708CharacterAttribute &attr,
                            const MythRect &area,
                            int whichImageCache,
                            std::chrono::milliseconds start,
                            std::chrono::milliseconds duration);
    int GetBackgroundAlpha(const QString &family);
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

static QString srtColorString(const QColor& color)
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
        .arg(static_cast<int>(f->GetFace()->italic()))
        .arg(f->GetFace()->weight())
        .arg(static_cast<int>(f->GetFace()->underline()));
    QPoint offset;
    QColor color;
    int alpha = 0;
    int size = 0;
    f->GetShadow(offset, color, alpha);
    result += QString(" shadow=%1 shadowoffset=%2 "
                      "shadowcolor=%3 shadowalpha=%4")
        .arg(QString::number(static_cast<int>(f->hasShadow())),
             QString("(%1,%2)").arg(offset.x()).arg(offset.y()),
             srtColorString(color),
             QString::number(alpha));
    f->GetOutline(color, size, alpha);
    result += QString(" outline=%1 outlinecolor=%2 "
                      "outlinesize=%3 outlinealpha=%4")
        .arg(static_cast<int>(f->hasOutline()))
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
    float scale = zoom / 100.0F;
    if ((attr.m_penSize & 0x3) == k708AttrSizeSmall)
        scale = scale * 32 / 42;
    else if ((attr.m_penSize & 0x3) == k708AttrSizeLarge)
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
        result->GetFace()->setItalic(attr.m_italics);
    if (IsUnlocked(prefix, kSubAttrUnderline))
        result->GetFace()->setUnderline(attr.m_underline);
    if (IsUnlocked(prefix, kSubAttrBold))
        result->GetFace()->setBold(attr.m_boldface);
    if (IsUnlocked(prefix, kSubAttrColor))
        result->SetColor(attr.GetFGColor());

    MythPoint offset;
    QColor color;
    int alpha = 0;
    bool shadow = result->hasShadow();
    result->GetShadow(offset, color, alpha);
    if (IsUnlocked(prefix, kSubAttrShadowcolor))
        color = attr.GetEdgeColor();
    if (IsUnlocked(prefix, kSubAttrShadowalpha))
        alpha = attr.GetFGAlpha();
    if (IsUnlocked(prefix, kSubAttrShadowoffset))
    {
        int off = lroundf(scale * pixelSize / 20);
        offset = QPoint(off, off);
        if (attr.m_edgeType == k708AttrEdgeLeftDropShadow)
        {
            shadow = true;
            offset.setX(-off);
        }
        else if (attr.m_edgeType == k708AttrEdgeRightDropShadow)
        {
            shadow = true;
        }
        else
        {
            shadow = false;
        }
    }
    else
    {
        offset = m_shadowOffsetMap[prefix];
        offset.NormPoint();
        offset.setX(lroundf(offset.x() * scale));
        offset.setY(lroundf(offset.y() * scale));
    }
    result->SetShadow(shadow, offset, color, alpha);

    int off = 0;
    bool outline = result->hasOutline();
    result->GetOutline(color, off, alpha);
    if (IsUnlocked(prefix, kSubAttrOutlinecolor))
        color = attr.GetEdgeColor();
    if (IsUnlocked(prefix, kSubAttrOutlinealpha))
        alpha = attr.GetFGAlpha();
    if (IsUnlocked(prefix, kSubAttrOutlinesize))
    {
        if (attr.m_edgeType == k708AttrEdgeUniform ||
            attr.m_edgeType == k708AttrEdgeRaised  ||
            attr.m_edgeType == k708AttrEdgeDepressed)
        {
            outline = true;
            off = lroundf(scale * pixelSize / 20);
        }
        else
        {
            outline = false;
        }
    }
    else
    {
        off = m_outlineSizeMap[prefix];
        MythPoint point(off, off);
        point.NormPoint();
        off = lroundf(point.x() * scale);
    }
    result->SetOutline(outline, color, off, alpha);

    LOG(VB_VBI, LOG_DEBUG,
        QString("GetFont(family=%1, prefix=%2, orig pixelSize=%3, "
                "new pixelSize=%4 zoom=%5) = %6")
        .arg(family, prefix).arg(origPixelSize).arg(pixelSize)
        .arg(zoom).arg(fontToString(result)));
    return result;
}

SubtitleFormat::~SubtitleFormat(void)
{
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (int i = 0; i < m_cleanup.size(); ++i)
    {
        m_cleanup[i]->DeleteAllChildren();
        m_cleanup[i]->deleteLater();
        m_cleanup[i] = nullptr; // just to be safe
    }
}

QString SubtitleFormat::MakePrefix(const QString &family,
                                   const CC708CharacterAttribute &attr)
{
    if (family == kSubFamily708)
        return family + "_" + QString::number(attr.m_fontTag & 0x7);
    return family;
}

void SubtitleFormat::CreateProviderDefault(const QString &family,
                                           const CC708CharacterAttribute &attr,
                                           MythUIType *parent,
                                           bool isComplement,
                                           MythFontProperties **returnFont,
                                           MythUIShape **returnBg)
{
    auto *font = new MythFontProperties();
    auto *bg = new MythUIShape(parent, kSubProvider);
    if (family == kSubFamily608)
    {
        font->GetFace()->setFamily("FreeMono");
        QBrush brush(Qt::black);
        bg->SetFillBrush(brush);
        bg->SetLinePen(QPen(brush, 0));
    }
    else if (family == kSubFamily708)
    {
        static const std::array<const std::string,8> s_cc708Fonts {
            "FreeMono",        // default
            "FreeMono",        // mono serif
            "Droid Serif",     // prop serif
            "Droid Sans Mono", // mono sans
            "Droid Sans",      // prop sans
            "Purisa",          // casual
            "TeX Gyre Chorus", // cursive
            "Droid Serif"      // small caps, QFont::SmallCaps will be applied
        };
        font->GetFace()->setFamily(QString::fromStdString(s_cc708Fonts[attr.m_fontTag & 0x7]));
    }
    else if (family == kSubFamilyText)
    {
        font->GetFace()->setFamily("Droid Sans");
        QBrush brush(Qt::black);
        bg->SetFillBrush(brush);
        bg->SetLinePen(QPen(brush, 0));
    }
    else if (family == kSubFamilyTeletext)
    {
        font->GetFace()->setFamily("FreeMono");
        // Set up a background with default alpha=100% so that the theme can
        // override the background alpha.
        QBrush brush(Qt::black);
        bg->SetFillBrush(brush);
        bg->SetLinePen(QPen(brush, 0));
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
    int alpha = 0;
    int size = 0;
    QFont *face = font->GetFace();
    face->setItalic(!face->italic());
    face->setPixelSize(face->pixelSize() + 1);
    face->setUnderline(!face->underline());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    face->setWeight((face->weight() + 1) % 32);
#else
    // qt6: Weight has become an enum. Leave it alone.
#endif
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
    auto *baseParent = new MythUIType(nullptr, "base");
    m_cleanup += baseParent;
    MythFontProperties *providerBaseFont = nullptr;
    MythUIShape *providerBaseShape = nullptr;
    CreateProviderDefault(family, attr, baseParent, false,
                          &providerBaseFont, &providerBaseShape);

    // Widgets for the "negative" values
    auto *negParent = new MythUIType(nullptr, "base");
    m_cleanup += negParent;
    MythFontProperties *negFont = nullptr;
    MythUIShape *negBG = nullptr;
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
    auto *resultBG = dynamic_cast<MythUIShape *>(baseParent->GetChild(prefix));

    // The providerBaseShape object is not leaked here.  It is added
    // to a list of children in its base class constructor.
    if (!resultBG)
        resultBG = providerBaseShape;
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    MythFontProperties *testFont = negParent->GetFont(prefix);
    if (!testFont)
        testFont = negFont;
    auto *testBG = dynamic_cast<MythUIShape *>(negParent->GetChild(prefix));
    if (!testBG)
        testBG = negBG;
    if (family == kSubFamily708 &&
        (attr.m_fontTag & 0x7) == k708AttrFontSmallCaps)
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
    int alpha = 0;
    int size = 0;
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
        QPoint offset1;
        QPoint offset2;
        QColor color1;
        QColor color2;
        int alpha1 = 0;
        int alpha2 = 0;
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
        QColor color1;
        QColor color2;
        int size1 = 0;
        int size2 = 0;
        int alpha1 = 0;
        int alpha2 = 0;
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
    for (auto i = result.constBegin(); i != result.constEnd(); ++i)
        values += " " + (*i);
    LOG(VB_VBI, LOG_INFO,
        QString("Subtitle family %1 allows provider to change:%2")
        .arg(MakePrefix(family, attr), values));

    return result;
}

SubShape *
SubtitleFormat::GetBackground(MythUIType *parent, const QString &name,
                              const QString &family,
                              const CC708CharacterAttribute &attr,
                              const MythRect &area,
                              int whichImageCache,
                              std::chrono::milliseconds start,
                              std::chrono::milliseconds duration)
{
    QString prefix = MakePrefix(family, attr);
    if (!m_shapeMap.contains(prefix))
        Load(family, attr);
    if (m_shapeMap[prefix]->GetAlpha() == 0)
        return nullptr;
    auto *result = new SubShape(parent, name, area, whichImageCache,
                                start + duration);
    result->CopyFrom(m_shapeMap[prefix]);
    if (family == kSubFamily708)
    {
        if (IsUnlocked(prefix, kSubAttrBGfill))
        {
            QBrush brush(attr.GetBGColor());
            result->SetFillBrush(brush);
            result->SetLinePen(QPen(brush, 0));
        }
    }
    else if (family == kSubFamilyTeletext)
    {
        // add code here when teletextscreen.cpp is refactored
    }
    LOG(VB_VBI, LOG_DEBUG,
        QString("GetBackground(prefix=%1) = "
                "{type=%2 alpha=%3 brushstyle=%4 brushcolor=%5}")
        .arg(prefix, result->m_type, QString::number(result->GetAlpha()),
             QString::number(result->m_fillBrush.style()),
             srtColorString(result->m_fillBrush.color())));
    return result;
}

int SubtitleFormat::GetBackgroundAlpha(const QString &family)
{
    // This is a "temporary" hack for allowing teletextscreen.cpp to get the
    // background alpha value from osd_subtitle.xml.
    CC708CharacterAttribute attr(false, false, false, Qt::white);
    SubShape *bgShape = GetBackground(nullptr, "dummyName", family, attr,
                                      MythRect(), -1, 0ms, -1ms);
    return bgShape->m_fillBrush.color().alpha();
}

////////////////////////////////////////////////////////////////////////////

QSize FormattedTextChunk::CalcSize(float layoutSpacing) const
{
    return m_parent->CalcTextSize(m_text, m_format, layoutSpacing);
}

void FormattedTextChunk::CalcPadding(bool isFirst, bool isLast,
                                     int &left, int &right) const
{
    m_parent->CalcPadding(m_format, isFirst, isLast, left, right);
}

bool FormattedTextChunk::Split(FormattedTextChunk &newChunk)
{
    LOG(VB_VBI, LOG_INFO,
        QString("Attempting to split chunk '%1'").arg(m_text));
    int lastSpace = m_text.lastIndexOf(' ', -2); // -2 to ignore trailing space
    if (lastSpace < 0)
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Failed to split chunk '%1'").arg(m_text));
        return false;
    }
    newChunk.m_parent = m_parent;
    newChunk.m_format = m_format;
    newChunk.m_text = m_text.mid(lastSpace + 1).trimmed() + ' ';
    m_text = m_text.left(lastSpace).trimmed();
    LOG(VB_VBI, LOG_INFO,
        QString("Split chunk into '%1' + '%2'").arg(m_text, newChunk.m_text));
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
        .arg(m_format.m_edgeType);
    str += QString("off=%1 pensize=%2 ")
        .arg(m_format.m_offset)
        .arg(m_format.m_penSize);
    str += QString("it=%1 ul=%2 bf=%3 ")
        .arg(static_cast<int>(m_format.m_italics))
        .arg(static_cast<int>(m_format.m_underline))
        .arg(static_cast<int>(m_format.m_boldface));
    str += QString("font=%1 ").arg(m_format.m_fontTag);
    str += QString(" text='%1'").arg(m_text);
    return str;
}

bool FormattedTextChunk::PreRender(bool isFirst, bool isLast,
                                   int &x, int y, int height)
{
    m_textFont = m_parent->GetFont(m_format);
    if (!m_textFont)
        return false;
    QFontMetrics font(*(m_textFont->GetFace()));
    // If the chunk starts with whitespace, the leading whitespace
    // ultimately gets lost due to the text.trimmed() operation in the
    // MythUISimpleText constructor.  To compensate, we manually
    // indent the chunk accordingly.
    int count = 0;
    while (count < m_text.length() && m_text.at(count) == ' ')
    {
        ++count;
    }
    int x_adjust = count * font.horizontalAdvance(" ");
    int leftPadding = 0;
    int rightPadding = 0;
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
    m_bgShapeName = QString("subbg%1x%2@%3,%4")
        .arg(chunk_sz.width()).arg(height).arg(x).arg(y);
    m_bgShapeRect = bgrect;
    // Shift to the right to account for leading spaces that
    // are removed by the MythUISimpleText constructor.  Also
    // add in padding at the end to avoid clipping.
    m_textName = QString("subtxt%1x%2@%3,%4")
        .arg(chunk_sz.width()).arg(height).arg(x).arg(y);
    m_textRect = QRect(x + x_adjust, y,
                     chunk_sz.width() - x_adjust + rightPadding, height);

    x += chunk_sz.width();
    return true;
}

QSize FormattedTextLine::CalcSize(float layoutSpacing) const
{
    int height = 0;
    int width = 0;
    int leftPadding = 0;
    int rightPadding = 0;
    QList<FormattedTextChunk>::const_iterator it;
    bool isFirst = true;
    for (it = chunks.constBegin(); it != chunks.constEnd(); ++it)
    {
        bool isLast = (it + 1 == chunks.constEnd());
        QSize tmp = (*it).CalcSize(layoutSpacing);
        height = std::max(height, tmp.height());
        width += tmp.width();
        (*it).CalcPadding(isFirst, isLast, leftPadding, rightPadding);
        if (it == chunks.constBegin())
            width += leftPadding;
        isFirst = false;
    }
    return {width + rightPadding, height};
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
    for (const auto & line : std::as_const(m_lines))
    {
        QSize sz = line.CalcSize(LINE_SPACING);
        anchor_width = std::max(anchor_width, sz.width());
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
    anchor_y = std::clamp(anchor_y, 0, m_safeArea.height() - anchor_height);
    anchor_x = std::clamp(anchor_x, 0, m_safeArea.width() - anchor_width);

    m_bounds = QRect(anchor_x, anchor_y, anchor_width, anchor_height);

    // Fill in missing coordinates
    int y = anchor_y;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (int i = 0; i < m_lines.size(); i++)
    {
        if (m_lines[i].m_xIndent < 0)
            m_lines[i].m_xIndent = anchor_x;
        if (m_lines[i].m_yIndent < 0)
            m_lines[i].m_yIndent = y;
        y += m_lines[i].CalcSize(LINE_SPACING).height();
        // Prune leading all-whitespace chunks.
        while (!m_lines[i].chunks.isEmpty() &&
               m_lines[i].chunks.first().m_text.trimmed().isEmpty())
        {
            m_lines[i].m_xIndent +=
                m_lines[i].chunks.first().CalcSize().width();
            m_lines[i].chunks.removeFirst();
        }
        // Prune trailing all-whitespace chunks.
        while (!m_lines[i].chunks.isEmpty() &&
               m_lines[i].chunks.last().m_text.trimmed().isEmpty())
        {
            m_lines[i].chunks.removeLast();
        }
        // Trim trailing whitespace from last chunk.  (Trimming
        // leading whitespace from all chunks is handled in the Draw()
        // routine.)
        if (!m_lines[i].chunks.isEmpty())
        {
            QString *str = &m_lines[i].chunks.last().m_text;
            int idx = str->length() - 1;
            while (idx >= 0 && str->at(idx) == ' ')
                --idx;
            str->truncate(idx + 1);
        }
    }
}

void FormattedTextSubtitle::PreRender(void)
{
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (int i = 0; i < m_lines.size(); i++)
    {
        int x = m_lines[i].m_xIndent;
        int y = m_lines[i].m_yIndent;
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
    QList<MythUIType *> textList;
    QList<MythUIType *> shapeList;
    for (auto & line : m_lines)
    {
        QList<FormattedTextChunk>::const_iterator chunk;
        for (chunk = line.chunks.constBegin();
             chunk != line.chunks.constEnd();
             ++chunk)
        {
            MythFontProperties *mythfont =
                m_subScreen->GetFont((*chunk).m_format);
            if (!mythfont)
                continue;
            // Note: nullptr is passed as the parent argument to the
            // MythUI constructors so that we can control the drawing
            // order of the children.  In particular, background
            // shapes should be added/drawn first, and text drawn on
            // top.
            if ((*chunk).m_textRect.width() > 0) {
                auto *text = new SubSimpleText((*chunk).m_text, *mythfont,
                                      (*chunk).m_textRect,
                                      Qt::AlignLeft|Qt::AlignTop,
                                      /*m_subScreen*/nullptr,
                                      (*chunk).m_textName, CacheNum(),
                                      m_start + m_duration);
                textList += text;
            }
            if ((*chunk).m_bgShapeRect.width() > 0) {
                MythUIShape *bgshape = m_subScreen->GetSubtitleFormat()->
                    GetBackground(/*m_subScreen*/nullptr,
                                  (*chunk).m_bgShapeName,
                                  m_base, (*chunk).m_format,
                                  MythRect((*chunk).m_bgShapeRect), CacheNum(),
                                  m_start, m_duration);
                if (bgshape)
                {
                    bgshape->SetArea(MythRect((*chunk).m_bgShapeRect));
                    shapeList += bgshape;
                }
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
    for (const auto & ftl : std::as_const(m_lines))
    {
        QString line;
        if (ftl.m_origX > 0)
            line.fill(' ', ftl.m_origX);
        QList<FormattedTextChunk>::const_iterator chunk;
        for (chunk = ftl.chunks.constBegin();
             chunk != ftl.chunks.constEnd();
             ++chunk)
        {
            const QString &text = (*chunk).m_text;
            const CC708CharacterAttribute &attr = (*chunk).m_format;
            bool isBlank = !attr.m_underline && text.trimmed().isEmpty();
            if (!isBlank)
            {
                if (attr.m_boldface)
                    line += "<b>";
                if (attr.m_italics)
                    line += "<i>";
                if (attr.m_underline)
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
                if (attr.m_underline)
                    line += "</u>";
                if (attr.m_italics)
                    line += "</i>";
                if (attr.m_boldface)
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
    static const QRegularExpression htmlTag {
        "</?.+>", QRegularExpression::InvertedGreedinessOption };
    QString htmlPrefix("<font color=\"");
    QString htmlSuffix("\">");
    for (const QString& subtitle : std::as_const(subs))
    {
        FormattedTextLine line;
        QString text(subtitle);
        while (!text.isEmpty())
        {
            auto match = htmlTag.match(text);
            int pos = match.capturedStart();
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
                int htmlLen = match.capturedLength();
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
                {
                    color = Qt::white;
                }
                else if (html == "<b>")
                {
                    isBold = true;
                }
                else if (html == "</b>")
                {
                    isBold = false;
                }
                else if (html == "<u>")
                {
                    isUnderline = true;
                }
                else if (html == "</u>")
                {
                    isUnderline = false;
                }
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
                m_lines += FormattedTextLine(m_lines[i].m_xIndent,
                                             m_lines[i].m_yIndent);
            m_lines[i+1].chunks.prepend(m_lines[i].chunks.takeLast());
            LOG(VB_VBI, LOG_INFO,
                QString("Wrapping chunk to next line: '%1'")
                .arg(m_lines[i+1].chunks[0].m_text));
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
                    m_lines += FormattedTextLine(m_lines[i].m_xIndent,
                                                 m_lines[i].m_yIndent);
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

    // Handle an initial control sequence.
    if (text.length() >= 1 && text[0] >= QChar(0x7000))
    {
        int op = text[0].unicode() - 0x7000;
        isUnderline = ((op & 0x1) != 0);
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
    static const QRegularExpression kControlCharsRE { "[\\x{7000}-\\x{7fff}]" };
    int nextControl = text.indexOf(kControlCharsRE);
    if (nextControl < 0)
    {
        result = text;
        text.clear();
    }
    else
    {
        result = text.left(nextControl);
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
    int totalHeight = 0;
    int totalSpace = 0;
    int firstY = 0;
    int prevY = 0;
    QVector<int> heights(m_lines.size());
    QVector<int> spaceBefore(m_lines.size());
    // Calculate totalHeight and totalSpace
    for (int i = 0; i < m_lines.size(); i++)
    {
        m_lines[i].m_yIndent = std::max(m_lines[i].m_yIndent, prevY); // avoid overlap
        int y = m_lines[i].m_yIndent;
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
    int overage = std::min(totalHeight - safeHeight, totalSpace);

    // Recalculate Y coordinates, applying the shrink factor to space
    // between each line.
    if (overage > 0 && totalSpace > 0)
    {
        float shrink = (totalSpace - overage) / (float)totalSpace;
        prevY = firstY;
        for (int i = 0; i < m_lines.size(); i++)
        {
            m_lines[i].m_yIndent = prevY + (spaceBefore[i] * shrink);
            prevY = m_lines[i].m_yIndent + heights[i];
        }
    }

    // Shift Y coordinates back up into the safe area.
    int shift = std::min(firstY, std::max(0, prevY - safeHeight));
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (int i = 0; i < m_lines.size(); i++)
        m_lines[i].m_yIndent -= shift;

    FormattedTextSubtitle::Layout();
}

void FormattedTextSubtitle608::Init(const std::vector<CC608Text*> &buffers)
{
    static const std::array<const QColor,8> kClr
    {
        Qt::white,   Qt::green,   Qt::blue,    Qt::cyan,
        Qt::red,     Qt::yellow,  Qt::magenta, Qt::white,
    };

    if (buffers.empty())
        return;
    auto i = buffers.cbegin();
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
        CC708CharacterAttribute def_attr(false, false, false, kClr[0]);
        QFont *font = m_subScreen->GetFont(def_attr)->GetFace();
        QFontMetrics fm(*font);
        fontwidth = fm.averageCharWidth();
        xmid = m_safeArea.width() / 2;
        // Disable centering for zoom factor >= 100%
        if (zoom >= 100)
            xscale = m_safeArea.width() / fontwidth;
    }

    for (; i != buffers.cend(); ++i)
    {
        CC608Text *cc = (*i);
        int color = 0;
        bool isItalic = false;
        bool isUnderline = false;
        const bool isBold = false;
        QString text(cc->m_text);

        int orig_x = cc->m_x;
        // position as if we use a fixed size font
        // - font size already has zoom factor applied

        int x = 0;
        if (xmid)
        {
            // center horizontally
            x = xmid + ((orig_x - xscale / 2) * fontwidth);
        }
        else
        {
            // fallback
            x = (orig_x + 3) * m_safeArea.width() / xscale;
        }

        int orig_y = cc->m_y;
        int y = 0;
        if (orig_y < yscale / 2)
        {
            // top half -- anchor up
            y = (orig_y * m_safeArea.height() * zoom / (yscale * 100));
        }
        else
        {
            // bottom half -- anchor down
            y = m_safeArea.height() -
                ((yscale - orig_y - 0.5) * m_safeArea.height() * zoom /
                 (yscale * 100));
        }

        FormattedTextLine line(x, y, orig_x, orig_y);
        while (!text.isNull())
        {
            QString captionText =
                extract_cc608(text, color, isItalic, isUnderline);
            CC708CharacterAttribute attr(isItalic, isBold, isUnderline,
                                         kClr[std::clamp(color, 0, 7)]);
            FormattedTextChunk chunk(captionText, attr, m_subScreen);
            line.chunks += chunk;
            LOG(VB_VBI, LOG_INFO,
                QString("Adding cc608 chunk (%1,%2): %3")
                .arg(cc->m_x).arg(cc->m_y).arg(chunk.ToLogString()));
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
        auto *shape = new SubShape(m_subScreen, QString("cc708bg%1").arg(m_num),
                         MythRect(m_bounds), m_num, m_start + m_duration);
        shape->SetFillBrush(fill);
        shape->SetArea(MythRect(m_bounds));
    }
    // The background is drawn first so that it appears behind
    // everything else.
    FormattedTextSubtitle::Draw();
}

void FormattedTextSubtitle708::Init(const CC708Window &win,
                                    const std::vector<CC708String*> &list,
                                    float aspect)
{
    LOG(VB_VBI, LOG_DEBUG,LOC +
        QString("Display Win %1, Anchor_id %2, x_anch %3, y_anch %4, "
                "relative %5")
            .arg(m_num).arg(win.m_anchor_point).arg(win.m_anchor_horizontal)
            .arg(win.m_anchor_vertical).arg(win.m_relative_pos));
    int pixelSize = m_safeArea.height() / 20;
    if (m_subScreen)
        m_subScreen->SetFontSize(pixelSize);

    float xrange { 160.0F };
    if (win.m_relative_pos)
        xrange = 100.0F;
    else if (aspect > 1.4F)
        xrange = 210.0F;
    float yrange  = win.m_relative_pos ? 100.0F : 75.0F;
    float xmult   = (float)m_safeArea.width() / xrange;
    float ymult   = (float)m_safeArea.height() / yrange;
    uint anchor_x = (uint)(xmult * (float)win.m_anchor_horizontal);
    uint anchor_y = (uint)(ymult * (float)win.m_anchor_vertical);
    m_xAnchorPoint = win.m_anchor_point % 3;
    m_yAnchorPoint = win.m_anchor_point / 3;
    m_xAnchor = anchor_x;
    m_yAnchor = anchor_y;

    for (auto *str708 : list)
    {
        if (str708->m_y >= (uint)m_lines.size())
            m_lines.resize(str708->m_y + 1);
        FormattedTextChunk chunk(str708->m_str, str708->m_attr, m_subScreen);
        m_lines[str708->m_y].chunks += chunk;
        LOG(VB_VBI, LOG_INFO, QString("Adding cc708 chunk: win %1 row %2: %3")
            .arg(m_num).arg(str708->m_y).arg(chunk.ToLogString()));
    }
}

////////////////////////////////////////////////////////////////////////////

SubtitleScreen::SubtitleScreen(MythPlayer *Player, MythPainter *Painter,
                               const QString &Name, int FontStretch)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_player(Player),
    m_fontStretch(FontStretch),
    m_format(new SubtitleFormat)
{
    m_painter = Painter;

    connect(m_player, &MythPlayer::SeekingDone, this, [&]()
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "SeekingDone signal received.");
        m_atEnd = false;
    });
}

SubtitleScreen::~SubtitleScreen(void)
{
    ClearAllSubtitles();
    delete m_format;
#if CONFIG_LIBASS
    CleanupAssLibrary();
#endif
}

void SubtitleScreen::EnableSubtitles(int type, bool forced_only)
{
    int prevType = m_subtitleType;
    m_subtitleType = type;

    if (forced_only)
    {
        if (prevType == kDisplayNone)
        {
            SetElementDeleted();
            SetVisible(true);
            SetArea(MythRect());
        }
    }
    else
    {
        if (m_subreader)
        {
            m_subreader->EnableAVSubtitles(kDisplayAVSubtitle == m_subtitleType);
            m_subreader->EnableTextSubtitles(kDisplayTextSubtitle == m_subtitleType);
            m_subreader->EnableRawTextSubtitles(kDisplayRawTextSubtitle == m_subtitleType);
        }
        if (m_cc608reader)
            m_cc608reader->SetEnabled(kDisplayCC608 == m_subtitleType);
        if (m_cc708reader)
            m_cc708reader->SetEnabled(kDisplayCC708 == m_subtitleType);
        ClearAllSubtitles();
        SetVisible(m_subtitleType != kDisplayNone);
        SetArea(MythRect());
    }
    if (!forced_only || m_family.isEmpty()) {
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
    }
    m_textFontZoomPrev = m_textFontZoom;
    m_textFontDelayMsPrev = m_textFontDelayMs;
    m_textFontMinDurationMsPrev = m_textFontMinDurationMs;
    m_textFontDurationExtensionMsPrev = m_textFontDurationExtensionMs;
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
#if CONFIG_LIBASS
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
    if (m_cc608reader && (kDisplayCC608 == m_subtitleType))
        m_cc608reader->ClearBuffers(true, true);
    if (m_cc708reader && (kDisplayCC708 == m_subtitleType))
        m_cc708reader->ClearBuffers();
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

    MythVideoOutput *vo = m_player->GetVideoOutput();
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
    QImage bg_image(hl_button->data[0], w, h, w, QImage::Format_Indexed8);
    auto *bgpalette = (uint32_t *)(hl_button->data[1]);

    QVector<uint32_t> bg_palette(4);
    for (int i = 0; i < 4; i++)
        bg_palette[i] = bgpalette[i];
    bg_image.setColorTable(bg_palette);

    // copy button region of background image
    const QRect fg_rect(buttonPos.translated(-hl_button->x, -hl_button->y));
    QImage fg_image = bg_image.copy(fg_rect);
    QVector<uint32_t> fg_palette(4);
    auto *fgpalette = (uint32_t *)(dvdButton->rects[1]->data[1]);
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

void SubtitleScreen::SetDelay(std::chrono::milliseconds ms)
{
    m_textFontDelayMs = ms;
}

std::chrono::milliseconds SubtitleScreen::GetDelay(void) const
{
    return m_textFontDelayMs;
}

void SubtitleScreen::Clear708Cache(uint64_t mask)
{
    QList<MythUIType *> list = m_childrenList;
    QList<MythUIType *>::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        MythUIType *child = *it;
        auto *wrapper = dynamic_cast<SubWrapper *>(child);
        if (wrapper)
        {
            int whichImageCache = wrapper->GetWhichImageCache();
            if (whichImageCache != -1 && (mask & (1UL << whichImageCache)))
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
    int alpha = 0;
    int outlineSize = 0;
    int shadowWidth = 0;
    int shadowHeight = 0;
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
        shadowWidth = std::max(shadowWidth, outlineSize);
        shadowHeight = std::max(shadowHeight, outlineSize);
    }
    return {shadowWidth + outlineSize, shadowHeight + outlineSize};
}

QSize SubtitleScreen::CalcTextSize(const QString &text,
                                   const CC708CharacterAttribute &format,
                                   float layoutSpacing) const
{
    MythFontProperties *mythfont = GetFont(format);
    QFont *font = mythfont->GetFace();
    QFontMetrics fm(*font);
    int width = fm.horizontalAdvance(text);
    int height = fm.height() * (1 + PAD_HEIGHT);
    if (layoutSpacing > 0 && !text.trimmed().isEmpty())
        height = std::max(height, (int)(font->pixelSize() * layoutSpacing));
    height += CalcShadowOffsetPadding(mythfont).height();
    return {width, height};
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

int SubtitleScreen::GetTeletextBackgroundAlpha(void)
{
    SubtitleFormat format;
    return format.GetBackgroundAlpha(kSubFamilyTeletext);
}

bool SubtitleScreen::Create(void)
{
    if (!m_player)
        return false;

    m_subreader = m_player->GetSubReader();
    m_cc608reader = m_player->GetCC608Reader();
    m_cc708reader = m_player->GetCC708Reader();
    if (!m_subreader)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to get subtitle reader.");
    if (!m_cc608reader)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to get CEA-608 reader.");
    if (!m_cc708reader)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to get CEA-708 reader.");

    return true;
}

void SubtitleScreen::Pulse(void)
{
    QList<MythUIType *>::iterator it;
    QList<MythUIType *>::iterator itNext;

    MythVideoOutput *videoOut = m_player->GetVideoOutput();
    MythVideoFrame *currentFrame = videoOut ? videoOut->GetLastShownFrame() : nullptr;
    std::chrono::milliseconds now =
        currentFrame ? currentFrame->m_timecode : std::chrono::milliseconds::max();
    bool needRescale = (m_textFontZoom != m_textFontZoomPrev);

    for (it = m_childrenList.begin(); it != m_childrenList.end(); it = itNext)
    {
        itNext = it + 1;
        MythUIType *child = *it;
        auto *wrapper = dynamic_cast<SubWrapper *>(child);
        if (!wrapper)
            continue;

        // Expire the subtitle object if needed.
        std::chrono::milliseconds expireTime = wrapper->GetExpireTime();
        if (expireTime > 0ms && expireTime < now)
        {
            DeleteChild(child);
            SetElementDeleted();
            continue;
        }

        // Rescale the AV subtitle image if the zoom changed.
        if (expireTime > 0ms && needRescale)
        {
            auto *image = dynamic_cast<SubImage *>(child);
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

    if (kDisplayCC608 == m_subtitleType)
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
        delete fsub;
        SetElementAdded();
    }

    OptimiseDisplayedArea();
    MythScreenType::Pulse();
    m_textFontZoomPrev = m_textFontZoom;
    m_textFontDelayMsPrev = m_textFontDelayMs;
    m_textFontMinDurationMsPrev = m_textFontMinDurationMs;
    m_textFontDurationExtensionMsPrev = m_textFontDurationExtensionMs;
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
    QListIterator<MythUIType *> i(m_childrenList);
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        auto *wrapper = dynamic_cast<SubWrapper *>(img);
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
        auto *wrapper = dynamic_cast<SubWrapper *>(img);
        if (wrapper && img->IsVisible())
            img->SetArea(MythRect(wrapper->GetOrigArea().translated(left, top)));
    }
}

void SubtitleScreen::DisplayAVSubtitles(void)
{
    if (!m_player || !m_subreader)
        return;

    AVSubtitles* subs = m_subreader->GetAVSubtitles();
    QMutexLocker lock(&(subs->m_lock));
    if (subs->m_buffers.empty()
      && (kDisplayAVSubtitle != m_subtitleType)
      && (kDisplayTextSubtitle != m_subtitleType))
        return;

    MythVideoOutput *videoOut = m_player->GetVideoOutput();
    MythVideoFrame *currentFrame = videoOut ? videoOut->GetLastShownFrame() : nullptr;
    int ret {0};

    if (!currentFrame || !videoOut)
        return;

    if (subs->m_buffers.empty() && (m_subreader->GetParser() != nullptr))
    {
        if (subs->m_needSync)
        {
            // Seeking on a subtitle stream is a pain. The internals
            // of ff_subtitles_queue_seek() calls search_sub_ts(),
            // which always returns the subtitle that starts on or
            // before the current timestamp.
            //
            // If avformat_seek_file() has been asked to search
            // forward, then the subsequent range check will always
            // fail because avformat_seek_file() has set the minimum
            // timestamp to the requested timestamp.  This call only
            // succeeds if the timestamp matches to the millisecond.
            //
            // If avformat_seek_file() has been asked to search
            // backward then a subtitle will be returned, but because
            // that subtitle is in the past the code below this
            // comment will always consume that subtitle, resulting in
            // a new seek every time this function is called.
            //
            // The solution seems to be to seek backwards so that we
            // get the subtitle that should have most recently been
            // displayed, then skip that subtitle to get the one that
            // should be displayed next.
            lock.unlock();
            m_subreader->SeekFrame(currentFrame->m_timecode.count()*1000,
                                   AVSEEK_FLAG_BACKWARD);
            ret = m_subreader->ReadNextSubtitle();
            if (ret < 0)
            {
                m_atEnd = true;
#ifdef DEBUG_SUBTITLES
                if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
                {
                    LOG(VB_PLAYBACK, LOG_DEBUG,
                        LOC + QString("time %1, no subtitle available after seek")
                        .arg(toString(currentFrame)));
                }
#endif
            }
            lock.relock();
            subs->m_needSync = false;

            // extra check to avoid segfault
            if (subs->m_buffers.empty())
                return;
            AVSubtitle subtitle = subs->m_buffers.front();
            if (subtitle.end_display_time < currentFrame->m_timecode.count())
            {
                subs->m_buffers.pop_front();
#ifdef DEBUG_SUBTITLES
                if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
                {
                    LOG(VB_PLAYBACK, LOG_DEBUG,
                        LOC + QString("time %1, drop %2")
                        .arg(toString(currentFrame), toString(subtitle)));
                }
#endif
            }
        }

        // Always add one subtitle.
        lock.unlock();
        if (!m_atEnd)
        {
            ret = m_subreader->ReadNextSubtitle();
            if (ret < 0)
            {
                m_atEnd = true;
#ifdef DEBUG_SUBTITLES
                if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
                {
                    LOG(VB_PLAYBACK, LOG_DEBUG,
                        LOC + QString("time %1, no subtitle available")
                        .arg(toString(currentFrame)));
                }
#endif
            }
        }
        lock.relock();
    }

    float tmp = 0.0;
    QRect dummy;
    videoOut->GetOSDBounds(dummy, m_safeArea, tmp, tmp, tmp);

    [[maybe_unused]] bool assForceNext {false};
    while (!subs->m_buffers.empty())
    {
        AVSubtitle subtitle = subs->m_buffers.front();
        if (subtitle.start_display_time > currentFrame->m_timecode.count())
        {
#ifdef DEBUG_SUBTITLES
            if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
            {
                LOG(VB_PLAYBACK, LOG_DEBUG,
                    LOC + QString("time %1, next %2")
                    .arg(toString(currentFrame), toString(subtitle)));
            }
#endif
            break;
        }

        // If this is the most recently displayed subtitle and a
        // backward jump means that it needs to be displayed again,
        // the call to ass_render_frame will say there is no work to
        // be done. Force RenderAssTrack to display the subtitle
        // anyway.
        assForceNext = true;

        auto displayfor = std::chrono::milliseconds(subtitle.end_display_time -
                                                    subtitle.start_display_time);
#ifdef DEBUG_SUBTITLES
        if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
        {
            LOG(VB_PLAYBACK, LOG_DEBUG,
                LOC + QString("time %1, show %2")
                .arg(toString(currentFrame), toString(subtitle)));
        }
#endif
        if (displayfor == 0ms)
            displayfor = 60s;
        displayfor = (displayfor < 50ms) ? 50ms : displayfor;
        std::chrono::milliseconds late = currentFrame->m_timecode -
                         std::chrono::milliseconds(subtitle.start_display_time);

        ClearDisplayedSubtitles();
        subs->m_buffers.pop_front();
        for (std::size_t i = 0; i < subtitle.num_rects; ++i)
        {
            AVSubtitleRect* rect = subtitle.rects[i];

            bool displaysub = true;
            if (!subs->m_buffers.empty() &&
                subs->m_buffers.front().end_display_time <
                currentFrame->m_timecode.count())
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
                if (subs->m_fixPosition || (currentFrame->m_height < bottom) ||
                    (currentFrame->m_width  < right) ||
                    !display.width() || !display.height())
                {
                    int sd_height = 576;
                    if ((m_player->GetFrameRate() > 26.0F ||
                         m_player->GetFrameRate() < 24.0F) && bottom <= 480)
                        sd_height = 480;

                    int height { 1080 };
                    if ((currentFrame->m_height <= sd_height) &&
                        (bottom <= sd_height))
                        height = sd_height;
                    else if ((currentFrame->m_height <= 720) && bottom <= 720)
                        height = 720;

                    int width { 1920 };
                    if ((currentFrame->m_width  <= 720) && (right <= 720))
                        width = 720;
                    else if ((currentFrame->m_width  <= 1280) &&
                             (right <= 1280))
                        width = 1280;
                    display = QRect(0, 0, width, height);
                }

                // split into upper/lower to allow zooming
                QRect bbox;
                int uh = (display.height() / 2) - rect->y;
                std::chrono::milliseconds displayuntil = currentFrame->m_timecode + displayfor;
                if (uh > 0)
                {
                    bbox = QRect(0, 0, rect->w, uh);
                    uh = DisplayScaledAVSubtitles(rect, bbox, true, display,
                                                  rect->flags & AV_SUBTITLE_FLAG_FORCED,
                                                  QString("avsub%1t").arg(i),
                                                  displayuntil, late);
                }
                else
                {
                    uh = 0;
                }
                int lh = rect->h - uh;
                if (lh > 0)
                {
                    bbox = QRect(0, uh, rect->w, lh);
                    DisplayScaledAVSubtitles(rect, bbox, false, display,
                                             rect->flags & AV_SUBTITLE_FLAG_FORCED,
                                             QString("avsub%1b").arg(i),
                                             displayuntil, late);
                }
            }
#if CONFIG_LIBASS
            else if (displaysub && rect->type == SUBTITLE_ASS)
            {
                InitialiseAssTrack(m_player->GetDecoder()->GetTrack(kTrackTypeSubtitle));
                AddAssEvent(rect->ass, subtitle.start_display_time, subtitle.end_display_time);
            }
#endif
        }
        SubtitleReader::FreeAVSubtitle(subtitle);
    }
#if CONFIG_LIBASS
    RenderAssTrack(currentFrame->m_timecode, assForceNext);
#endif
}

int SubtitleScreen::DisplayScaledAVSubtitles(const AVSubtitleRect *rect,
                                             QRect &bbox, bool top,
                                             QRect &display, int forced,
                                             const QString& imagename,
                                             std::chrono::milliseconds displayuntil,
                                             std::chrono::milliseconds late)
{
    // split image vertically if it spans middle of display
    // - split point is empty line nearest the middle
    // crop image to reduce scaling time
    bool prev_empty = false;

    // initialize to opposite edges
    int xmin = bbox.right();
    int xmax = bbox.left();
    int ymin = bbox.bottom();
    int ymax = bbox.top();
    int ylast = bbox.top();
    int ysplit = bbox.bottom();

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
                rect->data[0][(y * rect->linesize[0]) + x];
            const uint32_t pixel = *((uint32_t *)rect->data[1] + color);
            if (pixel & 0xff000000)
            {
                empty = false;
                xmin = std::min(x, xmin);
                xmax = std::max(x, xmax);
            }
        }

        if (!empty)
        {
            ymin = std::min(y, ymin);
            ymax = std::max(y, ymax);
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
                rect->data[0][(ysrc * rect->linesize[0]) + xsrc];
            const uint32_t pixel = *((uint32_t *)rect->data[1] + color);
            qImage.setPixel(x, y, pixel);
        }
    }

    // translate to absolute coordinates
    bbox.translate(rect->x, rect->y);

    // scale and move according to zoom factor
    bbox.setWidth(bbox.width() * m_textFontZoom / 100);
    bbox.setHeight(bbox.height() * m_textFontZoom / 100);

    MythVideoOutput *videoOut = m_player->GetVideoOutput();
    QRect scaled = videoOut->GetImageRect(bbox, &display);

    if (scaled.size() != orig_rect.size())
        qImage = qImage.scaled(scaled.width(), scaled.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    int hsize = m_safeArea.width();
    int vsize = m_safeArea.height();

    scaled.moveLeft(((100 - m_textFontZoom) * hsize / 2 + m_textFontZoom * scaled.left()) / 100);
    if (top)
    {
        // anchor up
        scaled.moveTop(scaled.top() * m_textFontZoom / 100);
    }
    else
    {
        // anchor down
        scaled.moveTop(((100 - m_textFontZoom) * vsize + m_textFontZoom * scaled.top()) / 100);
    }

    MythImage* image = m_painter->GetFormatImage();
    SubImage *uiimage = nullptr;

    if (image)
    {
        image->Assign(qImage);
        uiimage = new SubImage(this, imagename, MythRect(scaled), displayuntil);
        if (uiimage)
        {
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(scaled));
            SetElementAdded();
        }
        image->DecrRef();
        image = nullptr;
    }

    if (uiimage)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Display %1AV sub until %2ms")
            .arg(forced ? "FORCED " : "").arg(displayuntil.count()));
        if (late > 50ms)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("AV Sub was %1ms late").arg(late.count()));
    }

    return (ysplit + 1);
}

void SubtitleScreen::DisplayRawTextSubtitles(void)
{
    if (!m_player || !m_subreader)
        return;

    std::chrono::milliseconds duration = 0ms;
    QStringList subs = m_subreader->GetRawTextSubtitles(duration);
    if (subs.empty())
        return;

    MythVideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    MythVideoFrame *currentFrame = vo->GetLastShownFrame();
    if (!currentFrame)
        return;

    m_safeArea = vo->GetSafeRect();

    // delete old subs that may still be on screen
    DeleteAllChildren();
    SetElementDeleted();
    DrawTextSubtitles(subs, currentFrame->m_timecode, duration);
}

void SubtitleScreen::DrawTextSubtitles(const QStringList &subs,
                                       std::chrono::milliseconds start,
                                       std::chrono::milliseconds duration)
{
    auto *fsub = new FormattedTextSubtitleSRT(m_family, m_safeArea, start,
                                              duration, this, subs);
    m_qInited.append(fsub);
}

void SubtitleScreen::DisplayCC608Subtitles(void)
{
    if (!m_cc608reader)
        return;

    bool changed = (m_textFontZoom != m_textFontZoomPrev);

    if (!m_player || !m_player->GetVideoOutput())
        return;
    m_safeArea = m_player->GetVideoOutput()->GetSafeRect();

    CC608Buffer* textlist = m_cc608reader->GetOutputText(changed);
    if (!changed)
        return;

    SetElementDeleted();
    DeleteAllChildren();

    if (!textlist)
        return;

    QMutexLocker locker(&textlist->m_lock);

    if (textlist->m_buffers.empty())
        return;

    auto *fsub = new FormattedTextSubtitle608(textlist->m_buffers, m_family,
                                     m_safeArea, this/*, m_textFontZoom*/);
    m_qInited.append(fsub);
}

void SubtitleScreen::DisplayCC708Subtitles(void)
{
    if (!m_cc708reader || !m_player || !m_player->GetVideoOutput())
        return;

    CC708Service *cc708service = m_cc708reader->GetCurrentService();
    float video_aspect = m_player->GetVideoAspect();
    QRect oldsafe = m_safeArea;
    m_safeArea = m_player->GetVideoOutput()->GetSafeRect();
    bool changed = (oldsafe != m_safeArea || m_textFontZoom != m_textFontZoomPrev);
    if (changed)
    {
        for (auto & window : cc708service->m_windows)
            window.SetChanged();
    }

    uint64_t clearMask = 0;
    QList<FormattedTextSubtitle *> addList;
    for (int i = 0; i < k708MaxWindows; i++)
    {
        CC708Window &win = cc708service->m_windows[i];
        if (win.GetExists() && win.GetVisible() && !win.GetChanged())
            continue;
        if (!win.GetChanged())
            continue;

        clearMask |= (1UL << i);
        win.ResetChanged();
        if (!win.GetExists() || !win.GetVisible())
            continue;

        QMutexLocker locker(&win.m_lock);
        std::vector<CC708String*> list = win.GetStrings();
        if (!list.empty())
        {
            auto *fsub = new FormattedTextSubtitle708(win, i, list, m_family,
                                             m_safeArea, this, video_aspect);
            addList.append(fsub);
        }
        CC708Window::DisposeStrings(list);
    }
    if (clearMask)
    {
        Clear708Cache(clearMask);
    }
    if (!addList.empty())
        m_qInited.append(addList);
}

void SubtitleScreen::AddScaledImage(QImage &img, QRect &pos)
{
    MythVideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    QRect scaled = vo->GetImageRect(pos);
    if (scaled.size() != pos.size())
    {
        img = img.scaled(scaled.width(), scaled.height(),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    MythImage* image = m_painter->GetFormatImage();
    if (image)
    {
        image->Assign(img);
        MythUIImage *uiimage = new SubImage(this, "dvd_button", MythRect(scaled), 0ms);
        if (uiimage)
        {
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(scaled));
            SetElementAdded();
        }
        image->DecrRef();
    }
}

#if CONFIG_LIBASS
static void myth_libass_log(int level, const char *fmt, va_list vl, void */*ctx*/)
{
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

    static QMutex s_stringLock;
    s_stringLock.lock();

    QString str = QString::vasprintf(fmt, vl);
    LOG(verbose_mask, verbose_level, QString("libass: %1").arg(str));
    s_stringLock.unlock();
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

        ass_set_message_cb(m_assLibrary, myth_libass_log, nullptr);
        ass_set_extract_fonts(m_assLibrary, static_cast<int>(true));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialised libass object.");
    }

    LoadAssFonts();

    if (!m_assRenderer)
    {
        m_assRenderer = ass_renderer_init(m_assLibrary);
        if (!m_assRenderer)
            return false;

#ifdef Q_OS_ANDROID
        // fontconfig doesn't yet work for us on Android.  For the
        // time being, more explicitly set a font we know should
        // exist.  This was adapted from VLC master as of 2019-01-21.
        const char *psz_font = "/system/fonts/DroidSans.ttf";
        const char *psz_font_family = "Droid Sans";
#else
        const char *psz_font = nullptr;
        const char *psz_font_family = "sans-serif";
#endif
        ass_set_fonts(m_assRenderer, psz_font, psz_font_family, 1, nullptr, 1);
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
    m_assRenderer = nullptr;

    if (m_assLibrary)
    {
        ass_clear_fonts(m_assLibrary);
        m_assFontCount = 0;
        ass_library_done(m_assLibrary);
    }
    m_assLibrary = nullptr;
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
    if (header.isNull())
    {
        TextSubtitleParser* parser = m_player->GetSubReader(tracknum)->GetParser();
        if (parser)
            header = parser->GetSubHeader();
    }
    if (!header.isNull())
        ass_process_codec_private(m_assTrack, header.data(), header.size());

    m_safeArea = m_player->GetVideoOutput()->GetDisplayVideoRect();
    ResizeAssRenderer();
}

void SubtitleScreen::CleanupAssTrack(void)
{
    if (m_assTrack)
        ass_free_track(m_assTrack);
    m_assTrack = nullptr;
}

void SubtitleScreen::AddAssEvent(char *event, uint32_t starttime, uint32_t endtime)
{
    if (m_assTrack && event)
        ass_process_chunk(m_assTrack, event, strlen(event), starttime, endtime-starttime);
}

void SubtitleScreen::ResizeAssRenderer(void)
{
    ass_set_frame_size(m_assRenderer, m_safeArea.width(), m_safeArea.height());
    ass_set_margins(m_assRenderer, 0, 0, 0, 0);
    ass_set_use_margins(m_assRenderer, static_cast<int>(true));
    ass_set_font_scale(m_assRenderer, 1.0);
}

void SubtitleScreen::RenderAssTrack(std::chrono::milliseconds timecode, bool force)
{
    if (!m_player || !m_assRenderer || !m_assTrack)
        return;

    MythVideoOutput *vo = m_player->GetVideoOutput();
    if (!vo )
        return;

    QRect oldscreen = m_safeArea;
    m_safeArea = vo->GetDisplayVideoRect();
    if (oldscreen != m_safeArea)
        ResizeAssRenderer();

    int changed = 0;
    ASS_Image *images = ass_render_frame(m_assRenderer, m_assTrack, timecode.count(), &changed);
    if (!changed && !force)
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

        MythImage* image = m_painter->GetFormatImage();
        SubImage *uiimage = nullptr;

        if (image)
        {
            image->Assign(qImage);
            QString name = QString("asssub%1").arg(count);
            uiimage = new SubImage(this, name, MythRect(img_rect), 0ms);
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
#endif // CONFIG_LIBASS
