#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qpixmap.h>
#include <qbitmap.h>
#include <qpainter.h>
#include <qimage.h>
#include <qdir.h>
#include <qdom.h>

#include <iostream>
#include <cmath>
using namespace std;

#include "exitcodes.h"
#include "themedmenu.h"
#include "mythcontext.h"
#include "util.h"
#include "mythplugin.h"
#include "dialogbox.h"
#include "lcddevice.h"



extern QMap<QString, fontProp> globalFontMap;

struct TextAttributes
{
    QRect textRect;
    QColor textColor;
    QFont font;
    int textflags;

    bool hasshadow;
    QColor shadowColor;
    QPoint shadowOffset;
    int shadowalpha;

    bool hasoutline;
    QColor outlineColor;
    int outlinesize;
};

struct ButtonIcon
{
    QString name;
    QImage *icon;
    QImage *activeicon;
    QImage *watermark;
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

struct MenuState
{
    QString name;
    int row;
    int col;
};

class ThemedMenuPrivate
{
  public:
    ThemedMenuPrivate(ThemedMenu *lparent, float lwmult, float lhmult,
                      int lscreenwidth, int lscreenheight);
   ~ThemedMenuPrivate();

    bool keyPressHandler(QKeyEvent *e);
    bool ReloadTheme(void);

    bool parseMenu(const QString &menuname, int row = -1, int col = -1);

    bool parseSettings(const QString &dir, const QString &menuname);

    void parseBackground(const QString &dir, QDomElement &element);
    void parseLogo(const QString &dir, QDomElement &element);
    void parseArrow(const QString &dir, QDomElement &element, bool up);
    void parseTitle(const QString &dir, QDomElement &element);
    void parseButtonDefinition(const QString &dir, QDomElement &element);
    void parseButton(const QString &dir, QDomElement &element);
    void parseThemeButton(QDomElement &element);

    void parseText(TextAttributes &attributes, QDomElement &element);
    void parseOutline(TextAttributes &attributes, QDomElement &element);
    void parseShadow(TextAttributes &attributes, QDomElement &element);
    
    void parseFont(QDomElement &element);

    void setDefaults(void);

    void addButton(const QString &type, const QString &text,
                   const QString &alttext, const QStringList &action);
    bool layoutButtons(void);
    void positionButtons(bool resetpos);
    bool makeRowVisible(int newrow, int oldrow, bool forcedraw = true);

    bool handleAction(const QString &action);
    bool findDepends(const QString &file);
    QString findMenuFile(const QString &menuname);

    QString getFirstText(QDomElement &element);
    QPoint parsePoint(const QString &text);
    QRect parseRect(const QString &text);
    
    QRect menuRect() const;

    void paintLogo(QPainter *p);
    void paintTitle(QPainter *p);
    void paintWatermark(QPainter *p);
    void paintButton(unsigned int button, QPainter *p, bool erased,
                     bool drawinactive = false);

    void drawText(QPainter *p, const QRect &rect,
                  const TextAttributes &attributes, const QString &text);

    void clearToBackground(void);
    void drawInactiveButtons(void);
    void drawScrollArrows(QPainter *p);
    bool checkPinCode(const QString &timestamp_setting,
                      const QString &password_setting,
                      const QString &text);

    void updateLCD(void);

    ThemedMenu *parent;

    float wmult;
    float hmult;

    int screenwidth;
    int screenheight;
    
    QRect buttonArea;

    QRect logoRect;
    QPixmap *logo;
    
    QImage *buttonnormal;
    QImage *buttonactive;

    QMap<QString, ButtonIcon> allButtonIcons;

    vector<ThemedButton> buttonList;
    ThemedButton *activebutton;
    int currentrow;
    int currentcolumn;

    vector<MenuRow> buttonRows;

    TextAttributes normalAttributes;
    TextAttributes activeAttributes;

    QString selection;
    bool foundtheme;
    bool balancerows;
    int menulevel;
    vector<MenuState> menufiles;

    void (*callback)(void *, QString &);
    void *callbackdata;

    bool killable;
    int exitModifier;

    bool spreadbuttons;
    bool buttoncenter;

    QMap<QString, QPixmap> titleIcons;
    QPixmap *curTitle;
    QString titleText;
    QPoint titlePos;
    QRect titleRect;
    bool drawTitle;

    QPixmap backgroundPixmap;
    QPixmap* buttonBackground;

    bool ignorekeys;

    int visiblerowlimit;
    int maxrows;
    int visiblerows;
    int columns;

    QPixmap *uparrow;
    QRect uparrowRect;
    QPixmap *downarrow;
    QRect downarrowRect;

    QPoint watermarkPos;
    QRect watermarkRect;

