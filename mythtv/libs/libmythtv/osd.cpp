// C headers
#include <cmath>
#include <unistd.h>

// C++ headers
#include <iostream>
#include <algorithm>
using namespace std;

// Qt headers
#include <qstring.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qbitmap.h>
#include <qdir.h>
#include <qfile.h>
#include <qcolor.h>
#include <qregexp.h>

// MythTV headers
#include "ttfont.h"
#include "osd.h"
#include "osdtypes.h"
#include "osdsurface.h"
#include "mythcontext.h"
#include "libmyth/oldsettings.h"
#include "udpnotify.h"

#include "osdlistbtntype.h"

static float sq(float a) { return a*a; }

OSD::OSD(const QRect &osd_bounds, int   frameRate,
         const QRect &vis_bounds, float visibleAspect, float fontScaling)
    : QObject(),
      osdBounds(osd_bounds),              frameint(frameRate),
      needPillarBox(false),
      themepath(FindTheme(gContext->GetSetting("OSDTheme"))),
      wscale(1.0f), fscale(fontScaling),
      hmult(vis_bounds.height() / 480.0f),wmult(vis_bounds.width() / 640.0f),
      xoffset(vis_bounds.left()),         yoffset(vis_bounds.top()),
      displaywidth(vis_bounds.width()),   displayheight(vis_bounds.height()),
      m_setsvisible(false),
      totalfadetime(0),                   timeType(0),
      timeFormat(""),                     setList(new vector<OSDSet*>),
      editarrowleft(NULL),                editarrowright(NULL),
      drawSurface(new OSDSurface(osd_bounds.width(), osd_bounds.height())),
      changed(false),                     runningTreeMenu(NULL),
      treeMenuContainer("")
{
    needPillarBox = visibleAspect > 1.51f;
    wscale = visibleAspect / 1.3333f;
    // adjust for wscale font size scaling
    fscale *= (float) sqrt(2.0/(sq(wscale) + 1.0));

    if (themepath.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Couldn't find OSD theme: "
                <<gContext->GetSetting("OSDTheme"));
        SetDefaults();
        return;
    }

    themepath += "/";
    if (!LoadTheme())
    {
        VERBOSE(VB_IMPORTANT, "Couldn't load OSD theme: "
                <<gContext->GetSetting("OSDTheme")<<" at "<<themepath);
    }
    SetDefaults();

    // Reinit since LoadThemes and SetDefaults() appear to mess things up.
    Reinit(osd_bounds, frameRate, vis_bounds, visibleAspect, fontScaling);
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
    delete drawSurface;
}

void OSD::SetFrameInterval(int frint)
{
    frameint = frint;

    QMap<QString, OSDSet *>::iterator sets = setMap.begin();
    for (; sets != setMap.end(); ++sets)
    {
        OSDSet *set = (*sets);
        if (set)
           set->SetFrameInterval(frameint);
    }
}

void OSD::SetDefaults(void)
{
    TTFFont *ccfont = GetFont("cc_font");
    if (!ccfont)
    {
        QString name = "cc_font";
        int fontsize = 480 / 27;

        ccfont = LoadFont(gContext->GetSetting("OSDCCFont"), fontsize);

        if (ccfont)
            fontMap[name] = ccfont;
    }

    if (!ccfont)
        return;

    OSDSet *container = GetSet("cc_page");
    if (!container)
    {
        QString name = "cc_page";
        container = new OSDSet(name, true,
                               osdBounds.width(), osdBounds.height(),
                               wmult, hmult, frameint);
        container->SetPriority(30);
        AddSet(container, name);

        int sub_dispw = displaywidth;
        int sub_disph = displayheight;
        int sub_xoff = xoffset;
        int sub_yoff = yoffset;
        if (needPillarBox)
        {
            // widescreen -- need to "pillarbox" captions
            sub_dispw = (int)(wscale * 4.0f*displayheight/3.0f);
            sub_disph = displayheight;
            sub_xoff = xoffset + (displaywidth-sub_dispw)/2;
            sub_yoff = yoffset;
        }

        OSDTypeCC *ccpage = new OSDTypeCC(name, ccfont, sub_xoff, sub_yoff,
                                          sub_dispw, sub_disph);
        container->AddType(ccpage);
    }

    container = GetSet("menu");
    if (!container)
    {
        QString name = "menu";
        container = new OSDSet(name, true,
                               osdBounds.width(), osdBounds.height(),
                               wmult, hmult, frameint);
        AddSet(container, name);
 
        QRect area = QRect(20, 40, 620, 300);
        QRect listarea = QRect(0, 0, 274, 260);

        normalizeRect(area);
        normalizeRect(listarea);
        listarea.moveBy((int)(-xoffset*hmult+0.5), (int)(-yoffset*hmult+0.5));

        OSDListTreeType *lb = new OSDListTreeType("menu", area, listarea, 10,
                                                  wmult, hmult);
 
        lb->SetItemRegColor(QColor("#505050"), QColor("#000000"), 100);
        lb->SetItemSelColor(QColor("#52CA38"), QColor("#349838"), 255);
 
        lb->SetSpacing(2);
        lb->SetMargin(3);

        TTFFont *actfont = GetFont("infofont");
        TTFFont *inactfont = GetFont("infofont");

        if (!actfont)
        {
            actfont = LoadFont(gContext->GetSetting("OSDFont"), 16);

            if (actfont)
                fontMap["treemenulistfont"] = actfont;
        }

        if (!actfont)
        {
            QMap<QString, TTFFont *>::Iterator it = fontMap.begin();
            actfont = it.data();
        }

        if (!inactfont)
            inactfont = actfont;

        lb->SetFontActive(actfont);
        lb->SetFontInactive(inactfont);

        container->AddType(lb);
    }

    // Create container for subtitles
    container = GetSet("subtitles");
    if (!container)
    {
        QString name = "subtitles";
        container = new OSDSet(name, true,
                               osdBounds.width(), osdBounds.height(),
                               wmult, hmult, frameint);
        container->SetPriority(30);
        AddSet(container, name);
    }
}

