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
  imageLen = 0;
  frameSize = 0;
  fps = 5;
  killWebcamThread = true; // Leave true whilst its not running

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
    }

    StartThread();
  }
  return true;
}


void Webcam::camClose()
{
  KillThread();

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

  picbuff1 = 0;
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
    cerr << "Webcam: Error setting capture size " << width << "x" << height << endl;

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


int Webcam::SetTargetFps(wcClient *client, int f)
{
  if ((f >= 1) && (f <= 30) && (client != 0))
  {
    WebcamLock.lock();
    client->fps = f;
    client->interframeTime = 1000/f;
    WebcamLock.unlock();
  }
  else
    cerr << "Invalid FPS parameter" << endl;

  return fps;
}


int Webcam::GetActualFps()
{
  return actualFps;
}


wcClient *Webcam::RegisterClient(int format, int fps, QObject *eventWin)
{
    wcClient *client = new wcClient;

    if (fps == 0)
    {
        fps = 10;
        cerr << "Webcam requested fps of zero\n";
    }

    client->eventWindow = eventWin;
    client->fps = fps;
    client->actualFps = fps;
    client->interframeTime = 1000/fps;
    client->timeLastCapture = QTime::currentTime();

    switch (format)
    {
    case VIDEO_PALETTE_RGB24:   client->frameSize = RGB24_LEN(vWin.width, vWin.height);   client->format = PIX_FMT_RGB24;      break;
    case VIDEO_PALETTE_RGB32:   client->frameSize = RGB32_LEN(vWin.width, vWin.height);   client->format = PIX_FMT_RGBA32;     break;
    case VIDEO_PALETTE_YUV420P: client->frameSize = YUV420P_LEN(vWin.width, vWin.height); client->format = PIX_FMT_YUV420P;    break;
    case VIDEO_PALETTE_YUV422P: client->frameSize = YUV422P_LEN(vWin.width, vWin.height); client->format = PIX_FMT_YUV422P;    break;
    default:
        cerr << "SIP: Attempt to register unsupported Webcam format\n";
        delete client;
        return 0;
    }

    // Create some buffers for the client
    for (int i=0; i<WC_CLIENT_BUFFERS; i++)
        client->BufferList.append(new unsigned char[client->frameSize]);

    WebcamLock.lock();
    wcClientList.append(client);
    WebcamLock.unlock();

    return client;
}

void Webcam::UnregisterClient(wcClient *client)
{
    WebcamLock.lock();
    wcClientList.remove(client);
    WebcamLock.unlock();

    // Delete client buffers
    unsigned char *it;
    while ((it=client->BufferList.first()) != 0)
    {
        client->BufferList.remove(it);
        delete it;
    }

    // Delete client buffers in the FULL queue
    while ((it=client->FullBufferList.first()) != 0)
    {
        client->FullBufferList.remove(it);
        delete it;
    }

    if (actualFps < client->fps)
        cerr << "Client wanted a FPS of " << client->fps << " but the camera delivered " << actualFps << endl;

    delete client;
}

unsigned char *Webcam::GetVideoFrame(wcClient *client)
{
    WebcamLock.lock();
    unsigned char *buffer = client->FullBufferList.first();
    if (buffer)
        client->FullBufferList.remove(buffer);
    WebcamLock.unlock();
    return buffer;
}

void Webcam::FreeVideoBuffer(wcClient *client, unsigned char *buffer)
{
    WebcamLock.lock();
    if (buffer)
        client->BufferList.append(buffer);
    WebcamLock.unlock();
}

void Webcam::StartThread()
{
    killWebcamThread = false;
    pthread_create(&webcamthread, NULL, WebcamThread, this);
}

void Webcam::KillThread()
{
    if (!killWebcamThread) // Is the thread even running?
    {
        killWebcamThread = true;
        pthread_join(webcamthread, NULL);
    }
}

void *Webcam::WebcamThread(void *p)
{
    Webcam *me = (Webcam *)p;
    me->WebcamThreadWorker();
    return NULL;
}

void Webcam::WebcamThreadWorker()
{
    int len=0;
    int wcFormat = 0;
    QTime cameraStartTime = QTime::currentTime();
    QTime cameraTime;
    int frameCount = 0;
    int totalCaptureMs = 0;

    switch(GetPalette())
    {
    case VIDEO_PALETTE_YUV420P:    wcFormat = PIX_FMT_YUV420P;    break;
    case VIDEO_PALETTE_YUV422P:    wcFormat = PIX_FMT_YUV422P;    break;
    case VIDEO_PALETTE_RGB24:      wcFormat = PIX_FMT_RGB24;      break;
    case VIDEO_PALETTE_RGB32:      wcFormat = PIX_FMT_RGBA32;      break;
    default:
        cerr << "Webcam: Unsupported palette mode " << GetPalette() << endl; // Should not get here, caught earlier
        return;
        break;
    }

    while((!killWebcamThread) && (hDev > 0))
    {
        if ((len = read(hDev, picbuff1, frameSize)) == frameSize)
        {
            WebcamLock.lock(); // Prevent changes to client registration structures whilst processing

            // Capture info to work out camera FPS
            if (frameCount++ > 0)
                totalCaptureMs += cameraTime.msecsTo(QTime::currentTime());
            cameraTime = QTime::currentTime();
            if (totalCaptureMs != 0)
                actualFps = (frameCount*1000)/totalCaptureMs;

            // Format convert for each registered client.  Note this is optimised for not having
            // multiple clients requesting the same format, as that is unexpected
            wcClient *it;
            for (it=wcClientList.first(); it; it=wcClientList.next())
            {
                // Meet the FPS rate of the requesting client
                if ((it->timeLastCapture).msecsTo(QTime::currentTime()) > it->interframeTime)
                {
                    // Get a buffer for the frame
                    unsigned char *buffer = it->BufferList.first();
                    if (buffer != 0)
                    {
                        it->BufferList.remove(buffer);
                        it->FullBufferList.append(buffer);

                        // Format conversion
                        if (wcFormat != it->format)
                        {                   
                            AVPicture image_in, image_out;
                            avpicture_fill(&image_in,  (uint8_t *)picbuff1, wcFormat, vWin.width, vWin.height);
                            avpicture_fill(&image_out, (uint8_t *)buffer, it->format, vWin.width, vWin.height);
                            img_convert(&image_out, it->format, &image_in, wcFormat, vWin.width, vWin.height);
                            
                            QApplication::postEvent(it->eventWindow, new WebcamEvent(WebcamEvent::FrameReady, it));
                        }
                        else
                        {
                            memcpy(buffer, picbuff1, len);
                            QApplication::postEvent(it->eventWindow, new WebcamEvent(WebcamEvent::FrameReady, it));
                        }
                    }
                    else
                        cerr << "No webcam buffers\n";

                    it->timeLastCapture = QTime::currentTime();
                }
            }

            WebcamLock.unlock();
        }
        else
            cerr << "Error reading from webcam; got " << len << " bytes; expected " << frameSize << endl;
    }
}

Webcam::~Webcam()
{
  if (hDev > 0)
    camClose();
}

