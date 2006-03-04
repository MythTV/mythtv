#include <qapplication.h>
#include <qimage.h>
#include <qdir.h>
#include <qdom.h>

#include <iostream>
#include <cmath>
using namespace std;

#include "exitcodes.h"
#include "myththemedmenu.h"
#include "mythcontext.h"
#include "util.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythimage.h"
#include "mythdialogbox.h"

#include "lcddevice.h"
#include "mythplugin.h"

/* FIXME, remove, for globalFontMap */
#include "uitypes.h"


struct TextAttributes
{
    QRect textRect;
    MythFontProperties font;
    int textflags;
};

struct ButtonIcon
{
    QString name;
    MythImage *icon;
    MythImage *activeicon;
    MythImage *watermark;
    QPoint offset;
};

struct ThemedButton
{
    QPoint pos;
    QRect  posRect;
    QRect  paintRect;

    ButtonIcon *buttonicon;
    QPoint iconPos;
    QRect iconRect;

    QString text;
    QString altText;
    QStringList action;

    int row;
    int col;

    int status;
    bool visible;
};

struct MenuRow
{
    int numitems;
    bool visible;
    vector<ThemedButton *> buttons;
};

static QPoint parsePoint(const QString &text)
{
    int x, y;
    QPoint retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);

    retval = GetMythMainWindow()->NormPoint(retval);

    return retval;
}

static QRect parseRect(const QString &text)
{
    int x, y, w, h;
    QRect retval;
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    retval = GetMythMainWindow()->NormRect(retval);

    return retval;
}

static QString getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

class MythThemedMenuPrivate;

class MythThemedMenuState
{
  public:
    MythThemedMenuState();
   ~MythThemedMenuState();

    bool parseSettings(const QString &dir, const QString &menuname);

    void parseBackground(const QString &dir, QDomElement &element);
    void parseLogo(const QString &dir, QDomElement &element);
    void parseArrow(const QString &dir, QDomElement &element, bool up);
    void parseTitle(const QString &dir, QDomElement &element);
    void parseButtonDefinition(const QString &dir, QDomElement &element);
    void parseButton(const QString &dir, QDomElement &element);

    void parseText(TextAttributes &attributes, QDomElement &element);
    void parseOutline(TextAttributes &attributes, QDomElement &element);
    void parseShadow(TextAttributes &attributes, QDomElement &element);

    /* FIXME: remove, compatability only */
    void parseFont(QDomElement &element);

    void Reset(void);
    void setDefaults(void);

    ButtonIcon *getButtonIcon(const QString &type);

    void paintLogo(MythPainter *p, int alpha);
    void paintTitle(MythPainter *p, int alpha, const QString &menumode);
    void paintButtonBackground(MythPainter *p, int alpha);
    void paintWatermark(MythPainter *p, int alpha, MythImage *watermark);

    void drawScrollArrows(MythPainter *p, int alpha, bool up, bool down);

    QRect buttonArea;

    QRect logoRect;
    MythImage *logo;

    MythImage *buttonnormal;
    MythImage *buttonactive;

    QMap<QString, ButtonIcon> allButtonIcons;

    TextAttributes normalAttributes;
    TextAttributes activeAttributes;

    void (*callback)(void *, QString &);
    void *callbackdata;

    bool killable;

    bool balancerows;
    bool spreadbuttons;
    bool buttoncenter;

    QMap<QString, MythImage *> titleIcons;
    MythImage *curTitle;
    QString titleText;
    QPoint titlePos;

    MythImage *buttonBackground;

    MythImage *uparrow;
    QRect uparrowRect;
    MythImage *downarrow;
    QRect downarrowRect;

    QPoint watermarkPos;
    QRect watermarkRect;

    bool allowreorder;
    int maxColumns;

    int visiblerowlimit;

    bool loaded;

    QString themeDir;

    /* FIXME, remove */
    static bool parseFonts;
    QString fontSizeType;
};

bool MythThemedMenuState::parseFonts = true;

class MythThemedMenuPrivate
{
  public:
    MythThemedMenuPrivate(MythThemedMenu *lparent, const char *cdir, 
                      MythThemedMenuState *lstate);
   ~MythThemedMenuPrivate();

    bool keyPressHandler(QKeyEvent *e);
    bool ReloadTheme(void);

    bool parseMenu(const QString &menuname);

    void parseThemeButton(QDomElement &element);

    void addButton(const QString &type, const QString &text,
                   const QString &alttext, const QStringList &action);
    bool layoutButtons(void);
    void positionButtons(bool resetpos);
    bool makeRowVisible(int newrow, int oldrow);

    bool handleAction(const QString &action);
    bool findDepends(const QString &fileList);
    QString findMenuFile(const QString &menuname);

    void paintButton(unsigned int button, MythPainter *p, int alpha);
    void paintWatermark(MythPainter *p, int alpha);

    void DrawSelf(MythPainter *p, int xoffset, int yoffset, int alphaMod);

    void drawScrollArrows(MythPainter *p, int alpha);
    bool checkPinCode(const QString &timestamp_setting,
                      const QString &password_setting,
                      const QString &text);

    void updateLCD(void);

    MythThemedMenu *parent;

    MythThemedMenuState *m_state;
    bool allocedstate;

    vector<ThemedButton> buttonList;
    ThemedButton *activebutton;
    int currentrow;
    int currentcolumn;

    vector<MenuRow> buttonRows;

    QString selection;
    bool foundtheme;

    int exitModifier;

    bool ignorekeys;

    int maxrows;
    int visiblerows;
    int columns;

    bool wantpop;

    QString titleText;
    QString menumode;
};

/* FIXME: remove */
extern QMap<QString, fontProp> globalFontMap;

/////////////////////////////////////////////////////////////////////////////

MythThemedMenuState::MythThemedMenuState()
{
    allowreorder = true;
    balancerows = true;

    logo = NULL;
    buttonnormal = NULL;
    buttonactive = NULL;
    uparrow = NULL;
    downarrow = NULL;
    buttonBackground = NULL;

    loaded = false;

    callback = NULL;

    killable = false;

    if (parseFonts)
        fontSizeType = gContext->GetSetting("ThemeFontSizeType", "default");
}

MythThemedMenuState::~MythThemedMenuState()
{
    Reset();
}

