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

#include "themedmenu.h"
#include "mythcontext.h"
#include "util.h"
#include "mythplugin.h"
#include "dialogbox.h"

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

    ButtonIcon *buttonicon;
    QPoint iconPos;
    QRect iconRect;

    QString text;
    QString altText;
    QString action;

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
    void ReloadTheme(void);

    void parseMenu(const QString &menuname, int row = -1, int col = -1);

    void parseSettings(const QString &dir, const QString &menuname);

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

    void setDefaults(void);

    void addButton(const QString &type, const QString &text,
                   const QString &alttext, const QString &action);
    void layoutButtons(void);
    void positionButtons(bool resetpos);
    bool makeRowVisible(int newrow, int oldrow, bool forcedraw = true);

    void handleAction(const QString &action);
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

    ThemedMenu *parent;

    float wmult;
    float hmult;

    int screenwidth;
    int screenheight;
    
    QString prefix;

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

    int menulevel;
    vector<MenuState> menufiles;

    void (*callback)(void *, QString &);
    void *callbackdata;

    bool killable;
    int exitModifier;

    bool spreadbuttons;

    QMap<QString, QPixmap> titleIcons;
    QPixmap *curTitle;
    QString titleText;
    QPoint titlePos;
    QRect titleRect;
    bool drawTitle;

    QPixmap backgroundPixmap;

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

    LCD *lcddev;
};

ThemedMenuPrivate::ThemedMenuPrivate(ThemedMenu *lparent, float lwmult, 
                                     float lhmult, int lscreenwidth, 
                                     int lscreenheight)
{
    parent = lparent;
    wmult = lwmult;
    hmult = lhmult;
    screenwidth = lscreenwidth;
    screenheight = lscreenheight;

    logo = NULL;
    buttonnormal = NULL;
    buttonactive = NULL;
    uparrow = NULL;
    downarrow = NULL;

    ignorekeys = false;

    lcddev = gContext->GetLCDDevice();
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

    spreadbuttons = true;
    visiblerowlimit = 6;	// the old default

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
            }
            else if (info.tagName() == "buttonspread")
            {
                QString val = getFirstText(info);
                if (val == "no")
                    spreadbuttons = false;
            }
            else if (info.tagName() == "visiblerowlimit")
            {
                visiblerowlimit = atoi(getFirstText(info).ascii());
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in background\n";
                return;
            }
        }
    }

    if (!hasarea)
    {
        cerr << "Missing buttonaread in background\n";
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
                cerr << "Unknown tag " << info.tagName() << " in text/shadow\n";
                return;
            }
        }
    }

    if (!hascolor)
    {
        cerr << "Missing color tag in shadow\n";
        return;
    }

    if (!hasalpha)
    {
        cerr << "Missing alpha tag in shadow\n";
        return;
    }

    if (!hasoffset)
    {
        cerr << "Missing offset tag in shadow\n";
        return;
    }
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
                cerr << "Unknown tag " << info.tagName() << " in text/shadow\n";
                return;
            }
        }
    }

    if (!hassize)
    {
        cerr << "Missing size in outline\n";
        return;
    }

    if (!hascolor)
    {
        cerr << "Missing color in outline\n";
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
                    attributes.textflags = Qt::AlignTop | Qt::AlignHCenter | 
                                           Qt::WordBreak;
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
                cerr << "Unknown tag " << info.tagName() << " in text\n";
                return;
            }
        }
    }

    attributes.font = QFont(fontname, (int)ceil(fontsize * hmult), weight, 
                            italic);

    if (!hasarea)
    {
        cerr << "Missing 'area' tag in 'text' element of 'genericbutton'\n";
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
                hasnormal = true;
            }
            else if (info.tagName() == "active")
            {
                setting = dir + getFirstText(info);
                buttonactive = gContext->LoadScaleImage(setting);
                hasactive = true;
            }
            else if (info.tagName() == "text")
            {
                if (!hasnormal)
                {
                    cerr << "The 'normal' tag needs to come before the "
                         << "'normaltext' tag\n";
                    return;
                }
                parseText(normalAttributes, info);
            }
            else if (info.tagName() == "activetext")
            {
                if (!hasactive)
                {
                    cerr << "The 'active' tag needs to come before the "
                         << "'activetext' tag\n";
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
                cerr << "Unknown tag " << info.tagName() 
                     << " in genericbutton\n";
                return;
            }
        }
    }

    if (!hasnormal)
    {
        cerr << "No normal button image defined\n";
        return;
    }

    if (!hasactive)
    {
        cerr << "No active button image defined\n";
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
                hasimage = true;
            }
            else if (info.tagName() == "position")
            {
                logopos = parsePoint(getFirstText(info));
		hasposition = true;
	    }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in logo\n";
                return;
            }
        }
    }

    if (!hasimage)
    {
        cerr << "Missing image tag in logo\n";
        return;
    }

    if (!hasposition)
    {
        cerr << "Missing position tag in logo\n";
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

                QString name = info.attribute("mode", "");
                if (name != "")
                {
                    titleIcons[name] = *tmppix;
                }
                else
                {
                    cerr << "Missing mode in titles/image\n";
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
                cerr << "Unknown tag " << info.tagName() << " in logo\n";
                return;
            }
        }
    }

    if (!hasimage)
    {
        cerr << "Missing image tag in titles\n";
        return;
    }

    if (!hasposition)
    {
        cerr << "Missing position tag in titles\n";
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
                hasimage = true;
            }
            else if (info.tagName() == "position")
            {
                arrowpos = parsePoint(getFirstText(info));
                hasposition = true;
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in arrow\n";
                return;
            }
        }
    }

    if (!hasimage)
    {
        cerr << "Missing image tag in arrow\n";
        return;
    }

    if (!hasposition)
    {
        cerr << "Missing position tag in arrow\n";
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
                cerr << "Unknown tag " << info.tagName() << " in buttondef\n";
                return;
            }
        }
    }

    if (!hasname)
    {
        cerr << "Missing name in button\n";
        return;
    }

    if (!hasoffset)
    {
        cerr << "Missing offset in buttondef " << name << endl;
        return;
    }

    if (!hasicon) 
    {
        cerr << "Missing image in buttondef " << name << endl;
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

void ThemedMenuPrivate::parseSettings(const QString &dir, 
                                      const QString &menuname)
{
    QString filename = dir + menuname;

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        cerr << "ThemedMenuPrivate::parseSettings(): Can't open: " << filename << endl;
        return;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "Error parsing: " << filename << endl;
        cerr << "at line: " << errorLine << "  column: " << errorColumn << endl;
        cerr << errorMsg << endl;
        f.close();
        return;
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
            else
            {
                cerr << "Unknown element " << e.tagName() << endl;
                return;
            }
        }
        n = n.nextSibling();
    }

    if (!setbackground)
    {
        cerr << "Missing background element\n";
        return;
    }

    if (!setbuttondef)
    {
        cerr << "Missing genericbutton definition\n";
        return;
    }

    return;
}