void OSD::Reinit(const QRect &totalBounds,   int   frameRate,
                 const QRect &visibleBounds,
                 float visibleAspect, float fontScaling)
{
    QMutexLocker locker(&osdlock);

    QRect oldB    = osdBounds;

    osdBounds     = totalBounds;
    xoffset       = visibleBounds.left();
    yoffset       = visibleBounds.top();
    displaywidth  = visibleBounds.width();
    displayheight = visibleBounds.height();
    wmult         = displaywidth  / 640.0;
    hmult         = displayheight / 480.0;
    needPillarBox = visibleAspect > 1.51f;
    frameint      = (frameRate <= 0) ? frameRate : frameint;

    wscale = visibleAspect / 1.3333f;
    fscale = fontScaling;
    // adjust for wscale font size scaling
    fscale *= (float) sqrt(2.0/(sq(wscale) + 1.0));

    QMap<QString, TTFFont *>::iterator fonts = fontMap.begin();
    for (; fonts != fontMap.end(); ++fonts)
    {
        TTFFont *font = (*fonts);
        if (font)
            font->Reinit(wscale, hmult * fscale);
    }

    QMap<QString, OSDSet *>::iterator sets = setMap.begin();
    for (; sets != setMap.end(); ++sets)
    {
        if (!(*sets))
            continue;

        int sub_xoff  = xoffset;
        int sub_yoff  = yoffset;
        int sub_dispw = displaywidth;
        int sub_disph = displayheight;
        if ((*sets)->GetName() == "cc_page" && needPillarBox)
        {
            // widescreen -- need to "pillarbox" captions
            sub_dispw = (int)(wscale * 4.0f*displayheight/3.0f);
            sub_disph = displayheight;
            sub_xoff = xoffset + (displaywidth-sub_dispw)/2;
            sub_yoff = yoffset;
        }
        (*sets)->Reinit(osdBounds.width(), osdBounds.height(),
                        sub_xoff, sub_yoff, sub_dispw, sub_disph, 
                        wmult, hmult, frameint);
    }

    if (true || oldB != osdBounds)
    {
        delete drawSurface;
        drawSurface = new OSDSurface(osdBounds.width(), osdBounds.height());
    }
    else
    {
        drawSurface->ClearUsed();
    }
}

QString OSD::FindTheme(QString name)
{
    QString testdir = MythContext::GetConfDir() + "/osd/" + name;
    
    QDir dir(testdir);
    if (dir.exists())
        return testdir;

    testdir = gContext->GetShareDir() + "themes/" + name;
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
    QString fullname = MythContext::GetConfDir() + "/" + name;
    TTFFont *font = new TTFFont((char *)fullname.ascii(), size,
                                wscale, hmult*fscale);

    if (font->isValid())
        return font;

    delete font;
    fullname = gContext->GetShareDir() + name;

    font = new TTFFont((char *)fullname.ascii(), size,
                       wscale, hmult*fscale);

    if (font->isValid())
        return font;

    delete font;
    if (themepath != "")
    {
        fullname = themepath + "/" + name;
        font = new TTFFont((char *)fullname.ascii(), size,
                           wscale, hmult*fscale);
        if (font->isValid())
            return font;
    }

    delete font;

    fullname = name;
    font = new TTFFont((char *)fullname.ascii(), size,
                       wscale, hmult*fscale);

    if (font->isValid())
        return font;
   
    VERBOSE(VB_IMPORTANT, QString("Unable to find font: %1\n\t\t\t"
                                  "No OSD will be displayed.").arg(name));

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
    QString fontfile = gContext->GetSetting("OSDFont");
    int size = -1;
    int sizeSmall = -1;
    int sizeBig = -1;    
    bool outline = false;
    QPoint shadowOffset = QPoint(0, 0);
    int color = 255;
    QColor color_normal, color_outline, color_shadow;

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Font needs a name");
        return;
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
                color = getFirstText(info).toInt();

                if (info.hasAttribute("normal"))
                    color_normal = parseColor(info.attribute("normal"));

                if (info.hasAttribute("outline"))
                    color_outline = parseColor(info.attribute("outline"));

                if (info.hasAttribute("shadow"))
                    color_shadow = parseColor(info.attribute("shadow"));
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
                VERBOSE(VB_IMPORTANT, "Unknown tag "
                        << info.tagName() << " in font");
                continue;
            }
        }
    }

    TTFFont *font = GetFont(name);
    if (font)
    {
        VERBOSE(VB_IMPORTANT, "Error: already have a font called: " << name);
        return;
    }

    QString fontSizeType = gContext->GetSetting("OSDThemeFontSizeType",
                                                "default");
    if (fontSizeType == "small")
    {
        if (sizeSmall > 0)
            size = sizeSmall;
    }
    else if (fontSizeType == "big")
    {
        if (sizeBig > 0)
            size = sizeBig;
    }
    
    if (size <= 0)
    {
        VERBOSE(VB_IMPORTANT, "Error: font size specified as: " << size);
        size = 10;
    }

    font = LoadFont(fontfile, size);
    if (!font)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't load font: " << fontfile);
        return;
    }

    font->setColor(color);
    font->setOutline(outline);
    font->setShadow(shadowOffset.x(), shadowOffset.y());

    if (color_normal.isValid())
        font->setColor(color_normal, kTTF_Normal);

    if (color_outline.isValid())
        font->setColor(color_outline, kTTF_Outline);

    if (color_shadow.isValid())
        font->setColor(color_shadow, kTTF_Shadow);

    fontMap[name] = font;
}