void MythThemedMenuState::Reset(void)
{
    if (logo)
        logo->DownRef();
    if (buttonnormal)
        buttonnormal->DownRef();
    if (buttonactive)
        buttonactive->DownRef();
    if (uparrow)
        uparrow->DownRef();
    if (downarrow)
        downarrow->DownRef();
    if (buttonBackground)
        buttonBackground->DownRef();

    logo = NULL;
    buttonnormal = NULL;
    buttonactive = NULL;
    uparrow = NULL;
    downarrow = NULL;
    buttonBackground = NULL;

    QMap<QString, ButtonIcon>::Iterator it;
    for (it = allButtonIcons.begin(); it != allButtonIcons.end(); ++it)
    {
        if (it.data().icon)
            it.data().icon->DownRef();
        if (it.data().activeicon)
            it.data().activeicon->DownRef();
        if (it.data().watermark)
            it.data().watermark->DownRef();
    }
    allButtonIcons.clear();

    QMap<QString, MythImage *>::Iterator jt;
    for (jt = titleIcons.begin(); jt != titleIcons.end(); ++jt)
    {
        jt.data()->DownRef();
    }
    titleIcons.clear();

    normalAttributes = activeAttributes = TextAttributes();
    setDefaults();

    loaded = false;
}

ButtonIcon *MythThemedMenuState::getButtonIcon(const QString &type)
{
    if (allButtonIcons.find(type) != allButtonIcons.end())
        return &(allButtonIcons[type]);
    return NULL;
}

void MythThemedMenuState::parseBackground(const QString &dir, QDomElement& element)
{
    QString path;

    bool hasarea = false;

    buttoncenter = true;
    spreadbuttons = true;
    maxColumns = 20;        // Arbitrary number
    visiblerowlimit = 6;    // the old default

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
            }
            else if (info.tagName() == "buttonarea")
            {
                buttonArea = parseRect(getFirstText(info));
                hasarea = true;

                if (info.hasAttribute("background"))
                {
                    QString bPath = dir + info.attribute("background");
                    QImage *image = gContext->LoadScaleImage(bPath);
                    buttonBackground = MythImage::FromQImage(&image);
                }
            }
            else if (info.tagName() == "buttonspread")
            {
                QString val = getFirstText(info);
                if (val == "no")
                    spreadbuttons = false;
            }
            else if (info.tagName() == "buttoncenter")
            {
                QString val = getFirstText(info);
                if (val == "no")
                    buttoncenter = false;
            }
            else if (info.tagName() == "balancerows")
            {
                QString val = getFirstText(info);
                if (val == "no")
                    balancerows = false;
            }
            else if (info.tagName() == "columns")
            {
                QString val = getFirstText(info);
                maxColumns = val.toInt();
            }
            else if (info.tagName() == "visiblerowlimit")
            {
                visiblerowlimit = atoi(getFirstText(info).ascii());
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 in "
                                            "background").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasarea)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing buttonaread in background");
        return;
    }
}

void MythThemedMenuState::parseShadow(TextAttributes &attributes, 
                                      QDomElement &element)
{
    QPoint offset;
    QColor color;
    int alpha;

    bool hascolor = false;
    bool hasoffset = false;
    bool hasalpha = false;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "color")
            {
                // workaround alpha bug with named colors
                QColor temp(getFirstText(info));
                color = QColor(temp.name());
                hascolor = true;
            }
            else if (info.tagName() == "offset")
            {
                offset = parsePoint(getFirstText(info));
                hasoffset = true;
            }
            else if (info.tagName() == "alpha")
            {
                alpha = atoi(getFirstText(info).ascii());
                hasalpha = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 in "
                                            "text/shadow").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hascolor)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing color tag in shadow");
        return;
    }

    if (!hasalpha)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing alpha tag in shadow");
        return;
    }

    if (!hasoffset)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing offset tag in shadow");
        return;
    }

    attributes.font.SetShadow(true, offset, color, alpha);
}

void MythThemedMenuState::parseOutline(TextAttributes &attributes, 
                                       QDomElement &element)
{
    QColor color;
    int size;
    int alpha = 255;

    bool hascolor = false;
    bool hassize = false;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "color")
            {
                // workaround alpha bug with named colors
                QColor temp(getFirstText(info));
                color = QColor(temp.name());
                hascolor = true;
            }
            else if (info.tagName() == "size")
            {
                int lsize = atoi(getFirstText(info).ascii());
                size = GetMythMainWindow()->NormY(lsize);
                hassize = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 in "
                                            "text/shadow").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hassize)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing size in outline");
        return;
    }

    if (!hascolor)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing color in outline");
        return;
    }

    attributes.font.SetOutline(true, color, size, alpha);
}

void MythThemedMenuState::parseText(TextAttributes &attributes, 
                                    QDomElement &element)
{
    bool hasarea = false;

    int weight = QFont::Normal;
    int fontsize = 14;
    QString fontname = "Arial";
    bool italic = false;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area") 
            {
                hasarea = true;
                attributes.textRect = parseRect(getFirstText(info));
                attributes.textRect = QRect(attributes.textRect.x(), 
                                            attributes.textRect.y(),
                                            buttonnormal->width() - 
                                            attributes.textRect.width() - 
                                            attributes.textRect.x(), 
                                            buttonnormal->height() - 
                                            attributes.textRect.height() - 
                                            attributes.textRect.y());
            }
            else if (info.tagName() == "fontsize")
            {
                fontsize = atoi(getFirstText(info).ascii());
            }
            else if (info.tagName() == "fontname")
            { 
                fontname = getFirstText(info);
            }
            else if (info.tagName() == "bold")
            {
                if (getFirstText(info) == "yes")
                    weight = QFont::Bold;
            }
            else if (info.tagName() == "italics")
            {
                if (getFirstText(info) == "yes")
                    italic = true;
            }
            else if (info.tagName() == "color")
            {
                // workaround alpha bug with named colors
                QColor temp(getFirstText(info));
                attributes.font.SetColor(QColor(temp.name()));
            }
            else if (info.tagName() == "centered")
            {
                if (getFirstText(info) == "yes")
                {
                    if (gContext->GetLanguage() == "ja")
                    {
                        attributes.textflags = Qt::AlignVCenter | 
                                               Qt::AlignHCenter |
                                               Qt::WordBreak;
                    }
                    else
                    {
                        attributes.textflags = Qt::AlignTop | 
                                               Qt::AlignHCenter |
                                               Qt::WordBreak;
                    }
                }
            } 
            else if (info.tagName() == "outline")
            {
                parseOutline(attributes, info);
            }
            else if (info.tagName() == "shadow")
            {
                parseShadow(attributes, info);
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 "
                                            "in text").arg(info.tagName()));
                return;
            }
        }
    }

    QFont font = GetMythMainWindow()->CreateFont(fontname, fontsize, 
                                                 weight, italic);

    attributes.font.SetFace(font);

    if (!hasarea)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing 'area' "
                "tag in 'text' element of 'genericbutton'");
        return;
    }
}