void ThemedMenuPrivate::parseThemeButton(QDomElement &element)
{
    QString type = "";
    QString text = "";
    QString action = "";
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
                action = getFirstText(info);
            }
            else if (info.tagName() == "depends")
            {
                addit = findDepends(getFirstText(info));
            }
            else if (info.tagName() == "dependssetting")
            {
                addit = gContext->GetNumSetting(getFirstText(info));
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in button\n";
                return;
            }
        }
    }

    if (text == "")
    {
        cerr << "Missing 'text' in button\n";
        return;
    }
   
    if (action == "")
    {
        cerr << "Missing 'action' in button\n";
        return;
    }

    if (addit)
        addButton(type, text, alttext, action);
}

void ThemedMenuPrivate::parseMenu(const QString &menuname, int row, int col)
{
    QString filename = findMenuFile(menuname);

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        cerr << "Couldn't read menu file " << menuname << endl;
        exit(1);
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "Error parsing: " << filename << endl;
        cerr << "at line: " << errorLine << "  column: " << errorColumn << endl;
        cerr << errorMsg << endl;
        f.close();
        exit(1);
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
                cerr << "Unknown element " << e.tagName() << endl;
                exit(1);
            }
        }
        n = n.nextSibling();
    }

    if (buttonList.size() == 0)
    {
        cerr << "No buttons for menu " << menuname << endl;
        exit(1);
    }

    layoutButtons();
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

    if (lcddev)
    {
        titleText = "MYTH-";
        titleText += menumode;
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

    selection = "";
    parent->update(menuRect());
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

    return retval;
}

void ThemedMenuPrivate::addButton(const QString &type, const QString &text, 
                                  const QString &alttext, const QString &action)
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

