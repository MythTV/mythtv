#ifndef OSD_H
#define OSD_H

#include "ttfont.h"
#include <string>

using namespace std;

class OSD
{
 public:
    OSD(int width, int height, const string &filename);
   ~OSD(void);

    void Display(unsigned char *yuvptr);
    
    void SetInfoText(char *text, int length);
    void SetChannumText(char *text, int length);

 private:
    string fontname;

    int vid_width;
    int vid_height;
    int info_y_start;
    int info_y_end;
    int info_x_start;
    int info_x_end;
    int info_width;
    int info_height;
    bool show_info;
    char *infotext;
    Efont *info_font;

    int channum_y_start;
    int channum_x_start;
    int channum_y_end;
    int channum_x_end;
    int channum_width;
    int channum_height;
    char *channumtext;
    bool show_channum;
    Efont *channum_font;
    int displayframes;
};
    
#endif