void MythThemedMenuState::parseFont(QDomElement &element)
{
    QString name;
    QString face;
    QString bold;
    QString ital;
    QString under;

    int size = -1;
    int sizeSmall = -1;
    int sizeBig = -1;

    QPoint shadowOffset = QPoint(0, 0);
    QString color = "#ffffff";
    QString dropcolor = "#000000";
    QString hint;
    QFont::StyleHint styleHint = QFont::AnyStyle;

    bool haveSizeSmall = false;
    bool haveSizeBig = false;
    bool haveSize = false;
    bool haveFace = false;
    bool haveColor = false;
    bool haveDropColor = false;
    bool haveBold = false;
    bool haveShadow = false;
    bool haveItal = false;
    bool haveUnder = false;

    fontProp *baseFont = NULL;

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Font needs a name");
        return;
    }
    QString base = element.attribute("base", "");
    if (!base.isNull() && !base.isEmpty())
    {
        if (globalFontMap.contains(base))
            baseFont = &globalFontMap[base];

        if (!baseFont)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("ThemedMenuPrivate: Specified base font '%1' does not "
                            "exist for font %2").arg(base).arg(face));
            return;
        }
    }

    face = element.attribute("face", "");
    if (face.isNull() || face.isEmpty())
    {
        if (!baseFont)
        {
            VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Font needs a face");
            return;
        }
    }
    else
    {
        haveFace = true;
    }

    hint = element.attribute("stylehint", "");
    if (!hint.isNull() && !hint.isEmpty())
    {
        styleHint = (QFont::StyleHint)hint.toInt();
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "size")
            {
                haveSize = true;
                size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:small")
            {
                haveSizeSmall = true;
                sizeSmall = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:big")
            {
                haveSizeBig = true;
                sizeBig = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                haveColor = true;
                color = getFirstText(info);
            }
            else if (info.tagName() == "dropcolor")
            {
                haveDropColor = true;
                dropcolor = getFirstText(info);
            }
            else if (info.tagName() == "shadow")
            {
                haveShadow = true;
                shadowOffset = parsePoint(getFirstText(info));
            }
            else if (info.tagName() == "bold")
            {
                haveBold = true;
                bold = getFirstText(info);
            }
            else if (info.tagName() == "italics")
            {
                haveItal = true;
                ital = getFirstText(info);
            }
            else if (info.tagName() == "underline")
            {
                haveUnder = true;
                under = getFirstText(info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Unknown tag %1 in "
                                              "font").arg(info.tagName()));
                return;
            }
        }
    }

    if (globalFontMap.contains(name))
    {
        VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Error, already have a "
                                      "global font called: %1").arg(name));
        return;
    }

    fontProp newFont;

    if (baseFont)
        newFont = *baseFont;

    if (haveSizeSmall && fontSizeType == "small")
    {
        if (sizeSmall > 0)
            size = sizeSmall;
    }
    else if (haveSizeBig && fontSizeType == "big")
    {
        if (sizeBig > 0)
            size = sizeBig;
    }

    if (size < 0 && !baseFont)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Error, font size must be > 0");
        return;
    }

    if (baseFont && !haveSize)
        size = baseFont->face.pointSize();

    // If we don't have to, don't load the font.
    if (!haveFace && baseFont)
    {
        newFont.face = baseFont->face;
        if (haveSize)
            newFont.face.setPointSize(size);
    }
    else
    {
        QFont temp = GetMythMainWindow()->CreateFont(face, size);
        temp.setStyleHint(styleHint);
        newFont.face = temp;
    }

    if (baseFont && !haveBold)
        newFont.face.setBold(baseFont->face.bold());
    else
    {
        if (bold.lower() == "yes")
            newFont.face.setBold(true);
        else
            newFont.face.setBold(false);
    }

    if (baseFont && !haveItal)
        newFont.face.setItalic(baseFont->face.italic());
    else
    {
        if (ital.lower() == "yes")
            newFont.face.setItalic(true);
        else
            newFont.face.setItalic(false);
    }

    if (baseFont && !haveUnder)
        newFont.face.setUnderline(baseFont->face.underline());
    else
    {
        if (under.lower() == "yes")
            newFont.face.setUnderline(true);
        else
            newFont.face.setUnderline(false);
    }

    if (haveColor)
    {
        QColor foreColor(color);
        newFont.color = foreColor;
    }

    if (haveDropColor)
    {
        QColor dropColor(dropcolor);
        newFont.dropColor = dropColor;
    }

    if (haveShadow)
        newFont.shadowOffset = shadowOffset;

    globalFontMap[name] = newFont;
}

void MythThemedMenuState::parseButtonDefinition(const QString &dir, 
                                                QDomElement &element)
{
    bool hasnormal = false;
    bool hasactive = false;
    bool hasactivetext = false;

    QString setting;

    QImage *tmp;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "normal")
            {
                setting = dir + getFirstText(info);
                tmp = gContext->LoadScaleImage(setting);
                if (tmp)
                {
                    buttonnormal = MythImage::FromQImage(&tmp);
                    hasnormal = true;
                }
            }
            else if (info.tagName() == "active")
            {
                setting = dir + getFirstText(info);
                tmp = gContext->LoadScaleImage(setting);
                if (tmp)
                {
                    buttonactive = MythImage::FromQImage(&tmp);
                    hasactive = true;
                }
            }
            else if (info.tagName() == "text")
            {
                if (!hasnormal)
                {
                    VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: The 'normal' "
                            "tag needs to come before the 'normaltext' tag");
                    return;
                }
                parseText(normalAttributes, info);
            }
            else if (info.tagName() == "activetext")
            {
                if (!hasactive)
                {
                    VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: The 'active' "
                            "tag needs to come before the 'activetext' tag");
                    return;
                }
                parseText(activeAttributes, info);
                hasactivetext = true;
            }
            else if (info.tagName() == "watermarkposition")
            {
                watermarkPos = parsePoint(getFirstText(info));
            }
            else
            {
                VERBOSE(VB_GENERAL,
                        QString("MythThemedMenuPrivate: Unknown tag %1 in "
                                "genericbutton").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasnormal)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: No normal button image defined");
        return;
    }

    if (!hasactive)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: No active button image defined");
        return;
    }

    if (!hasactivetext)
    {
        activeAttributes = normalAttributes;
    }

    watermarkRect = QRect(watermarkPos, QSize(0, 0));
}

void MythThemedMenuState::parseLogo(const QString &dir, QDomElement &element)
{
    bool hasimage = false;
    bool hasposition = false;

    QPoint logopos;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
                QString logopath = dir + getFirstText(info);
                QImage *tmp = gContext->LoadScaleImage(logopath); 
                if (tmp)
                {
                    logo = MythImage::FromQImage(&tmp);
                    hasimage = true;
                }
            }
            else if (info.tagName() == "position")
            {
                logopos = parsePoint(getFirstText(info));
                hasposition = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 "
                                            "in logo").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasimage)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing image tag in logo");
        return;
    }

    if (!hasposition)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing position tag in logo");
        return;
    }

    logoRect = QRect(logopos.x(), logopos.y(), logo->width(),
                     logo->height());
}

