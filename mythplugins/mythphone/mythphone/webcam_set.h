#if 0 // Depracated

/*
	webcam_set.h

	(c) 2003 Paul Volkaerts
	
  Header for the webcam config screen
*/

#ifndef WEBCAMSETTINGS_H_
#define WEBCAMSETTINGS_H_

#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>

#include "webcam.h"
#include "h263.h"

#define MAX_FPS	30


class WebcamSettingSlider
{

public:
  WebcamSettingSlider(UIStatusBarType  *UI,
                      UIPushButtonType *up, UIPushButtonType *down,
                      WebcamSettingSlider *pr)
  { statusUI = UI; up_button = up; down_button = down; prevObj = this; nextObj = this;
    if (pr!=0) { pr->setNext(this); prevObj = pr; }};
  ~WebcamSettingSlider(void);
  void up(void)   { if (up_button) up_button->push();};
  void down(void) { if (down_button) down_button->push();};
  WebcamSettingSlider *prev(void) { return prevObj; };
  WebcamSettingSlider *next(void) { return nextObj; };
  void setPrev(WebcamSettingSlider *w) { prevObj = w; };
  void setNext(WebcamSettingSlider *w) { nextObj = w; };

private:
  UIStatusBarType  *statusUI;
  UIPushButtonType *up_button;
  UIPushButtonType *down_button;
  WebcamSettingSlider *nextObj;
  WebcamSettingSlider *prevObj;
};


class WebcamSettingsBox : public MythThemedDialog
{

  Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;
    
    WebcamSettingsBox(MythMainWindow *parent, QString window_name,
                      QString theme_filename, const char *name = 0);

    ~WebcamSettingsBox(void);

    void keyPressEvent(QKeyEvent *e);
    
  public slots:

    void DrawLocalWebcamImage(uchar *yuv, int w, int h);
    void SettingDisplayTimerExpiry();
    void saveSettings();
    void brightnessUp();
    void brightnessDown();
    void contrastUp();
    void contrastDown();
    void colourUp();
    void colourDown();
    void hueUp();
    void hueDown();
    void fpsUp();
    void fpsDown();


  private:

    void    wireUpTheme();
    void    checkPaletteModes();

    Webcam  *webcam;
    QTimer  *settingDisplayTimer; // Timer controlling display of stats
    int     fps;                  // Webcam Frame Per Second 0..30
    int     brightness;           // as 0..255
    int     contrast;             // as 0..255
    int     colour;               // as 0..255
    int     hue;                  // as 0..255

    uchar localRgbBuffer[MAX_RGB_704_576];

    // From Settings SQL database
    QString WebcamDevice;
    QString TxResolution;

    QString palModes;

    //
    //  Theme-related "widgets"
    //
    WebcamSettingSlider *activeSlider;
    WebcamSettingSlider *brightnessSlider;
    WebcamSettingSlider *contrastSlider;
    WebcamSettingSlider *colourSlider;
    WebcamSettingSlider *hueSlider;
    WebcamSettingSlider *fpsSlider;

    UITextType       *webcam_name_text;
    UITextType       *webcam_maxsize_text;
    UITextType       *webcam_cursize_text;
    UITextType       *webcam_colour_bw_text;
    UITextType       *webcam_brightness_text;
    UITextType       *webcam_contrast_text;
    UITextType       *webcam_colour_text;
    UITextType       *webcam_hue_text;
    UITextType       *webcam_fps_text;
    UIBlackHoleType  *webcamArea;
    UIStatusBarType  *brightnessUI;
    UIStatusBarType  *contrastUI;
    UIStatusBarType  *colourUI;
    UIStatusBarType  *hueUI;
    UIStatusBarType  *fpsUI;
    UITextButtonType *save_button;
    UIPushButtonType *br_up_button;
    UIPushButtonType *br_down_button;
    UIPushButtonType *con_up_button;
    UIPushButtonType *con_down_button;
    UIPushButtonType *col_up_button;
    UIPushButtonType *col_down_button;
    UIPushButtonType *hue_up_button;
    UIPushButtonType *hue_down_button;
    UIPushButtonType *fps_up_button;
    UIPushButtonType *fps_down_button;
};


#endif


#endif