    bool allowreorder;
    int maxColumns;
    QString fontSizeType;
    static bool parseFonts;
};

bool ThemedMenuPrivate::parseFonts = true;

ThemedMenuPrivate::ThemedMenuPrivate(ThemedMenu *lparent, float lwmult, 
                                     float lhmult, int lscreenwidth, 
                                     int lscreenheight)
{
    parent = lparent;
    wmult = lwmult;
    hmult = lhmult;
    screenwidth = lscreenwidth;
    screenheight = lscreenheight;
    if (parseFonts)
        fontSizeType = gContext->GetSetting("ThemeFontSizeType", "default");
    allowreorder = true;

    logo = NULL;
    buttonnormal = NULL;
    buttonactive = NULL;
    uparrow = NULL;
    downarrow = NULL;
    buttonBackground = NULL;

    ignorekeys = false;
}

ThemedMenuPrivate::~ThemedMenuPrivate()
{
    if (logo)
        delete logo;
    if (buttonnormal)
        delete buttonnormal;
    if (buttonactive)
        delete buttonactive;
    if (uparrow)
        delete uparrow;
    if (downarrow)
        delete downarrow;
    if (buttonBackground)
        delete buttonBackground;
        
    QMap<QString, ButtonIcon>::Iterator it;
    for (it = allButtonIcons.begin(); it != allButtonIcons.end(); ++it)
    {
        if (it.data().icon)
            delete it.data().icon;
        if (it.data().activeicon)
            delete it.data().activeicon;
        if (it.data().watermark)
            delete it.data().watermark;
    }
}

QString ThemedMenuPrivate::getFirstText(QDomElement &element)
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

void ThemedMenuPrivate::parseBackground(const QString &dir, 
                                        QDomElement &element)
{
    bool tiledbackground = false;
    QPixmap *bground = NULL;    
    QString path;

    bool hasarea = false;
    buttoncenter = true;
    spreadbuttons = true;
    maxColumns = 20;        // Arbitrary number
    visiblerowlimit = 6;    // the old default

    QString type = element.attribute("style", "");
    if (type == "tiled")
        tiledbackground = true;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
                path = dir + getFirstText(info);
                bground = gContext->LoadScalePixmap(path);
            }
            else if (info.tagName() == "buttonarea")
            {
                QRect tmpArea = parseRect(getFirstText(info));

                tmpArea.moveTopLeft(QPoint((int)(tmpArea.x() * wmult), 
                                           (int)(tmpArea.y() * hmult)));
                tmpArea.setWidth((int)(tmpArea.width() * wmult));
                tmpArea.setHeight((int)(tmpArea.height() * hmult));

                buttonArea = tmpArea;
                hasarea = true;
                
                if (info.hasAttribute( "background" ))
                {
                    cerr << "Loading "  << info.attribute( "background" ) << endl;
                    QString bPath = dir + info.attribute( "background" );
                    buttonBackground = gContext->LoadScalePixmap(bPath);
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
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 in "
                                            "background").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasarea)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing buttonaread in background");
        return;
    }

    if (bground)
    {
        QPixmap background(screenwidth, screenheight);
        QPainter tmp(&background);

        if (tiledbackground)
            tmp.drawTiledPixmap(0, 0, screenwidth, screenheight,
                                *bground);
        else
            tmp.drawPixmap(menuRect(), *bground);

        tmp.end();

        parent->setPaletteBackgroundPixmap(background);
        parent->erase(menuRect());

        backgroundPixmap = background;

        delete bground;
    }
}

void ThemedMenuPrivate::parseShadow(TextAttributes &attributes, 
                                    QDomElement &element)
{
    attributes.hasshadow = true;

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
                attributes.shadowColor = QColor(getFirstText(info));
                hascolor = true;
            }
            else if (info.tagName() == "offset")
            {
                attributes.shadowOffset = parsePoint(getFirstText(info));
                attributes.shadowOffset.setX((int)(attributes.shadowOffset.x() *
                                             wmult));
                attributes.shadowOffset.setY((int)(attributes.shadowOffset.y() *
                                             hmult));
                hasoffset = true;
            }
            else if (info.tagName() == "alpha")
            {
                attributes.shadowalpha = atoi(getFirstText(info).ascii());
                hasalpha = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 in "
                                            "text/shadow").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hascolor)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing color tag in shadow");
        return;
    }

    if (!hasalpha)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing alpha tag in shadow");
        return;
    }

    if (!hasoffset)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing offset tag in shadow");
        return;
    }
}


void ThemedMenuPrivate::parseFont(QDomElement &element)
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

    

    QString base =  element.attribute("base", "");
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
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
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
    
    if(baseFont)
        newFont = *baseFont;

    if ( haveSizeSmall && fontSizeType == "small")
    {
        if (sizeSmall > 0)
            size = sizeSmall;
    }
    else if( haveSizeBig && fontSizeType == "big")
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
    else    
        size = (int)ceil(size * hmult);
    
    // If we don't have to, don't load the font.
    if (!haveFace && baseFont)
    {
        newFont.face = baseFont->face;
        if(haveSize)
            newFont.face.setPointSize(size);
    }
    else
    {
        QFont temp(face, size);
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




void ThemedMenuPrivate::parseOutline(TextAttributes &attributes, 
                                     QDomElement &element)
{
    attributes.hasoutline = true;

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
                attributes.outlineColor = QColor(getFirstText(info));
                hascolor = true;
            }
            else if (info.tagName() == "size")
            {
                attributes.outlinesize = atoi(getFirstText(info).ascii());
                attributes.outlinesize = (int)(attributes.outlinesize * hmult);
                hassize = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 in "
                                            "text/shadow").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hassize)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing size in outline");
        return;
    }

    if (!hascolor)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing color in outline");
        return;
    }
}