void MythThemedMenuState::parseTitle(const QString &dir, QDomElement &element)
{
    bool hasimage = false;
    bool hasposition = false;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
                QString titlepath = dir + getFirstText(info);
                QImage *tmppix = gContext->LoadScaleImage(titlepath);

                if (!tmppix)
                    continue;

                QString name = info.attribute("mode", "");
                if (name != "")
                {
                    titleIcons[name] = MythImage::FromQImage(&tmppix);
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: "
                            "Missing mode in titles/image");
                    return;
                }

                hasimage = true;
            }
            else if (info.tagName() == "position")
            {
                titlePos = parsePoint(getFirstText(info));
                hasposition = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 "
                                            "in logo").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasimage)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing image tag in titles");
        return;
    }

    if (!hasposition)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing position tag in titles");
        return;
    }
}

void MythThemedMenuState::parseArrow(const QString &dir, QDomElement &element, 
                                     bool up)
{
    QRect arrowrect;
    QPoint arrowpos;
    QImage *pix = NULL;    

    bool hasimage = false;
    bool hasposition = false;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
                QString arrowpath = dir + getFirstText(info);
                pix = gContext->LoadScaleImage(arrowpath);
                if (pix)
                    hasimage = true;
            }
            else if (info.tagName() == "position")
            {
                arrowpos = parsePoint(getFirstText(info));
                hasposition = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 "
                                            "in arrow").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasimage)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing image tag in arrow");
        return;
    }

    if (!hasposition)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing position tag in arrow");
        return;
    }

    arrowrect = QRect(arrowpos.x(), arrowpos.y(), pix->width(),
                      pix->height());

    if (up)
    {
        uparrow = MythImage::FromQImage(&pix);
        uparrowRect = arrowrect;
    }
    else
    {
        downarrow = MythImage::FromQImage(&pix);
        downarrowRect = arrowrect;
    }
}

void MythThemedMenuState::parseButton(const QString &dir, QDomElement &element)
{
    bool hasname = false;
    bool hasoffset = false;
    bool hasicon = false;

    QString name = "";
    QImage *image = NULL;
    QImage *activeimage = NULL;
    QImage *watermark = NULL;
    QPoint offset;

    name = element.attribute("name", "");
    if (name != "")
        hasname = true;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {    
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
                QString imagepath = dir + getFirstText(info); 
                image = gContext->LoadScaleImage(imagepath);
                if (image)
                    hasicon = true;
            }
            else if (info.tagName() == "activeimage")
            {
                QString imagepath = dir + getFirstText(info);
                activeimage = gContext->LoadScaleImage(imagepath);
            }
            else if (info.tagName() == "offset")
            {
                offset = parsePoint(getFirstText(info));
                hasoffset = true;
            }
            else if (info.tagName() == "watermarkimage")
            {
                QString imagepath = dir + getFirstText(info);
                watermark = gContext->LoadScaleImage(imagepath);
            }    
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 "
                                            "in buttondef").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasname)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing name in button");
        return;
    }

    if (!hasoffset)
    {
        VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate: Missing offset "
                                      "in buttondef %1").arg(name));
        return;
    }

    if (!hasicon) 
    {
        VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate: Missing image "
                                      "in buttondef %1").arg(name));
        return;
    }

    ButtonIcon newbutton;

    newbutton.name = name;
    newbutton.icon = MythImage::FromQImage(&image);
    newbutton.offset = offset;
    newbutton.activeicon = MythImage::FromQImage(&activeimage);

    if (watermark)
    {
        if (watermark->width() > watermarkRect.width())
            watermarkRect.setWidth(watermark->width());

        if (watermark->height() > watermarkRect.height())
            watermarkRect.setHeight(watermark->height());
    }

    newbutton.watermark = MythImage::FromQImage(&watermark);

    allButtonIcons[name] = newbutton;
}

void MythThemedMenuState::setDefaults(void)
{
    logo = NULL;
    buttonnormal = buttonactive = NULL;
    balancerows = true;

    normalAttributes.textflags = Qt::AlignTop | Qt::AlignLeft | Qt::WordBreak;
    activeAttributes.textflags = Qt::AlignTop | Qt::AlignLeft | Qt::WordBreak;

    titleIcons.clear();
    titleText = "";
    curTitle = NULL;
    uparrow = NULL;
    downarrow = NULL;
    watermarkPos = QPoint(0, 0);
    watermarkRect = QRect(0, 0, 0, 0);
}

bool MythThemedMenuState::parseSettings(const QString &dir, const QString &menuname)
{
    QString filename = dir + menuname;

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate::parseSettings(): "
                                      "Can't open: %1").arg(filename));
        return false;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate: Error, parsing %1\n"
                                      "at line: %2  column: %3 msg: %4").
                arg(filename).arg(errorLine).arg(errorColumn).arg(errorMsg));
        f.close();
        return false;
    }

    f.close();

    bool setbackground = false;
    bool setbuttondef = false;

    setDefaults();

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "background")
            {
                parseBackground(dir, e);
                setbackground = true;
            }
            else if (e.tagName() == "genericbutton")
            {
                parseButtonDefinition(dir, e);
                setbuttondef = true;
            }
            else if (e.tagName() == "logo")
            {
                parseLogo(dir, e);
            }
            else if (e.tagName() == "buttondef")
            {
                parseButton(dir, e);
            }
            else if (e.tagName() == "titles")
            {
                parseTitle(dir, e);
            }
            else if (e.tagName() == "uparrow")
            {
                parseArrow(dir, e, true);
            }
            else if (e.tagName() == "downarrow")
            {
                parseArrow(dir, e, false);
            }
            else if (e.tagName() == "font")
            {
                if (parseFonts)
                    parseFont(e);
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown "
                                            "element %1").arg(e.tagName()));
            }
        }
        n = n.nextSibling();
    }

    if (!setbackground)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing background element");
        return false;
    }

    if (!setbuttondef)
    {
        VERBOSE(VB_IMPORTANT,
                "MythThemedMenuPrivate: Missing genericbutton definition");
        return false;
    }

    parseFonts = false;
    loaded = true;
    return true;
}

void MythThemedMenuState::drawScrollArrows(MythPainter *p, int alpha, bool up,
                                           bool down)
{
    if (!uparrow || !downarrow)
        return;

    if (!up && !down)
        return;

    if (up)
        p->DrawImage(uparrowRect.topLeft(), uparrow, alpha);
    if (down)
        p->DrawImage(downarrowRect.topLeft(), downarrow, alpha);
}

