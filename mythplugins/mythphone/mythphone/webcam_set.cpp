#if 0  // Removed support for this screen; will probably remove code altogether eventually


/*
	webcam_set.cpp

	(c) 2003 Paul Volkaerts
	
  A screen which shows the webcam picture and allows you to adjust certain parameteres.
*/
#include <qapplication.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>

using namespace std;

#include <linux/videodev.h>
#include <mythtv/mythcontext.h>

#include "config.h"
#include "webcam.h"
#include "h263.h"
#include "webcam_set.h"



WebcamSettingsBox::WebcamSettingsBox(MythMainWindow *parent, QString window_name,
                                     QString theme_filename, const char *name)

           : MythThemedDialog(parent, window_name, theme_filename, name)
{
    //
    //  A WebcamSettings box is a single dialog that allows you to test your
    //  webcam and check it is functioning, as well as set some parameters
    //  such as brightness.
    //

    brightness = 32768;
    contrast = 32768;
    colour = 32768;
    hue = 32768;
    fps = 10; // Default to s-l-o-w to avoid problems

    settingDisplayTimer = 0;

    //
    //  Wire up the theme 
    //  (connect operational widgets to whatever the theme ui.xml
    //  file says is supposed to be on this dialog)
    //
    
    wireUpTheme();


    // Create and open the webcam
    webcam = new Webcam(this);

    WebcamDevice = gContext->GetSetting("WebcamDevice");
    TxResolution = gContext->GetSetting("TxResolution");
    if (WebcamDevice.length() < 1)
    {
        DialogBox *NoDeviceDialog = new DialogBox(gContext->GetMainWindow(),
                                                  QObject::tr("\n\nYou need to go to settings and configure the WEBCAM device."));
        NoDeviceDialog->AddButton(QObject::tr("OK"));
        NoDeviceDialog->exec();
        delete NoDeviceDialog;
    }
    else
    {
        if (webcam->camOpen(WebcamDevice, 352, 288))
        {
            int pal = webcam->GetPalette();
            checkPaletteModes();
            webcam->SetPalette(pal);
            //webcam->SetBrightness(brightness);
            //webcam->SetContrast(contrast);
            //webcam->SetColour(colour);
            //webcam->SetHue(hue);
            webcam->SetTargetFps(fps);


            connect(webcam, SIGNAL(webcamFrameReady(uchar *, int, int)), this, SLOT(DrawLocalWebcamImage(uchar *, int, int)));
        }

        settingDisplayTimer = new QTimer(this);
        settingDisplayTimer->start(1000);
        connect(settingDisplayTimer, SIGNAL(timeout()), this,
                SLOT(SettingDisplayTimerExpiry()));
    }
}



void WebcamSettingsBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Phone", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
    
            if (action == "Up")
            {
              activeSlider = activeSlider->prev();
            }
            else if (action == "Down")
            {
              activeSlider = activeSlider->next();
            }
            else if (action == "Left")
            {
              activeSlider->down();
            }
            else if (action == "Right")
            {
              activeSlider->up();
            }
            else if (action == "9")
            {
                cout << "9 Pressed" << endl;
                if (save_button)
                    save_button->push();
            }
            else
                handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}


void WebcamSettingsBox::DrawLocalWebcamImage(uchar *yuv, int w, int h)
{
    QPixmap Pixmap;

    YUV420PtoRGB32(w, h, w, yuv, localRgbBuffer, sizeof(localRgbBuffer)); 
    QImage Image((uchar *)localRgbBuffer, w, h, 32, (QRgb *)0, 0, QImage::LittleEndian);
    QRect puthere = webcamArea->getScreenArea();
    QImage ScaledImage = Image.scale(puthere.width(), puthere.height(), QImage::ScaleMax);
    Pixmap = ScaledImage;
    bitBlt(this, puthere.x(), puthere.y(), &Pixmap);
}


void WebcamSettingsBox::saveSettings()
{
    cout << "Save Settings selected" << endl;
}


void WebcamSettingsBox::brightnessUp()
{
    brightness = webcam->SetBrightness(brightness+2048);
    brightnessUI->SetUsed(brightness);
}


void WebcamSettingsBox::brightnessDown()
{
    brightness = webcam->SetBrightness(brightness-2048);
    brightnessUI->SetUsed(brightness);
}


