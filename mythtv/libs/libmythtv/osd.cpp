#include <qstring.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <qimage.h>

#include "yuv2rgb.h"
#include "ttfont.h"
#include "osd.h"

OSDImage::OSDImage(QString pathname)
{
    QImage tmpimage(pathname);
    QImage tmp2 = tmpimage.smoothScale(tmpimage.width() * 0.91,  
                                       tmpimage.height());
    width = (tmp2.width() / 2) * 2;
    height = (tmp2.height() / 2) * 2;
    yuv = new unsigned char[width * height * 3 / 2];

    ybuffer = yuv;
    ubuffer = yuv + (width * height);
    vbuffer = yuv + (width * height * 5 / 4);

    alpha = new unsigned char[width * height]; 
            
    rgb32_to_yuv420p(ybuffer, ubuffer, vbuffer, alpha, 
                     tmp2.bits(), width, height);
}

OSDImage::~OSDImage()
{
    delete [] yuv;
    delete [] alpha;
}

OSD::OSD(int width, int height, const QString &filename, const QString &prefix)
{
    vid_width = width;
    vid_height = height;

    fontname = filename;
    fontprefix = prefix;

    displayframes = 0;
    show_info = false;
    show_channum = false;
    show_dialog = false;

    currentdialogoption = 1;

    infotext = channumtext = "";

    usingtheme = false;
    SetNoThemeDefaults();

    if (vid_width < 400 || vid_height < 400)
    {
        infofontsize /= 2;
        channumfontsize /= 2;
    }
  
    enableosd = false;
 
    info_font = LoadFont(fontname, infofontsize);
    channum_font = LoadFont(fontname, channumfontsize);

    if (!info_font || !channum_font)
    {
        printf("Coudln't open the OSD font, disabling the OSD.\n");
        return;
    }

    enableosd = true;

    int mwidth, twidth;
    Efont_extents(info_font, "M M", NULL, NULL, &twidth, NULL, NULL, NULL, 
                  NULL);
    Efont_extents(info_font, "M", NULL, NULL, &mwidth, NULL, NULL, NULL, NULL);

    space_width = twidth - (mwidth * 2);

    channelbackground = NULL;
}

OSD::~OSD(void)
{
    if (info_font)
        Efont_free(info_font);
    if (channum_font)
        Efont_free(channum_font);
    if (channelbackground)
        delete channelbackground;
}

void OSD::SetNoThemeDefaults(void)
{
    info_y_start = vid_height * 5 / 8;
    info_y_end = vid_height * 15 / 16;
    info_x_start = vid_width / 8;
    info_x_end = vid_width * 7 / 8;

    info_width = info_x_end - info_x_start;
    info_height = info_y_end - info_y_start;

    channum_y_start = vid_height * 1 / 16;
    channum_y_end = vid_height * 2 / 8;
    channum_x_start = vid_width * 3 / 4;
    channum_x_end = vid_width * 15 * 16;

    channum_width = channum_x_end - channum_x_start;
    channum_height = channum_y_end - channum_y_start;

    dialog_y_start = vid_height / 8;
    dialog_y_end = vid_height * 7 / 8;
    dialog_x_start = vid_width / 8;
    dialog_x_end = vid_width * 7 / 8;

    infofontsize = 16;
    channumfontsize = 40;
}

Efont *OSD::LoadFont(QString name, int size)
{
    QString fullname = fontprefix + "/share/mythtv/" + name;

    Efont *font = Efont_load((char *)fullname.ascii(), size, vid_width,
                             vid_height);
    if (!info_font)
    {
        fullname = name;
        info_font = Efont_load((char *)fullname.ascii(), size,
                               vid_width, vid_height);
    }

    return font;
}

void OSD::SetInfoText(const QString &text, const QString &subtitle,
                      const QString &desc, const QString &category,
                      const QString &start, const QString &end, int length)
{
    displayframes = time(NULL) + length;
    show_info = true;

    infotext = text;
    subtitletext = subtitle;
    desctext = desc;

    QString dummy = category;
    dummy = start;
    dummy = end;
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
        DisplayInfoNoTheme(yuvptr);
    }

    if (show_channum)
    {
        DisplayChannumNoTheme(yuvptr);
    }
}