void MythThemedMenuState::paintLogo(MythPainter *p, int alpha)
{
    if (logo)
        p->DrawImage(logoRect.topLeft(), logo, alpha);
}

void MythThemedMenuState::paintTitle(MythPainter *p, int alpha, 
                                     const QString &menumode)
{
    if (titleIcons.contains(menumode))
        curTitle = titleIcons[menumode];

    if (curTitle)
        p->DrawImage(titlePos, curTitle, alpha);
}

void MythThemedMenuState::paintButtonBackground(MythPainter *p, int alpha)
{
    if (buttonBackground)
        p->DrawImage(buttonArea.topLeft(), buttonBackground, alpha);
}

void MythThemedMenuState::paintWatermark(MythPainter *p, int alpha, 
                                         MythImage *watermark)
{
    if (watermark)
    {
        p->DrawImage(watermarkPos, watermark, alpha);
    }
}

/////////////////////////////////////////////////////////////////////////////

MythThemedMenuPrivate::MythThemedMenuPrivate(MythThemedMenu *lparent, 
                                             const char *cdir,
                                             MythThemedMenuState *lstate)
{
    if (!lstate)
    {
        m_state = new MythThemedMenuState();
        allocedstate = true;
    }
    else
    {
        m_state = lstate;
        allocedstate = false;
    }

    parent = lparent;
    ignorekeys = false;
    wantpop = false;
    exitModifier = -1;

    m_state->themeDir = cdir;
}

MythThemedMenuPrivate::~MythThemedMenuPrivate()
{
    if (allocedstate)
        delete m_state;
}

void MythThemedMenuPrivate::parseThemeButton(QDomElement &element)
{
    QString type = "";
    QString text = "";
    QStringList action;
    QString alttext = "";

    bool addit = true;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "type")
            {
                type = getFirstText(info);
            }
            else if (info.tagName() == "text")
            {
                if ((text.isNull() || text.isEmpty()) && 
                    info.attribute("lang","") == "")
                {
                    text = getFirstText(info);
                }
                else if (info.attribute("lang","").lower() == 
                         gContext->GetLanguageAndVariant())
                {
                    text = getFirstText(info);
                }
                else if (info.attribute("lang","").lower() == 
                         gContext->GetLanguage())
                {
                    text = getFirstText(info);
                }
            }
            else if (info.tagName() == "alttext")
            {
                if ((alttext.isNull() || alttext.isEmpty()) &&
                    info.attribute("lang","") == "")
                {
                    alttext = getFirstText(info);
                }
                else if (info.attribute("lang","").lower() == 
                         gContext->GetLanguageAndVariant())
                {
                    alttext = getFirstText(info);
                }
                else if (info.attribute("lang","").lower() ==
                         gContext->GetLanguage())
                {
                    alttext = getFirstText(info);
                }
            }
            else if (info.tagName() == "action")
            {
                action += getFirstText(info);
            }
            else if (info.tagName() == "depends")
            {
                addit = findDepends(getFirstText(info));
            }
            else if (info.tagName() == "dependssetting")
            {
                addit = gContext->GetNumSetting(getFirstText(info));
            }
            else if (info.tagName() == "dependjumppoint")
            {
                addit = GetMythMainWindow()->DestinationExists(getFirstText(info));
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("MythThemedMenuPrivate: Unknown tag %1 "
                                            "in button").arg(info.tagName()));
                return;
            }
        }
    }

    if (text == "")
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing 'text' in button");
        return;
    }
   
    if (action.empty())
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Missing 'action' in button");
        return;
    }

    if (addit)
        addButton(type, text, alttext, action);
}

bool MythThemedMenuPrivate::parseMenu(const QString &menuname)
{
    QString filename = findMenuFile(menuname);

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate: Couldn't read "
                                      "menu file %1").arg(menuname));
        if (menuname == "mainmenu.xml" )
        {
            return false;
        }
        else
        {
            parent->GetScreenStack()->PopScreen();
#if 0
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("No Menu File"),
                                      QObject::tr(QString("Myth could not locate the menu file %1.\n\n"
                                      "We will now return to the main menu.").arg(menuname)));
#endif
            return false;
        }
        
        
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Error parsing: %1\nat line: %2  column: %3 msg: %4").
                arg(filename).arg(errorLine).arg(errorColumn).arg(errorMsg));
        f.close();

        if (menuname == "mainmenu.xml" )
        {
            return false;
        }

        parent->GetScreenStack()->PopScreen();
#if 0
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Bad Menu File"),
                                  QObject::tr(QString("The menu file %1 is "
                                              "incomplete.\n\nWe will now "
                                              "return to the main menu.")
                                              .arg(menuname)));
#endif
        return false;
    }

    f.close();

    buttonList.clear();
    buttonRows.clear();

    QDomElement docElem = doc.documentElement();

    menumode = docElem.attribute("name", "");

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "button")
            {
                parseThemeButton(e);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate: Unknown "
                                              "element %1").arg(e.tagName()));
                return false;
            }
        }
        n = n.nextSibling();
    }

    if (buttonList.size() == 0)
    {
        VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate: No buttons "
                                      "for menu %1").arg(menuname));
        return false;
    }

    if (!layoutButtons())
        return false;
    positionButtons(true);

    if (LCD::Get())
    {
        titleText = "MYTH-";
        titleText += menumode;
        updateLCD();
    }

    selection = "";
    parent->SetRedraw();
    return true;
}

void MythThemedMenuPrivate::updateLCD(void)
{
    LCD *lcddev = LCD::Get();
    if (lcddev == NULL)
        return;

    // Build a list of the menu items
    QPtrList<LCDMenuItem> menuItems;
    menuItems.setAutoDelete(true);
    bool selected;

    for (int r = 0; r < (int)buttonRows.size(); r++)
    {
        if (r == currentrow)
            selected = true;
        else
            selected = false;

        if (currentcolumn < buttonRows[r].numitems)
            menuItems.append(new LCDMenuItem(selected, NOTCHECKABLE,
                             buttonRows[r].buttons[currentcolumn]->text));
    }

    if (!menuItems.isEmpty())
        lcddev->switchToMenu(&menuItems, titleText);
}

void MythThemedMenuPrivate::addButton(const QString &type, const QString &text, 
                                      const QString &alttext, 
                                      const QStringList &action)
{
    ThemedButton newbutton;

    newbutton.buttonicon = m_state->getButtonIcon(type);
    newbutton.text = text;
    newbutton.altText = alttext;
    newbutton.action = action;
    newbutton.status = -1;
    newbutton.visible = false;

    buttonList.push_back(newbutton);
}

