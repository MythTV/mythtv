#include <qstring.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <qimage.h>
#include <qpixmap.h>
#include <qbitmap.h>
#include <qdir.h>
#include <qfile.h>
#include <qcolor.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include "ttfont.h"
#include "osd.h"
#include "osdtypes.h"
#include "libmyth/oldsettings.h"

OSD::OSD(int width, int height, int framerate, const QString &font, 
         const QString &ccfont, const QString &prefix, const QString &osdtheme,
         int dispx, int dispy, int dispw, int disph)
{
    pthread_mutex_init(&osdlock, NULL);

    vid_width = width;
    vid_height = height;
    fps = framerate;

    editarrowleft = editarrowright = NULL;

    wmult = dispw / 640.0;
    hmult = disph / 480.0;

    xoffset = dispx;
    yoffset = dispy;   
    displaywidth = dispw;
    displayheight = disph;

    timeType = 0; 
    totalfadeframes = 0;
    fontname = font;
    ccfontname = ccfont;
    fontprefix = prefix;

    m_setsvisible = false;
    setList = NULL;

    setList = new vector<OSDSet *>;

    themepath = FindTheme(osdtheme);

    if (themepath == "")
    {
        cerr << "Couldn't find osdtheme: " << osdtheme << endl;
    }
    else
    {
        themepath += "/";
        if (!LoadTheme())
        {
            cerr << "Couldn't load osdtheme: " << osdtheme << " at " 
                 << themepath << endl;
        }
    }

    SetDefaults();
}

OSD::~OSD(void)
{
    QMap<QString, TTFFont *>::iterator fonts = fontMap.begin();
    for (; fonts != fontMap.end(); ++fonts)
    {
        TTFFont *font = (*fonts);
        if (font)
            delete font;
    }

    QMap<QString, OSDSet *>::iterator sets = setMap.begin();
    for (; sets != setMap.end(); ++sets)
    { 
        OSDSet *set = (*sets);
        if (set)
            delete set;
    }

    if (editarrowleft)
        delete editarrowleft;
    if (editarrowright)
        delete editarrowright;

    delete setList;
}

void OSD::SetDefaults(void)
{
    TTFFont *ccfont = GetFont("cc_font");
    if (!ccfont)
    {
        QString name = "cc_font";
        int fontsize = 480 / 27;
        fontsize = (int)(fontsize * hmult);

        ccfont = LoadFont(ccfontname, fontsize);

        if (ccfont)
            fontMap[name] = ccfont;
    }

    if (!ccfont)
        return;

    OSDSet *container = GetSet("cc_page");
    if (!container)
    {
        QString name = "cc_page";
        container = new OSDSet(name, true, vid_width, vid_height, wmult, hmult);
        AddSet(container, name);

        OSDTypeCC *ccpage = new OSDTypeCC(name, ccfont, xoffset, yoffset,
                                          displaywidth, displayheight);
        container->AddType(ccpage);
    }
}

void OSD::Reinit(int width, int height, int fps, int dispx, int dispy, 
                 int dispw, int disph)
{
    vid_width = width;
    vid_height = height;
    if (fps != -1)
       this->fps = fps;

    wmult = dispw / 640.0;
    hmult = disph / 480.0;

    xoffset = dispx;
    yoffset = dispy;
    displaywidth = dispw;
    displayheight = disph;

    QMap<QString, TTFFont *>::iterator fonts = fontMap.begin();
    for (; fonts != fontMap.end(); ++fonts)
    {
        TTFFont *font = (*fonts);
        if (font)
            font->Reinit(vid_width, vid_height);
    }

    QMap<QString, OSDSet *>::iterator sets = setMap.begin();
    for (; sets != setMap.end(); ++sets)
    {
        OSDSet *set = (*sets);
        if (set)
            set->Reinit(vid_width, vid_height, dispx, dispy, dispw, disph, 
                        wmult, hmult);
    }
}

