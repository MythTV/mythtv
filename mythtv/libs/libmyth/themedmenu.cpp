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
#include <math.h>
#include <qdom.h>

#include <iostream>
using namespace std;

#include "themedmenu.h"
#include "mythcontext.h"
#include "util.h"
#include "mythplugin.h"

ThemedMenu::ThemedMenu(const char *cdir, const char *menufile, 
                       MythMainWindow *parent, const char *name)
          : MythDialog(parent, name)
{
    ignorekeys = false;

    QString dir = QString(cdir) + "/";
    QString filename = dir + "theme.xml";

    foundtheme = true;
    QFile filetest(filename);
    if (!filetest.exists())
    {
        foundtheme = false;
        return;
    }

    prefix = gContext->GetInstallPrefix();
    menulevel = 0;

    callback = NULL;

    ReloadExitKey();
    
    parseSettings(dir, "theme.xml");

    parseMenu(menufile);
}

ThemedMenu::~ThemedMenu(void)
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

void ThemedMenu::ReloadExitKey(void)
{
    int allowsd = gContext->GetNumSetting("AllowQuitShutdown");
    killable = (allowsd == 4);

    if (allowsd == 1)
        exitModifier = Qt::ControlButton;
    else if (allowsd == 2)
        exitModifier = Qt::MetaButton;
    else if (allowsd == 3)
        exitModifier = Qt::AltButton;
    else
        exitModifier = -1;
}

QString ThemedMenu::getFirstText(QDomElement &element)
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
	
void ThemedMenu::parseBackground(QString dir, QDomElement &element)
{
    bool tiledbackground = false;
    QPixmap *bground = NULL;    
    QString path;

    bool hasarea = false;

    spreadbuttons = true;

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
                buttonArea = parseRect(getFirstText(info));
                buttonArea.moveTopLeft(QPoint((int)(buttonArea.x() * wmult),
                                              (int)(buttonArea.y() * hmult)));
                buttonArea.setWidth((int)(buttonArea.width() * wmult));
                buttonArea.setHeight((int)(buttonArea.height() * hmult));
                hasarea = true;
            }
            else if (info.tagName() == "buttonspread")
            {
                QString val = getFirstText(info);
                if (val == "no")
                    spreadbuttons = false;
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in background\n";
                exit(0);
            }
        }
    }

    if (!hasarea)
    {
        cerr << "Missing buttonaread in background\n";
        exit(0);
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

        setPaletteBackgroundPixmap(background);
        erase(menuRect());

        backgroundPixmap = background;

        delete bground;
    }
}

void ThemedMenu::parseShadow(QDomElement &element)
{
    hasshadow = true;

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
                shadowColor = QColor(getFirstText(info));
                hascolor = true;
            }
            else if (info.tagName() == "offset")
            {
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
                hasoffset = true;
            }
            else if (info.tagName() == "alpha")
            {
                shadowalpha = atoi(getFirstText(info).ascii());
                hasalpha = true;
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in text/shadow\n";
                exit(0);
            }
        }
    }

    if (!hascolor)
    {
        cerr << "Missing color tag in shadow\n";
        exit(0);
    }

    if (!hasalpha)
    {
        cerr << "Missing alpha tag in shadow\n";
        exit(0);
    }

    if (!hasoffset)
    {
        cerr << "Missing offset tag in shadow\n";
        exit(0);
    }
}

void ThemedMenu::parseOutline(QDomElement &element)
{
    hasoutline = true;

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
                outlineColor = QColor(getFirstText(info));
                hascolor = true;
            }
            else if (info.tagName() == "size")
            {
                outlinesize = atoi(getFirstText(info).ascii());
                outlinesize = (int)(outlinesize * hmult);
                hassize = true;
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in text/shadow\n";
                exit(0);
            }
        }
    }

    if (!hassize)
    {
        cerr << "Missing size in outline\n";
        exit(0);
    }

    if (!hascolor)
    {
        cerr << "Missing color in outline\n";
        exit(0);
    }
}