void OSD::parseBox(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Box needs a name");
        return;
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
                normalizeRect(area);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in box: " << info.tagName());
                return;
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
        VERBOSE(VB_IMPORTANT, "Image needs a name");
        return;
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
                VERBOSE(VB_IMPORTANT, QString("Unknown: %1 in image")
                        .arg(info.tagName()));
                return;
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
    bool scroller = false;
    int scrollx = 0;
    int scrolly = 0;
    float linespacing = 1.5;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Text area needs a name");
        return;
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
                normalizeRect(area);
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
            else if (info.tagName() == "scroller")
            {
                if (getFirstText(info).lower() == "yes")
                    scroller = true;
            }
            else if (info.tagName() == "linespacing")
            {
                linespacing = getFirstText(info).toFloat();
            }
            else if (info.tagName() == "scrollmovement")
            {
                QPoint pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));

                scrollx = pos.x();
                scrolly = pos.y();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in textarea: "
                        << info.tagName());
                return;
            }                   
        }
    }    

    TTFFont *ttffont = GetFont(font);
    if (!ttffont)
    {
        VERBOSE(VB_IMPORTANT, "Unknown font: " << font
                << " in textarea: " << name);
        return;
    }

    OSDTypeText *text = new OSDTypeText(name, ttffont, "", area);
    container->AddType(text);

    text->SetMultiLine(multiline);
    text->SetLineSpacing(linespacing);

    if (altfont != "")
    {
        ttffont = GetFont(altfont);
        if (!ttffont)
        {
            VERBOSE(VB_IMPORTANT, "Unknown altfont: " << altfont
                    << " in textarea: " << name);
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

    if (scroller)
    {
        if (scrollx == 0 && scrolly == 0)
        {
            VERBOSE(VB_IMPORTANT,
                    "Text area set as scrolling, but no movement");
            scrollx = -5;
        }

        text->SetScrolling(scrollx, scrolly);
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
        VERBOSE(VB_IMPORTANT, "Slider needs a name");
        return;
    }

    QString type = element.attribute("type", "");
    if (type.isNull() || type.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Slider needs a type");
        return;
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
                normalizeRect(area);            
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
                VERBOSE(VB_IMPORTANT, QString("Unknown: %1 in image")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    if (filename == "")
    {
        VERBOSE(VB_IMPORTANT, "Slider needs a filename");
        return;
    }

    filename = themepath + filename;

    if (type.lower() == "fill")
    {
        OSDTypeFillSlider *slider = new OSDTypeFillSlider(
            name, filename, area, wmult, hmult);
        container->AddType(slider);
    }
    else if (type.lower() == "edit")
    {
        if (altfilename == "")
        {
            VERBOSE(VB_IMPORTANT, "Edit slider needs an altfilename");
            return;
        }

        altfilename = themepath + altfilename;

        OSDTypeEditSlider *tes = new OSDTypeEditSlider(
            name, filename, altfilename, area, wmult, hmult);
        container->AddType(tes);
    }
    else if (type.lower() == "position")
    {
        OSDTypePosSlider *pos = new OSDTypePosSlider(
            name, filename, area, wmult, hmult);
        container->AddType(pos);
    }
}

void OSD::parseEditArrow(OSDSet *container, QDomElement &element)
{
    container = container;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "editarrow needs a name");
        return;
    }

    if (name != "left" && name != "right")
    {
        VERBOSE(VB_IMPORTANT, "editarrow name needs "
                "to be either 'left' or 'right'");
        return;
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
                normalizeRect(area);
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in editarrow: "
                        << info.tagName());
                return;
            }
        }
    }

    if (filename == "")
    {
        VERBOSE(VB_IMPORTANT, "editarrow needs a filename");
        return;
    }

    editarrowRect = area;

    QString setname = "arrowimage";

    filename = themepath + filename;

    OSDTypeImage *image = new OSDTypeImage(
        setname, filename, QPoint(0, 0), wmult, hmult);

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
        VERBOSE(VB_IMPORTANT, "positionsrects needs a name");
        return;
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
                normalizeRect(area);

                rects->AddPosition(area);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in editarrow: "
                        << info.tagName());
                return;
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
        VERBOSE(VB_IMPORTANT, "positionimage needs a name");
        return;
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
                VERBOSE(VB_IMPORTANT, QString("Unknown: %1 in positionimage")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    if (filename != "")
        filename = themepath + filename;

    image->SetStaticSize(scale.x(), scale.y());
    image->LoadImage(filename, wmult, hmult, scale.x(), scale.y());

    container->AddType(image);
}