QString OSD::FindTheme(QString name)
{
    char *home = getenv("HOME");
    QString testdir = QString(home) + "/.mythtv/osd/" + name;
    
    QDir dir(testdir);
    if (dir.exists())
        return testdir;

    testdir = fontprefix + "/share/mythtv/themes/" + name;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = "../libNuppelVideo/" + name;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    return "";
}

TTFFont *OSD::LoadFont(QString name, int size)
{
    char *home = getenv("HOME");
    QString fullname = QString(home) + "/.mythtv/" + name;
    TTFFont *font = new TTFFont((char *)fullname.ascii(), size, vid_width,
                                vid_height);

    if (font->isValid())
        return font;

    delete font;
    fullname = fontprefix + "/share/mythtv/" + name;

    font = new TTFFont((char *)fullname.ascii(), size, vid_width,
                       vid_height);

    if (font->isValid())
        return font;

    delete font;
    if (themepath != "")
    {
        fullname = themepath + "/" + name;
        font = new TTFFont((char *)fullname.ascii(), size, vid_width,
                           vid_height);
        if (font->isValid())
            return font;
    }

    delete font;

    fullname = name;
    font = new TTFFont((char *)fullname.ascii(), size, vid_width, vid_height);

    if (font->isValid())
        return font;
   
    cerr << "Unable to find font: " << name << endl;
    cerr << "No OSD will be displayed.\n";

    delete font;
    return NULL;
}

QString OSD::getFirstText(QDomElement &element)
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

void OSD::parseFont(QDomElement &element)
{
    QString name;
    QString fontfile = fontname;
    int size = -1;
    bool outline = false;
    QPoint shadowOffset = QPoint(0, 0);
    int color = 255;

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Font needs a name\n";
        exit(0);
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
            else if (info.tagName() == "color")
            {
                color = getFirstText(info).toInt();
            }
            else if (info.tagName() == "outline")
            {
                if (getFirstText(info).lower() == "yes")
                    outline = true;
            }
            else if (info.tagName() == "shadow")
            {
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
            }
            else if (info.tagName() == "filename")
            {
                fontfile = getFirstText(info);
            }
            else
            {
                cerr << "Unknown tag " << info.tagName() << " in font\n";
                exit(0);
            }
        }
    }

    TTFFont *font = GetFont(name);
    if (font)
    {
        cerr << "Error: already have a font called: " << name << endl;
        exit(0);
    }

    if (size < 0) 
    {
        cerr << "Error: font size must be > 0\n";
        exit(0); 
    }

    size = (int)(size * hmult);

    font = LoadFont(fontfile, size);
    if (!font)
    {
        cerr << "Couldn't load font: " << fontfile << endl;
        exit(0);
    }

    font->setColor(color);
    font->setOutline(outline);
    font->setShadow(shadowOffset.x(), shadowOffset.y());

    fontMap[name] = font;
}

void OSD::parseBox(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Box needs a name\n";
        exit(0);
    }

    QRect area(0, 0, 0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(&area);
            }
            else
            {
                cerr << "Unknown tag in box: " << info.tagName() << endl;
                exit(0);
            }
        }
    }

    OSDTypeBox *box = new OSDTypeBox(name, area);
    container->AddType(box);
}

void OSD::parseImage(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Image needs a name\n";
        exit(0);
    }
    
    QString filename = "";
    QPoint pos = QPoint(0, 0);

    QPoint scale = QPoint(-1, -1);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult + xoffset));
                pos.setY((int)(pos.y() * hmult + yoffset));
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in image\n";
                exit(0);
            }
        }
    }

    if (filename != "")
        filename = themepath + filename;

    OSDTypeImage *image = new OSDTypeImage(name, filename, pos, wmult, hmult,
                                           scale.x(), scale.y());
    container->AddType(image);
}

