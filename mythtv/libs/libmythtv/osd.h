#ifndef OSD_H
#define OSD_H

#include "ttfont.h"
#include <string>

using namespace std;

class OSD
{
 public:
    OSD(int width, int height, const string &filename, const string &prefix);
   ~OSD(void);

    void Display(unsigned char *yuvptr);
    
    void SetInfoText(const string &text, const string &subtitle, 
                     const string &desc, const string &category,
                     const string &start, const string &end, int length);
    void SetChannumText(const string &text, int length);

    void ShowLast(int length);
    void TurnOff(void);
    
    bool Visible(void) { return displayframes > 0; }
   
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
    string infotext;
    string subtitletext;
    string desctext;
    Efont *info_font;
    int infofontsize;

    int channum_y_start;
    int channum_x_start;
    int channum_y_end;
    int channum_x_end;
    int channum_width;
    int channum_height;
    string channumtext;
    bool show_channum;
    Efont *channum_font;
    int channumfontsize;

    int displayframes;

    int space_width;

    bool enableosd;
};
    
#endif
