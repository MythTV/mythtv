/*
	webcam.cpp

	(c) 2003 Paul Volkaerts

    Webcam control and capture
*/
#include <qapplication.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
using namespace std;

#include <linux/videodev.h>
#include <mythtv/mythcontext.h>

#include "config.h"
#include "h263.h"
#include "webcam.h"




Webcam::Webcam(QObject *parent, const char *name)
             : QObject(parent, name)
{
  hDev = 0;
  DevName = "";
  picbuff1 = 0;
  picbuff2 = 0;
  dispbuff = 0;
  imageLen = 0;
  frameSize = 0;
  fps = 5;

  vCaps.name[0] = 0;
  vCaps.maxwidth = 0;
  vCaps.maxheight = 0;
  vCaps.minwidth = 0;
  vCaps.minheight = 0;
  memset(&vWin, 0, sizeof(struct video_window));
  vWin.width = 0;
  vWin.height = 0;
  vPic.brightness = 0;
  vPic.depth = 0;
  vPic.palette = 0;
  vPic.colour = 0;
  vPic.contrast = 0;
  vPic.hue = 0;

  // Create a timer to grab and display images
  grabTimer = new QTimer(this);
  connect(grabTimer, SIGNAL(timeout()), this, SLOT(grabTimerExpiry()));

  // Create a timer to measure the actual FPS rate
  fpsMeasureTimer = new QTimer(this);
  connect(fpsMeasureTimer, SIGNAL(timeout()), this, SLOT(fpsMeasureTimerExpiry()));

}


QString Webcam::devName(QString WebcamName)
{
  int handle = open(WebcamName, O_RDWR);
  if (handle <= 0)
    return "";
  
  struct video_capability tempCaps;
  ioctl(handle, VIDIOCGCAP, &tempCaps);
  ::close(handle);
  return tempCaps.name;
}


bool Webcam::camOpen(QString WebcamName, int width, int height)
{
  DevName = WebcamName;

  if ((hDev <= 0) && (WebcamName.length() > 0))
    hDev = open(DevName, O_RDWR);
  if ((hDev <= 0) || (WebcamName.length() <= 0))
  {
    cerr << "Couldn't open camera " << DevName << endl;
    return false;
  }
  else
  {
    readCaps();

    if (!SetPalette(VIDEO_PALETTE_YUV420P) && 
        !SetPalette(VIDEO_PALETTE_YUV422P) &&
        !SetPalette(VIDEO_PALETTE_RGB24))
    {
      cout << "Webcam does not support YUV420P, YUV422P, or RGB24 modes; these are the only ones currently supported. Closing webcam.\n";
      camClose();
      return false;
    }

    SetSize(width, height);
    int actWidth, actHeight;
    GetCurSize(&actWidth, &actHeight);
    if ((width != actWidth) || (height != actHeight))
    {
      cout << "Could not set webcam to " << width << "x" << height << "; got " << actWidth << "x" << actHeight << " instead.\n";
    }

    //Allocate picture buffer memory
    if (isGreyscale())
    {
      cerr << "Greyscale not yet supported" << endl;
      //picbuff1 = new unsigned char [vCaps.maxwidth * vCaps.maxheight];
      //picbuff2 = new unsigned char [vCaps.maxwidth * vCaps.maxheight];
      camClose();
      return false;
    }
    else 
    {
      switch (GetPalette())
      {
      case VIDEO_PALETTE_RGB24:   frameSize = RGB24_LEN(vWin.width, vWin.height);   break;
      case VIDEO_PALETTE_RGB32:   frameSize = RGB32_LEN(vWin.width, vWin.height);   break;
      case VIDEO_PALETTE_YUV420P: frameSize = YUV420P_LEN(vWin.width, vWin.height); break;
      case VIDEO_PALETTE_YUV422P: frameSize = YUV422P_LEN(vWin.width, vWin.height); break;
      default:
          cerr << "Palette mode " << GetPalette() << " not yet supported" << endl;
          camClose();
          return false;
          break;
      }

      picbuff1 = new unsigned char [frameSize];
      picbuff2 = new unsigned char [frameSize];
      dispbuff = new unsigned char [RGB32_LEN(vCaps.maxwidth, vCaps.maxheight)];
    }

    // We run the timer as single-shot in case it is running
    // too fast for the device
    grabTimer->start(1000/fps, true);
    frames_last_period=0;
    fpsMeasureTimer->start(10*1000); // 10 second timer
  }
  return true;
}


