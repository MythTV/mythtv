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

ThemedMenu::ThemedMenu(MythContext *context, const char *cdir, 
                       const char *menufile, 
                       QWidget *parent, const char *name)
          : QDialog(parent, name)
{
    m_context = context;
  
    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedWidth(screenwidth);
    setFixedHeight(screenheight);

    setPalette(QPalette(QColor(250, 250, 250)));
    setCursor(QCursor(Qt::BlankCursor));

    QString dir = QString(cdir) + "/";

    QString filename = dir + "theme.xml";

    foundtheme = true;
    QFile filetest(filename);
    if (!filetest.exists())
    {
        foundtheme = false;
        return;
    }

    prefix = context->GetInstallPrefix();
    menulevel = 0;

    callback = NULL;
    int allowsd = m_context->GetNumSetting("AllowQuitShutdown");
    killable = (allowsd == 4);

    if (allowsd == 1)
        exitModifier = Qt::ControlButton;
    else if (allowsd == 2)
        exitModifier = Qt::MetaButton;
    else if (allowsd == 3)
        exitModifier = Qt::AltButton;
    else
        exitModifier = -1;
    
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

    QMap<QString, ButtonIcon>::Iterator it;
    for (it = allButtonIcons.begin(); it != allButtonIcons.end(); ++it)
    {
        if (it.data().icon)
            delete it.data().icon;
        if (it.data().activeicon)
            delete it.data().activeicon;
    }
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
                bground = m_context->LoadScalePixmap(path);
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
                buttonnormal = m_context->LoadScalePixmap(setting);
                hasnormal = true;
            }
            else if (info.tagName() == "active")
            {
                setting = dir + getFirstText(info);
                buttonactive = m_context->LoadScalePixmap(setting);
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
}

void ThemedMenu::parseLogo(QString dir, QDomElement &element)
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
                QString logopath = dir + getFirstText(info);
                logo = m_context->LoadScalePixmap(logopath); 
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

void ThemedMenu::parseButton(QString dir, QDomElement &element)
{
    bool hasname = false;
    bool hasoffset = false;
    bool hasicon = false;

    QString name = "";
    QPixmap *image = NULL;
    QPixmap *activeimage = NULL;
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
                image = m_context->LoadScalePixmap(imagepath);
                hasicon = true;
            }
            else if (info.tagName() == "activeimage")
            {
                QString imagepath = dir + getFirstText(info);
                activeimage = m_context->LoadScalePixmap(imagepath);
            }
            else if (info.tagName() == "offset")
            {
                offset = parsePoint(getFirstText(info));
                offset.setX((int)(offset.x() * wmult));
                offset.setY((int)(offset.y() * hmult));
                hasoffset = true;
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
}

