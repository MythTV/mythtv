#include <stdlib.h>
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
    channum_x_start = width * 6 / 8;
    channum_x_end = width * 15 * 16;
    
    channum_width = channum_x_end - channum_x_start;
    channum_height = channum_y_end - channum_y_start;
    
    displayframes = 0;
    show_info = false;
    show_channum = false;

    infotext = channumtext = NULL;
    
    fontname = filename;

    info_font = Efont_load((char *)fontname.c_str(), 16);
    channum_font = Efont_load((char *)fontname.c_str(), 40);
}

OSD::~OSD(void)
{
}

void OSD::SetInfoText(char *text, int length)
{
    displayframes = length;
    show_info = true;

    infotext = strdup(text);
}

void OSD::SetChannumText(char *text, int length)
{
    displayframes = length;
    show_channum = true;

    channumtext = strdup(text);
}

void OSD::Display(unsigned char *yuvptr)
{
    if (displayframes <= 0)
    {
        show_info = false; 
        if (infotext)
            free(infotext);
        if (channumtext)
            free(channumtext);
	infotext = NULL;
	channumtext = NULL;
    	return;
    }

    displayframes--;

    unsigned char *src;

    char c = 128;
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
                          info_font, info_x_end - 10, info_y_end - 10, 
                          vid_width);
    }

    if (show_channum)
    {
        EFont_draw_string(yuvptr, channum_x_start, channum_y_start, channumtext,
                          channum_font, channum_x_end, channum_y_end, 
                          vid_width);
    }
}
