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

#include <algorithm>
using namespace std;

#include "ttfont.h"
#include "osd.h"
#include "osdtypes.h"
#include "libmyth/settings.h"

OSD::OSD(int width, int height, int framerate, const QString &filename, 
         const QString &prefix, const QString &osdtheme)
{
    pthread_mutex_init(&osdlock, NULL);

    vid_width = width;
    vid_height = height;
    fps = framerate;

    editarrowleft = editarrowright = NULL;

    wmult = vid_width / 640.0;
    hmult = vid_height / 480.0;

    fontname = filename;
    fontprefix = prefix;
    m_setsvisible = false;
    setList = NULL;

    setList = new vector<OSDSet *>;

    themepath = FindTheme(osdtheme);

    if (themepath == "")
    {
        cerr << "Couldn't find osdtheme: " << osdtheme << endl;
        usingtheme = false;
    }
    else
    {
        themepath += "/";
        usingtheme = LoadTheme();
        if (!usingtheme)
        {
            cerr << "Couldn't load osdtheme: " << osdtheme << " at " 
                 << themepath << endl;
        }
    }

    SetNoThemeDefaults();
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

void OSD::SetNoThemeDefaults(void)
{
    QString name;

    TTFFont *chanfont = GetFont("channel_font");
    if (!chanfont)
    {
        name = "channel_font";
        int fontsize = 40;
        if (vid_width < 400)
            fontsize /= 2;

        chanfont = LoadFont(fontname, fontsize);
       
        if (chanfont) 
            fontMap[name] = chanfont;
    }
  
    if (!chanfont)
        return;

    OSDSet *container = GetSet("channel_number");
    if (!container)
    {
        QString name = "channel_number";
        container = new OSDSet(name, true, vid_width, vid_height, wmult, hmult);
        AddSet(container, name);

        QRect channumRect;

        channumRect.setTop(vid_height * 1 / 16);
        channumRect.setBottom(vid_height * 2 / 8);
        channumRect.setLeft(vid_width * 3 / 4);
        channumRect.setRight(vid_width * 15 * 16);

        OSDTypeText *chantext = new OSDTypeText(name, chanfont, "", 
                                                channumRect);
        container->AddType(chantext);
    }

    TTFFont *infofont = GetFont("info_font");
    if (!infofont)
    {
        name = "info_font";
        int fontsize = 16;
        if (vid_width < 400) 
            fontsize /= 2;

        infofont = LoadFont(fontname, fontsize);

        if (infofont)
            fontMap[name] = infofont;
    }

    container = GetSet("program_info");
    if (!container)
    {
        QString name = "program_info";
        container = new OSDSet(name, true, vid_width, vid_height, wmult, hmult);
        AddSet(container, name);

        name = "background";
        QRect rect;

        rect.setTop(vid_height * 9 / 16);
        rect.setBottom(vid_height * 7 / 8);
        rect.setLeft(vid_width / 8);
        rect.setRight(vid_width * 7 / 8);

        OSDTypeBox *box = new OSDTypeBox(name, rect);
        container->AddType(box);

        QRect tmprect = rect;
        tmprect.setTop(rect.top() + 5);
        tmprect.setBottom(rect.top() + infofont->Size() * 3 / 2);
        tmprect.setLeft(rect.left() + 5);

        name = "title";
        OSDTypeText *text = new OSDTypeText(name, infofont, "", tmprect);
        container->AddType(text);

        tmprect = rect;
        tmprect.setTop(rect.top() + infofont->Size() * 3 / 2 + 5);
        tmprect.setBottom(rect.bottom() - infofont->Size() * 3 / 2 + 5);
        tmprect.setLeft(rect.left() + 5);

        name = "subtitle";
        text = new OSDTypeText(name, infofont, "", tmprect);
        container->AddType(text);

        tmprect = rect;
        tmprect.setTop(rect.top() + infofont->Size() * 3 + 5);
        tmprect.setBottom(rect.bottom() - infofont->Size() * 3 / 2 + 5);
        tmprect.setLeft(rect.left() + 5);
    
        name = "description";
        text = new OSDTypeText(name, infofont, "", tmprect);
        text->SetMultiLine(true);
        container->AddType(text);
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

bool OSD::LoadTheme(void)
{
    QString themefile = themepath + "/osd.txt";

    QFile testfile(themefile);
    if (!testfile.exists())
        return false;

    Settings *settings = new Settings(themefile);
    QString name;
    OSDSet *container;

    QString bgname = settings->GetSetting("InfoBackground");
    if (bgname != "")
    {
        QString name = "program_info";
        container = new OSDSet(name, true, vid_width, vid_height, wmult, hmult);
        AddSet(container, name);

        name = "background";

        bgname = themepath + bgname;

        QPoint position = QPoint(0, 0);

        OSDTypeImage *image = new OSDTypeImage(name, bgname, position, wmult, 
                                               hmult);
        container->AddType(image);

        QString coords = settings->GetSetting("InfoPosition");
        position = parsePoint(coords);

        int imageheight = image->ImageSize().height();

        int x = position.x();
        int y = position.y();

        x += (int)(vid_width * 0.045);
        if (y == -1)
            y = (int)(vid_height * 0.95 - imageheight);

        position.setX(x);
        position.setY(y);

        image->SetPosition(position);

        int fontsize = settings->GetNumSetting("InfoTextFontSize");
        if (fontsize == 0)
            fontsize = 16;

        if (vid_width < 400)
            fontsize /= 2;

        name = "info_font";
        TTFFont *infofont = LoadFont(fontname, fontsize);

        if (infofont)
            fontMap[name] = infofont;
        else
        {
            delete settings;
            return false;
        }

        coords = settings->GetSetting("InfoIconPosition");
        if (coords.length() > 1) 
        {
            QPoint pos = parsePoint(coords);
            pos.setX((int)(x + pos.x() * wmult));
            pos.setY((int)(y + pos.y() * hmult));

            name = "callsign_image";
            OSDTypeImage *image = new OSDTypeImage(name, "", pos, 
                                                   wmult, hmult);
            container->AddType(image);
        }

        coords = settings->GetSetting("InfoCallSignRect");
        if (coords.length() > 1) 
        {
            QRect rect = parseRect(coords);
            normalizeRect(&rect);
            rect.moveBy(x, y);

            name = "callsign_text";
            OSDTypeText *text = new OSDTypeText(name, infofont, "", rect); 
            container->AddType(text);
        }

        coords = settings->GetSetting("InfoTimeRect");
        if (coords.length() > 1)
        {
            QRect rect = parseRect(coords);
            normalizeRect(&rect);
            rect.moveBy(x, y);

            name = "time";
            OSDTypeText *text = new OSDTypeText(name, infofont, "", rect);
            container->AddType(text);
        }

        timeFormat = settings->GetSetting("InfoTimeFormat");
        if (timeFormat == "")
            timeFormat = "h:mm ap";

        coords = settings->GetSetting("InfoTextBox");
        QRect infoRect = parseRect(coords);
        normalizeRect(&infoRect);
        infoRect.moveBy(x, y);

        QRect titleRect = QRect(0, 0, 0, 0);
        coords = settings->GetSetting("InfoTitleTextBox");
        if (coords != "")
        {
            titleRect = parseRect(coords);
            normalizeRect(&titleRect);
            titleRect.moveBy(x, y);
        }

        QRect tmprect = infoRect;

        if (titleRect.width() > 0)
            tmprect = titleRect;

        name = "title";
        OSDTypeText *text = new OSDTypeText(name, infofont, "", tmprect);
        container->AddType(text);

        if (titleRect.width() > 0)
            tmprect = infoRect;
        else
        {
            tmprect.setTop(tmprect.top() + infofont->Size() * 3 / 2);
            tmprect.setBottom(tmprect.bottom() - infofont->Size() * 3 / 2);
        }

        name = "subtitle";
        text = new OSDTypeText(name, infofont, "", tmprect);
        container->AddType(text);

        tmprect = infoRect;
        if (titleRect.width() > 0)
            tmprect.setTop(tmprect.top() + infofont->Size() * 3 / 2);
        else
            tmprect.setTop(tmprect.top() + infofont->Size() * 3);

        name = "description";
        text = new OSDTypeText(name, infofont, "", tmprect);
        text->SetMultiLine(true);
        container->AddType(text);
    }

    bgname = settings->GetSetting("SeekBackground");
    if (bgname != "")
    {  
        QString name = "status";
        container = new OSDSet(name, true, vid_width, vid_height, wmult, hmult);
        container->SetPriority(10);
        AddSet(container, name);
        
        container->SetFadeMovement(0, (int)(6 * hmult));
 
        bgname = themepath + bgname;

        QPoint position = QPoint(0, 0);

        OSDTypeImage *image = new OSDTypeImage(name, bgname, position, wmult,
                                               hmult);
        container->AddType(image);

        QString coords = settings->GetSetting("SeekPosition");
        position = parsePoint(coords);

        int x = position.x();
        int y = position.y();

        x += (int)(vid_width * 0.045);
        if (y == -1)
            y = (int)(vid_height * 0.95 - image->ImageSize().height());

        position.setX(x);
        position.setY(y);

        image->SetPosition(position);

        int fontsize = settings->GetNumSetting("SeekSliderFontSize");
        if (fontsize == 0)
            fontsize = 16;

        if (vid_width < 400)
            fontsize /= 2;

        name = "pause_font";
        TTFFont *pausefont = LoadFont(fontname, fontsize);

        if (pausefont)
            fontMap[name] = pausefont;

        coords = settings->GetSetting("SeekStatusRect");
        if (coords.length() > 1)
        {
            QRect rect = parseRect(coords);
            normalizeRect(&rect);
            rect.moveBy(x, y);

            TTFFont *infofont = GetFont("info_font");
            if (!infofont)
                infofont = pausefont;

            name = "status";
            OSDTypeText *text = new OSDTypeText(name, infofont, "", rect);
            container->AddType(text);
        }

        QRect pauseslider = parseRect(settings->GetSetting("SeekSliderRect"));
        normalizeRect(&pauseslider);
        pauseslider.moveBy(x, y);
                                                
        bgname = settings->GetSetting("SeekSliderNormal");
        if (bgname != "")
        {
            name = "status_slider";
            bgname = themepath + bgname;

            OSDTypeFillSlider *image = new OSDTypeFillSlider(name, bgname, 
                                                             pauseslider,
                                                             wmult, hmult);
            container->AddType(image);
        }

        coords = settings->GetSetting("SeekSliderTextRect");
        if (coords.length() > 1)
        {
            QRect rect = parseRect(coords);
            normalizeRect(&rect);
            rect.moveBy(x, y);

            name = "slidertext";
            OSDTypeText *text = new OSDTypeText(name, pausefont, "", rect);
            text->SetCentered(true);
            container->AddType(text);
        }
    }

    totalfadeframes = settings->GetNumSetting("FadeAwayFrames");

    if (settings->GetSetting("ChannelNumberRect") != "")
    {
        QString chanrect = settings->GetSetting("ChannelNumberRect");

        QRect channumRect = parseRect(chanrect);
        normalizeRect(&channumRect);
        channumRect.moveBy((int)(vid_width * 0.045), 
                           (int)(vid_height * 0.05));

        int channumfontsize = settings->GetNumSetting("ChannelNumberFontSize");
        if (!channumfontsize)
            channumfontsize = 40;

        if (vid_height < 400)
            channumfontsize /= 2;

        name = "channel_font";
        TTFFont *chanfont = LoadFont(fontname, channumfontsize);

        fontMap[name] = chanfont;

        name = "channel_number";
        OSDSet *channelnumber = new OSDSet(name, true, vid_width, vid_height,
                                           wmult, hmult);
        AddSet(channelnumber, name);

        OSDTypeText *chantext = new OSDTypeText(name, chanfont, "", 
                                                channumRect);
        channelnumber->AddType(chantext);
    }

    if (settings->GetSetting("EditSliderRect") != "")
    {
        QString esect = settings->GetSetting("EditSliderRect");

        QRect esRect = parseRect(esect);
        if (esRect.top() == -1)
            esRect.moveBy(0, vid_height - esRect.height());

        normalizeRect(&esRect);

        name = "editslider";
        OSDSet *set = new OSDSet(name, true, vid_width, vid_height, wmult, 
                                 hmult);
        set->SetPriority(2);
        AddSet(set, name);

        QString bluename = settings->GetSetting("EditSliderNormal");
        QString redname = settings->GetSetting("EditSliderDelete");

        bluename = themepath + bluename;
        redname = themepath + redname;

        OSDTypeEditSlider *tes = new OSDTypeEditSlider(name, bluename, redname,
                                                       esRect, wmult, hmult);
        set->AddType(tes);

        QString posname = settings->GetSetting("EditSliderPosition");
        posname = themepath + posname;

        QRect posRect = esRect;
        int yoff = settings->GetNumSetting("EditSliderYOffset");
        posRect.moveBy(0, (int)(yoff * hmult));

        name = "editposition";
        OSDTypePosSlider *pos = new OSDTypePosSlider(name, posname, posRect,
                                                     wmult, hmult);
        pos->SetPosition(0);
        set->AddType(pos);

        QString leftname = settings->GetSetting("EditSliderLeftArrow");
        QString rightname = settings->GetSetting("EditSliderRightArrow");

        leftname = themepath + leftname;
        rightname = themepath + rightname;

        name = "arrowimage";
        editarrowleft = new OSDTypeImage(name, leftname, QPoint(0, 0), 
                                         wmult, hmult);
        editarrowright = new OSDTypeImage(name, rightname, QPoint(0, 0),
                                          wmult, hmult);

        editarrowRect = esRect;
        editarrowRect.moveBy(0, -(editarrowleft->ImageSize().height()));
    }
    
    delete settings;

    return true;
}

void OSD::normalizeRect(QRect *rect)
{   
    rect->setWidth((int)(rect->width() * 0.91 * wmult));
    rect->setHeight((int)(rect->height() * hmult));
    rect->moveTopLeft(QPoint((int)(rect->left() * wmult),
                             (int)(rect->top() * hmult)));
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

void OSD::SetInfoText(const QString &text, const QString &subtitle,
                      const QString &desc, const QString &category,
                      const QString &start, const QString &end, 
                      const QString &callsign, const QString &iconpath,
                      int length)
{
    QString tmp = category;
    tmp = start;
    tmp = end;

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
        type = (OSDTypeText *)container->GetType("description");
        if (type)
            type->SetText(desc);
        type = (OSDTypeText *)container->GetType("callsign_text");
        if (type)
            type->SetText(callsign.left(5));
        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("callsign_image");
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
                      (OSDTypeFillSlider *)container->GetType("status_slider");
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
                      (OSDTypeFillSlider *)container->GetType("status_slider");
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

void OSD::NewDialogBox(const QString &name, const QString &message, 
                       const QString &optionone, const QString &optiontwo, 
                       const QString &optionthree, const QString &optionfour,
                       int length)
{
    pthread_mutex_lock(&osdlock);
    OSDSet *container = GetSet(name);
    if (container)
    {
        cerr << "dialog: " << name << " already exists.\n";
        return;
    }       

    TTFFont *font = GetFont("info_font");
    if (!font)
    {
        cerr << "no info_font defined, dead dialog\n";
        pthread_mutex_unlock(&osdlock);
        return;
    }

    int numoptions = 4;
    if (optionfour == "")
        numoptions = 3;
    if (optionthree == "")
        numoptions = 2;
    if (optiontwo == "")
        numoptions = 1;

    container = new OSDSet(name, false, vid_width, vid_height, wmult, hmult);
    container->SetPriority(0);
    container->SetAllowFade(false);
    container->SetFrameRate(fps);
    AddSet(container, name, false);

    QRect dialogRect;
    dialogRect.setTop(vid_height / 8);
    dialogRect.setBottom(vid_height * 7 / 8);
    dialogRect.setLeft(vid_width / 8);
    dialogRect.setRight(vid_width * 7 / 8);
    OSDTypeBox *box = new OSDTypeBox("background", dialogRect); 
    container->AddType(box);

    QRect rect = dialogRect;
    rect.setBottom(rect.bottom() - font->Size() * 2 * numoptions - 5);
    rect.setTop(rect.top() + 5);
    rect.setLeft(rect.left() + 5);
    rect.setRight(rect.right() - 5);
    OSDTypeText *text = new OSDTypeText("message", font, message, rect);
    text->SetMultiLine(true);
    container->AddType(text);

    for (int i = 1; i <= numoptions; i++)
    {
        int off = numoptions - i + 1;

        rect = dialogRect;
        rect.setTop(rect.bottom() - font->Size() * 2 * off + 5);
        rect.setBottom(rect.bottom() + font->Size() - 5);
        rect.setLeft(rect.left() + 5);
        rect.setRight(rect.right() - 5);

        QString name, option;
        switch (i)
        {
            case 1: name = "option1"; option = optionone; break;
            case 2: name = "option2"; option = optiontwo; break;
            case 3: name = "option3"; option = optionthree; break;
            case 4: name = "option4"; option = optionfour; break;
            default: name = "blah"; option = "error, unknown option"; break;
        }

        text = new OSDTypeText(name, font, option, rect);
        container->AddType(text);
    }
 
    OSDTypePositionRectangle *opr = new OSDTypePositionRectangle("selector");
    container->AddType(opr);

    for (int i = numoptions; i > 0; i--)
    {
        rect = dialogRect;
        rect.setTop(rect.bottom() - font->Size() * 2 * i - 2);
        rect.setBottom(rect.bottom() - font->Size() * 2 * (i - 1) - 2);
        opr->AddPosition(rect);
    }

    opr->SetPosition(0);
    dialogResponseList[name] = 0;

    if (length > 0)
        container->DisplayFor(length * fps);
    else
        container->Display();
    m_setsvisible = true;

    pthread_mutex_unlock(&osdlock);
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
        OSDTypePositionRectangle *type;
        type = (OSDTypePositionRectangle *)container->GetType("selector");
        if (type)
        {
            type->PositionUp();
            dialogResponseList[name] = type->GetPosition();
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
        OSDTypePositionRectangle *type;
        type = (OSDTypePositionRectangle *)container->GetType("selector");
        if (type)
        {
            type->PositionDown();
            dialogResponseList[name] = type->GetPosition();
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
    sprintf(name, "%lld", number);

    int xpos = (int)((editarrowRect.width() * 1.0 / totalframes) * number);
    xpos = editarrowRect.left() + xpos;
    int ypos = editarrowRect.top();

    pthread_mutex_lock(&osdlock);

    OSDSet *set = new OSDSet(name, false, vid_width, vid_height, wmult, hmult);
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

void OSD::HideEditArrow(long long number)
{
    char name[128];
    sprintf(name, "%lld", number);

    pthread_mutex_lock(&osdlock);
    OSDSet *set = GetSet(name);
    if (set)
        set->Hide();
    pthread_mutex_unlock(&osdlock);
}

OSDSet *OSD::ShowText(const QString &name, const QString &message, int xpos,
                      int ypos, int width, int height, int secs, int color)
{
    pthread_mutex_lock(&osdlock);

    OSDSet *set = GetSet(name);
    if (set)
    {
        OSDTypeText *text = (OSDTypeText *)set->GetType("message");
        if (text)
            text->SetText(message);

        if (secs > 0)
            set->DisplayFor(secs * fps);
        else
            set->Display();
        m_setsvisible = true;
        pthread_mutex_unlock(&osdlock);
        return set;
    }

    set = new OSDSet(name, false, vid_width, vid_height, wmult, hmult);
    set->SetPriority(3);
    set->SetFrameRate(fps);
    AddSet(set, name, false);

    TTFFont *font = GetFont("channel_font");
    if (!font)
    {
        delete set;
        pthread_mutex_unlock(&osdlock);
        return NULL;
    }

    QRect rect = QRect(xpos, ypos, width, height); 
    OSDTypeText *text = new OSDTypeText("message", font, message, rect);
    text->SetMultiLine(true);
    text->SetColor(color);
    set->AddType(text);

    if (secs > 0)
        set->DisplayFor(secs * fps);
    else
    {
        set->SetAllowFade(false);
        set->Display();
    }

    m_setsvisible = true;

    pthread_mutex_unlock(&osdlock);
    return set;
}

void OSD::HideText(const QString &name)
{
    pthread_mutex_lock(&osdlock);

    OSDSet *set = GetSet(name);
    if (set)
    {
        set->Hide();
    }
    pthread_mutex_unlock(&osdlock);
}

void OSD::DoEditSlider(QMap<long long, int> deleteMap, long long curFrame,
                       long long totalFrames)
{
    pthread_mutex_lock(&osdlock);

    QString name = "editslider";
    OSDSet *set = GetSet(name);
    if (set)
    {
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
