#ifndef OSD_H
#define OSD_H

#include "ttfont.h"
#include <qstring.h>
#include <time.h>

class QImage;

class OSDImage
{
  public:
    OSDImage(QString pathname);
   ~OSDImage();
   
    unsigned char *yuv;
    unsigned char *ybuffer;
    unsigned char *ubuffer;
    unsigned char *vbuffer;

    unsigned char *alpha;

    int width;
    int height;
};
    
class OSD
{
 public:
    OSD(int width, int height, const QString &filename, const QString &prefix);
   ~OSD(void);

    void Display(unsigned char *yuvptr);
    
    void SetInfoText(const QString &text, const QString &subtitle, 
                     const QString &desc, const QString &category,
                     const QString &start, const QString &end, int length);
    void SetChannumText(const QString &text, int length);

    void ShowLast(int length);
    void TurnOff(void);
   
    void SetDialogBox(const QString &message, const QString &optionone, 
                      const QString &optiontwo, const QString &optionthree,
                      int length);
    void DialogUp(void);
    void DialogDown(void);
    bool DialogShowing(void) { return show_dialog; } 
    int GetDialogSelection(void) { return currentdialogoption; }
  
    bool Visible(void) { return (time(NULL) <= displayframes); }
   
 private:
    void SetNoThemeDefaults();
    Efont *LoadFont(QString name, int size); 
    
    void DarkenBox(int xstart, int ystart, int xend, int yend,
                   unsigned char *screen);
    void DrawStringIntoBox(int xstart, int ystart, int xend, int yend, 
                           const QString &text, unsigned char *screen);

    void DrawStringWithOutline(unsigned char *yuvptr, int x, int y, 
                               const QString &text, Efont *font, int maxx,
                               int maxy, bool rightjustify = false);

    void DrawRectangle(int xstart, int ystart, int xend, int yend,
                       unsigned char *screen);

    void BlendImage(OSDImage *image, int xstart, int ystart, 
                    unsigned char *screen);

    
    void DisplayDialogNoTheme(unsigned char *yuvptr);
    void DisplayInfoNoTheme(unsigned char *yuvptr);
    void DisplayChannumNoTheme(unsigned char *yuvptr);
    
    QString fontname;

    int vid_width;
    int vid_height;

    int info_y_start;
    int info_y_end;
    int info_x_start;
    int info_x_end;
    int info_width;
    int info_height;
    bool show_info;
    QString infotext;
    QString subtitletext;
    QString desctext;
    Efont *info_font;
    int infofontsize;

    int channum_y_start;
    int channum_x_start;
    int channum_y_end;
    int channum_x_end;
    int channum_width;
    int channum_height;
    QString channumtext;
    bool show_channum;
    Efont *channum_font;
    int channumfontsize;

    int displayframes;

    int space_width;

    bool enableosd;

    int dialog_y_start;
    int dialog_x_start;
    int dialog_y_end;
    int dialog_x_end;
    int dialog_width;
    int dialog_height;
    QString dialogmessagetext;
    QString dialogoptionone;
    QString dialogoptiontwo;
    QString dialogoptionthree;

    int currentdialogoption;
    bool show_dialog;

    QString fontprefix;
    
    
    bool usingtheme;
    
    OSDImage *channelbackground;
};
    
#endif