void OSD::parseTextArea(OSDSet *container, QDomElement &element)
{
    QRect area = QRect(0, 0, 0, 0);
    QString font = "", altfont = "";
    QString statictext = "";
    QString defaulttext = "";
    bool multiline = false;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Text area needs a name\n";
        exit(0);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(&area);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "altfont")
            {
                altfont = getFirstText(info);
            }
            else if (info.tagName() == "multiline")
            {
                if (getFirstText(info).lower() == "yes")
                    multiline = true;
            }
            else if (info.tagName() == "statictext")
            {
                statictext = getFirstText(info);
            }
            else if (info.tagName() == "value")
            {
                defaulttext = getFirstText(info);
            }
            else
            {
                cerr << "Unknown tag in textarea: " << info.tagName() << endl;
                exit(0);
            }                   
        }
    }    

    TTFFont *ttffont = GetFont(font);
    if (!ttffont)
    {
        cerr << "Unknown font: " << font << " in textarea: " << name << endl;
        exit(0);
    }

    OSDTypeText *text = new OSDTypeText(name, ttffont, "", area);
    container->AddType(text);

    text->SetMultiLine(multiline);

    if (altfont != "")
    {
        ttffont = GetFont(altfont);
        if (!ttffont)
        {
            cerr << "Unknown altfont: " << altfont << " in textarea: "
                 << name << endl;
        }
        else
            text->SetAltFont(ttffont);
    }

    if (statictext != "")
        text->SetText(statictext);
    if (defaulttext != "")
        text->SetDefaultText(defaulttext);

    QString align = element.attribute("align", "");
    if (!align.isNull() && !align.isEmpty())
    {
        if (align.lower() == "center")
            text->SetCentered(true);
        else if (align.lower() == "right")
            text->SetRightJustified(true);
    }
}

void OSD::parseSlider(OSDSet *container, QDomElement &element)
{
    QRect area = QRect(0, 0, 0, 0);
    QString filename = "";
    QString altfilename = "";

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Slider needs a name\n";
        exit(0);
    }

    QString type = element.attribute("type", "");
    if (type.isNull() || type.isEmpty())
    {
        cerr << "Slider needs a type";
        exit(0);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(&area);            
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "altfilename")
            {
                altfilename = getFirstText(info);
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in image\n";
                exit(0);
            }
        }
    }

    if (filename == "")
    {
        cerr << "Slider needs a filename\n";
        exit(0);
    }

    filename = themepath + filename;

    if (type.lower() == "fill")
    {
        OSDTypeFillSlider *slider = new OSDTypeFillSlider(name, filename, area,
                                                          wmult, hmult);
        container->AddType(slider);
    }
    else if (type.lower() == "edit")
    {
        if (altfilename == "")
        {
            cerr << "Edit slider needs an altfilename\n";
            exit(0);
        }

        altfilename = themepath + altfilename;

        OSDTypeEditSlider *tes = new OSDTypeEditSlider(name, filename, 
                                                       altfilename, area,
                                                       wmult, hmult);
        container->AddType(tes);
    }
    else if (type.lower() == "position")
    {
        OSDTypePosSlider *pos = new OSDTypePosSlider(name, filename, area,
                                                     wmult, hmult);
        container->AddType(pos);
    }
}

void OSD::parseEditArrow(OSDSet *container, QDomElement &element)
{
    container = container;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "editarrow needs a name\n";
        exit(0);
    }

    if (name != "left" && name != "right")
    {
        cerr << "editarrow name needs to be either 'left' or 'right'\n";
        exit(0);
    }

    QRect area(0, 0, 0, 0);
    QString filename = "";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(&area);
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else
            {
                cerr << "Unknown tag in editarrow: " << info.tagName() << endl;
                exit(0);
            }
        }
    }

    if (filename == "")
    {
        cerr << "editarrow needs a filename\n";
        exit(0);
    }

    editarrowRect = area;

    QString setname = "arrowimage";

    filename = themepath + filename;

    OSDTypeImage *image = new OSDTypeImage(setname, filename, QPoint(0, 0),
                                           wmult, hmult);

    if (name == "left")
        editarrowleft = image;
    else
        editarrowright = image;
}