void WebcamSettingsBox::contrastUp()
{
    contrast = webcam->SetContrast(contrast+2048);
    contrastUI->SetUsed(contrast);
}


void WebcamSettingsBox::contrastDown()
{
    contrast = webcam->SetContrast(contrast-2048);
    contrastUI->SetUsed(contrast);
}


void WebcamSettingsBox::colourUp()
{
    colour = webcam->SetColour(colour+2048);
    colourUI->SetUsed(colour);
}


void WebcamSettingsBox::colourDown()
{
    colour = webcam->SetColour(colour-2048);
    colourUI->SetUsed(colour);
}


void WebcamSettingsBox::hueUp()
{
    hue = webcam->SetHue(hue+2048);
    hueUI->SetUsed(hue);
}


void WebcamSettingsBox::hueDown()
{
    hue = webcam->SetHue(hue-2048);
    hueUI->SetUsed(hue);
}


void WebcamSettingsBox::fpsUp()
{
    fps+=2;
    fps = webcam->SetTargetFps(fps);
    fpsUI->SetUsed(fps);
}


void WebcamSettingsBox::fpsDown()
{
    fps-=2;
    fps = webcam->SetTargetFps(fps);
    fpsUI->SetUsed(fps);
}


void WebcamSettingsBox::SettingDisplayTimerExpiry()
{
    if (webcam_name_text)
    {
        QString Name = "Name : " + webcam->GetName();
        webcam_name_text->SetText(Name);
    }

    if (webcam_maxsize_text)
    {
        int x,y,x2,y2;
        webcam->GetMaxSize(&x2, &y2);
        QString x2s, y2s, xs, ys;
        x2s.setNum(x2);
        y2s.setNum(y2);
        webcam->GetMinSize(&x, &y);
        xs.setNum(x);
        ys.setNum(y);
        QString Size = "Supported Sizes : " + xs + "x" + ys +
                       " to " + x2s + "x" + y2s;
        webcam_maxsize_text->SetText(Size);
    }

    if (webcam_cursize_text)
    {
        int x,y;
        webcam->GetCurSize(&x, &y);
        QString xs, ys;
        xs.setNum(x);
        ys.setNum(y);
        QString Size = "Current Size : " + xs + "x" + ys;
        webcam_cursize_text->SetText(Size);
    }

    if (webcam_colour_bw_text)
    {
        if (webcam->isGreyscale())
          webcam_colour_bw_text->SetText("Type : Monochrome (not supported!)");
        else
        {
          int pal = webcam->GetPalette();
          QString colourMode = QString("Type : %1 (%2)").arg(pal).arg(palModes);
          webcam_colour_bw_text->SetText(colourMode);
        }
    }

    if (webcam_brightness_text)
    {
        int v;
        v = webcam->GetBrightness();
        QString vs;
        vs.setNum(v);
        vs.insert(0, "Brightness : ");
        webcam_brightness_text->SetText(vs);
    }

    if (webcam_contrast_text)
    {
        int v;
        v = webcam->GetContrast();
        QString vs;
        vs.setNum(v);
        vs.insert(0, "Contrast : ");
        webcam_contrast_text->SetText(vs);
    }

    if (webcam_colour_text)
    {
        int v;
        v = webcam->GetColour();
        QString vs;
        vs.setNum(v);
        vs.insert(0, "Colour : ");
        webcam_colour_text->SetText(vs);
    }

    if (webcam_hue_text)
    {
        int v;
        v = webcam->GetHue();
        QString vs;
        vs.setNum(v);
        vs.insert(0, "Hue : ");
        webcam_hue_text->SetText(vs);
    }

    if (webcam_fps_text)
    {
        QString vs;
        vs.setNum(webcam->GetActualFps());
        vs.insert(0, "Frames / Second : ");
        webcam_fps_text->SetText(vs);
    }
}


void WebcamSettingsBox::checkPaletteModes()
{
    palModes = "";
    for (int p=1; p<16; p++)
    {
        if (webcam->SetPalette(p))
        {
            QString temp = QString("%1 ").arg(p);
            palModes += temp;
        }
    }
}