void ThemedMenu::parseText(QDomElement &element)
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
                textRect = parseRect(getFirstText(info));
                textRect.moveTopLeft(QPoint((int)(textRect.x() * wmult),
                                            (int)(textRect.y() * hmult)));
                textRect.setWidth((int)(textRect.width() * wmult));
                textRect.setHeight((int)(textRect.height() * hmult));
                textRect = QRect(textRect.x(), textRect.y(),
                                 buttonnormal->width() - textRect.width() - 
                                 textRect.x(), buttonnormal->height() - 
                                 textRect.height() - textRect.y());
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
                textColor = QColor(getFirstText(info));
            }
            else if (info.tagName() == "centered")
            {
                if (getFirstText(info) == "yes")
                    textflags = Qt::AlignTop | Qt::AlignHCenter | WordBreak;
            } 
            else if (info.tagName() == "outline")
            {
                parseOutline(info);
            }
            else if (info.tagName() == "shadow")
            {
                parseShadow(info);
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in text\n";
                exit(0);
            }
        }
    }

    font = QFont(fontname, (int)(fontsize * hmult), weight, italic);
    setFont(font);

    if (!hasarea)
    {
        cerr << "Missing 'area' tag in 'text' element of 'genericbutton'\n";
        exit(0);
    }
}

void ThemedMenu::parseButtonDefinition(QString dir, QDomElement &element)
{
    bool hasnormal = false;
    bool hasactive = false;

    QString setting;

    textflags = Qt::AlignTop | Qt::AlignLeft | WordBreak;

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
                    cerr << "The 'normal' tag needs to come before the 'text' "
                         << "tag\n";
                    exit(0);
                }
                parseText(info);
            }
            else if (info.tagName() == "watermarkposition")
            {
                watermarkPos = parsePoint(getFirstText(info));
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() 
                     << " in genericbutton\n";
                exit(0);
            }
        }
    }

    if (!hasnormal)
    {
        cerr << "No normal button image defined\n";
        exit(0);
    }
    if (!hasactive)
    {
        cerr << "No active button image defined\n";
        exit(0);
    }

    watermarkPos.setX((int)(watermarkPos.x() * wmult));
    watermarkPos.setY((int)(watermarkPos.y() * hmult));

    watermarkRect = QRect(watermarkPos, QSize(0, 0));
}

void ThemedMenu::parseLogo(QString dir, QDomElement &element)
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
                exit(0);
            }
        }
    }

    if (!hasimage)
    {
        cerr << "Missing image tag in logo\n";
        exit(0);
    }

    if (!hasposition)
    {
        cerr << "Missing position tag in logo\n";
        exit(0);
    }

    logopos.setX((int)(logopos.x() * wmult));
    logopos.setY((int)(logopos.y() * hmult));
    logoRect = QRect(logopos.x(), logopos.y(), logo->width(),
                     logo->height());
}

void ThemedMenu::parseTitle(QString dir, QDomElement &element)
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
                    exit(0);
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
                exit(0);
            }
        }
    }

    if (!hasimage)
    {
        cerr << "Missing image tag in titles\n";
        exit(0);
    }

    if (!hasposition)
    {
        cerr << "Missing position tag in titles\n";
        exit(0);
    }

    titlePos.setX((int)(titlePos.x() * wmult));
    titlePos.setY((int)(titlePos.y() * hmult));
}