void ThemedMenuPrivate::parseText(TextAttributes &attributes, 
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
                attributes.textRect.moveTopLeft(QPoint(
                                       (int)(attributes.textRect.x() * wmult),
                                       (int)(attributes.textRect.y() * hmult)));
                attributes.textRect.setWidth((int)
                                       (attributes.textRect.width() * wmult));
                attributes.textRect.setHeight((int)
                                       (attributes.textRect.height() * hmult));
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
                attributes.textColor = QColor(getFirstText(info));
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
                        attributes.textflags = Qt::AlignTop | Qt::AlignHCenter |
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
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 "
                                            "in text").arg(info.tagName()));
                return;
            }
        }
    }

    attributes.font = QFont(fontname, (int)ceil(fontsize * hmult), weight, 
                            italic);

    if (!hasarea)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing 'area' "
                "tag in 'text' element of 'genericbutton'");
        return;
    }
}

void ThemedMenuPrivate::parseButtonDefinition(const QString &dir, 
                                              QDomElement &element)
{
    bool hasnormal = false;
    bool hasactive = false;
    bool hasactivetext = false;

    QString setting;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "normal")
            {
                setting = dir + getFirstText(info);
                buttonnormal = gContext->LoadScaleImage(setting);
                if (buttonnormal)
                    hasnormal = true;
            }
            else if (info.tagName() == "active")
            {
                setting = dir + getFirstText(info);
                buttonactive = gContext->LoadScaleImage(setting);
                if (buttonactive)
                    hasactive = true;
            }
            else if (info.tagName() == "text")
            {
                if (!hasnormal)
                {
                    VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: The 'normal' "
                            "tag needs to come before the 'normaltext' tag");
                    return;
                }
                parseText(normalAttributes, info);
            }
            else if (info.tagName() == "activetext")
            {
                if (!hasactive)
                {
                    VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: The 'active' "
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
                        QString("ThemedMenuPrivate: Unknown tag %1 in "
                                "genericbutton").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasnormal)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: No normal button image defined");
        return;
    }

    if (!hasactive)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: No active button image defined");
        return;
    }

    if (!hasactivetext)
    {
        activeAttributes = normalAttributes;
    }

    watermarkPos.setX((int)(watermarkPos.x() * wmult));
    watermarkPos.setY((int)(watermarkPos.y() * hmult));

    watermarkRect = QRect(watermarkPos, QSize(0, 0));
}

void ThemedMenuPrivate::parseLogo(const QString &dir, QDomElement &element)
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
                logo = gContext->LoadScalePixmap(logopath); 
                if (logo)
                    hasimage = true;
            }
            else if (info.tagName() == "position")
            {
                logopos = parsePoint(getFirstText(info));
                hasposition = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 "
                                            "in logo").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasimage)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing image tag in logo");
        return;
    }

    if (!hasposition)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing position tag in logo");
        return;
    }

    logopos.setX((int)(logopos.x() * wmult));
    logopos.setY((int)(logopos.y() * hmult));
    logoRect = QRect(logopos.x(), logopos.y(), logo->width(),
                     logo->height());
}

void ThemedMenuPrivate::parseTitle(const QString &dir, QDomElement &element)
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
                QPixmap *tmppix = gContext->LoadScalePixmap(titlepath);

                if (!tmppix)
                    continue;

                QString name = info.attribute("mode", "");
                if (name != "")
                {
                    titleIcons[name] = *tmppix;
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: "
                            "Missing mode in titles/image");
                    return;
                }

                delete tmppix;

                hasimage = true;
            }
            else if (info.tagName() == "position")
            {
                titlePos = parsePoint(getFirstText(info));
                hasposition = true;
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 "
                                            "in logo").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasimage)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing image tag in titles");
        return;
    }

    if (!hasposition)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing position tag in titles");
        return;
    }

    titlePos.setX((int)(titlePos.x() * wmult));
    titlePos.setY((int)(titlePos.y() * hmult));
}

void ThemedMenuPrivate::parseArrow(const QString &dir, QDomElement &element, 
                                   bool up)
{
    QRect arrowrect;
    QPoint arrowpos;
    QPixmap *pix = NULL;    

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
                pix = gContext->LoadScalePixmap(arrowpath);
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
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 "
                                            "in arrow").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasimage)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing image tag in arrow");
        return;
    }

    if (!hasposition)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing position tag in arrow");
        return;
    }

    arrowpos.setX((int)(arrowpos.x() * wmult));
    arrowpos.setY((int)(arrowpos.y() * hmult));
    arrowrect = QRect(arrowpos.x(), arrowpos.y(), pix->width(),
                      pix->height());

    if (up)
    {
        uparrow = pix;
        uparrowRect = arrowrect;
    }
    else
    {
        downarrow = pix;
        downarrowRect = arrowrect;
    }
}