void OSD::parseListTree(OSDSet *container, QDomElement &element)
{
    QRect   area = QRect(0,0,0,0);
    QRect   listsize = QRect(0,0,0,0);
    int     leveloffset = 0;
    QString fontActive;
    QString fontInactive;
    bool    showArrow = true;
    bool    showScrollArrows = false;
    QColor  grUnselectedBeg(Qt::black);
    QColor  grUnselectedEnd(80,80,80);
    uint    grUnselectedAlpha(100);
    QColor  grSelectedBeg(82,202,56);
    QColor  grSelectedEnd(52,152,56);
    uint    grSelectedAlpha(255);
    int     spacing = 2;
    int     margin = 3;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "listtree needs a name");
        return;
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
                normalizeRect(area);
            }
            else if (info.tagName() == "listsize")
            {
                listsize = parseRect(getFirstText(info));
                normalizeRect(listsize);
                listsize.moveBy(-xoffset, -yoffset);
            }
            else if (info.tagName() == "leveloffset")
            {
                leveloffset = getFirstText(info).toInt();
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontName = info.attribute("name", "");
                QString fontFcn  = info.attribute("function", "");

                if (fontFcn.lower() == "active")
                    fontActive = fontName;
                else if (fontFcn.lower() == "inactive")
                    fontInactive = fontName;
                else 
                {
                    VERBOSE(VB_IMPORTANT, "Unknown font function "
                            "for listtree area: " << fontFcn);
                    return;
                }
            }
            else if (info.tagName() == "showarrow") 
            {
                if (getFirstText(info).lower() == "no")
                    showArrow = false;
            }
            else if (info.tagName() == "showscrollarrows") 
            {
                if (getFirstText(info).lower() == "yes")
                    showScrollArrows = true;
            }
            else if (info.tagName() == "gradient") 
            {
                if (info.attribute("type","").lower() == "selected") 
                {
                    grSelectedBeg = QColor(info.attribute("start"));
                    grSelectedEnd = QColor(info.attribute("end"));
                    grSelectedAlpha = info.attribute("alpha","255").toUInt();
                }
                else if (info.attribute("type","").lower() == "unselected") 
                {
                    grUnselectedBeg = QColor(info.attribute("start"));
                    grUnselectedEnd = QColor(info.attribute("end"));
                    grUnselectedAlpha = info.attribute("alpha","100").toUInt();
                }
                else 
                {
                    VERBOSE(VB_IMPORTANT,
                            "Unknown type for gradient in listtree area");
                    return;
                }

                if (!grSelectedBeg.isValid() || !grSelectedEnd.isValid() ||
                    !grUnselectedBeg.isValid() || !grUnselectedEnd.isValid()) 
                {
                    VERBOSE(VB_IMPORTANT,
                            "Unknown color for gradient in listtree area");
                    return;
                }

                if (grSelectedAlpha > 255 || grUnselectedAlpha > 255)
                {
                    VERBOSE(VB_IMPORTANT,
                            "Incorrect alpha for gradient in listtree area");
                    return;
                }
            }
            else if (info.tagName() == "spacing") {
                spacing = getFirstText(info).toInt();
            }
            else if (info.tagName() == "margin") {
                margin = getFirstText(info).toInt();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in listtree area: "
                     << info.tagName());
                return;
            }
        }
    }

    TTFFont *fpActive = GetFont(fontActive);
    if (!fpActive)
    {
        VERBOSE(VB_IMPORTANT, "Unknown font: " << fontActive << " in listtree");
        return;
    }

    TTFFont *fpInactive = GetFont(fontInactive);
    if (!fpInactive)
    {
        VERBOSE(VB_IMPORTANT, "Unknown font: "
                << fontInactive << " in listtree");
        return;
    }

    OSDListTreeType *lb = new OSDListTreeType(
        name, area, listsize, leveloffset, wmult, hmult);
    lb->SetFontActive(fpActive);
    lb->SetFontInactive(fpInactive);
    lb->SetItemRegColor(grUnselectedBeg, grUnselectedEnd, grUnselectedAlpha);
    lb->SetItemSelColor(grSelectedBeg, grSelectedEnd, grSelectedAlpha);
    lb->SetSpacing(spacing);
    lb->SetMargin(margin);

    container->AddType(lb);
}