bool MythThemedMenuPrivate::layoutButtons(void)
{
    int numbuttons = buttonList.size();
  
    columns = m_state->buttonArea.width() / m_state->buttonnormal->width();
    columns = columns > m_state->maxColumns ? m_state->maxColumns : columns;

    maxrows = m_state->buttonArea.height() / m_state->buttonnormal->height();

    if (maxrows < 1)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Must have "
                "room for at least 1 row of buttons");
        return false;
    }
    
    if (columns < 1)
    {
        VERBOSE(VB_IMPORTANT, "MythThemedMenuPrivate: Must have "
                "room for at least 1 column of buttons");
        return false;
    }

    if (m_state->balancerows)
    {
        // keep the rows balanced
        if (numbuttons <= 4)
        {
            if (columns > 2)
                columns = 2;
        }
        else
        {
            if (columns > 3)
                columns = 3;
        }
    }    

    // limit it to 6 items displayed at one time
    if (columns * maxrows > m_state->visiblerowlimit)
    {
        maxrows = m_state->visiblerowlimit / columns;
    }
                             
    vector<ThemedButton>::iterator iter = buttonList.begin();

    int rows = numbuttons / columns;
    rows++;

    visiblerows = 0;

    for (int i = 0; i < rows; i++)
    {
        MenuRow newrow;
        newrow.numitems = 0;

        for (int j = 0; j < columns && iter != buttonList.end(); 
             j++, iter++)
        {
            if (columns == 3 && j == 1 && m_state->allowreorder)
                newrow.buttons.insert(newrow.buttons.begin(), &(*iter));
            else
                newrow.buttons.push_back(&(*iter));
            newrow.numitems++;
        }

        if (i < maxrows && newrow.numitems > 0)
        {
            newrow.visible = true;
            visiblerows++;
        }
        else
            newrow.visible = false;
 
        if (newrow.numitems > 0)
            buttonRows.push_back(newrow);
    }           

    return true; 
}

void MythThemedMenuPrivate::positionButtons(bool resetpos)
{
    QRect buttonArea = m_state->buttonArea;
    int buttonHeight = m_state->buttonnormal->height();
    int buttonWidth = m_state->buttonnormal->width();

    int rows = visiblerows;
    int yspacing = (buttonArea.height() - buttonHeight * rows) /
                   (rows + 1);
    int ystart = 0;
    
    if (!m_state->spreadbuttons)
    {
        yspacing = 0;
        if (m_state->buttoncenter)
            ystart = (buttonArea.height() - buttonHeight * rows) / 2;
    }

    int row = 1;

    vector<MenuRow>::iterator menuiter = buttonRows.begin();
    for (; menuiter != buttonRows.end(); menuiter++)
    {
        if (!(*menuiter).visible)
        {
            vector<ThemedButton *>::iterator biter;
            biter = (*menuiter).buttons.begin();
            for (; biter != (*menuiter).buttons.end(); biter++)
            {
                ThemedButton *tbutton = (*biter);
                tbutton->visible = false;
            }
            continue;
        }

        int ypos = yspacing * row + (buttonHeight * (row - 1));
        ypos += buttonArea.y() + ystart;

        int xspacing = (buttonArea.width() - buttonWidth *
                       (*menuiter).numitems) / ((*menuiter).numitems + 1);
        int col = 1;
        vector<ThemedButton *>::iterator biter = (*menuiter).buttons.begin();
        for (; biter != (*menuiter).buttons.end(); biter++)
        {
            int xpos = xspacing * col + (buttonWidth * (col - 1));
            xpos = (m_state->maxColumns == 1) ? 0 : xpos;
            xpos += buttonArea.x();

            ThemedButton *tbutton = (*biter);

            tbutton->visible = true;
            tbutton->row = row;
            tbutton->col = col;
            tbutton->pos = QPoint(xpos, ypos);
            tbutton->posRect = QRect(tbutton->pos.x(), tbutton->pos.y(),
                                     buttonWidth, buttonHeight);

            if (tbutton->buttonicon)
            {
                tbutton->iconPos = tbutton->buttonicon->offset + tbutton->pos;
                tbutton->iconRect = QRect(tbutton->iconPos.x(),
                                          tbutton->iconPos.y(),
                                          tbutton->buttonicon->icon->width(),
                                          tbutton->buttonicon->icon->height());
                tbutton->paintRect = tbutton->posRect.unite(tbutton->iconRect);
            }
            else
            {
                tbutton->paintRect = tbutton->posRect;
            }
            
            col++;
        }

        row++;
    }

    if (resetpos)
    {
        activebutton = &(*(buttonList.begin()));
        currentrow = activebutton->row - 1;
        currentcolumn = activebutton->col - 1;
    }
}

bool MythThemedMenuPrivate::makeRowVisible(int newrow, int oldrow)
{
    if (buttonRows[newrow].visible)
        return true;

    if (newrow > oldrow)
    {
        int row;
        for (row = newrow; row >= 0; row--)
        {
            if (row > newrow - visiblerows)
                buttonRows[row].visible = true;
            else
                buttonRows[row].visible = false;
        }
    }
    else
    {
        int row;
        for (row = newrow; row < (int)buttonRows.size(); row++)
        {
            if (row < newrow + visiblerows)
                buttonRows[row].visible = true;
            else
                buttonRows[row].visible = false;
        }
    }

    positionButtons(false);

    /* need to set which to draw.. */

    return true;
}

void MythThemedMenuPrivate::drawScrollArrows(MythPainter *p, int alpha)
{
    bool needup = false;
    bool needdown = false;

    if (!buttonRows.front().visible)
        needup = true;
    if (!buttonRows.back().visible)
        needdown = true;

    if (!needup && !needdown)
        return;

    m_state->drawScrollArrows(p, alpha, needup, needdown);
}

void MythThemedMenuPrivate::paintWatermark(MythPainter *p, int alpha)
{
    if (activebutton && activebutton->buttonicon &&
        activebutton->buttonicon->watermark)
    {
        m_state->paintWatermark(p, alpha,  activebutton->buttonicon->watermark);
    }
}

