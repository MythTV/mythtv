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

#include "yuv2rgb.h"
#include "ttfont.h"
#include "osd.h"
#include "settings.h"

class OSDImage
{
  public:
    OSDImage(QString pathname, float hmult, float wmult, bool scaletoscreen);
   ~OSDImage();
   
    QString filename;

    bool isvalid;

    unsigned char *yuv;
    unsigned char *ybuffer;
    unsigned char *ubuffer;
    unsigned char *vbuffer;

    unsigned char *alpha;

    int width;
    int height;

    QPoint position;

  private:
    void AddAlpha(QImage *image);
};

OSDImage::OSDImage(QString pathname, float hmult, float wmult, 
                   bool scaletoscreen)
{
    filename = pathname;

    QImage tmpimage(pathname);

    yuv = alpha = NULL;
    isvalid = false;

    if (tmpimage.width() == 0 || tmpimage.height() == 0)
        return;

    float mult = wmult;
    if (scaletoscreen)
        mult *= 0.91;

    width = ((int)(tmpimage.width() * mult) / 2) * 2;
    height = ((int)(tmpimage.height() * hmult) / 2) * 2;

    QImage tmp2 = tmpimage.smoothScale(width, height);  

    isvalid = true;
    yuv = new unsigned char[width * height * 3 / 2];

    ybuffer = yuv;
    ubuffer = yuv + (width * height);
    vbuffer = yuv + (width * height * 5 / 4);

    alpha = new unsigned char[width * height]; 
            
    rgb32_to_yuv420p(ybuffer, ubuffer, vbuffer, alpha, 
                     tmp2.bits(), width, height, tmp2.width());
}

OSDImage::~OSDImage()
{
    if (yuv)
        delete [] yuv;
    if (alpha)
        delete [] alpha;
}

OSD::OSD(int width, int height, const QString &filename, const QString &prefix,
         const QString &osdtheme)
{
    vid_width = width;
    vid_height = height;

    wmult = vid_width / 640.0;
    hmult = vid_height / 480.0;

    fontname = filename;
    fontprefix = prefix;

    displayframes = 0;
    show_info = false;
    show_channum = false;
    show_dialog = false;
    show_pause = false;

    currentdialogoption = 1;

    infotext = channumtext = "";
    infoicon = infobackground = NULL;

    pausesliderfont = NULL;
    usepause = false;
    pausesliderfill = pausesliderdelete = pausebackground = NULL;
    pausesliderpos = NULL;   
    pausesliderfontsize = 0;
 
    SetNoThemeDefaults();

    themepath = FindTheme(osdtheme);

    if (themepath == "")
    {
        usingtheme = false;
    }
    else
    {
        themepath += "/";
        usingtheme = LoadTheme();
    }

    if (vid_width < 400 || vid_height < 400)
    {
        infofontsize /= 2;
        channumfontsize /= 2;
        pausesliderfontsize /= 2;
    }
  
    enableosd = false;
 
    info_font = LoadFont(fontname, infofontsize);
    channum_font = LoadFont(fontname, channumfontsize);

    if (!info_font || !channum_font)
    {
        printf("Coudln't open the OSD font, disabling the OSD.\n");
        return;
    }

    if (pausesliderfontsize > 0)
        pausesliderfont = LoadFont(fontname, pausesliderfontsize);

    enableosd = true;

    int mwidth, twidth;
    Efont_extents(info_font, "M M", NULL, NULL, &twidth, NULL, NULL, NULL, 
                  NULL);
    Efont_extents(info_font, "M", NULL, NULL, &mwidth, NULL, NULL, NULL, NULL);

    space_width = twidth - (mwidth * 2);
}

OSD::~OSD(void)
{
    if (info_font)
        Efont_free(info_font);
    if (channum_font)
        Efont_free(channum_font);
    if (pausesliderfont)
        Efont_free(pausesliderfont);
    if (infobackground)
        delete infobackground;
    if (infoicon)
        delete infoicon;
    if (pausebackground)
        delete pausebackground;
    if (pausesliderfill)
        delete pausesliderfill;
    if (pausesliderdelete)
        delete pausesliderdelete;
    if (pausesliderpos)
        delete pausesliderpos;
}