void ThemedMenuPrivate::parseButton(const QString &dir, QDomElement &element)
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
                offset.setX((int)(offset.x() * wmult));
                offset.setY((int)(offset.y() * hmult));
                hasoffset = true;
            }
            else if (info.tagName() == "watermarkimage")
            {
                QString imagepath = dir + getFirstText(info);
                watermark = gContext->LoadScaleImage(imagepath);
            }    
            else
            {
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 "
                                            "in buttondef").arg(info.tagName()));
                return;
            }
        }
    }

    if (!hasname)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing name in button");
        return;
    }

    if (!hasoffset)
    {
        VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Missing offset "
                                      "in buttondef %1").arg(name));
        return;
    }

    if (!hasicon) 
    {
        VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Missing image "
                                      "in buttondef %1").arg(name));
        return;
    }

    ButtonIcon newbutton;

    newbutton.name = name;
    newbutton.icon = image;
    newbutton.offset = offset;
    newbutton.activeicon = activeimage;
    newbutton.watermark = watermark;

    if (watermark)
    {
        if (watermark->width() > watermarkRect.width())
            watermarkRect.setWidth(watermark->width());

        if (watermark->height() > watermarkRect.height())
            watermarkRect.setHeight(watermark->height());
    }

    allButtonIcons[name] = newbutton;
}

void ThemedMenuPrivate::setDefaults(void)
{
    logo = NULL;
    buttonnormal = buttonactive = NULL;
    balancerows = true;
    normalAttributes.textflags = Qt::AlignTop | Qt::AlignLeft | Qt::WordBreak;
    normalAttributes.textColor = QColor(QWidget::white);
    normalAttributes.hasoutline = false;
    normalAttributes.hasshadow = false;

    activeAttributes.textflags = Qt::AlignTop | Qt::AlignLeft | Qt::WordBreak;
    activeAttributes.textColor = QColor(QWidget::white);
    activeAttributes.hasoutline = false;
    activeAttributes.hasshadow = false;

    titleIcons.clear();
    titleText = "";
    curTitle = NULL;
    drawTitle = false;
    uparrow = NULL;
    downarrow = NULL;
    watermarkPos = QPoint(0, 0);
    watermarkRect = QRect(0, 0, 0, 0);
}

bool ThemedMenuPrivate::parseSettings(const QString &dir, 
                                      const QString &menuname)
{
    QString filename = dir + menuname;

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate::parseSettings(): "
                                      "Can't open: %1").arg(filename));
        return false;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Error, parsing %1\n"
                                      "at line: %2  column: %3 msg: %4").
                arg(filename).arg(errorLine).arg(errorColumn).arg(errorMsg));
        f.close();
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Bad Menu File"),
                                  QObject::tr(QString("The menu file %1 is "
                                              "incomplete.\n\nWe will now "
                                              "return to the main menu.")
                                              .arg(menuname)));
        menulevel = 0;
        return parseMenu("mainmenu.xml");
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
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown "
                                            "element %1. Ignoring.")
                        .arg(e.tagName()));
            }
        }
        n = n.nextSibling();
    }

    if (!setbackground)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing background element");
        return false;
    }

    if (!setbuttondef)
    {
        VERBOSE(VB_IMPORTANT,
                "ThemedMenuPrivate: Missing genericbutton definition");
        return false;
    }

    parseFonts = false;
    return true;
}

void ThemedMenuPrivate::parseThemeButton(QDomElement &element)
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
                addit = gContext->GetMainWindow()->DestinationExists(getFirstText(info));
            }
            else
            {
                VERBOSE(VB_GENERAL, QString("ThemedMenuPrivate: Unknown tag %1 "
                                            "in button").arg(info.tagName()));
                return;
            }
        }
    }

    if (text == "")
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing 'text' in button");
        return;
    }
   
    if (action.empty())
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Missing 'action' in button");
        return;
    }

    if (addit)
        addButton(type, text, alttext, action);
}

bool ThemedMenuPrivate::parseMenu(const QString &menuname, int row, int col)
{
    QString filename = findMenuFile(menuname);

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Couldn't read "
                                      "menu file %1").arg(menuname));
        if (menuname == "mainmenu.xml" )
        {
            return false;
        }
        else
        {
            MythPopupBox::showOkPopup(
                gContext->GetMainWindow(), QObject::tr("No Menu File"),
                QObject::tr(QString("Myth could not locate the menu file %1.\n\n"
                                    "We will now return to the main menu.").
                            arg(menuname)));
            menulevel = 0;
            return parseMenu("mainmenu.xml");
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

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Bad Menu File"),
                                  QObject::tr(QString("The menu file %1 is "
                                              "incomplete.\n\nWe will now "
                                              "return to the main menu.")
                                              .arg(menuname)));
        menulevel = 0;
        return parseMenu("mainmenu.xml");
    }

    f.close();

    buttonList.clear();
    buttonRows.clear();

    QDomElement docElem = doc.documentElement();

    QString menumode = docElem.attribute("name", "");

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
                VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Unknown "
                                              "element %1").arg(e.tagName()));
                return false;
            }
        }
        n = n.nextSibling();
    }

    if (buttonList.size() == 0)
    {
        VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: No buttons "
                                      "for menu %1").arg(menuname));
        return false;
    }

    if (!layoutButtons())
        return false;
    positionButtons(true);

    if (row != -1 && col != -1)
    {
        int oldrow = currentrow;

        if (row < (int)buttonRows.size() && col < buttonRows[row].numitems)
        {
            currentrow = row;
            currentcolumn = col;
        }

        while (!buttonRows[currentrow].visible)
        {
            makeRowVisible(oldrow + 1, oldrow, false);
            oldrow = oldrow + 1;
        }

        activebutton = buttonRows[currentrow].buttons[currentcolumn];
    }