void ThemedMenu::parseSettings(QString dir, QString menuname)
{
    QString filename = dir + menuname;

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
        return;
    if (!doc.setContent(&f))
    {
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
                text = getFirstText(info);
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
        addButton(type, text, action);
}

void ThemedMenu::parseMenu(QString menuname)
{
    QString filename = findMenuFile(menuname);

    QDomDocument doc;
    QFile f(filename);

    if (!f.open(IO_ReadOnly))
    {
        cerr << "Couldn't read menu file " << menuname << endl;
        exit(0);
    }
    if (!doc.setContent(&f))
    {
        f.close();
        return;
    }

    f.close();

    buttonList.clear();
    buttonRows.clear();

    QDomElement docElem = doc.documentElement();
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

    WFlags flags = getWFlags();
    setWFlags(flags | Qt::WRepaintNoErase);

    menulevel++;
    menufiles.push_back(menuname);

    selection = "";
    update(menuRect());
}

void ThemedMenu::Show()
{
    showFullScreen();
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

void ThemedMenu::addButton(QString &type, QString &text, QString &action)
{
    ThemedButton newbutton;

    newbutton.buttonicon = NULL;
    if (allButtonIcons.find(type) != allButtonIcons.end())
        newbutton.buttonicon = &(allButtonIcons[type]);

    newbutton.text = text;
    newbutton.action = action;
    newbutton.status = -1;

    buttonList.push_back(newbutton);
}

void ThemedMenu::layoutButtons(void)
{
    int numbuttons = buttonList.size();
  
    if (numbuttons > 6)
    {
        cerr << "Only displaying 6 buttons\n";
        numbuttons = 6;
    }

    int columns = buttonArea.width() / buttonnormal->width();
    int rows = buttonArea.height() / buttonnormal->height();

    if (rows < 2)
    {
        cerr << "Must have room for at least 2 rows of buttons\n";
        exit(0);
    }
    
    if (columns < 2)
    {
        cerr << "Must have room for at least 2 columns of buttons\n";
        exit(0);
    }

    if (rows * columns < numbuttons)
    {
        cerr << "Not enough room to display " << numbuttons << " buttons\n";
        numbuttons = rows * columns;
    }

    // keep the rows balanced
    if (numbuttons <= 4)
    {
        if (columns > 2)
            columns = 2;
    }
    else if (numbuttons <= 6)
    {
        if (columns > 3)
            columns = 3;
    }    

    vector<ThemedButton>::iterator iter = buttonList.begin();

    rows = numbuttons / columns;
    rows++;

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

        if (newrow.numitems > 0)
            buttonRows.push_back(newrow);
    }            

    rows = buttonRows.size();

    int yspacing = (buttonArea.height() - buttonnormal->height() * rows) /
                   (rows + 1);
    int row = 1;

    activebutton = &(*(buttonList.begin()));

    vector<MenuRow>::iterator menuiter = buttonRows.begin();
    for (; menuiter != buttonRows.end(); menuiter++)
    {
        int ypos = yspacing * row + (buttonnormal->height() * (row - 1));
        ypos += buttonArea.y();

        int xspacing = (buttonArea.width() - buttonnormal->width() *
                       (*menuiter).numitems) / ((*menuiter).numitems + 1);
        int col = 1;
        vector<ThemedButton *>::iterator biter = (*menuiter).buttons.begin();
        for (; biter != (*menuiter).buttons.end(); biter++)
        {
            int xpos = xspacing * col + (buttonnormal->width() * (col - 1));
            xpos += buttonArea.x();

            ThemedButton *tbutton = (*biter);
            if (activebutton == tbutton)
            {
                currentrow = row - 1;
                currentcolumn = col - 1;
            }

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

    activebutton = &(*(buttonList.begin()));
}

QRect ThemedMenu::menuRect() const
{
    QRect r(0, 0, screenwidth, screenheight);
    return r;
}

void ThemedMenu::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(logoRect))
    {
        paintLogo(&p);
    }

    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        if (r.intersects(buttonList[i].posRect))
            paintButton(i, &p, e->erased());
    }
}

void ThemedMenu::paintLogo(QPainter *p)
{
    if (logo)
    {
        QPixmap pix(logoRect.size());
        pix.fill(this, logoRect.topLeft());

        QPainter tmp;
        tmp.begin(&pix, this);

        tmp.drawPixmap(0, 0, *logo);

        tmp.end();

        p->drawPixmap(logoRect.topLeft(), pix);
    }
}