void ThemedMenu::parseArrow(QString dir, QDomElement &element, bool up)
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
                exit(0);
            }
        }
    }

    if (!hasimage)
    {
        cerr << "Missing image tag in arrow\n";
        exit(0);
    }

    if (!hasposition)
    {
        cerr << "Missing position tag in arrow\n";
        exit(0);
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

void ThemedMenu::parseButton(QString dir, QDomElement &element)
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
                exit(0);
            }
        }
    }

    if (!hasname)
    {
        cerr << "Missing name in button\n";
        exit(0);
    }

    if (!hasoffset)
    {
        cerr << "Missing offset in buttondef " << name << endl;
        exit(0);
    }

    if (!hasicon) 
    {
        cerr << "Missing image in buttondef " << name << endl;
        exit(0);
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

void ThemedMenu::setDefaults(void)
{
    logo = NULL;
    buttonnormal = buttonactive = NULL;
    textflags = Qt::AlignTop | Qt::AlignLeft | WordBreak;
    textColor = QColor(white);
    hasoutline = false;
    hasshadow = false;
    titleIcons.clear();
    titleText = "";
    curTitle = NULL;
    drawTitle = false;
    uparrow = NULL;
    downarrow = NULL;
    watermarkPos = QPoint(0, 0);
    watermarkRect = QRect(0, 0, 0, 0);
}

void ThemedMenu::parseSettings(QString dir, QString menuname)
{
    QString filename = dir + menuname;

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        cerr << "ThemedMenu::parseSettings(): Can't open: " << filename << endl;
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
                exit(0);
            }
        }
        n = n.nextSibling();
    }

    if (!setbackground)
    {
        cerr << "Missing background element\n";
        exit(0);
    }

    if (!setbuttondef)
    {
        cerr << "Missing genericbutton definition\n";
        exit(0);
    }

    return;
}

void ThemedMenu::parseThemeButton(QDomElement &element)
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
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in button\n";
                exit(0);
            }
        }
    }

    if (text == "")
    {
        cerr << "Missing 'text' in button\n";
        exit(0);
    }
   
    if (action == "")
    {
        cerr << "Missing 'action' in button\n";
        exit(0);
    }

    if (addit)
        addButton(type, text, alttext, action);
}

void ThemedMenu::parseMenu(QString menuname, int row, int col)
{
    QString filename = findMenuFile(menuname);

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        cerr << "Couldn't read menu file " << menuname << endl;
        exit(0);
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
                exit(0);
            }
        }
        n = n.nextSibling();
    }

    if (buttonList.size() == 0)
    {
        cerr << "No buttons for menu " << menuname << endl;
        exit(0);
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
            makeRowVisible(oldrow + 1, oldrow);
            oldrow = oldrow + 1;
        }

        activebutton = buttonRows[currentrow].buttons[currentcolumn];
    }

    setNoErase();

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

    titleText = "MYTH-";
    titleText +=  menumode;
    gContext->LCDpopMenu(activebutton->text, titleText);

    selection = "";
    update(menuRect());
}

QPoint ThemedMenu::parsePoint(QString text)
{
    int x, y;
    QPoint retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QRect ThemedMenu::parseRect(QString text)
{
    int x, y, w, h;
    QRect retval;
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    return retval;
}

void ThemedMenu::addButton(QString &type, QString &text, QString &alttext,
                           QString &action)
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

void ThemedMenu::layoutButtons(void)
{
    int numbuttons = buttonList.size();
  
    int columns = buttonArea.width() / buttonnormal->width();
    maxrows = buttonArea.height() / buttonnormal->height();

    if (maxrows < 2)
    {
        cerr << "Must have room for at least 2 rows of buttons\n";
        exit(0);
    }
    
    if (columns < 1)
    {
        cerr << "Must have room for at least 1 column of buttons\n";
        exit(0);
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
    if (columns * maxrows > 6)
    {
        maxrows = 6 / columns;
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

void ThemedMenu::positionButtons(bool resetpos)
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

bool ThemedMenu::makeRowVisible(int newrow, int oldrow)
{
    if (buttonRows[newrow].visible)
        return true;

    int need = abs(newrow - oldrow);
    if (need != 1)
    {
        cerr << "moving: " << need << " rows, bad.\n";
        return false;
    }

    if (newrow > oldrow)
    {
        vector<MenuRow>::iterator menuiter = buttonRows.begin();
        for (; menuiter != buttonRows.end(); menuiter++)
        {
            if ((*menuiter).visible)
            {
                (*menuiter).visible = false;
                break;
            }
        }
    }
    else
    {
        vector<MenuRow>::reverse_iterator menuiter = buttonRows.rbegin();
        for (; menuiter != buttonRows.rend(); menuiter++)
        {
            if ((*menuiter).visible)
            {
                (*menuiter).visible = false;
                break;
            }
        }
    }

    buttonRows[newrow].visible = true;

    positionButtons(false);
    clearToBackground();

    return true;
}

QRect ThemedMenu::menuRect() const
{
    QRect r(0, 0, screenwidth, screenheight);
    return r;
}

void ThemedMenu::clearToBackground(void)
{
    drawInactiveButtons();
}

void ThemedMenu::drawInactiveButtons(void)
{
    QPixmap bground = backgroundPixmap;

    QPainter tmp(&bground);

    ThemedButton *store = activebutton;
    activebutton = NULL;
    
    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        paintButton(i, &tmp, true, true);
    }

    drawScrollArrows(&tmp);

    activebutton = store;

    tmp.end();

    setPaletteBackgroundPixmap(bground);

    erase(buttonArea);
    erase(uparrowRect);
    erase(downarrowRect);
}

void ThemedMenu::drawScrollArrows(QPainter *p)
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

void ThemedMenu::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(logoRect))
        paintLogo(&p);

    if (drawTitle && r.intersects(titleRect))
        paintTitle(&p);

    if (r.intersects(watermarkRect))
        paintWatermark(&p);

    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        if (r.intersects(buttonList[i].posRect))
            paintButton(i, &p, e->erased());
    }
}