void OSD::DisplayDialogNoTheme(unsigned char *yuvptr)
{
    DarkenBox(dialog_x_start, dialog_y_start, dialog_x_end, dialog_y_end,
              yuvptr);
    DrawStringIntoBox(dialog_x_start, dialog_y_start, dialog_x_end,
                      dialog_y_end - infofontsize * 2 * 3,
                      dialogmessagetext, yuvptr);
    DrawStringIntoBox(dialog_x_start, dialog_y_end -
                      infofontsize * 2 * 3, dialog_x_end,
                      dialog_y_end - infofontsize * (3 / 2) * 2,
                      dialogoptionone, yuvptr);
    DrawStringIntoBox(dialog_x_start, dialog_y_end
                      - infofontsize * 2 * 2,
                      dialog_x_end, dialog_y_end - infofontsize / 2,
                      dialogoptiontwo, yuvptr);
    DrawStringIntoBox(dialog_x_start, dialog_y_end - infofontsize * 2,
                      dialog_x_end, dialog_y_end + infofontsize,
                      dialogoptionthree, yuvptr);

    if (currentdialogoption == 1)
    {
        DrawRectangle(dialog_x_start, dialog_y_end -
                      infofontsize * 2 * 3 - 2, dialog_x_end,
                      dialog_y_end - infofontsize * 2 * 2 - 2, yuvptr);
    }
    else if (currentdialogoption == 2)
    {
        DrawRectangle(dialog_x_start, dialog_y_end -
                      infofontsize * 2 * 2 - 2, dialog_x_end,
                      dialog_y_end - infofontsize * 2 - 2, yuvptr);
    }
    else if (currentdialogoption == 3)
    {
        DrawRectangle(dialog_x_start, dialog_y_end -
                      infofontsize * 2 - 2, dialog_x_end,
                      dialog_y_end - 2, yuvptr);
    }
}

void OSD::DisplayInfoNoTheme(unsigned char *yuvptr)
{
    DarkenBox(info_x_start, info_y_start, info_x_end, info_y_end, yuvptr);

    DrawStringWithOutline(yuvptr, info_x_start + 5, info_y_start + 5,
                          infotext, info_font, info_x_end - 5,
                          info_y_end - 5);
    DrawStringWithOutline(yuvptr, info_x_start + 5, info_y_start + 5 +
                          infofontsize * 3 / 2, subtitletext, info_font,
                          info_x_end - 5,
                          info_y_end - 5 - infofontsize * 3 / 2);
    DrawStringIntoBox(info_x_start, info_y_start + infofontsize * 3,
                      info_x_end, info_y_end, desctext, yuvptr);
}

void OSD::DisplayChannumNoTheme(unsigned char *yuvptr)
{
    DrawStringWithOutline(yuvptr, channum_x_start, channum_y_start,
                          channumtext, channum_font, channum_x_end,
                          channum_y_end);
}

void OSD::DrawStringWithOutline(unsigned char *yuvptr, int x, int y, 
                                const QString &text, Efont *font, int maxx,
                                int maxy, bool rightjustify)
{
    EFont_draw_string(yuvptr, x - 1, y - 1, text, font, maxx, maxy, false,
                      rightjustify);
    EFont_draw_string(yuvptr, x + 1, y + 1, text, font, maxx, maxy, false,
                      rightjustify);
    EFont_draw_string(yuvptr, x + 1, y - 1, text, font, maxx, maxy, false,
                      rightjustify);
    EFont_draw_string(yuvptr, x - 1, y + 1, text, font, maxx, maxy, false,
                      rightjustify);
    EFont_draw_string(yuvptr, x, y, text, font, maxx, maxy, true, rightjustify);
}    

void OSD::DrawStringIntoBox(int xstart, int ystart, int xend, int yend,
                            const QString &text, unsigned char *screen)
{
    int textlength = 0;
    Efont_extents(info_font, text, NULL, NULL, &textlength, NULL, NULL, NULL,
                  NULL);

    int maxlength = (xend - 5) - (xstart + 5);
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
                DrawStringWithOutline(screen, xstart + 5, ystart + 5 +
                                      infofontsize * (lines) * 3 / 2, line,
                                      info_font, xend - 5, yend - 5);
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
        DrawStringWithOutline(screen, xstart + 5, ystart + 5 + infofontsize *
                              (lines) * 3 / 2, line, info_font, xend - 5,
                              yend - 5);
        free(orig);
    }
    else
    {
        DrawStringWithOutline(screen, xstart + 5, ystart + 5, text, info_font, 
                              xend - 5, yend - 5);
    }
}

void OSD::DarkenBox(int xstart, int ystart, int xend, int yend, 
                    unsigned char *screen)
{
    unsigned char *src;
    char c = -128;

    for (int y = ystart ; y < yend; y++)
    {
        for (int x = xstart; x < xend; x++)
        {
            src = screen + x + y * vid_width;
            *src = ((*src * c) >> 8) + *src;
        }
    }
}

void OSD::DrawRectangle(int xstart, int ystart, int xend, int yend,
                       unsigned char *screen)
{
    unsigned char *src;

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