void OSD::parsePositionRects(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "positionsrects needs a name\n";
        exit(0);
    }

    OSDTypePositionRectangle *rects = new OSDTypePositionRectangle(name);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                QRect area = parseRect(getFirstText(info));
                normalizeRect(&area);

                rects->AddPosition(area);
            }
            else
            {
                cerr << "Unknown tag in editarrow: " << info.tagName() << endl;
                exit(0);
            }
        }
    }
   
    container->AddType(rects);
}

void OSD::parsePositionImage(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "positionimage needs a name\n";
        exit(0);
    }

    QString filename = "";
    QPoint scale = QPoint(-1, -1);

    OSDTypePositionImage *image = new OSDTypePositionImage(name);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                QPoint pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult + xoffset));
                pos.setY((int)(pos.y() * hmult + yoffset));

                image->AddPosition(pos);
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in positionimage\n";
                exit(0);
            }
        }
    }

    if (filename != "")
        filename = themepath + filename;

    image->SetStaticSize(scale.x(), scale.y());
    image->LoadImage(filename, wmult, hmult, scale.x(), scale.y());

    container->AddType(image);
}

void OSD::parseContainer(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Container needs a name\n";
        exit(0);
    }

    OSDSet *container = GetSet(name);
    if (container) 
    {
        cerr << "Container: " << name << " already exists\n";
        exit(0);
    }

    container = new OSDSet(name, true, vid_width, vid_height, wmult, hmult);

    QString prio = element.attribute("priority", "");
    if (!prio.isNull() && !prio.isEmpty())
    {
        container->SetPriority(prio.toInt());
    }

    AddSet(container, name);    

    QString movement = element.attribute("fademovement", "");
    if (!movement.isNull() && !movement.isEmpty())
    {
        QPoint movefade = parsePoint(movement);
        container->SetFadeMovement((int)(movefade.x() * wmult),
                                   (int)(movefade.y() * hmult));
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
                parseImage(container, info);
            }
            else if (info.tagName() == "textarea")
            {
                parseTextArea(container, info);
            }
            else if (info.tagName() == "slider")
            {
                parseSlider(container, info);
            }
            else if (info.tagName() == "box")
            {
                parseBox(container, info);
            }
            else if (info.tagName() == "editarrow")
            {
                parseEditArrow(container, info);
            }
            else if (info.tagName() == "positionrects")
            {
                parsePositionRects(container, info);
            }
            else if (info.tagName() == "positionimage")
            {
                parsePositionImage(container, info);
            }
            else
            {
                cerr << "Unknown container child: " << info.tagName() << endl;
                exit(0);
            }
        }
    }
}

bool OSD::LoadTheme(void)
{
    QString themefile = themepath + "/osd.xml";

    QDomDocument doc;
    QFile f(themefile);

    if (!f.open(IO_ReadOnly))
    {
        cerr << "OSD::LoadTheme(): Can't open: " << themefile << endl;
        return false; 
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cout << "Error parsing: " << themefile << endl;
        cout << "at line: " << errorLine << "  column: " << errorColumn << endl;
        cout << errorMsg << endl;
        f.close();
        return false;
    }

    f.close();

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "timeformat")
            {
                timeFormat = getFirstText(e);
            }
            else if (e.tagName() == "fadeaway")
            {
                totalfadeframes = getFirstText(e).toInt();
            }
            else if (e.tagName() == "font")
            {
                parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
        n = n.nextSibling();
    }

    return true;
}

void OSD::normalizeRect(QRect *rect)
{   
    rect->setWidth((int)(rect->width() * wmult));
    rect->setHeight((int)(rect->height() * hmult));
    rect->moveTopLeft(QPoint((int)(xoffset + rect->x() * wmult),
                             (int)(yoffset + rect->y() * hmult)));
}       