#ifndef QWS
    parent->setNoErase();
#endif

    menulevel++;

    MenuState state;
    state.name = menuname;
    state.row = currentrow;
    state.col = currentcolumn;
    menufiles.push_back(state);

    if (titleIcons.contains(menumode))
    {
        drawTitle = true;
        curTitle = &(titleIcons[menumode]);
        titleRect = QRect(titlePos.x(), titlePos.y(), curTitle->width(), 
                          curTitle->height());
    }
    else
    {
        drawTitle = false;
    }
    drawInactiveButtons();

    if (LCD::Get())
    {
        titleText = "MYTH-";
        titleText += menumode;
        updateLCD();
    }

    selection = "";
    parent->update(menuRect());
    return true;
}

void ThemedMenuPrivate::updateLCD()
{
    class LCD * lcddev = LCD::Get();
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

QPoint ThemedMenuPrivate::parsePoint(const QString &text)
{
    int x, y;
    QPoint retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QRect ThemedMenuPrivate::parseRect(const QString &text)
{
    int x, y, w, h;
    QRect retval;
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    retval = retval.normalize();

    return retval;
}

void ThemedMenuPrivate::addButton(const QString &type, const QString &text, 
                                  const QString &alttext, 
                                  const QStringList &action)
{
    ThemedButton newbutton;

    newbutton.buttonicon = NULL;
    if (allButtonIcons.find(type) != allButtonIcons.end())
        newbutton.buttonicon = &(allButtonIcons[type]);

    newbutton.text = text;
    newbutton.altText = alttext;
    newbutton.action = action;
    newbutton.status = -1;
    newbutton.visible = false;

    buttonList.push_back(newbutton);
}

bool ThemedMenuPrivate::layoutButtons(void)
{
    int numbuttons = buttonList.size();
  
    columns = buttonArea.width() / buttonnormal->width();
    columns = columns > maxColumns ? maxColumns : columns;
    
    maxrows = buttonArea.height() / buttonnormal->height();

    if (maxrows < 1)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Must have "
                "room for at least 1 row of buttons");
        return false;
    }
    
    if (columns < 1)
    {
        VERBOSE(VB_IMPORTANT, "ThemedMenuPrivate: Must have "
                "room for at least 1 column of buttons");
        return false;
    }

    if (balancerows)
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
    if (columns * maxrows > visiblerowlimit)
    {
        maxrows = visiblerowlimit / columns;
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
            if (columns == 3 && j == 1 && allowreorder)
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

void ThemedMenuPrivate::positionButtons(bool resetpos)
{
    int rows = visiblerows;
    int yspacing = (buttonArea.height() - buttonnormal->height() * rows) /
                   (rows + 1);
    int ystart = 0;
    
    if (!spreadbuttons)
    {
        yspacing = 0;
        if (buttoncenter)
        {
            ystart = (buttonArea.height() - buttonnormal->height() * rows) / 2;
        }
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

        int ypos = yspacing * row + (buttonnormal->height() * (row - 1));
        ypos += buttonArea.y() + ystart;

        int xspacing = (buttonArea.width() - buttonnormal->width() *
                       (*menuiter).numitems) / ((*menuiter).numitems + 1);
        int col = 1;
        vector<ThemedButton *>::iterator biter = (*menuiter).buttons.begin();
        for (; biter != (*menuiter).buttons.end(); biter++)
        {
            int xpos = xspacing * col + (buttonnormal->width() * (col - 1));
            xpos = (maxColumns == 1) ? 0 : xpos;
            xpos += buttonArea.x();

            ThemedButton *tbutton = (*biter);

            tbutton->visible = true;
            tbutton->row = row;
            tbutton->col = col;
            tbutton->pos = QPoint(xpos, ypos);
            tbutton->posRect = QRect(tbutton->pos.x(), tbutton->pos.y(),
                                     buttonnormal->width(),
                                     buttonnormal->height());
                
                
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

bool ThemedMenuPrivate::makeRowVisible(int newrow, int oldrow, bool forcedraw)
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

    if (forcedraw)
        clearToBackground();

    return true;
}

QRect ThemedMenuPrivate::menuRect() const
{
    QRect r(0, 0, screenwidth, screenheight);
    return r;
}

void ThemedMenuPrivate::clearToBackground(void)
{
    drawInactiveButtons();
}

void ThemedMenuPrivate::drawInactiveButtons(void)
{
    QPixmap bground = backgroundPixmap;

    QPainter tmp(&bground);

    paintLogo(&tmp);
    paintTitle(&tmp);
    
    
    if (buttonBackground)
    {
        tmp.drawPixmap(buttonArea.topLeft(), *buttonBackground);
         
    }

    ThemedButton *store = activebutton;
    activebutton = NULL;
    
    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        paintButton(i, &tmp, true, true);
    }

    drawScrollArrows(&tmp);

    activebutton = store;

    tmp.end();

    parent->setPaletteBackgroundPixmap(bground);

    parent->erase(buttonArea);
    parent->erase(uparrowRect);
    parent->erase(downarrowRect);
    parent->erase(logoRect);
    if (drawTitle)
        parent->erase(titleRect);
}

void ThemedMenuPrivate::drawScrollArrows(QPainter *p)
{
    if (!uparrow || !downarrow)
        return;

    bool needup = false;
    bool needdown = false;

    if (!buttonRows.front().visible)
        needup = true;
    if (!buttonRows.back().visible)
        needdown = true;

    if (!needup && !needdown)
        return;

    if (needup)
        p->drawPixmap(uparrowRect.topLeft(), *uparrow);
    if (needdown)
        p->drawPixmap(downarrowRect.topLeft(), *downarrow);
}

void ThemedMenuPrivate::paintLogo(QPainter *p)
{
    if (logo)
        p->drawPixmap(logoRect.topLeft(), *logo);
}

void ThemedMenuPrivate::paintTitle(QPainter *p)
{
    if (curTitle)
        p->drawPixmap(titleRect.topLeft(), *curTitle);
}

void ThemedMenuPrivate::paintWatermark(QPainter *p)
{
    QPixmap pix(watermarkRect.size());

    QPainter tmp(&pix, this);
    tmp.drawPixmap(QPoint(0, 0), backgroundPixmap, watermarkRect);

    if (activebutton && activebutton->buttonicon &&
        activebutton->buttonicon->watermark)
    {
        tmp.drawImage(QPoint(0, 0), *(activebutton->buttonicon->watermark));
    }

    tmp.end();
    p->drawPixmap(watermarkPos, pix);
}

void ThemedMenuPrivate::paintButton(unsigned int button, QPainter *p, 
                                    bool erased, bool drawinactive)
{
    TextAttributes attributes;
    ThemedButton *tbutton = &(buttonList[button]);

    if (!tbutton->visible)
        return;

    QRect cr;
    if (tbutton->buttonicon)
        cr = tbutton->posRect.unite(tbutton->iconRect);
    else
        cr = tbutton->posRect;

    
    if (!erased)
    {
        if (tbutton->status == 1 && activebutton == tbutton)
            return;
        if (tbutton->status == 0 && activebutton != tbutton)
            return;
    }

    QRect newRect(0, 0, tbutton->posRect.width(), tbutton->posRect.height());
    newRect.moveBy(tbutton->posRect.x() - cr.x(), 
                   tbutton->posRect.y() - cr.y());

    QImage *buttonback = NULL;
    if (tbutton == activebutton)
    {
        buttonback = buttonactive;
        tbutton->status = 1;
        attributes = activeAttributes;
    }
    else
    {
        tbutton->status = 0;

        if (!drawinactive)
        {
            parent->erase(cr);
            return;
        }
        buttonback = buttonnormal;
        attributes = normalAttributes;
    }

    QPixmap pix(cr.size());

    QPainter tmp;
    tmp.begin(&pix, this);

    tmp.drawPixmap(QPoint(0, 0), backgroundPixmap, cr);

    tmp.drawImage(newRect.topLeft(), *buttonback);

    QRect buttonTextRect = attributes.textRect;
    buttonTextRect.moveBy(newRect.x(), newRect.y());

    QString message = tbutton->text;

    QRect testBoundRect = buttonTextRect;
    testBoundRect.addCoords(0, 0, 0, 40);
    tmp.setFont(attributes.font);
    QRect testBound = tmp.boundingRect(testBoundRect, attributes.textflags, 
                                       message);

    if (testBound.height() > buttonTextRect.height() && tbutton->altText != "")
        message = buttonList[button].altText;

#if !defined(QWS) || defined(_WIN32)
    if (attributes.hasshadow && attributes.shadowalpha > 0)
    {
        QPixmap textpix(buttonTextRect.size());
        textpix.fill(QColor(attributes.shadowColor));
        textpix.setMask(textpix.createHeuristicMask());

        QRect myrect = buttonTextRect;
        myrect.moveTopLeft(attributes.shadowOffset);

        QPainter texttmp;
        texttmp.begin(&textpix, this);
        texttmp.setPen(QPen(attributes.shadowColor, 1));
        texttmp.setFont(attributes.font);
        texttmp.drawText(myrect, attributes.textflags, message);
        texttmp.end();

        texttmp.begin(textpix.mask());
        texttmp.setPen(QPen(Qt::color1, 1));
        texttmp.setFont(attributes.font);
        texttmp.drawText(myrect, attributes.textflags, message);
        texttmp.end();

        QImage im = textpix.convertToImage();

        for (int x = 0; x < im.width(); x++)
        {
            for (int y = 0; y < im.height(); y++)
            {
               uint *p = (uint *)im.scanLine(y) + x;
               QRgb col = *p;
               if (qAlpha(col) > 128)
                   *p = qRgba(qRed(col), qGreen(col), qBlue(col), 
                              attributes.shadowalpha);
            }
        } 

        textpix.convertFromImage(im);

        myrect.moveTopLeft(QPoint(0, 0));
        bitBlt(&pix, buttonTextRect.topLeft(), &textpix, myrect, Qt::CopyROP);
    }
#endif

    tmp.setFont(attributes.font);
    drawText(&tmp, buttonTextRect, attributes, message);

    if (buttonList[button].buttonicon)
    {
        newRect = QRect(tbutton->iconRect.x() - cr.x(), 
                        tbutton->iconRect.y() - cr.y(), 
                        tbutton->iconRect.width(), tbutton->iconRect.height());

        QImage *blendImage = tbutton->buttonicon->icon;

        if (tbutton == activebutton && tbutton->buttonicon->activeicon)
            blendImage = tbutton->buttonicon->activeicon;

        tmp.drawImage(newRect.topLeft(), *blendImage);
    }


    tmp.flush();
    tmp.end();

    p->drawPixmap(cr.topLeft(), pix);
}

void ThemedMenuPrivate::drawText(QPainter *p, const QRect &rect, 
                                 const TextAttributes &attributes, 
                                 const QString &text)
{
    if (attributes.hasoutline)
    {
        QRect outlinerect = rect;

        p->setPen(QPen(attributes.outlineColor, 1));
        outlinerect.moveBy(0 - attributes.outlinesize, 
                           0 - attributes.outlinesize);
        p->drawText(outlinerect, attributes.textflags, text);

        for (int i = (0 - attributes.outlinesize + 1); 
             i <= attributes.outlinesize; i++)
        {
            outlinerect.moveBy(1, 0);
            p->drawText(outlinerect, attributes.textflags, text);
        }

        for (int i = (0 - attributes.outlinesize + 1); 
             i <= attributes.outlinesize; i++)
        {
            outlinerect.moveBy(0, 1);
            p->drawText(outlinerect, attributes.textflags, text);
        }
  
        for (int i = (0 - attributes.outlinesize + 1); 
             i <= attributes.outlinesize; i++)
        {
            outlinerect.moveBy(-1, 0);
            p->drawText(outlinerect, attributes.textflags, text);
        }

        for (int i = (0 - attributes.outlinesize + 1); 
             i <= attributes.outlinesize; i++)
        {
            outlinerect.moveBy(0, -1);
            p->drawText(outlinerect, attributes.textflags, text);
        }

        p->setPen(QPen(attributes.textColor, 1));
        p->drawText(rect, attributes.textflags, text);
    }
    else
    {
        p->setPen(QPen(attributes.textColor, 1));
        p->drawText(rect, attributes.textflags, text);
    }
}

bool ThemedMenuPrivate::ReloadTheme(void)
{
    
    globalFontMap.clear();
    parseFonts = true;
    buttonList.clear();
    buttonRows.clear();

    parent->ReloadExitKey();
  
    QMap<QString, ButtonIcon>::Iterator it;
    for (it = allButtonIcons.begin(); it != allButtonIcons.end(); ++it)
    {
        if (it.data().icon)
            delete it.data().icon;
        if (it.data().activeicon)
            delete it.data().activeicon;
        if (it.data().watermark)
            delete it.data().watermark;
    }
    allButtonIcons.clear();

    if (logo)
        delete logo;
    logo = NULL;

    titleIcons.clear();

    if (buttonnormal)
        delete buttonnormal;
    buttonnormal = NULL;

    if (buttonactive)
        delete buttonactive;
    buttonactive = NULL;

    if (uparrow)
        delete uparrow;
    uparrow = NULL;

    if (downarrow)
        delete downarrow;
    downarrow = NULL;

    QString themedir = gContext->GetThemeDir();
    
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    parent->setFixedSize(QSize(screenwidth, screenheight));

    parent->setFont(gContext->GetMediumFont());

    gContext->ThemeWidget(parent);

    
    bool ok = parseSettings(themedir, "theme.xml");
    if (!ok)
        return ok;

    QString file = menufiles.back().name;
    int row = menufiles.back().row;
    int col = menufiles.back().col;
    menufiles.pop_back();
    menulevel--;

    return parseMenu(file, row, col);
}

bool ThemedMenuPrivate::keyPressHandler(QKeyEvent *e)
{
    ThemedButton *lastbutton = activebutton;
    int oldrow = currentrow;
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("menu", e, actions);

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
            if( maxrows > 1)
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
            currentrow = max(currentrow - visiblerowlimit, 0);

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
            if( maxrows > 1)
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
            currentrow = min(currentrow + visiblerowlimit,
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
            if (class LCD * lcddev = LCD::Get())
                lcddev->stopAll();

            lastbutton = activebutton;
            activebutton = NULL;
            parent->repaint(lastbutton->posRect);

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
            if (menulevel > 1)
                handleAction(action);
            else if (killable || e->state() == exitModifier)
                parent->done(0);
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

    parent->update(watermarkRect);
    if (lastbutton)
        parent->update(lastbutton->paintRect);
    parent->update(activebutton->paintRect);

    return true;
} 

QString ThemedMenuPrivate::findMenuFile(const QString &menuname)
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

bool ThemedMenuPrivate::handleAction(const QString &action)
{
    bool ok = true;
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
                VERBOSE(VB_IMPORTANT, QString("ThemedMenuPrivate: Card %1 is "
                                              "already locked").arg(cardid));
            
            DialogBox *error_dialog = new DialogBox(gContext->GetMainWindow(),
                    "\n\nAll tuners are currently in use. If you want to watch "
                    "TV, you can cancel one of the in-progress recordings from "
                    "the delete menu");
            error_dialog->AddButton("OK");
            error_dialog->exec();
            delete error_dialog;
        }
    }
    else if (action.left(5) == "MENU ")
    {
        QString rest = action.right(action.length() - 5);

        menufiles.back().row = currentrow;
        menufiles.back().col = currentcolumn;

        if (rest == "main_settings.xml" && 
            gContext->GetNumSetting("SetupPinCodeRequired", 0) &&
            !checkPinCode("SetupPinCodeTime", "SetupPinCode", "Setup Pin:"))
        {
            return true;
        }

        ok = parseMenu(rest);
    }
    else if (action.left(6) == "UPMENU")
    {
        menufiles.pop_back();
        QString file = menufiles.back().name;
        int row = menufiles.back().row;
        int col = menufiles.back().col;
        menufiles.pop_back();

        menulevel -= 2;
 
        ok = parseMenu(file, row, col);
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
        if (menulevel == 1)
            parent->done(0);
    }
    else if (action.left(5) == "JUMP ")
    {
        QString rest = action.right(action.length() - 5);
        gContext->GetMainWindow()->JumpTo(rest);
    }
    else
    {
        selection = action;
        if (callback != NULL)
        {
            callback(callbackdata, selection);
        }
    }

    if (!ok)
    {
        exit(FIXME_BUG__LIBRARY_EXIT_NO_THEME);
    }

    return true;
}

bool ThemedMenuPrivate::findDepends(const QString &fileList)
{
    QStringList files = QStringList::split(" ", fileList);
    QString filename;
    
    for ( QStringList::Iterator it = files.begin(); it != files.end(); ++it ) {
        filename = findMenuFile(*it);
        if (filename != "")
            return true;
    
        QString newname = gContext->FindPlugin(*it);
    
        QFile checkFile(newname);
        if (checkFile.exists())
            return true;
    }
    
    return false;
}

bool ThemedMenuPrivate::checkPinCode(const QString &timestamp_setting, 
                              const QString &password_setting,
                              const QString &text)
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting(timestamp_setting);
    QString password = gContext->GetSetting(password_setting);

    if (password.length() < 1)
        return true;

    if (last_time_stamp.length() < 1)
    {
        VERBOSE(VB_IMPORTANT,
                "ThemedMenuPrivate: Could not read password/pin time stamp.\n"
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
        MythPasswordDialog *pwd = new MythPasswordDialog(text, &ok, password,
                                                     gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
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

ThemedMenu::ThemedMenu(const char *cdir, const char *menufile,
                       MythMainWindow *parent, bool allowreorder)
          : MythDialog(parent, 0)
{
    d = new ThemedMenuPrivate(this, wmult, hmult, screenwidth, screenheight);
    d->allowreorder = allowreorder;

    Init(cdir, menufile);
}

ThemedMenu::ThemedMenu(const char *cdir, const char *menufile,
                       MythMainWindow *parent, const char *name)
          : MythDialog(parent, name)
{
    d = new ThemedMenuPrivate(this, wmult, hmult, screenwidth, screenheight);

    Init(cdir, menufile);
}

void ThemedMenu::Init(const char *cdir, const char *menufile)
{
    QString dir = QString(cdir) + "/";
    QString filename = dir + "theme.xml";

    QFile filetest(filename);
    if (!filetest.exists())
    {
        d->foundtheme = false;
        return;
    }

    d->menulevel = 0;

    d->callback = NULL;

    ReloadExitKey();

    if (d->foundtheme = d->parseSettings(dir, "theme.xml"))
        d->foundtheme = d->parseMenu(menufile);
}

ThemedMenu::~ThemedMenu(void)
{
    if (d)
        delete d;
}

bool ThemedMenu::foundTheme(void)
{
    return d->foundtheme;
}

void ThemedMenu::setCallback(void (*lcallback)(void *, QString &), void *data)
{
    d->callback = lcallback;
    d->callbackdata = data;
}

void ThemedMenu::setKillable(void)
{
    d->killable = true;
}

QString ThemedMenu::getSelection(void)
{
    return d->selection;
}

void ThemedMenu::ReloadExitKey(void)
{
    int allowsd = gContext->GetNumSetting("AllowQuitShutdown");
    d->killable = (allowsd == 4);

    if (allowsd == 1)
        d->exitModifier = Qt::ControlButton;
    else if (allowsd == 2)
        d->exitModifier = Qt::MetaButton;
    else if (allowsd == 3)
        d->exitModifier = Qt::AltButton;
    else
        d->exitModifier = -1;
}

void ThemedMenu::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(d->watermarkRect))
        d->paintWatermark(&p);

    for (unsigned int i = 0; i < d->buttonList.size(); i++)
    {
        if (r.intersects(d->buttonList[i].paintRect))
            d->paintButton(i, &p, e->erased());
    }

    d->updateLCD();
}

void ThemedMenu::ReloadTheme(void)
{
    if (!d->ReloadTheme())
        d->foundtheme = false;
}

void ThemedMenu::keyPressEvent(QKeyEvent *e)
{
    if (d->ignorekeys)
        return;

    d->ignorekeys = true;

    if (!d->keyPressHandler(e))
        MythDialog::keyPressEvent(e);

    d->ignorekeys = false;
}