void MythThemedMenuPrivate::paintButton(unsigned int button, MythPainter *p, 
                                        int alpha)
{
    TextAttributes *attributes;
    ThemedButton *tbutton = &(buttonList[button]);

    if (!tbutton->visible)
        return;

    MythImage *buttonback = NULL;
    if (tbutton == activebutton)
    {
        buttonback = m_state->buttonactive;
        tbutton->status = 1;
        attributes = &m_state->activeAttributes;
    }
    else
    {
        tbutton->status = 0;
        buttonback = m_state->buttonnormal;
        attributes = &m_state->normalAttributes;
    }

    p->DrawImage(tbutton->posRect.topLeft(), buttonback, alpha);

    QRect buttonTextRect = attributes->textRect;
    buttonTextRect.moveBy(tbutton->posRect.x(), tbutton->posRect.y());

    QString message = tbutton->text;

    QRect testBoundRect = buttonTextRect;
    testBoundRect.addCoords(0, 0, 0, 40);

    QFontMetrics tmp(attributes->font.face());
    QRect testBound = tmp.boundingRect(testBoundRect.x(), testBoundRect.y(), 
                                       testBoundRect.width(), 
                                       testBoundRect.height(), 
                                       attributes->textflags, 
                                       message);

    if (testBound.height() > buttonTextRect.height() && tbutton->altText != "")
        message = buttonList[button].altText;

    p->DrawText(buttonTextRect, message, attributes->textflags, 
                attributes->font, alpha);

    if (buttonList[button].buttonicon)
    {
        MythImage *blendImage = tbutton->buttonicon->icon;

        if (tbutton == activebutton && tbutton->buttonicon->activeicon)
            blendImage = tbutton->buttonicon->activeicon;

        p->DrawImage(tbutton->iconRect.topLeft(), blendImage, alpha);
    }
}

bool MythThemedMenuPrivate::ReloadTheme(void)
{
    globalFontMap.clear();
    m_state->parseFonts = true;

    buttonList.clear();
    buttonRows.clear();

    parent->ReloadExitKey();
 
    m_state->Reset();
 
    QString themedir = gContext->GetThemeDir();
    bool ok = m_state->parseSettings(themedir, "theme.xml");
    if (!ok)
        return ok;

    return parseMenu("mainmenu.xml");
}

bool MythThemedMenuPrivate::keyPressHandler(QKeyEvent *e)
{
    ThemedButton *lastbutton = activebutton;
    int oldrow = currentrow;
    bool handled = false;

    QStringList actions;
    GetMythMainWindow()->TranslateKeyPress("menu", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (columns == 1)
        {
            if (action == "LEFT")
                action = "ESCAPE";
            else if (action == "RIGHT")
                action = "SELECT";
        }

        if (action == "UP")
        {
            if (maxrows > 1)
            {
                if (currentrow > 0)
                   currentrow--;
                else if (columns == 1)
                    currentrow = buttonRows.size() - 1;

                if (currentcolumn >= buttonRows[currentrow].numitems)
                    currentcolumn = buttonRows[currentrow].numitems - 1;
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "PAGEUP")
        {
            currentrow = max(currentrow - m_state->visiblerowlimit, 0);

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;
        }
        else if (action == "LEFT")
        {
            if (currentcolumn > 0)
                currentcolumn--;
            else
                currentcolumn = buttonRows[currentrow].numitems - 1;
        }
        else if (action == "DOWN")
        {
            if (maxrows > 1)
            {
                if (currentrow < (int)buttonRows.size() - 1)
                    currentrow++;
                else if (columns == 1)
                    currentrow = 0;

                if (currentcolumn >= buttonRows[currentrow].numitems)
                    currentcolumn = buttonRows[currentrow].numitems - 1;
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "PAGEDOWN")
        {
            currentrow = min(currentrow + m_state->visiblerowlimit,
                             (int)buttonRows.size() - 1);

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;
        }
        else if (action == "RIGHT")
        {
            if (currentcolumn < buttonRows[currentrow].numitems - 1)
                currentcolumn++;
            else
                currentcolumn = 0;
        }
        else if (action == "SELECT")
        {
            if (LCD *lcddev = LCD::Get())
                lcddev->switchToTime();

            lastbutton = activebutton;
            activebutton = NULL;
            parent->SetRedraw();

            QStringList::Iterator it = lastbutton->action.begin();
            for (; it != lastbutton->action.end(); it++)
            {
                if (handleAction(*it))
                    break;
            }

            lastbutton = NULL;
        }
        else if (action == "ESCAPE")
        {
            QString action = "UPMENU";
            if (!allocedstate)
                handleAction(action);
            else if (m_state->killable)
            {
                wantpop = true;
                if (m_state->callback != NULL)
                {
                    QString sel = "EXITING_MENU";
                    m_state->callback(m_state->callbackdata, sel);
                }

                if (GetMythMainWindow()->GetMainStack()->TotalScreens() == 1)
                    QApplication::exit();
            }
            else if (exitModifier >= 0 && e->state() == exitModifier &&
                     GetMythMainWindow()->GetMainStack()->TotalScreens() == 1)
            {
                QApplication::exit();
                wantpop = true;
            }
            lastbutton = NULL;
        }
        else
            handled = false;
    }

    if (!handled)
        return false;

    if (!buttonRows[currentrow].visible)
    {
        makeRowVisible(currentrow, oldrow);
        lastbutton = NULL;
    }

    activebutton = buttonRows[currentrow].buttons[currentcolumn];
    updateLCD();

    parent->SetRedraw();
    return true;
} 

QString MythThemedMenuPrivate::findMenuFile(const QString &menuname)
{
    QString testdir = MythContext::GetConfDir() + "/" + menuname;
    QFile file(testdir);
    if (file.exists())
        return testdir;

    testdir = gContext->GetMenuThemeDir() + "/" + menuname;
    file.setName(testdir);
    if (file.exists())
        return testdir;

        
    testdir = gContext->GetThemeDir() + "/" + menuname;
    file.setName(testdir);
    if (file.exists())
        return testdir;
        
    testdir = gContext->GetShareDir() + menuname;
    file.setName(testdir);
    if (file.exists())
        return testdir;
        
    testdir = "../mythfrontend/" + menuname;
    file.setName(testdir);
    if (file.exists())
        return testdir;
        
    return "";
}

bool MythThemedMenuPrivate::handleAction(const QString &action)
{
    if (action.left(5) == "EXEC ")
    {
        QString rest = action.right(action.length() - 5);
        myth_system(rest);

        return false;
    }
    else if (action.left(7) == "EXECTV ")
    {
        QString rest = action.right(action.length() - 7).stripWhiteSpace();
        QStringList strlist = QString("LOCK_TUNER");
        gContext->SendReceiveStringList(strlist);
        int cardid = strlist[0].toInt();

        if (cardid >= 0)
        {
            rest = rest.sprintf(rest,
                                (const char*)strlist[1],
                                (const char*)strlist[2],
                                (const char*)strlist[3]);

            myth_system(rest);

            strlist = QString("FREE_TUNER %1").arg(cardid);
            gContext->SendReceiveStringList(strlist);
            QString ret = strlist[0];
        }
        else
        {
            if (cardid == -2)
                VERBOSE(VB_IMPORTANT, QString("MythThemedMenuPrivate: Card %1 is "
                                              "already locked").arg(cardid));
           
#if 0 
            DialogBox *error_dialog = new DialogBox(gContext->GetMainWindow(),
                    "\n\nAll tuners are currently in use. If you want to watch "
                    "TV, you can cancel one of the in-progress recordings from "
                    "the delete menu");
            error_dialog->AddButton("OK");
            error_dialog->exec();
            delete error_dialog;
#endif
        }
    }
    else if (action.left(5) == "MENU ")
    {
        QString rest = action.right(action.length() - 5);

        if (rest == "main_settings.xml" && 
            gContext->GetNumSetting("SetupPinCodeRequired", 0) &&
            !checkPinCode("SetupPinCodeTime", "SetupPinCode", "Setup Pin:"))
        {
            return true;
        }

        MythScreenStack *stack = parent->GetScreenStack();

        MythThemedMenu *newmenu = new MythThemedMenu(m_state->themeDir.local8Bit(),
                                                     rest, stack, rest, 
                                                     m_state->allowreorder, 
                                                     m_state);
        stack->AddScreen(newmenu);
    }
    else if (action.left(6) == "UPMENU")
    {
        wantpop = true;
    }
    else if (action.left(12) == "CONFIGPLUGIN")
    {
        QString rest = action.right(action.length() - 13);
        MythPluginManager *pmanager = gContext->getPluginManager();
        if (pmanager)
            pmanager->config_plugin(rest.stripWhiteSpace());
    }
    else if (action.left(6) == "PLUGIN")
    {
        QString rest = action.right(action.length() - 7);
        MythPluginManager *pmanager = gContext->getPluginManager();
        if (pmanager)
            pmanager->run_plugin(rest.stripWhiteSpace());
    }
    else if (action.left(8) == "SHUTDOWN")
    {
        if (allocedstate)
        {
            wantpop = true;
        }
    }
    else if (action.left(5) == "JUMP ")
    {
        QString rest = action.right(action.length() - 5);
        GetMythMainWindow()->JumpTo(rest, false);
    }
    else
    {
        selection = action;
        if (m_state->callback != NULL)
            m_state->callback(m_state->callbackdata, selection);
    }

    return true;   
}

bool MythThemedMenuPrivate::findDepends(const QString &fileList)
{
    QStringList files = QStringList::split(" ", fileList);
    QString filename;

    for ( QStringList::Iterator it = files.begin(); it != files.end(); ++it ) 
    {
        QString filename = findMenuFile(*it);
        if (filename != "" && filename.endsWith(".xml"))
            return true;

        QString newname = gContext->FindPlugin(*it);

        QFile checkFile(newname);
        if (checkFile.exists())
            return true;
    }

    return false;
}

bool MythThemedMenuPrivate::checkPinCode(const QString &timestamp_setting, 
                              const QString &password_setting,
                              const QString& /* text */)
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting(timestamp_setting);
    QString password = gContext->GetSetting(password_setting);

    if (password.length() < 1)
        return true;

    if (last_time_stamp.length() < 1)
    {
        VERBOSE(VB_IMPORTANT,
                "MythThemedMenuPrivate: Could not read password/pin time stamp.\n"
                "This is only an issue if it happens repeatedly.");
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp, 
                                                    Qt::TextDate);
        if (last_time.secsTo(curr_time) < 120)
        {
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting(timestamp_setting, last_time_stamp);
            gContext->SaveSetting(timestamp_setting, last_time_stamp);
            return true;
        }
    }

    if (password.length() > 0)
    {
        bool ok = false;
#if 0
        MythPasswordDialog *pwd = new MythPasswordDialog(text, &ok, password,
                                                     gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
#endif
        if (ok)
        {
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting(timestamp_setting, last_time_stamp);
            gContext->SaveSetting(timestamp_setting, last_time_stamp);
            return true;
        }
    }
    else
    {
        return true;
    }

    return false;
}

void MythThemedMenuPrivate::DrawSelf(MythPainter *p, 
                                     int /* xoffset */, 
                                     int /* yoffset */, 
                                     int alphaMod)
{
    m_state->paintLogo(p, alphaMod);
    m_state->paintTitle(p, alphaMod, menumode);
    m_state->paintButtonBackground(p, alphaMod);

    paintWatermark(p, alphaMod);

    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        paintButton(i, p, alphaMod);
    }

    drawScrollArrows(p, alphaMod);
}