void WebcamSettingsBox::wireUpTheme()
{
    brightnessUI = getUIStatusBarType("brightness");
    if(brightnessUI)
    {
        brightnessUI->SetTotal(65535);
        brightnessUI->SetUsed(brightness);
    }
    
    contrastUI = getUIStatusBarType("contrast");
    if(contrastUI)
    {
        contrastUI->SetTotal(65535);
        contrastUI->SetUsed(contrast);
    }

    colourUI = getUIStatusBarType("colour");
    if(colourUI)
    {
        colourUI->SetTotal(65535);
        colourUI->SetUsed(colour);
    }

    hueUI = getUIStatusBarType("hue");
    if(hueUI)
    {
        hueUI->SetTotal(65535);
        hueUI->SetUsed(hue);
    }

    fpsUI = getUIStatusBarType("fps");
    if(fpsUI)
    {
        fpsUI->SetTotal(MAX_FPS);
        fpsUI->SetUsed(fps);
    }

    webcam_name_text      = getUITextType("webcam_name_text");
    webcam_maxsize_text   = getUITextType("webcam_maxsize_text");
    webcam_cursize_text   = getUITextType("webcam_cursize_text");
    webcam_colour_bw_text = getUITextType("webcam_colour_bw_text");
    webcam_brightness_text = getUITextType("webcam_brightness_text");
    webcam_contrast_text  = getUITextType("webcam_contrast_text");
    webcam_colour_text    = getUITextType("webcam_colour_text");
    webcam_hue_text       = getUITextType("webcam_hue_text");
    webcam_fps_text       = getUITextType("webcam_fps_text");

    webcamArea = getUIBlackHoleType("visual_blackhole");

    br_up_button = getUIPushButtonType("br_up_button");
    if(br_up_button)
        connect(br_up_button, SIGNAL(pushed()), this, SLOT(brightnessUp()));

    br_down_button = getUIPushButtonType("br_down_button");
    if(br_down_button)
        connect(br_down_button, SIGNAL(pushed()), this, SLOT(brightnessDown()));

    con_up_button = getUIPushButtonType("con_up_button");
    if(con_up_button)
        connect(con_up_button, SIGNAL(pushed()), this, SLOT(contrastUp()));

    con_down_button = getUIPushButtonType("con_down_button");
    if(con_down_button)
        connect(con_down_button, SIGNAL(pushed()), this, SLOT(contrastDown()));

    col_up_button = getUIPushButtonType("col_up_button");
    if(col_up_button)
        connect(col_up_button, SIGNAL(pushed()), this, SLOT(colourUp()));

    col_down_button = getUIPushButtonType("col_down_button");
    if(col_down_button)
        connect(col_down_button, SIGNAL(pushed()), this, SLOT(colourDown()));

    hue_up_button = getUIPushButtonType("hue_up_button");
    if(hue_up_button)
        connect(hue_up_button, SIGNAL(pushed()), this, SLOT(hueUp()));

    hue_down_button = getUIPushButtonType("hue_down_button");
    if(hue_down_button)
        connect(hue_down_button, SIGNAL(pushed()), this, SLOT(hueDown()));

    fps_up_button = getUIPushButtonType("fps_up_button");
    if(fps_up_button)
        connect(fps_up_button, SIGNAL(pushed()), this, SLOT(fpsUp()));

    fps_down_button = getUIPushButtonType("fps_down_button");
    if(fps_down_button)
        connect(fps_down_button, SIGNAL(pushed()), this, SLOT(fpsDown()));

    brightnessSlider = new WebcamSettingSlider (brightnessUI, br_up_button, br_down_button, 0);
    contrastSlider = new WebcamSettingSlider (contrastUI, con_up_button, con_down_button, brightnessSlider);
    colourSlider = new WebcamSettingSlider (colourUI, col_up_button, col_down_button, contrastSlider);
    hueSlider = new WebcamSettingSlider (hueUI, hue_up_button, hue_down_button, colourSlider);
    fpsSlider = new WebcamSettingSlider (fpsUI, fps_up_button, fps_down_button, hueSlider);
    activeSlider = brightnessSlider;

    save_button = getUITextButtonType("save_button");
    if(save_button)
    {
        save_button->setText(tr("9 Save"));
        connect(save_button, SIGNAL(pushed()), this, SLOT(saveSettings()));
    }
    else
      cerr << "Can't get button save" << endl;
}

WebcamSettingsBox::~WebcamSettingsBox(void)
{
    if (settingDisplayTimer != 0)
        settingDisplayTimer->stop();
}



#endif