void ThemedMenu::paintButton(unsigned int button, QPainter *p, bool erased)
{
    QRect cr;
    if (buttonList[button].buttonicon)
        cr = buttonList[button].posRect.unite(buttonList[button].iconRect);
    else
        cr = buttonList[button].posRect;

    if (!erased)
    {
        if (buttonList[button].status == 1 && 
            activebutton == &(buttonList[button]))
            return;
        if (buttonList[button].status == 0 && 
            activebutton != &(buttonList[button]))
            return;
    }

    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp;
    tmp.begin(&pix, this);

    QRect newRect(0, 0, buttonList[button].posRect.width(),
                  buttonList[button].posRect.height());
    newRect.moveBy(buttonList[button].posRect.x() - cr.x(), 
                   buttonList[button].posRect.y() - cr.y());

    if (&(buttonList[button]) == activebutton)
    {
        tmp.drawPixmap(newRect.topLeft(), *buttonactive);
        buttonList[button].status = 1;
    }
    else
    {
        tmp.drawPixmap(newRect.topLeft(), *buttonnormal);
        buttonList[button].status = 0;
    }

    QRect buttonTextRect = textRect;
    buttonTextRect.moveBy(newRect.x(), newRect.y());

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
        texttmp.drawText(myrect, textflags, buttonList[button].text);
        texttmp.end();

        texttmp.begin(textpix.mask());
        texttmp.setPen(QPen(Qt::color1, 1));
        texttmp.setFont(font);
        texttmp.drawText(myrect, textflags, buttonList[button].text);
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
    drawText(&tmp, buttonTextRect, textflags, buttonList[button].text);

    if (buttonList[button].buttonicon)
    {
        newRect = QRect(buttonList[button].iconRect.x() - cr.x(), 
                        buttonList[button].iconRect.y() - cr.y(), 
                        buttonList[button].iconRect.width(),
                        buttonList[button].iconRect.height());

        if ((&(buttonList[button]) == activebutton) && 
            (buttonList[button].buttonicon->activeicon))
            tmp.drawPixmap(newRect.topLeft(),
                           *(buttonList[button].buttonicon->activeicon));
        else
            tmp.drawPixmap(newRect.topLeft(), 
                           *(buttonList[button].buttonicon->icon));
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
  
    QMap<QString, ButtonIcon>::Iterator it;
    for (it = allButtonIcons.begin(); it != allButtonIcons.end(); ++it)
    {
        if (it.data().icon)
            delete it.data().icon;
        if (it.data().activeicon)
            delete it.data().activeicon;
    }
    allButtonIcons.clear();

    if (logo)
        delete logo;
    logo = NULL;

    if (buttonnormal)
        delete buttonnormal;
    buttonnormal = NULL;

    if (buttonactive)
        delete buttonactive;
    buttonactive = NULL;

    QString theme = m_context->GetSetting("Theme");
    QString themedir = m_context->FindThemeDir(theme);
 
    erase(menuRect());

    parseSettings(themedir + "/", "theme.xml");

    QString file = menufiles.back();
    menufiles.pop_back();
    menulevel--;

    parseMenu(file);
}

void ThemedMenu::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    ThemedButton *lastbutton = activebutton;

    switch (e->key())
    {
        case Key_Up:
        {
            currentrow--;
            if (currentrow < 0)
            {
                currentrow = buttonRows.size() - 1;
                currentcolumn--;
                if (currentcolumn < 0)
                    currentcolumn = buttonRows[currentrow].numitems - 1;
            }

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;

            handled = true;
            break;
        }
        case Key_Left:
        {
            currentcolumn--;
            if (currentcolumn < 0)
            {
                currentrow--;
                if (currentrow < 0)
                    currentrow = buttonRows.size() - 1;
                currentcolumn = buttonRows[currentrow].numitems - 1;
            }

            handled = true;
            break;
        }
        case Key_Down:
        {
            currentrow++;
            if (currentrow >= (int)buttonRows.size())
            {
                currentrow = 0;
                currentcolumn++;
                if (currentcolumn >= buttonRows[currentrow].numitems)
                    currentcolumn = 0;
            }

            if (currentcolumn >= buttonRows[currentrow].numitems)
                currentcolumn = buttonRows[currentrow].numitems - 1;

            handled = true;
            break;
        }
        case Key_Right:
        {
            currentcolumn++;
            if (currentcolumn >= buttonRows[currentrow].numitems)
            {
                currentrow++;
                if (currentrow >= (int)buttonRows.size())
                    currentrow = 0;
                currentcolumn = 0;
            }
            handled = true;
            break;
        }
        case Key_Enter:
        case Key_Return:
        case Key_Space:
        {
            handleAction(activebutton->action);
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
        }
        default: break;
    }

    if (handled)
    {
        activebutton = buttonRows[currentrow].buttons[currentcolumn];

        update(lastbutton->posRect);
        update(activebutton->posRect);
    }
    else
        QDialog::keyPressEvent(e);
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
 
        erase(menuRect());
        parseMenu(rest);
    }
    else if (action.left(6) == "UPMENU")
    {
        menufiles.pop_back();
        QString file = menufiles.back();
        menufiles.pop_back();

        menulevel -= 2;
 
        erase(menuRect());
        parseMenu(file);
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
    if (filename == "")
        return false;
    return true;
}