void OSD::parseContainer(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Container needs a name");
        return;
    }

    OSDSet *container = GetSet(name);
    if (container) 
    {
        VERBOSE(VB_IMPORTANT, "Container: " << name << " already exists");
        return;
    }

    container = new OSDSet(name, true,
                           osdBounds.width(), osdBounds.height(),
                           wmult, hmult, frameint);

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

    QString showwith = element.attribute("showwith", "");
    if (!showwith.isEmpty())
        container->SetShowWith(showwith);

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
            else if (info.tagName() == "listtreemenu")
            {
                parseListTree(container, info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown container child: %1")
                        .arg(info.tagName()));
                return;
            }
        }
    }
}

bool OSD::LoadTheme(void)
{
    // HACK begin -- needed to address ticket #989
    xoffset = 0;
    yoffset = 0;
    displaywidth  = 640;
    displayheight = 480;
    hmult = 1.0f;
    wmult = 1.0f;
    // HACK end

    QString themefile = themepath + "/osd.xml";

    QDomDocument doc;
    QFile f(themefile);

    if (!f.open(IO_ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, "OSD::LoadTheme(): Can't open: " << themefile);
        return false; 
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    { 
        VERBOSE(VB_IMPORTANT, QString("Error parsing: %1\n\t\t\t"
                                      "at line: %2  column: %3\n\t\t\t%4")
                .arg(themefile).arg(errorLine).arg(errorColumn).arg(errorMsg));
        f.close();
        return false;
    }

    f.close();

    QDomElement docElem = doc.documentElement();
    for (QDomNode n = docElem.firstChild(); !n.isNull();
         n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "timeformat")
            {
                timeFormat = getFirstText(e);
                if (timeFormat.upper() == "FROMSETTINGS")
                    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
            }
            else if (e.tagName() == "fadeaway")
            {
                totalfadetime = (getFirstText(e).toInt() * 1000000) / 30;
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
                VERBOSE(VB_IMPORTANT, "Unknown element: " << e.tagName());
                continue;
            }
        }
    }

    return true;
}

void OSD::normalizeRect(QRect &rect)
{   
    rect.setWidth((int)(rect.width() * wmult));
    rect.setHeight((int)(rect.height() * hmult));
    rect.moveTopLeft(QPoint((int)(xoffset + rect.x() * wmult),
                            (int)(yoffset + rect.y() * hmult)));
    rect = rect.normalize();
}       

QPoint OSD::parsePoint(QString text)
{
    int x, y;
    QPoint retval(0, 0);
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QColor OSD::parseColor(QString text)
{
    QColor retval;
    QRegExp regexp("#([0-9a-fA-F]){6}");
    if (regexp.exactMatch(text))
    {
        int val;
        if (sscanf(text.data(), "#%x", &val) == 1)
            retval = QColor((val >> 16) & 0xff, (val >> 8) & 0xff, val & 0xff);
    }
    else
    {
        int r, g, b;
        if (sscanf(text.data(), "%d,%d,%d", &r, &g, &b) == 3)
            retval = QColor(r, g, b);
    }
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

void OSD::ClearAll(const QString &name)
{
    osdlock.lock();
    OSDSet *container = GetSet(name);
    if (container)
        container->Clear();

    osdlock.unlock();
}

void OSD::ClearAllText(const QString &name)
{
    osdlock.lock();
    OSDSet *container = GetSet(name);
    if (container)
        container->ClearAllText();

    osdlock.unlock();
}

void OSD::SetText(const QString &name,
                          QMap<QString, QString> &infoMap, int length)
{
    HideAllExcept(name);

    osdlock.lock();
    OSDSet *container = GetSet(name);
    if (container)
    {
        container->SetText(infoMap);
        if (length >= 0)
            container->DisplayFor(length * 1000000);
        else
            container->Display();

        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
        if (cs)
        {
            if ((infoMap.contains("iconpath")) && (infoMap["iconpath"] != ""))
                cs->LoadImage(infoMap["iconpath"], wmult, hmult, 30, 30);
            else
                cs->LoadImage(" ", wmult, hmult, 30, 30);
        }

        m_setsvisible = true;
        changed = true;
    }
    osdlock.unlock();
}

void OSD::SetInfoText(QMap<QString, QString> infoMap, int length)
{
    osdlock.lock();
    OSDSet *container = GetSet("program_info");
    if (container)
    {
        container->SetText(infoMap);

        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
        if (cs)
        {
            if ((infoMap.contains("iconpath")) && (infoMap["iconpath"] != ""))
                cs->LoadImage(infoMap["iconpath"], wmult, hmult, 30, 30);
            else
                cs->LoadImage(" ", wmult, hmult, 30, 30);
        }

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }
    osdlock.unlock();
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

    osdlock.lock();
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

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }
    osdlock.unlock();
}

void OSD::SetUpOSDClosedHandler(TV *tv)
{
    OSDSet *container = GetSet("status");

    if (container)
        connect((QObject *)container, SIGNAL(OSDClosed(int)), (QObject *)tv,
                SLOT(HandleOSDClosed(int)));

}

void OSD::ShowStatus(int pos, bool fill, QString msgtext, QString desc,
                     int displaytime, int osdFunctionalType)
{
    struct StatusPosInfo posInfo;

    posInfo.desc = desc;
    posInfo.position = pos;
    posInfo.progBefore = false;
    posInfo.progAfter = false;

    ShowStatus(posInfo, fill, msgtext, displaytime, osdFunctionalType);
}

void OSD::ShowStatus(struct StatusPosInfo posInfo,
                       bool fill, QString msgtext, int displaytime,
                       int osdFunctionalType)
{
    fill = fill;

    HideAllExcept("status");

    osdlock.lock();
    OSDSet *container = GetSet("status");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("status");
        if (type)
            type->SetText(msgtext);
        type = (OSDTypeText *)container->GetType("slidertext");
        if (type)
            type->SetText(posInfo.desc);

        OSDTypeFillSlider *slider = 
                      (OSDTypeFillSlider *)container->GetType("statusslider");
        if (slider)
            slider->SetPosition(posInfo.position);

        OSDTypePosSlider *ppos =
                     (OSDTypePosSlider *)container->GetType("statusposition");
        if (ppos)
            ppos->SetPosition(posInfo.position);

        OSDTypeImage *beforeImage =
                      (OSDTypeImage *)container->GetType("progbefore");
        if (beforeImage)
            beforeImage->Hide(!posInfo.progBefore);

        OSDTypeImage *afterImage =
                      (OSDTypeImage *)container->GetType("progafter");
        if (afterImage)
            afterImage->Hide(!posInfo.progAfter);

        if (displaytime > 0)
            container->DisplayFor(displaytime * 1000000, osdFunctionalType);
        else
            container->Display();

        m_setsvisible = true;
        changed = true;
    }
    osdlock.unlock();
}