QPoint OSD::parsePoint(QString text)
{
    int x, y;
    QPoint retval(0, 0);
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QRect OSD::parseRect(QString text)
{
    int x, y, w, h;
    QRect retval(0, 0, 0, 0);
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    return retval;
}

void OSD::ClearAllText(const QString &name)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet(name);
    if (container)
        container->ClearAllText();

    pthread_mutex_unlock(&osdlock);
}

void OSD::SetTextByRegexp(const QString &name,
                          QMap<QString, QString> &regexpMap, int length)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet(name);
    if (container)
    {
        container->SetTextByRegexp(regexpMap);
        if (length >= 0)
            container->DisplayFor(length * fps);
        else
            container->Display();

        if ((regexpMap.contains("iconpath")) &&
            (regexpMap["iconpath"] != ""))
        {
            OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
            if (cs)
                cs->LoadImage(regexpMap["iconpath"], wmult, hmult, 30, 30);
        }

        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::SetInfoText(QMap<QString, QString> regexpMap, int length)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("program_info");
    if (container)
    {
        container->SetTextByRegexp(regexpMap);

        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
        if ((cs) &&
            (regexpMap.contains("iconpath")) &&
            (regexpMap["iconpath"] != ""))
            cs->LoadImage(regexpMap["iconpath"], wmult, hmult, 30, 30);

        container->DisplayFor(length * fps);
        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::SetInfoText(const QString &text, const QString &subtitle,
                      const QString &desc, const QString &category,
                      const QString &start, const QString &end, 
                      const QString &callsign, const QString &iconpath,
                      int length)
{
    QString tmp = category;
    tmp = start;
    tmp = end;

    bool hassubtitle = true;

    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("program_info");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("title");
        if (type)
            type->SetText(text);

        type = (OSDTypeText *)container->GetType("subtitle");
        if (type)
            type->SetText(subtitle);
        else
            hassubtitle = false;

        type = (OSDTypeText *)container->GetType("description");
        if (type)
        {
            if (!hassubtitle && subtitle.length() > 1)
            {
                QString tmpdesc = "\"" + subtitle + "\"";
                if (desc.length() > 1)
                    tmpdesc += ", " + desc;
                type->SetText(tmpdesc);
            }
            else
                type->SetText(desc);
        }
        type = (OSDTypeText *)container->GetType("callsign");
        if (type)
            type->SetText(callsign.left(5));
        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
        if (cs)
            cs->LoadImage(iconpath, wmult, hmult, 30, 30);

        container->DisplayFor(length * fps);
        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::StartPause(int position, bool fill, QString msgtext,
                     QString slidertext, int displaytime)
{
    fill = fill;

    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("status");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("status");
        if (type)
            type->SetText(msgtext);
        type = (OSDTypeText *)container->GetType("slidertext");
        if (type)
            type->SetText(slidertext);
        OSDTypeFillSlider *slider = 
                      (OSDTypeFillSlider *)container->GetType("statusslider");
        if (slider)
            slider->SetPosition(position);

        if (displaytime > 0)
            container->DisplayFor(displaytime * fps);
        else
            container->Display();
        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::UpdatePause(int position, QString slidertext)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("status");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("slidertext");
        if (type)
            type->SetText(slidertext);
        OSDTypeFillSlider *slider =
                      (OSDTypeFillSlider *)container->GetType("statusslider");
        if (slider)
            slider->SetPosition(position);

        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::EndPause(void)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("status");
    if (container)
    {
        container->Hide();
        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::SetChannumText(const QString &text, int length)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("channel_number");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("channel_number");
        if (type)
            type->SetText(text);

        container->DisplayFor(length * fps);
        m_setsvisible = true;
    }

    pthread_mutex_unlock(&osdlock);
}

void OSD::AddCCText(const QString &text, int x, int y, int color, 
                    bool teletextmode)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("cc_page");
    if (container)
    {
        OSDTypeCC *ccpage = (OSDTypeCC *)container->GetType("cc_page");
        if (ccpage)
            ccpage->AddCCText(text, x, y, color, teletextmode);

        container->Display();
        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::ClearAllCCText()
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("cc_page");
    if (container)
    {
        OSDTypeCC *ccpage = (OSDTypeCC *)container->GetType("cc_page");
        if (ccpage)
        {
            ccpage->ClearAllCCText();
        }

        container->Display(false);
        m_setsvisible = true;
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::SetSettingsText(const QString &text, int length)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet("settings");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("settings");
        if (type)
            type->SetText(text);

        container->DisplayFor(length * fps);
        m_setsvisible = true;
    }

    pthread_mutex_unlock(&osdlock);
}

void OSD::NewDialogBox(const QString &name, const QString &message, 
                       QStringList &options, int length)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet(name);
    if (container)
    {
        cerr << "dialog: " << name << " already exists.\n";
        return;
    }       

    OSDSet *base = GetSet("basedialog");
    if (!base)
    {
        cerr << "couldn't find base dialog\n";
        return;
    }

    container = new OSDSet(*base);
    container->SetName(name);
    container->SetCache(false);
    container->SetPriority(0);
    container->SetAllowFade(false);
    container->SetFrameRate(fps);
    AddSet(container, name, false);

    OSDTypeText *question = (OSDTypeText *)container->GetType("message");
    if (question)
        question->SetText(message);

    int availoptions = 0;
    OSDTypeText *text = NULL;
    do
    {
        QString name = QString("option%1").arg(availoptions + 1);
        text = (OSDTypeText *)container->GetType(name);
        if (text)
            availoptions++;
    }
    while (text);

    int numoptions = options.size();

    for (int i = 1; i <= numoptions && i <= availoptions; i++)
    {
        QString name = QString("option%1").arg(availoptions - numoptions + i);
        text = (OSDTypeText *)container->GetType(name);
        if (!text)
        {
            cerr << "Couldn't find: " << name << endl;
            exit(0);
        }

        text->SetText(options[i - 1]);
        text->SetUseAlt(true);
    }

    OSDTypePositionIndicator *opr = 
        dynamic_cast<OSDTypePositionIndicator*>(container->GetType("selector"));
    if (!opr)
    {
        cerr << "Need a positionindicator named 'selector' in the basedialog\n";
        exit(0);
    }

    opr->SetOffset(availoptions - numoptions);
    opr->SetPosition(0);

    dialogResponseList[name] = 0;

    HighlightDialogSelection(container, availoptions - numoptions);

    if (length > 0)
        container->DisplayFor(length * fps);
    else
        container->Display();
    m_setsvisible = true;

    pthread_mutex_unlock(&osdlock);
}

void OSD::HighlightDialogSelection(OSDSet *container, int num)
{
    int availoptions = 0;
    OSDTypeText *text = NULL;
    do
    {
       QString name = QString("option%1").arg(availoptions + 1);
       text = (OSDTypeText *)container->GetType(name);
       if (text)
           availoptions++;
    }
    while (text);

    for (int i = 1; i <= availoptions; i++)
    {
        QString name = QString("option%1").arg(i);
        text = (OSDTypeText *)container->GetType(name);
        if (text)
        {
            if (i == num + 1) 
                text->SetUseAlt(false);
            else
                text->SetUseAlt(true);
        }
    }
}

void OSD::TurnDialogOff(const QString &name)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet(name);
    if (container)
    {
        container->Hide();
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::DialogUp(const QString &name)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet(name);
    if (container)
    {
        OSDType *basetype = container->GetType("selector");
        OSDTypePositionIndicator *type = 
                            dynamic_cast<OSDTypePositionIndicator*>(basetype);
        if (type)
        {
            type->PositionUp();
            dialogResponseList[name] = type->GetPosition();

            int selected = type->GetPosition() + type->GetOffset();
            HighlightDialogSelection(container, selected);
        }
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::DialogDown(const QString &name)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet(name);
    if (container)
    {
        OSDType *basetype = container->GetType("selector");
        OSDTypePositionIndicator *type =
                            dynamic_cast<OSDTypePositionIndicator*>(basetype);
        if (type)
        {
            type->PositionDown();
            dialogResponseList[name] = type->GetPosition();

            int selected = type->GetPosition() + type->GetOffset();
            HighlightDialogSelection(container, selected);
        }
    }
    pthread_mutex_unlock(&osdlock);
}

bool OSD::DialogShowing(const QString &name)
{
    if (name == "")
        return false;

    return (GetSet(name) != NULL);
}

void OSD::DialogAbort(const QString &name)
{
    dialogResponseList[name] = -1;
}

int OSD::GetDialogResponse(const QString &name)
{
    if (dialogResponseList.contains(name))
    {
        int ret = dialogResponseList[name] + 1;
        dialogResponseList.erase(name);

        return ret;
    }
    return -1;
}

void OSD::ShowEditArrow(long long number, long long totalframes, int type)
{
    if (!editarrowleft || !editarrowright)
        return;

    char name[128];
    sprintf(name, "%lld-%d", number, type);

    int xpos = (int)((editarrowRect.width() * 1.0 / totalframes) * number);
    xpos = editarrowRect.left() + xpos;
    int ypos = editarrowRect.top();

    pthread_mutex_lock(&osdlock);

    OSDSet *set = new OSDSet(name, false, vid_width, vid_height, wmult, hmult);
    set->SetAllowFade(false);
    AddSet(set, name, false);

    OSDTypeImage *image;
    if (type == 0)
    {
        image = new OSDTypeImage(*editarrowleft);
        xpos -= image->ImageSize().width();
    }
    else
    {
        image = new OSDTypeImage(*editarrowright);
    }

    image->SetPosition(QPoint(xpos, ypos));

    set->AddType(image);
    set->Display();

    pthread_mutex_unlock(&osdlock);
}

void OSD::HideEditArrow(long long number, int type)
{
    char name[128];
    sprintf(name, "%lld-%d", number, type);

    pthread_mutex_lock(&osdlock);
    OSDSet *set = GetSet(name);
    if (set)
        set->Hide();
    pthread_mutex_unlock(&osdlock);
}

void OSD::HideSet(const QString &name)
{
    pthread_mutex_lock(&osdlock);

    OSDSet *set = GetSet(name);
    if (set)
    {
        set->Hide();
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::UpdateEditText(const QString &seek_amount, const QString &deletemarker, 
		           const QString &edittime, const QString &framecnt)
{
    pthread_mutex_lock(&osdlock);

    QString name = "editmode";
    OSDSet *set = GetSet(name);
    if (set)
    {
        OSDTypeText *text = (OSDTypeText *)set->GetType("seekamount");
        if (text && seek_amount != QString::null)
            text->SetText(seek_amount);
        text = (OSDTypeText *)set->GetType("cutindicator");
        if (text && deletemarker != QString::null)
            text->SetText(deletemarker);
        text = (OSDTypeText *)set->GetType("timedisplay");
        if (text && edittime != QString::null)
            text->SetText(edittime);
        text = (OSDTypeText *)set->GetType("framedisplay");
        if (text && framecnt != QString::null)
            text->SetText(framecnt);

        set->Display();
        m_setsvisible = true;
    }

    pthread_mutex_unlock(&osdlock);
}

void OSD::DoEditSlider(QMap<long long, int> deleteMap, long long curFrame,
                       long long totalFrames)
{
    pthread_mutex_lock(&osdlock);

    QString name = "editmode";
    OSDSet *set = GetSet(name);
    if (set)
    {
        QString name = "editslider";
        OSDTypeEditSlider *tes = (OSDTypeEditSlider *)set->GetType(name);
        if (tes)
        {
            tes->ClearAll();

            bool indelete = false;
            int startpos = 0;
            int endpos = 0;
            bool first = true;

            QMap<long long, int>::Iterator i = deleteMap.begin();
            for (; i != deleteMap.end(); ++i)
            {
                long long frame = i.key();
                int direction = i.data();

                if (direction == 0 && !indelete && first)
                {
                    startpos = 0;
                    endpos = frame * 1000 / totalFrames;
                    tes->SetRange(startpos, endpos);
                    first = false;
                }
                else if (direction == 0)
                {
                    endpos = frame * 1000 / totalFrames;
                    tes->SetRange(startpos, endpos);
                    indelete = false;
                    first = false;
                }
                else if (direction == 1 && !indelete)
                {
                    startpos = frame * 1000 / totalFrames;
                    indelete = true;
                    first = false;
                }
            }
           
            if (indelete)
            {
                endpos = 1000;
                tes->SetRange(startpos, endpos);
            }
        }

        name = "editposition";
        OSDTypePosSlider *pos = (OSDTypePosSlider *)set->GetType(name);
        if (pos)
        {
            int num = curFrame * 1000 / totalFrames;
            pos->SetPosition(num);
        }

        set->Display();
        m_setsvisible = true;
    }

    pthread_mutex_unlock(&osdlock);
}

void OSD::SetVisible(OSDSet *set, int length)
{
    pthread_mutex_lock(&osdlock);
    if (length > 0)
        set->DisplayFor(length * fps);
    else
        set->Display();
    m_setsvisible = true;
    pthread_mutex_unlock(&osdlock);
}

void OSD::Display(unsigned char *yuvptr)
{
    bool anytodisplay = false;
    if (!setList)
        return;

    vector<OSDSet *> removeList;

    pthread_mutex_lock(&osdlock);
    vector<OSDSet *>::iterator i = setList->begin();
    for (; i != setList->end(); i++)
    {
        OSDSet *container = (*i);
        if (container->Displaying())
        {
            OSDTypeText *timedisp = (OSDTypeText *)container->GetType("time");
            if (timedisp)
            {
                QString thetime = QTime::currentTime().toString(timeFormat);
                timedisp->SetText(thetime);
            }

            container->Draw(yuvptr);
            anytodisplay = true;
            if (container->GetFramesLeft() == 0 && totalfadeframes > 0)
            {
                container->FadeFor(totalfadeframes);
            }
        }
        else if (container->HasDisplayed())
        {
            if (!container->GetCache())
                removeList.push_back(container);
        }
    }

    while (removeList.size() > 0)
    {
        OSDSet *container = removeList.back();
        RemoveSet(container);
        removeList.pop_back();
    }

    pthread_mutex_unlock(&osdlock);

    m_setsvisible = anytodisplay;
}

bool OSD::Visible(void)
{
    return m_setsvisible;
}

OSDSet *OSD::GetSet(const QString &text)
{
    OSDSet *ret = NULL;
    if (setMap.contains(text))
        ret = setMap[text];

    return ret;
}

TTFFont *OSD::GetFont(const QString &text)
{
    TTFFont *ret = NULL;
    if (fontMap.contains(text))
        ret = fontMap[text];

    return ret;
}

class comp
{
  public:
    bool operator()(const OSDSet *a, const OSDSet *b)
    {
        return (a->GetPriority() > b->GetPriority());
    }
};

void OSD::AddSet(OSDSet *set, QString name, bool withlock)
{
    if (withlock)
        pthread_mutex_lock(&osdlock);

    setMap[name] = set;
    setList->push_back(set);

    sort(setList->begin(), setList->end(), comp());     

    if (withlock)
        pthread_mutex_unlock(&osdlock);
}

void OSD::RemoveSet(OSDSet *set)
{
    setMap.erase(set->GetName());
    vector<OSDSet *>::iterator i = setList->begin();
    for (; i != setList->end(); i++)
        if (*i == set)
            break;

    if (i != setList->end())
        setList->erase(i);

    delete set;
}