void ThemedMenu::paintLogo(QPainter *p)
{
    QPixmap pix(logoRect.size());
   
    QPainter tmp(&pix, this);
    tmp.drawPixmap(QPoint(0, 0), backgroundPixmap, logoRect);

    if (logo)
        tmp.drawPixmap(QPoint(0, 0), *logo);

    tmp.end();
    p->drawPixmap(logoRect.topLeft(), pix);
}

void ThemedMenu::paintTitle(QPainter *p)
{
    if (curTitle)
        p->drawPixmap(titleRect.topLeft(), *curTitle);
}

void ThemedMenu::paintWatermark(QPainter *p)
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

void ThemedMenu::paintButton(unsigned int button, QPainter *p, bool erased,
                             bool drawinactive)
{
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
    }
    else
    {
        tbutton->status = 0;

        if (!drawinactive)
        {
            erase(cr);
            return;
        }
        buttonback = buttonnormal;
    }

    QPixmap pix(cr.size());

    QPainter tmp;
    tmp.begin(&pix, this);

    tmp.drawPixmap(QPoint(0, 0), backgroundPixmap, cr);

    tmp.drawImage(newRect.topLeft(), *buttonback);

    QRect buttonTextRect = textRect;
    buttonTextRect.moveBy(newRect.x(), newRect.y());

    QString message = tbutton->text;
    QRect testBound = tmp.boundingRect(buttonTextRect, textflags, message);

    if (testBound.height() > buttonTextRect.height() && tbutton->altText != "")
        message = buttonList[button].altText;

    if (hasshadow && shadowalpha > 0)
    {
        QPixmap textpix(buttonTextRect.size());
        textpix.fill(QColor(color0));
        textpix.setMask(textpix.createHeuristicMask());

        QRect myrect = buttonTextRect;
        myrect.moveTopLeft(shadowOffset);

        QPainter texttmp;
        texttmp.begin(&textpix, this);
        texttmp.setPen(QPen(shadowColor, 1));
        texttmp.setFont(font);
        texttmp.drawText(myrect, textflags, message);
        texttmp.end();

        texttmp.begin(textpix.mask());
        texttmp.setPen(QPen(Qt::color1, 1));
        texttmp.setFont(font);
        texttmp.drawText(myrect, textflags, message);
        texttmp.end();

        QImage im = textpix.convertToImage();

        for (int x = 0; x < im.width(); x++)
        {
            for (int y = 0; y < im.height(); y++)
            {
               uint *p = (uint *)im.scanLine(y) + x;
               QRgb col = *p;
               if (qAlpha(col) > 128)
                   *p = qRgba(qRed(col), qGreen(col), qBlue(col), shadowalpha);
            }
        } 

        textpix.convertFromImage(im);

        myrect.moveTopLeft(QPoint(0, 0));
        bitBlt(&pix, buttonTextRect.topLeft(), &textpix, myrect, Qt::CopyROP);
    }

    tmp.setFont(font);
    drawText(&tmp, buttonTextRect, textflags, message);

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

