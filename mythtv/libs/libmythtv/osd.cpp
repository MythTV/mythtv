#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ttfont.h"
#include "osd.h"

OSD::OSD(int width, int height, const string &filename)
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
    
    displayframes = 0;
    show_info = false;
    show_channum = false;

    infotext = channumtext = "";
    
    fontname = filename;

    infofontsize = 16;
    channumfontsize = 40;

    if (vid_width < 600)
    {
        infofontsize /= 2;
        channumfontsize /= 2;
    }
    
    info_font = Efont_load((char *)fontname.c_str(), infofontsize);
    channum_font = Efont_load((char *)fontname.c_str(), channumfontsize);

    int mwidth, twidth;
    Efont_extents(info_font, "M M", NULL, NULL, &twidth, NULL, NULL, NULL, 
                  NULL);
    Efont_extents(info_font, "M", NULL, NULL, &mwidth, NULL, NULL, NULL, NULL);

    space_width = twidth - (mwidth * 2);
}

OSD::~OSD(void)
{
}

void OSD::SetInfoText(const string &text, const string &subtitle,
                      const string &desc, const string &category,
                      const string &start, const string &end, int length)
{
    displayframes = length;
    show_info = true;

    infotext = text;
    subtitletext = subtitle;
    desctext = desc;
}

void OSD::SetChannumText(const string &text, int length)
{
    displayframes = length;
    show_channum = true;

    channumtext = text;
}

void OSD::ShowLast(int length)
{
    displayframes = length;
    show_channum = true;
    show_info = true;
}

void OSD::TurnOff(void)
{
    displayframes = 0;
}

void OSD::Display(unsigned char *yuvptr)
{
    if (displayframes <= 0)
    {
        show_info = false; 
	show_channum = false;
    	return;
    }

    displayframes--;

    unsigned char *src;

    char c = -128;
    if (show_info)
    {
        for (int y = info_y_start ; y < info_y_end; y++)
        {
            for (int x = info_x_start; x < info_x_end; x++)
            {
                src = yuvptr + x + y * vid_width;
	        *src = ((*src * c) >> 8) + *src;
            }
        }

	EFont_draw_string(yuvptr, info_x_start + 5, info_y_start + 5, infotext,
                          info_font, info_x_end - 5, info_y_end - 5, 
                          vid_width, vid_height);
        EFont_draw_string(yuvptr, info_x_start + 5, info_y_start + 5 + 
                          infofontsize * 3 / 2, subtitletext, info_font, 
                          info_x_end - 5, info_y_end - 5 - infofontsize * 3 / 2,
                          vid_width, vid_height);
        int textlength = 0;
        Efont_extents(info_font, desctext, NULL, NULL, &textlength, NULL, NULL,
                      NULL, NULL);
       
        int maxlength = (info_x_end - 5) - (info_x_start + 5);
        if (textlength > maxlength)
        {
            char *orig = strdup((char *)desctext.c_str());
            int length = 0;
            int lines = 0;
            char line[512];
            memset(line, '\0', 512);

            char *word = strtok(orig, " ");
            while (word)
            {
                Efont_extents(info_font, word, NULL, NULL, &textlength,
                              NULL, NULL, NULL, NULL);
                if (textlength + space_width + length > maxlength)
                {
                    EFont_draw_string(yuvptr, info_x_start + 5, info_y_start +
                                      5 + infofontsize * (lines + 2) * 3 / 2,
                                      line, info_font, info_x_end - 5, 
                                      info_y_end - 5, vid_width, vid_height);
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
            EFont_draw_string(yuvptr, info_x_start + 5, info_y_start + 5 +
                              infofontsize * (lines + 2) * 3 / 2, line, 
                              info_font, info_x_end - 5, info_y_end - 5, 
                              vid_width, vid_height);
            free(orig);
        }
        else
        {
            EFont_draw_string(yuvptr, info_x_start + 5, info_y_start + 5 +
                              infofontsize * 3, desctext, info_font,
                              info_x_end - 5, info_y_end - 5 - 
                              infofontsize * 3, vid_width, vid_height);
        }
    }

    if (show_channum)
    {
	EFont_draw_string(yuvptr, channum_x_start - 1, channum_y_start - 1, 
                          channumtext, channum_font, channum_x_end, 
                          channum_y_end, vid_width, vid_height, false);
        EFont_draw_string(yuvptr, channum_x_start + 1, channum_y_start + 1,
                          channumtext, channum_font, channum_x_end,
                          channum_y_end, vid_width, vid_height, false); 
        EFont_draw_string(yuvptr, channum_x_start + 1, channum_y_start - 1,
                          channumtext, channum_font, channum_x_end,
                          channum_y_end, vid_width, vid_height, false);
        EFont_draw_string(yuvptr, channum_x_start - 1, channum_y_start + 1,
                          channumtext, channum_font, channum_x_end,
                          channum_y_end, vid_width, vid_height, false); 
        EFont_draw_string(yuvptr, channum_x_start, channum_y_start, channumtext,
                          channum_font, channum_x_end, channum_y_end, 
                          vid_width, vid_height, true, true);
    }
}