void OSD::UpdateStatus(struct StatusPosInfo posInfo)
{
    osdlock.lock();
    OSDSet *container = GetSet("status");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("slidertext");
        if (type)
        {
            if (type->GetText() != posInfo.desc)
            {
                type->SetText(posInfo.desc);
                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypeFillSlider *slider =
                      (OSDTypeFillSlider *)container->GetType("statusslider");
        if (slider)
        {
            if (slider->GetPosition() != posInfo.position)
            {
                slider->SetPosition(posInfo.position);

                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypePosSlider *ppos =
             (OSDTypePosSlider *)container->GetType("statusposition");
        if (ppos)
        {
            if (ppos->GetPosition() != posInfo.position)
            {
                ppos->SetPosition(posInfo.position);

                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypeImage *beforeImage =
                      (OSDTypeImage *)container->GetType("progbefore");
        if (beforeImage)
        {
            if (beforeImage->isHidden() != !posInfo.progBefore)
            {
                beforeImage->Hide(!posInfo.progBefore);

                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypeImage *afterImage =
                      (OSDTypeImage *)container->GetType("progafter");
        if (afterImage)
        {
            if (afterImage->isHidden() != !posInfo.progAfter)
            {
                afterImage->Hide(!posInfo.progAfter);

                m_setsvisible = true;
                changed = true;
            }
        }
    }
    osdlock.unlock();
}

void OSD::EndStatus(void)
{
    osdlock.lock();
    OSDSet *container = GetSet("status");
    if (container)
    {
        container->Hide();
        m_setsvisible = true;
        changed = true;
    }
    osdlock.unlock();
}

void OSD::SetChannumText(const QString &text, int length)
{
    osdlock.lock();
    OSDSet *container = GetSet("channel_number");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("channel_number");
        if (type)
            type->SetText(text);

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }

    osdlock.unlock();
}

void OSD::AddCCText(const QString &text, int x, int y, int color, 
                    bool teletextmode)
{
    osdlock.lock();
    OSDSet *container = GetSet("cc_page");
    if (container)
    {
        OSDTypeCC *ccpage = (OSDTypeCC *)container->GetType("cc_page");
        if (ccpage)
            ccpage->AddCCText(text, x, y, color, teletextmode);

        container->Display();
        m_setsvisible = true;
        changed = true;
    }
    osdlock.unlock();
}

void OSD::ClearAllCCText()
{
    osdlock.lock();
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
        changed = true;
    }
    osdlock.unlock();
}

void OSD::UpdateCCText(vector<ccText*> *ccbuf,
                       int replace, int scroll, bool scroll_prsv,
                       int scroll_yoff, int scroll_ymax)
{
    osdlock.lock();
    OSDSet *container = GetSet("cc_page");
    if (container)
    {
        OSDTypeCC *ccpage = (OSDTypeCC *)container->GetType("cc_page");
        int visible = 0;

        if (ccpage)
            visible = ccpage->UpdateCCText(ccbuf,
                                           replace, scroll, scroll_prsv,
                                           scroll_yoff, scroll_ymax);

        container->Display(visible);
        m_setsvisible = true;
        changed = true;
    }
    osdlock.unlock();
}

void OSD::SetSettingsText(const QString &text, int length)
{
    HideAllExcept("settings");

    osdlock.lock();
    OSDSet *container = GetSet("settings");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("settings");
        if (type)
            type->SetText(text);

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }

    osdlock.unlock();
}

void OSD::NewDialogBox(const QString &name, const QString &message, 
                       QStringList &options, int length,
                       int initial_selection)
{
    osdlock.lock();
    OSDSet *container = GetSet(name);
    if (container)
    {
        VERBOSE(VB_IMPORTANT, "dialog: " << name << " already exists.");
        return;
    }       

    OSDSet *base = GetSet("basedialog");
    if (!base)
    {
        VERBOSE(VB_IMPORTANT, "couldn't find base dialog");
        return;
    }

    container = new OSDSet(*base);
    container->SetName(name);
    container->SetCache(false);
    container->SetPriority(0);
    container->SetAllowFade(false);
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
    int offset = availoptions - numoptions;
    initial_selection = max(min(numoptions - 1, initial_selection), 0);

    for (int i = 1; i <= numoptions && i <= availoptions; i++)
    {
        QString name = QString("option%1").arg(offset + i);
        text = (OSDTypeText *)container->GetType(name);
        if (!text)
        {
            VERBOSE(VB_IMPORTANT, "Couldn't find: " << name);
            return;
        }

        text->SetText(options[i - 1]);
        text->SetUseAlt(true);
    }

    OSDTypePositionIndicator *opr = 
        dynamic_cast<OSDTypePositionIndicator*>(container->GetType("selector"));
    if (!opr)
    {
        VERBOSE(VB_IMPORTANT,
                "Need a positionindicator named 'selector' in the basedialog");
        return;
    }

    opr->SetOffset(offset);
    opr->SetPosition(initial_selection);

    dialogResponseList[name] = 0;

    HighlightDialogSelection(container, offset + initial_selection);

    if (length > 0)
        container->DisplayFor(length * 1000000);
    else
        container->Display();

    m_setsvisible = true;
    changed = true;

    osdlock.unlock();

    int count = 0;
    while (!container->HasDisplayed() && count++ < 10)
        usleep(1000);
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
    osdlock.lock();

    OSDSet *container = GetSet(name);
    if (container)
    {
        container->Hide();
        changed = true;
    }

    osdlock.unlock();
}

void OSD::DialogUp(const QString &name)
{
    osdlock.lock();
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
            changed = true;
        }
    }
    osdlock.unlock();
}

void OSD::DialogDown(const QString &name)
{
    osdlock.lock();
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
            changed = true;
        }
    }
    osdlock.unlock();
}