void ThemedMenuPrivate::layoutButtons(void)
{
    int numbuttons = buttonList.size();
  
    columns = buttonArea.width() / buttonnormal->width();
    maxrows = buttonArea.height() / buttonnormal->height();

    if (maxrows < 2)
    {
        cerr << "Must have room for at least 2 rows of buttons\n";
        exit(1);
    }
    
    if (columns < 1)
    {
        cerr << "Must have room for at least 1 column of buttons\n";
        exit(1);
    }

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
            if (columns == 3 && j == 1)
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
        ystart = (buttonArea.height() - buttonnormal->height() * rows) / 2;
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
    QRect testBound = tmp.boundingRect(buttonTextRect, attributes.textflags, 
                                       message);

    if (testBound.height() > buttonTextRect.height() && tbutton->altText != "")
        message = buttonList[button].altText;

#ifndef QWS
    if (attributes.hasshadow && attributes.shadowalpha > 0)
    {
        QPixmap textpix(buttonTextRect.size());
        textpix.fill(QColor(QWidget::color0));
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

void ThemedMenuPrivate::ReloadTheme(void)
{
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

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    parent->setFixedSize(QSize(screenwidth, screenheight));

    parent->setFont(gContext->GetMediumFont());
    parent->setCursor(QCursor(Qt::BlankCursor));

    gContext->ThemeWidget(parent);

    QString themedir = gContext->GetThemeDir();
    parseSettings(themedir, "theme.xml");

    QString file = menufiles.back().name;
    int row = menufiles.back().row;
    int col = menufiles.back().col;
    menufiles.pop_back();
    menulevel--;

    parseMenu(file, row, col);
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
            if (currentrow > 0)
                currentrow--;

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;
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
        }
        else if (action == "DOWN")
        {
            if (currentrow < (int)buttonRows.size() - 1)
                currentrow++;

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;
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
        }
        else if (action == "SELECT")
        {
            lastbutton = activebutton;
            activebutton = NULL;
            parent->repaint(lastbutton->posRect);
            handleAction(lastbutton->action);
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
    if (lcddev)
    {
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

    parent->update(watermarkRect);
    if (lastbutton)
        parent->update(lastbutton->posRect);
    parent->update(activebutton->posRect);

    return true;
} 

QString ThemedMenuPrivate::findMenuFile(const QString &menuname)
{
    QString testdir = QDir::homeDirPath() + "/.mythtv/" + menuname;
   
    QFile file(testdir);
    if (file.exists())
        return testdir;
        
    testdir = prefix + "/share/mythtv/" + menuname;
    file.setName(testdir);
    if (file.exists())
        return testdir;
        
    testdir = "../mythfrontend/" + menuname;
    file.setName(testdir);
    if (file.exists())
        return testdir;
        
    return "";
}

void ThemedMenuPrivate::handleAction(const QString &action)
{
    if (action.left(5) == "EXEC ")
    {
        QString rest = action.right(action.length() - 5);
        myth_system(rest);
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
                cerr << "themedmenu.o: Tuner already locked" << endl;
            
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
            return;
        }

        parseMenu(rest);
    }
    else if (action.left(6) == "UPMENU")
    {
        menufiles.pop_back();
        QString file = menufiles.back().name;
        int row = menufiles.back().row;
        int col = menufiles.back().col;
        menufiles.pop_back();

        menulevel -= 2;
 
        parseMenu(file, row, col);
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
    else
    {
        selection = action;
        if (callback != NULL)
        {
            callback(callbackdata, selection);
        }
    }   
}

bool ThemedMenuPrivate::findDepends(const QString &file)
{
    QString filename = findMenuFile(file);
    if (filename != "")
        return true;

    QString newname = QString(PREFIX) + "/lib/mythtv/plugins/lib" + file +
                      ".so";

    QFile checkFile(newname);
    if (checkFile.exists())
        return true;

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
        cerr << "themedmenu.o: Could not read password/pin time stamp. "
             << "This is only an issue if it happens repeatedly. " << endl;
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
                       MythMainWindow *parent, const char *name)
          : MythDialog(parent, name)
{
    d = new ThemedMenuPrivate(this, wmult, hmult, screenwidth, screenheight);

    QString dir = QString(cdir) + "/";
    QString filename = dir + "theme.xml";

    d->foundtheme = true;
    QFile filetest(filename);
    if (!filetest.exists())
    {
        d->foundtheme = false;
        return;
    }

    d->prefix = gContext->GetInstallPrefix();
    d->menulevel = 0;

    d->callback = NULL;

    ReloadExitKey();

    d->parseSettings(dir, "theme.xml");

    d->parseMenu(menufile);
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
        if (r.intersects(d->buttonList[i].posRect))
            d->paintButton(i, &p, e->erased());
    }
}

void ThemedMenu::ReloadTheme(void)
{
    d->ReloadTheme();
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