void OSD::SetNoThemeDefaults(void)
{
    infoRect.setTop(vid_height * 5 / 8);
    infoRect.setBottom(vid_height * 15 / 16);
    infoRect.setLeft(vid_width / 8);
    infoRect.setRight(vid_width * 7 / 8);
    infobackground = NULL;

    channumRect.setTop(vid_height * 1 / 16);
    channumRect.setBottom(vid_height * 2 / 8);
    channumRect.setLeft(vid_width * 3 / 4);
    channumRect.setRight(vid_width * 15 * 16);

    dialogRect.setTop(vid_height / 8);
    dialogRect.setBottom(vid_height * 7 / 8);
    dialogRect.setLeft(vid_width / 8);
    dialogRect.setRight(vid_width * 7 / 8);

    infofontsize = 16;
    channumfontsize = 40;

    usepause = false;
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

Efont *OSD::LoadFont(QString name, int size)
{
    char *home = getenv("HOME");
    QString fullname = QString(home) + "/.mythtv/" + name;
    Efont *font = Efont_load((char *)fullname.ascii(), size, vid_width,
                             vid_height);

    if (font)
        return font;

    fullname = fontprefix + "/share/mythtv/" + name;

    font = Efont_load((char *)fullname.ascii(), size, vid_width,
                       vid_height);

    if (font)
        return font;

    if (themepath != "")
    {
        fullname = themepath + "/" + name;
        font = Efont_load((char *)fullname.ascii(), size, vid_width,
                          vid_height);
        if (font)
            return font;
    }

    fullname = name;
    font = Efont_load((char *)fullname.ascii(), size,
                           vid_width, vid_height);

    return font;
}

bool OSD::LoadTheme(void)
{
    QString themefile = themepath + "/osd.txt";

    QFile testfile(themefile);
    if (!testfile.exists())
        return false;

    Settings *settings = new Settings(themefile);

    QString bgname = settings->GetSetting("InfoBackground");
    if (bgname != "")
    {
        bgname = themepath + bgname;
        infobackground = new OSDImage(bgname, hmult, wmult, true);

        QString coords = settings->GetSetting("InfoPosition");
        infobackground->position = parsePoint(coords);

        int x = infobackground->position.x();
        int y = infobackground->position.y();

        x += (int)(vid_width * 0.045);
        if (y == -1)
            y = (int)(vid_height * 0.95 - infobackground->height);

        infobackground->position.setX(x);
        infobackground->position.setY(y);

        coords = settings->GetSetting("InfoTextBox");
        infoRect = parseRect(coords);
        normalizeRect(&infoRect);
        infoRect.moveBy(x, y);
	
        int fontsize = settings->GetNumSetting("InfoTextFontSize");
        if (fontsize > 0)
            infofontsize = fontsize;

        useinfoicon = false;
        coords = settings->GetSetting("InfoIconPosition");
        if (coords.length() > 1)
        {
            infoiconpos = parsePoint(coords);
            infoiconpos.setX(x + infoiconpos.x() * wmult);
            infoiconpos.setY(y + infoiconpos.y() * hmult);
            useinfoicon = true;
        }

        callsignRect = QRect(0, 0, 0, 0);
        coords = settings->GetSetting("InfoCallSignRect");
        if (coords.length() > 1) 
        {
            callsignRect = parseRect(coords);
            normalizeRect(&callsignRect);
            callsignRect.moveBy(x, y);
        }
    }

    bgname = settings->GetSetting("SeekBackground");
    if (bgname != "")
    {  
        usepause = true;
 
        bgname = themepath + bgname;
        pausebackground = new OSDImage(bgname, hmult, wmult, true);

        QString coords = settings->GetSetting("SeekPosition");
        pausebackground->position = parsePoint(coords);

        int x = pausebackground->position.x();
        int y = pausebackground->position.y();

        x += (int)(vid_width * 0.045);
        if (y == -1)
            y = (int)(vid_height * 0.95 - pausebackground->height);

        pausebackground->position.setX(x);
        pausebackground->position.setY(y);

        pausestatusRect = QRect(0, 0, 0, 0);
        coords = settings->GetSetting("SeekStatusRect");
        if (coords.length() > 1)
        {
            pausestatusRect = parseRect(coords);
            normalizeRect(&pausestatusRect);
            pausestatusRect.moveBy(x, y);
        }

        bgname = settings->GetSetting("SeekSliderNormal");
        if (bgname != "")
        {
            bgname = themepath + bgname;
            pausesliderfill = new OSDImage(bgname, hmult, wmult, true);
 
            pausesliderRect = parseRect(settings->GetSetting("SeekSliderRect"));
            normalizeRect(&pausesliderRect);
            pausesliderRect.moveBy(x, y);
        }

        bgname = settings->GetSetting("SeekSliderPosition");
        if (bgname != "")
        {
            bgname = themepath + bgname;
            pausesliderpos = new OSDImage(bgname, hmult, wmult, true);
        }

        pausesliderfontsize = settings->GetNumSetting("SeekSliderFontSize");
    }
 
    return true;
}

void OSD::normalizeRect(QRect *rect)
{
    rect->setWidth(rect->width() * 0.91 * wmult);
    rect->setHeight(rect->height() * hmult);
    rect->moveTopLeft(QPoint(rect->left() * wmult, rect->top() * hmult));
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
    displayframes = time(NULL) + length;
    show_info = true;

    infotext = text;
    subtitletext = subtitle;
    desctext = desc;

    QString dummy = category;
    dummy = start;
    dummy = end;

    infocallsign = callsign.left(5);

    if (useinfoicon)
    {
        if (infoicon)
        {
            if (iconpath != infoicon->filename)
            {
                delete infoicon;
                infoicon = new OSDImage(iconpath, hmult, wmult, false);
            }
        }
        else
        {
            infoicon = new OSDImage(iconpath, hmult, wmult, false);
        }
    }
}

void OSD::StartPause(int position, bool fill, QString msgtext,
                     QString slidertext, int displaytime)
{
    pausestatus = msgtext;
    pauseposition = position;
    pauseslidertext = slidertext;
    pausefilltype = fill; 
    displaypausetime = -1;
    if (displaytime > 0)
        displaypausetime = time(NULL) + displaytime;
    
    show_pause = true;
}

void OSD::UpdatePause(int position, QString slidertext)
{
    pauseposition = position;
    pauseslidertext = slidertext;
}

void OSD::EndPause(void)
{
    show_pause = false;
    displayframes = 0;
}

void OSD::SetChannumText(const QString &text, int length)
{
    displayframes = time(NULL) + length;
    show_channum = true;

    channumtext = text;
}

void OSD::DialogUp(void)
{
    if (currentdialogoption > 1)
        currentdialogoption--;
}

void OSD::DialogDown(void)
{
    if (currentdialogoption < 3)
        currentdialogoption++;
}

void OSD::SetDialogBox(const QString &message, const QString &optionone,
                       const QString &optiontwo, const QString &optionthree,
                       int length)
{
    currentdialogoption = 1;
    dialogmessagetext = message;
    dialogoptionone = optionone;
    dialogoptiontwo = optiontwo;
    dialogoptionthree = optionthree;
    
    displayframes = time(NULL) + length;
    show_dialog = true;
}

void OSD::ShowLast(int length)
{
    displayframes = time(NULL) + length;
    show_channum = true;
    show_info = true;
}

void OSD::TurnOff(void)
{
    displayframes = 0;
}

void OSD::Display(unsigned char *yuvptr)
{
    if (!enableosd)
        return;

    if (show_pause)
    {
        if (displaypausetime > 0 && time(NULL) > displaypausetime)
            show_pause = false;

        DisplayPause(yuvptr);
    }

    if (time(NULL) > displayframes)
    {
        show_info = false; 
	show_channum = false;
        show_dialog = false;
    	return;
    }

    if (show_dialog)
    {
        DisplayDialogNoTheme(yuvptr);
        return;
    }

    if (show_info)
    {
        DisplayInfo(yuvptr);
    }

    if (show_channum)
    {
        DisplayChannumNoTheme(yuvptr);
    }
}

void OSD::DisplayDialogNoTheme(unsigned char *yuvptr)
{
    DarkenBox(dialogRect, yuvptr);

    QRect rect = dialogRect;
    rect.setBottom(rect.bottom() - infofontsize * 2 * 3);
    DrawStringIntoBox(rect, dialogmessagetext, yuvptr);

    rect = dialogRect;
    rect.setTop(rect.bottom() - infofontsize * 2 * 3);
    rect.setBottom(rect.bottom() - infofontsize * (3 / 2) * 2);
    DrawStringIntoBox(rect, dialogoptionone, yuvptr);

    rect = dialogRect;
    rect.setTop(rect.bottom() - infofontsize * 2 * 2);
    rect.setBottom(rect.bottom() - infofontsize / 2);
    DrawStringIntoBox(rect, dialogoptiontwo, yuvptr);

    rect = dialogRect;
    rect.setTop(rect.bottom() - infofontsize * 2);
    rect.setBottom(rect.bottom() + infofontsize);
    DrawStringIntoBox(rect, dialogoptionthree, yuvptr);

    rect = dialogRect;
    if (currentdialogoption == 1)
    {
        rect.setTop(rect.bottom() - infofontsize * 2 * 3 - 2);
        rect.setBottom(rect.bottom() - infofontsize * 2 * 2 - 2);
    }
    else if (currentdialogoption == 2)
    {
        rect.setTop(rect.bottom() - infofontsize * 2 * 2 - 2);
        rect.setBottom(rect.bottom() - infofontsize * 2 - 2);
    }
    else if (currentdialogoption == 3)
    {
        rect.setTop(rect.bottom() - infofontsize * 2 - 2);
        rect.setBottom(rect.bottom() - 2);
    }
    DrawRectangle(rect, yuvptr);
}

void OSD::DisplayInfo(unsigned char *yuvptr)
{
    if (infobackground)
    {
        BlendImage(infobackground, infobackground->position.x(),
                   infobackground->position.y(), yuvptr);
        if (useinfoicon && infoicon)
            BlendImage(infoicon, infoiconpos.x(), infoiconpos.y(), yuvptr);
        if (callsignRect.width() > 0)
            DrawStringWithOutline(yuvptr, callsignRect, infocallsign, 
                                  info_font);
    }
    else
    {
        DarkenBox(infoRect, yuvptr);
    }

    QRect rect = infoRect;
    rect.setLeft(rect.left() + 5);
    rect.setRight(rect.right() - 5);
    rect.setTop(rect.top() + 5);
    rect.setBottom(rect.bottom() - 5);
    DrawStringWithOutline(yuvptr, rect, infotext, info_font);
 
    rect.setTop(rect.top() + infofontsize * 3 / 2);
    rect.setBottom(rect.bottom() - infofontsize * 3 / 2);
    DrawStringWithOutline(yuvptr, rect, subtitletext, info_font);

    rect = infoRect;
    rect.setTop(rect.top() + infofontsize * 3);
    DrawStringIntoBox(rect, desctext, yuvptr);
}

void OSD::DisplayChannumNoTheme(unsigned char *yuvptr)
{
    DrawStringWithOutline(yuvptr, channumRect, channumtext, channum_font, true);
}

void OSD::DisplayPause(unsigned char *yuvptr)
{
    if (pausebackground)
    {
        BlendImage(pausebackground, pausebackground->position.x(),
                   pausebackground->position.y(), yuvptr);
    }
 
    if (pausestatusRect.width() > 0)
    {
        DrawStringWithOutline(yuvptr, pausestatusRect, pausestatus,
                              info_font);
    }

    if (pausesliderRect.width() > 0)
    {
        if (pausefilltype || !pausesliderpos)
        {
            int width = (int)((pausesliderRect.width() / 1000.0) * 
                        pauseposition);
            if (width > pausesliderRect.width())
                width = pausesliderRect.width();
	    BlendFillSlider(pausesliderfill, pausesliderRect.x(),
                            pausesliderRect.y(), width, yuvptr);
        }
        else
        {
            int xpos = (int)((pausesliderRect.width() / 1000.0) *
                       pauseposition);
            xpos += pausesliderRect.left();
            BlendImage(pausesliderpos, xpos, pausesliderRect.y() - 3, yuvptr);
        }
    }

    if (pausesliderfont)
    {
        DrawStringCentered(yuvptr, pausesliderRect, pauseslidertext,
                           pausesliderfont);
    }
}

void OSD::DrawStringCentered(unsigned char *yuvptr, QRect rect,
                             const QString &text, Efont *font)
{
    int textlength = 0;
    Efont_extents(info_font, text, NULL, NULL, &textlength, NULL, NULL, NULL,
                  NULL);

    int xoffset = (rect.width() - textlength) / 2;

    if (xoffset > 0)
        rect.setLeft(rect.left() + xoffset);

    DrawStringWithOutline(yuvptr, rect, text, font);
}

void OSD::DrawStringWithOutline(unsigned char *yuvptr, QRect rect, 
                                const QString &text, Efont *font,
                                bool rightjustify)
{
    int x = rect.left();
    int y = rect.top();
    int maxx = rect.right();
    int maxy = rect.bottom();

    int i;
    for (i = x - 1; i <= x + 1; i++)
        EFont_draw_string(yuvptr, i, y - 1, text, font, maxx, maxy, false,
                          rightjustify);

    for (i = y - 1; i <= y + 1; i++)
        EFont_draw_string(yuvptr, x + 1, i, text, font, maxx, maxy, false,
                          rightjustify);

    for (i = x + 1; i >= x - 1; i--)
        EFont_draw_string(yuvptr, i, y + 1, text, font, maxx, maxy, false,
                          rightjustify);

    for (i = y + 1; i >= y - 1; i--)
        EFont_draw_string(yuvptr, x - 1, i, text, font, maxx, maxy, false,
                          rightjustify);

    EFont_draw_string(yuvptr, x, y, text, font, maxx, maxy, true, rightjustify);
}    

void OSD::DrawStringIntoBox(QRect rect, const QString &text, 
                            unsigned char *screen)
{
    int textlength = 0;
    Efont_extents(info_font, text, NULL, NULL, &textlength, NULL, NULL, NULL,
                  NULL);

    rect.setLeft(rect.left() + 5);
    rect.setRight(rect.right() - 5);
    rect.setTop(rect.top() + 5); 
    rect.setBottom(rect.bottom() - 5);

    int maxlength = rect.width();
    if (textlength > maxlength)
    {
        char *orig = strdup((char *)text.ascii());
        int length = 0;
        int lines = 0;
        char line[512];
        memset(line, '\0', 512);

        char *word = strtok(orig, " ");
        while (word)
        {
            if (word[0] == '%') 
            {
                if (word[1] == 'd')
                {
                    int timeleft = displayframes - time(NULL);
                    if (timeleft > 99) 
                        timeleft = 99;
                    if (timeleft < 0)
                        timeleft = 0;

                    sprintf(word, "%d", timeleft);
                }
            }
            Efont_extents(info_font, word, NULL, NULL, &textlength,
                          NULL, NULL, NULL, NULL);
            if (textlength + space_width + length > maxlength)
            {
                QRect drawrect = rect;
                drawrect.setTop(rect.top() + infofontsize * (lines) * 3 / 2);
                DrawStringWithOutline(screen, drawrect, line, info_font);
                length = 0;
                memset(line, '\0', 512);
                lines++;
            }
            if (length == 0)
            {
                length = textlength;
                strcpy(line, word);
            }
            else
            {
                length += textlength + space_width;
                strcat(line, " ");
                strcat(line, word);
            }
            word = strtok(NULL, " ");
        }
        QRect drawrect = rect;
        drawrect.setTop(rect.top() + infofontsize * (lines) * 3 / 2);
        DrawStringWithOutline(screen, drawrect, line, info_font);
        free(orig);
    }
    else
    {
        DrawStringWithOutline(screen, rect, text, info_font);
    }
}

void OSD::DarkenBox(QRect &rect, unsigned char *screen)
{
    unsigned char *src;
    char c = -128;

    int ystart = rect.top();
    int yend = rect.bottom();
    int xstart = rect.left();
    int xend = rect.right();

    for (int y = ystart; y < yend; y++)
    {
        for (int x = xstart; x < xend; x++)
        {
            src = screen + x + y * vid_width;
            *src = ((*src * c) >> 8) + *src;
        }
    }
}

void OSD::DrawRectangle(QRect &rect, unsigned char *screen)
{
    unsigned char *src;

    int ystart = rect.top();
    int yend = rect.bottom();
    int xstart = rect.left();
    int xend = rect.right();

    for (int y = ystart; y < yend; y++)
    {
        for (int x = xstart; x < xstart + 2; x++)
        {
            src = screen + x + y * vid_width;
            *src = 255;
        }
        for (int x = xend - 2; x < xend; x++)
        {
            src = screen + x + y * vid_width;
            *src = 255;
        }
    }

    for (int x = xstart; x < xend; x++)
    {
        for (int y = ystart; y < ystart + 2; y++)
        {
            src = screen + x + y * vid_width;
            *src = 255;
        }
        for (int y = yend - 2; y < yend; y++)
        {
            src = screen + x + y * vid_width;
            *src = 255;
        }
    }
}

void OSD::BlendImage(OSDImage *image, int xstart, int ystart, 
                     unsigned char *screen)
{
    if (!image->isvalid)
        return;

    unsigned char *dest, *src;
    int alpha, tmp1, tmp2;

    int width = image->width;
    int height = image->height;

    int ysrcwidth;
    int ydestwidth;

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * width;
        ydestwidth = (y + ystart) * vid_width;

        for (int x = 0; x < width; x++)
        {
            alpha = *(image->alpha + x + ysrcwidth);
            if (alpha == 0)
                continue;

	    dest = screen + x + xstart + ydestwidth;
            src = image->ybuffer + x + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }

    width /= 2;
    height /= 2;
  
    ystart /= 2;
    xstart /= 2;

    unsigned char *destuptr = screen + (vid_width * vid_height);
    unsigned char *destvptr = screen + (vid_width * vid_height * 5 / 4);

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * width;
        ydestwidth = (y + ystart) * (vid_width / 2);

        for (int x = 0; x < width; x++)
        {
            alpha = *(image->alpha + x * 2 + y * 2 * image->width);

	    if (alpha == 0)
		continue;
            dest = destuptr + x + xstart + ydestwidth;
            src = image->ubuffer + x + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + x + xstart + ydestwidth;
            src = image->vbuffer + x + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }
}

void OSD::BlendFillSlider(OSDImage *image, int xstart, int ystart,
                          int drawwidth, unsigned char *screen)
{
    if (!image->isvalid)
        return;

    unsigned char *dest, *src;
    int alpha, tmp1, tmp2;

    int width = drawwidth;
    int height = image->height;

    int ysrcwidth;
    int ydestwidth;

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * image->width;
        ydestwidth = (y + ystart) * vid_width;

        for (int x = 0; x < width; x++)
        {
            alpha = *(image->alpha + ysrcwidth);
            if (alpha == 0)
                continue;

            dest = screen + x + xstart + ydestwidth;
            src = image->ybuffer + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }  

    width /= 2;
    height /= 2;

    ystart /= 2;
    xstart /= 2;

    unsigned char *destuptr = screen + (vid_width * vid_height);
    unsigned char *destvptr = screen + (vid_width * vid_height * 5 / 4);

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * (image->width / 2);
        ydestwidth = (y + ystart) * (vid_width / 2);

        for (int x = 0; x < width; x++)
        {
            alpha = *(image->alpha + y * 2 * image->width);

            if (alpha == 0)
                continue;
            dest = destuptr + x + xstart + ydestwidth;
            src = image->ubuffer + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + x + xstart + ydestwidth;
            src = image->vbuffer + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }
}