bool OSD::DialogShowing(const QString &name)
{
    if (name == "")
        return false;

    osdlock.lock();
    bool ret = (GetSet(name) != NULL);
    osdlock.unlock();

    return ret;
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
    xpos = (editarrowRect.left() + xpos) * wmult;
    int ypos = editarrowRect.top() * hmult;

    osdlock.lock();

    OSDSet *set = new OSDSet(name, false,
                             osdBounds.width(), osdBounds.height(),
                             wmult, hmult, frameint);
    set->SetAllowFade(false);
    OSDSet *container = GetSet("editmode");
    if (container)
        set->SetPriority(container->GetPriority() - 1);
    else
        set->SetPriority(4);

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

    changed = true;

    osdlock.unlock();
}

void OSD::HideEditArrow(long long number, int type)
{
    char name[128];
    sprintf(name, "%lld-%d", number, type);

    osdlock.lock();
    OSDSet *set = GetSet(name);
    if (set)
        set->Hide();

    changed = true;
    osdlock.unlock();
}

bool OSD::HideAllExcept(const QString &other)
{
    bool result = false;

    osdlock.lock();

    OSDSet *oset = GetSet(other);

    vector<OSDSet *>::iterator i;
    for (i = setList->begin(); i != setList->end(); i++)
        if (*i && (*i)->Displaying())
        {
            QString name = (*i)->GetName();
            if (name != "cc_page" && name != "menu" && name != "subtitles" &&
                name != other && (!oset || !oset->CanShowWith(name)))
            {
                (*i)->Hide();
                result = true;
            }
        }

    changed = true;
    osdlock.unlock();

    return result;
}

bool OSD::HideSet(const QString &name)
{
    bool ret = false;
    osdlock.lock();

    OSDSet *set = GetSet(name);
    if (set)
    {
        if (set->Displaying())
            ret = true;
        set->Hide();
    }

    changed = true;
    osdlock.unlock();
    return ret;
}

bool OSD::HideSets(QStringList &name)
{
    bool ret = false;
    osdlock.lock();

    OSDSet *set;
    QStringList::Iterator i = name.begin();
    for (; i != name.end(); i++)
    {
        set = GetSet(*i);
        if (set)
        {
            if (set->Displaying())
                ret = true;
            set->Hide();
        }
    }

    changed = true;
    osdlock.unlock();
    return ret;
}