void ThemedMenu::drawText(QPainter *p, QRect &rect, int textflags, QString text)
{
    if (hasoutline)
    {
        QRect outlinerect = rect;

        p->setPen(QPen(outlineColor, 1));
        outlinerect.moveBy(0 - outlinesize, 0 - outlinesize);
        p->drawText(outlinerect, textflags, text);

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(1, 0);
            p->drawText(outlinerect, textflags, text);
        }

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(0, 1);
            p->drawText(outlinerect, textflags, text);
        }
  
        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(-1, 0);
            p->drawText(outlinerect, textflags, text);
        }

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(0, -1);
            p->drawText(outlinerect, textflags, text);
        }

        p->setPen(QPen(textColor, 1));
        p->drawText(rect, textflags, text);
    }
    else
    {
        p->setPen(QPen(textColor, 1));
        p->drawText(rect, textflags, text);
    }
}

void ThemedMenu::ReloadTheme(void)
{
    buttonList.clear();
    buttonRows.clear();

    ReloadExitKey();
  
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

    int x = 0, y = 0, w, h;
#ifndef QWS
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);
#endif

    setGeometry(x, y, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(gContext->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    gContext->ThemeWidget(this);

    QString themedir = gContext->GetThemeDir();
    parseSettings(themedir, "theme.xml");

    QString file = menufiles.back().name;
    int row = menufiles.back().row;
    int col = menufiles.back().col;
    menufiles.pop_back();
    menulevel--;

    parseMenu(file, row, col);
}

void ThemedMenu::keyPressEvent(QKeyEvent *e)
{
    if (ignorekeys)
        return;

    bool handled = false;
    ThemedButton *lastbutton = activebutton;

    ignorekeys = true;

    int oldrow = currentrow;

    switch (e->key())
    {
        case Key_Up:
        { 
            if (currentrow > 0)
                currentrow--;

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;

            handled = true;
            break;
        }
        case Key_Left:
        {
            if (currentcolumn > 0)
                currentcolumn--;

            handled = true;
            break;
        }
        case Key_Down:
        {
            if (currentrow < (int)buttonRows.size() - 1)
                currentrow++;

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;

            handled = true;
            break;
        }
        case Key_Right:
        {
            if (currentcolumn < buttonRows[currentrow].numitems - 1)
                currentcolumn++;

            handled = true;
            break;
        }
        case Key_Enter:
        case Key_Return:
        case Key_Space:
        {
            handleAction(activebutton->action);
            handled = true;
            break;
        }
        case Key_Escape:
        {
            QString action = "UPMENU";
            if (menulevel > 1)
                handleAction(action);
            else if (killable || e->state() == exitModifier)
                done(0);
            handled = true;
            break;
        }
        default: break;
    }

    if (handled)
    {
        if (!buttonRows[currentrow].visible)
            makeRowVisible(currentrow, oldrow);

        activebutton = buttonRows[currentrow].buttons[currentcolumn];
        gContext->LCDpopMenu(activebutton->text, titleText);
        update(watermarkRect);
        update(lastbutton->posRect);
        update(activebutton->posRect);
    }
    else
        MythDialog::keyPressEvent(e);

    ignorekeys = false;
} 

QString ThemedMenu::findMenuFile(QString menuname)
{
    char *home = getenv("HOME");
    QString testdir = QString(home) + "/.mythtv/" + menuname;
   
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

void ThemedMenu::handleAction(QString &action)
{
    if (action.left(5) == "EXEC ")
    {
        QString rest = action.right(action.length() - 5);
        system(rest);
    }
    else if (action.left(5) == "MENU ")
    {
        QString rest = action.right(action.length() - 5);

        menufiles.back().row = currentrow;
        menufiles.back().col = currentcolumn;

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
        MythPluginManager::config_plugin(rest.stripWhiteSpace());
    }
    else if (action.left(6) == "PLUGIN")
    {
        QString rest = action.right(action.length() - 7);
        MythPluginManager::run_plugin(rest.stripWhiteSpace());
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

bool ThemedMenu::findDepends(QString file)
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