////////////////////////////////////////////////////////////////////////////

MythThemedMenu::MythThemedMenu(const char *cdir, const char *menufile,
                               MythScreenStack *parent, const char *name,
                               bool allowreorder, MythThemedMenuState *state)
              : MythScreenType(parent, name)
{
    d = new MythThemedMenuPrivate(this, cdir, state);
    d->m_state->allowreorder = allowreorder;

    Init(cdir, menufile);
}

void MythThemedMenu::Init(const char *cdir, const char *menufile)
{
    QString dir = QString(cdir) + "/";
    QString filename = dir + "theme.xml";

    d->foundtheme = true;
    QFile filetest(filename);
    if (!filetest.exists())
    {
        d->foundtheme = false;
        return;
    }

    ReloadExitKey();

    if (!d->m_state->loaded)
        d->foundtheme = d->m_state->parseSettings(dir, "theme.xml");

    if (d->foundtheme)
        d->parseMenu(menufile);
}

MythThemedMenu::~MythThemedMenu(void)
{
    if (d)
        delete d;
}

bool MythThemedMenu::foundTheme(void)
{
    return d->foundtheme;
}

void MythThemedMenu::setCallback(void (*lcallback)(void *, QString &), void *data)
{
    d->m_state->callback = lcallback;
    d->m_state->callbackdata = data;
}

void MythThemedMenu::setKillable(void)
{
    d->m_state->killable = true;
}

QString MythThemedMenu::getSelection(void)
{
    return d->selection;
}

void MythThemedMenu::ReloadExitKey(void)
{
    int allowsd = gContext->GetNumSetting("AllowQuitShutdown");

    if (allowsd == 1)
        d->exitModifier = Qt::ControlButton;
    else if (allowsd == 2)
        d->exitModifier = Qt::MetaButton;
    else if (allowsd == 3)
        d->exitModifier = Qt::AltButton;
    else if (allowsd == 4)
        d->exitModifier = 0;
    else
        d->exitModifier = -1;
}

void MythThemedMenu::DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                              int alphaMod, QRect clipRect)
{
    int alpha = CalcAlpha(alphaMod);
    d->DrawSelf(p, xoffset, yoffset, alpha);
}

void MythThemedMenu::ReloadTheme(void)
{
    if (!d->ReloadTheme())
        d->foundtheme = false;
}

bool MythThemedMenu::keyPressEvent(QKeyEvent *e)
{
    if (d->ignorekeys)
        return false;

    d->ignorekeys = true;

    bool ret = true;
    if (!d->keyPressHandler(e))
    {
        ret = MythScreenType::keyPressEvent(e);
    }

    d->ignorekeys = false;


    if (d->wantpop)
        m_ScreenStack->PopScreen();

    return ret;
}