void OSD::UpdateEditText(const QString &seek_amount, const QString &deletemarker, 
                         const QString &edittime, const QString &framecnt)
{
    osdlock.lock();

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
        changed = true;
    }

    osdlock.unlock();
}

void OSD::DoEditSlider(QMap<long long, int> deleteMap, long long curFrame,
                       long long totalFrames)
{
    osdlock.lock();

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
        changed = true;
    }

    osdlock.unlock();
}

void OSD::SetVisible(OSDSet *set, int length)
{
    osdlock.lock();
    if (length > 0)
        set->DisplayFor(length * 1000000);
    else
        set->Display();
    m_setsvisible = true;
    changed = true;
    osdlock.unlock();
}

void OSD::DisableFade(void)
{
    totalfadetime = 0;
}

OSDSurface *OSD::Display(void)
{
    bool anytodisplay = false;
    if (!setList)
        return NULL;

    bool actuallydraw = false;
 
    if (changed)
    {
        actuallydraw = true;
        changed = false; 
    }

    drawSurface->SetChanged(false);

    //actuallydraw = true;

    if (actuallydraw)
    {
        drawSurface->SetChanged(true);
        drawSurface->ClearUsed();
    }

    vector<OSDSet *> removeList;

    osdlock.lock();
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
                if (timedisp->GetText() != thetime)
                {
                    timedisp->SetText(thetime);
                    changed = true;
                }
            }

            if (container->Fading())
                changed = true;

            container->Draw(drawSurface, actuallydraw);
            anytodisplay = true;
            if (container->GetTimeLeft() == 0 && totalfadetime > 0)
            {
                container->FadeFor(totalfadetime);
                changed = true;
            }

            if (container->NeedsUpdate())
                changed = true;
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

    osdlock.unlock();

    m_setsvisible = anytodisplay;

    if (m_setsvisible)
        return drawSurface;

    return NULL;
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
        osdlock.lock();

    setMap[name] = set;
    setList->push_back(set);

    sort(setList->begin(), setList->end(), comp());     

    if (withlock)
        osdlock.unlock();
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

/* Ken Bass additions for notify_info container */
void OSD::StartNotify(UDPNotifyOSDSet *notifySet, int displaytime)
{
    if (!notifySet)
        return;

    vector<UDPNotifyOSDTypeText *> *textList;

    osdlock.lock();

    OSDSet *container = GetSet(notifySet->GetName());
    if (container)
    {    
        textList = notifySet->GetTypeList();
    
        vector<UDPNotifyOSDTypeText *>::iterator j = textList->begin();
        for (; j != textList->end(); j++)
        {
            UDPNotifyOSDTypeText *type = (*j);
            if (type)
            {
                OSDTypeText *osdtype = (OSDTypeText *)container->GetType(type->GetName());
                if (osdtype)
                    osdtype->SetText(type->GetText());
            }
        }
      
        if (displaytime > 0)
            container->DisplayFor(displaytime * 1000000);
        else
            container->Display();
  
        m_setsvisible = true;
        changed = true;
    }

    osdlock.unlock();
}

void OSD::ClearNotify(UDPNotifyOSDSet *notifySet)
{
    if (!notifySet)
        return;

    osdlock.lock();

    OSDSet *container = GetSet(notifySet->GetName());
    if (container)
    {
        container->ClearAllText();
        container->Hide();
        m_setsvisible = true;
        changed = true;
    }

    osdlock.unlock();
}

OSDListTreeType *OSD::ShowTreeMenu(const QString &name, 
                                   OSDGenericTree *treeToShow)
{
    if (runningTreeMenu || !treeToShow)
        return NULL;

    OSDListTreeType *rettree = NULL;

    osdlock.lock();

    OSDSet *container = GetSet(name);
    if (container)
    {
        rettree = (OSDListTreeType *)container->GetType("menu");
        if (rettree)
        {
            rettree->SetAsTree(treeToShow);
            rettree->SetVisible(true);
            runningTreeMenu = rettree;
            treeMenuContainer = name;
            container->Display();
            m_setsvisible = true;
            changed = true;
        }
    }

    osdlock.unlock();

    return rettree;
}

bool OSD::IsRunningTreeMenu(void)
{
    if (runningTreeMenu)
        return true;
    return false;
}

bool OSD::TreeMenuHandleKeypress(QKeyEvent *e)
{
    if (!runningTreeMenu)
        return false;

    bool ret = runningTreeMenu->HandleKeypress(e);

    osdlock.lock();

    if (!runningTreeMenu->IsVisible())
    {
        OSDSet *container = GetSet(treeMenuContainer);
        if (container)
            container->Hide();

        runningTreeMenu = NULL;
    }

    changed = true;

    osdlock.unlock();

    return ret;
}

bool OSD::IsSetDisplaying(const QString &name)
{
    OSDSet *oset = GetSet(name);
    return Visible() && (oset != NULL) && oset->Displaying();
}

bool OSD::HasSet(const QString &name)
{
    return setMap.contains(name);
}

QRect OSD::GetSubtitleBounds()
{
    return QRect(xoffset, yoffset, displaywidth, displayheight);
}