void Webcam::camClose()
{
  grabTimer->stop();
  fpsMeasureTimer->stop();

  if (hDev <= 0)
    cerr << "Can't close a camera that isn't open" << endl;
  else
  {
    // There must be a widget procedure called close so make
    // sure we call the proper one. Screwed me up for ages!
    ::close(hDev);
    hDev = 0;
  }

  if (picbuff1)
    delete picbuff1;
  if (picbuff2)
    delete picbuff2;
  if (dispbuff)
    delete dispbuff;

  picbuff1 = 0;
  picbuff2 = 0;
  dispbuff = 0;
}


void Webcam::readCaps()
{
  if (hDev > 0)
  {
    ioctl(hDev, VIDIOCGCAP, &vCaps);
    ioctl(hDev, VIDIOCGWIN, &vWin);
    ioctl(hDev, VIDIOCGPICT, &vPic);

    /*cout << "WEBCAM Name:       " << vCaps.name << endl;
    cout << "WEBCAM Max Width:  " << vCaps.maxwidth << endl;
    cout << "WEBCAM Max Height: " << vCaps.maxheight << endl;
    cout << "WEBCAM Min Width:  " << vCaps.minwidth << endl;
    cout << "WEBCAM Min Height: " << vCaps.minheight << endl;
    cout << "WEBCAM Cur Width:  " << vWin.width << endl;
    cout << "WEBCAM Cur Height: " << vWin.height << endl;
    cout << "WEBCAM Brightness: " << vPic.brightness << endl;
    cout << "WEBCAM Depth:      " << vPic.depth << endl;
    cout << "WEBCAM Palette:    " << vPic.palette << endl;
    cout << "WEBCAM Colour:     " << vPic.colour << endl;
    cout << "WEBCAM Contrast:   " << vPic.contrast << endl;
    cout << "WEBCAM Hue:        " << vPic.hue << endl;*/
  }
}


void Webcam::SetSize(int width, int height)
{
  // Note you can't call this whilst the webcam is open because all the buffers will be the wrong size

  memset(&vWin, 0, sizeof(struct video_window));
  vWin.width = width;
  vWin.height = height;

  if (ioctl(hDev, VIDIOCSWIN, &vWin) == -1)
    cerr << "Error setting display size" << endl;

  readCaps();
}


bool Webcam::SetPalette(int palette)
{
  vPic.palette = palette;
  switch(palette)
  {
  case VIDEO_PALETTE_YUV420P: vPic.depth = 16;  break;
  case VIDEO_PALETTE_YUYV:    vPic.depth = 16;  break;
  case VIDEO_PALETTE_YUV422:  vPic.depth = 16;  break;
  case VIDEO_PALETTE_RGB32:   vPic.depth = 32;  break;
  case VIDEO_PALETTE_RGB24:   vPic.depth = 24;  break;
  case VIDEO_PALETTE_GREY:    vPic.depth = 8;   break;
  default:                    vPic.depth = 0;   break;
  }

  if (ioctl(hDev, VIDIOCSPICT, &vPic) == -1)
  {
      return false;
  }

  readCaps();

  return (vPic.palette == palette ? true : false);
}


int Webcam::SetBrightness(int v)
{
  if ((v >= 0) && (v <= 65535))
  {
    if (hDev > 0)
    {
      vPic.brightness = v;

      if (ioctl(hDev, VIDIOCSPICT, &vPic) == -1)
          cerr << "Error setting brightness" << endl;

      readCaps();
    }
  }
  else
    cerr << "Invalid Brightness parameter" << endl;
  return vPic.brightness;
}


