#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ttfont.h"
#include "osd.h"

OSD::OSD(int width, int height, const string &filename, const string &prefix)
{
    vid_width = width;
    vid_height = height;

    info_y_start = height * 5 / 8;
    info_y_end = height * 15 / 16;
    info_x_start = width / 8;
    info_x_end = width * 7 / 8;

    info_width = info_x_end - info_x_start;
    info_height = info_y_end - info_y_start;

    channum_y_start = height * 1 / 16;
    channum_y_end = height * 2 / 8;
    channum_x_start = width * 3 / 4;
    channum_x_end = width * 15 * 16;
    
    channum_width = channum_x_end - channum_x_start;
    channum_height = channum_y_end - channum_y_start;
   
    dialog_y_start = height / 8;
    dialog_y_end = height * 7 / 8;
    dialog_x_start = width / 8;
    dialog_x_end = width * 7 / 8;
 
    displayframes = 0;
    show_info = false;
    show_channum = false;
    show_dialog = false;

    currentdialogoption = 1;

    infotext = channumtext = "";
    
    fontname = filename;

    infofontsize = 16;
    channumfontsize = 40;

    if (vid_width < 400 || vid_height < 400)
    {
        infofontsize /= 2;
        channumfontsize /= 2;
    }
  
    enableosd = false;
 
    string fullname = prefix + string("/share/mythtv/") + fontname;

    info_font = Efont_load((char *)fullname.c_str(), infofontsize, vid_width,
                           vid_height);
    if (!info_font)
    {
        fullname = fontname;
        info_font = Efont_load((char *)fullname.c_str(), infofontsize,
                               vid_width, vid_height);
    }

    channum_font = Efont_load((char *)fullname.c_str(), channumfontsize,
                              vid_width, vid_height);

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
}

OSD::~OSD(void)
{
    if (info_font)
        Efont_free(info_font);
    if (channum_font)
        Efont_free(channum_font);
}

void OSD::SetInfoText(const string &text, const string &subtitle,
                      const string &desc, const string &category,
                      const string &start, const string &end, int length)
{
    displayframes = time(NULL) + length;
    show_info = true;

    infotext = text;
    subtitletext = subtitle;
    desctext = desc;
}

void OSD::SetChannumText(const string &text, int length)
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

void OSD::SetDialogBox(const string &message, const string &optionone,
                       const string &optiontwo, const string &optionthree,
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

        return;
    }

    if (show_info)
    {
        DarkenBox(info_x_start, info_y_start, info_x_end, info_y_end,
                  yuvptr);

	EFont_draw_string(yuvptr, info_x_start + 5, info_y_start + 5, infotext,
                          info_font, info_x_end - 5, info_y_end - 5);
        EFont_draw_string(yuvptr, info_x_start + 5, info_y_start + 5 + 
                          infofontsize * 3 / 2, subtitletext, info_font, 
                          info_x_end - 5, 
                          info_y_end - 5 - infofontsize * 3 / 2);
        DrawStringIntoBox(info_x_start, info_y_start + infofontsize * 3, 
                          info_x_end, info_y_end, desctext, 
                          yuvptr);
    }

    if (show_channum)
    {
	EFont_draw_string(yuvptr, channum_x_start - 1, channum_y_start - 1, 
                          channumtext, channum_font, channum_x_end, 
                          channum_y_end, false);
        EFont_draw_string(yuvptr, channum_x_start + 1, channum_y_start + 1,
                          channumtext, channum_font, channum_x_end,
                          channum_y_end, false); 
        EFont_draw_string(yuvptr, channum_x_start + 1, channum_y_start - 1,
                          channumtext, channum_font, channum_x_end,
                          channum_y_end, false);
        EFont_draw_string(yuvptr, channum_x_start - 1, channum_y_start + 1,
                          channumtext, channum_font, channum_x_end,
                          channum_y_end, false); 
        EFont_draw_string(yuvptr, channum_x_start, channum_y_start, channumtext,
                          channum_font, channum_x_end, channum_y_end, 
                          true, true);
    }
}

void OSD::DrawStringIntoBox(int xstart, int ystart, int xend, int yend,
                            const string &text, unsigned char *screen)
{
    int textlength = 0;
    Efont_extents(info_font, text, NULL, NULL, &textlength, NULL, NULL, NULL,
                  NULL);

    int maxlength = (xend - 5) - (xstart + 5);
    if (textlength > maxlength)
    {
        char *orig = strdup((char *)text.c_str());
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
                EFont_draw_string(screen, xstart + 5, ystart + 5 +
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
        EFont_draw_string(screen, xstart + 5, ystart + 5 + infofontsize *
                          (lines) * 3 / 2, line, info_font, xend - 5,
                          yend - 5);
        free(orig);
    }
    else
    {
        EFont_draw_string(screen, xstart + 5, ystart + 5, text, info_font, 
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