int Webcam::SetContrast(int v)
{
  if ((v >= 0) && (v <= 65535))
  {
    if (hDev > 0)
    {
      vPic.contrast = v ;

      if (ioctl(hDev, VIDIOCSPICT, &vPic) == -1)
          cerr << "Error setting contrast" << endl;

      readCaps();
    }
  }
  else
    cerr << "Invalid contrast parameter" << endl;
  return vPic.contrast;
}


int Webcam::SetColour(int v)
{
  if ((v >= 0) && (v <= 65535))
  {
    if (hDev > 0)
    {
      vPic.colour = v;

      if (ioctl(hDev, VIDIOCSPICT, &vPic) == -1)
          cerr << "Error setting colour" << endl;

      readCaps();
    }
  }
  else
    cerr << "Invalid colour parameter" << endl;
  return vPic.colour;
}


int Webcam::SetHue(int v)
{
  if ((v >= 0) && (v <= 65535))
  {
    if (hDev > 0)
    {
      vPic.hue = v;

      if (ioctl(hDev, VIDIOCSPICT, &vPic) == -1)
          cerr << "Error setting hue" << endl;

      readCaps();
    }
  }
  else
    cerr << "Invalid hue parameter" << endl;
  return vPic.hue;
}


int Webcam::SetTargetFps(int f)
{
  if ((f >= 1) && (f <= 30))
    fps = f;
  else
    cerr << "Invalid FPS parameter" << endl;

  return fps;
}


int Webcam::GetActualFps()
{
  return actualFps;
}


int Webcam::grabImage()
{
  int len=0;

  if (hDev > 0)
  {
    if ((len = read(hDev, picbuff1, frameSize)) == frameSize)
      imageLen = len;
    else
      cerr << "Error reading from camera; got " << len << " bytes; expected " << frameSize << endl;
  }
  else
    cerr << "Error : Trying to read from a closed webcam\n";

  return len;
}



void Webcam::grabTimerExpiry()
{
  if (hDev > 0)
  {
    grabImage();
    if (imageLen > 0)
    {
        int srcFmt = 0;
        switch(GetPalette())
        {
        case VIDEO_PALETTE_YUV420P:    srcFmt = PIX_FMT_YUV420P;    break;
        case VIDEO_PALETTE_YUV422P:    srcFmt = PIX_FMT_YUV422P;    break;
        case VIDEO_PALETTE_RGB24:      srcFmt = PIX_FMT_RGB24;      break;
        default:
            cerr << "Webcam: Unsupported palette mode " << GetPalette() << endl; // Should not get here, caught earlier
            break;
        }

        if (srcFmt != PIX_FMT_YUV420P) // Need to reformat image to YUV420P 
        {
            AVPicture image_in, image_out;
            avpicture_fill(&image_in,  (uint8_t *)picbuff1, srcFmt, vWin.width, vWin.height);
            avpicture_fill(&image_out, (uint8_t *)dispbuff, PIX_FMT_YUV420P, vWin.width, vWin.height);
            img_convert(&image_out, PIX_FMT_YUV420P, &image_in, srcFmt, vWin.width, vWin.height);
            emit webcamFrameReady(dispbuff, (int)vWin.width, (int)vWin.height );
        }
        else
            emit webcamFrameReady(picbuff1, (int)vWin.width, (int)vWin.height );
    }
    frames_last_period++;

    // The timer is restarted AFTER getting the image
    // which means less FPS, but also less chance of
    // starving Myth of CPU.  All will be fixed in
    // time when I make this a thread
    grabTimer->start(1000/fps, true);
  }
}


void Webcam::fpsMeasureTimerExpiry()
{
  actualFps = frames_last_period/10; // This is run every 10 secs
  frames_last_period=0;
}

Webcam::~Webcam()
{
  if (hDev > 0)
    camClose();
}

