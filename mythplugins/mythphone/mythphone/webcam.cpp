/*
	webcam.cpp

	(c) 2003 Paul Volkaerts

    Webcam control and capture
*/
#include <qapplication.h>

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;

#ifndef WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev.h>
#include <mythtv/mythcontext.h>

#include "config.h"
#else

#include <windows.h>
#include "vfw.h"
#endif

#include "h263.h"
#include "webcam.h"

#ifdef WIN32
QWidget *wcMainWidget;
HWND wcMainHwnd;
#endif

Webcam::Webcam(QWidget *parent, QWidget *localVideoWidget)
{
    hDev = 0;
    DevName = "";
    picbuff1 = 0;
    imageLen = 0;
    frameSize = 0;
    fps = 5;
    killWebcamThread = true; // Leave true whilst its not running
    wcFormat = 0;
    wcFlip = false;

#ifndef WIN32
    (void)parent;
    (void)localVideoWidget;
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

#else
    wcMainWidget = parent;
    hwndWebcam = localVideoWidget->winId();
    wcMainHwnd = hwndCap = capCreateCaptureWindow(NULL, WS_CHILD | WS_VISIBLE, 2, 2, 
                                     localVideoWidget->width()-4, 
                                     localVideoWidget->height()-4, 
                                     hwndWebcam, 1);

    capSetCallbackOnVideoStream(hwndCap, frameReadyCallbackProc);
    capSetCallbackOnError(hwndCap, ErrorCallbackProc) ;
    capSetCallbackOnStatus(hwndCap, StatusCallbackProc) ;
    capDriverConnect(hwndCap, 0);
    capSetUserData(hwndCap, (long)this);
#endif
}


QString Webcam::devName(QString WebcamName)
{
#ifndef WIN32
  int handle = open(WebcamName, O_RDWR);
  if (handle <= 0)
    return "";
  
  struct video_capability tempCaps;
  ioctl(handle, VIDIOCGCAP, &tempCaps);
  ::close(handle);
  return tempCaps.name;
#else
  return "WIN32 Webcam (TODO)"; // TODO
#endif
}


bool Webcam::camOpen(QString WebcamName, int width, int height)
{
    bool opened=true;
#ifdef WIN32
    capPreviewRate(hwndCap, fps); 
    capPreviewScale(hwndCap, true);
    capPreview(hwndCap, true);

    capCaptureGetSetup(hwndCap, &capParams, sizeof(capParams));
    capParams.fYield = true;
    capParams.fAbortLeftMouse = false;
    capParams.fAbortRightMouse = false;
    capParams.fLimitEnabled = false;
    capCaptureSetSetup(hwndCap, &capParams, sizeof(capParams));

    capCaptureSequenceNoFile(hwndCap);
#else

    DevName = WebcamName;

    if ((hDev <= 0) && (WebcamName.length() > 0))
        hDev = open(DevName, O_RDWR);
    if ((hDev <= 0) || (WebcamName.length() <= 0))
    {
        cerr << "Couldn't open camera " << DevName << endl;
        opened = false;
    }
#endif
    if (opened)
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

        // Counters to monitor frame rate
        frameCount = 0;
        totalCaptureMs = 0;

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
            case VIDEO_PALETTE_RGB24:   frameSize = RGB24_LEN(WCWIDTH, WCHEIGHT);   break;
            case VIDEO_PALETTE_RGB32:   frameSize = RGB32_LEN(WCWIDTH, WCHEIGHT);   break;
            case VIDEO_PALETTE_YUV420P: frameSize = YUV420P_LEN(WCWIDTH, WCHEIGHT); break;
            case VIDEO_PALETTE_YUV422P: frameSize = YUV422P_LEN(WCWIDTH, WCHEIGHT); break;
            default:
                cerr << "Palette mode " << GetPalette() << " not yet supported" << endl;
                camClose();
                return false;
                break;
            }

            picbuff1 = new unsigned char [frameSize];
        }

        switch(GetPalette())
        {
        case VIDEO_PALETTE_YUV420P:    wcFormat = PIX_FMT_YUV420P;    break;
        case VIDEO_PALETTE_YUV422P:    wcFormat = PIX_FMT_YUV422P;    break;
        case VIDEO_PALETTE_RGB24:      wcFormat = PIX_FMT_BGR24;      break;
        case VIDEO_PALETTE_RGB32:      wcFormat = PIX_FMT_RGBA32;     break;
        default:
            cerr << "Webcam: Unsupported palette mode " << GetPalette() << endl; // Should not get here, caught earlier
            camClose();
            return false;
            break;
        }

        StartThread();
    }
    return opened;
}


void Webcam::camClose()
{
    KillThread();

#ifndef WIN32
    if (hDev <= 0)
        cerr << "Can't close a camera that isn't open" << endl;
    else
    {
        // There must be a widget procedure called close so make
        // sure we call the proper one. Screwed me up for ages!
        ::close(hDev);
        hDev = 0;
    }
#else
    capCaptureStop(hwndCap);
    capPreview(hwndCap, false);
#endif

    if (picbuff1)
        delete picbuff1;

    picbuff1 = 0;
}


#ifdef WIN32

LRESULT CALLBACK Webcam::frameReadyCallbackProc(HWND hWnd, LPVIDEOHDR lpVHdr)
{
    Webcam *wc = (Webcam *)(capGetUserData(wcMainHwnd));
    if (wc)
    {
        wc->ProcessFrame(lpVHdr->lpData, lpVHdr->dwBytesUsed);
        return (LRESULT) true;
    }
    else
        return (LRESULT) false;
}

LRESULT CALLBACK Webcam::ErrorCallbackProc(HWND hWnd, int nErrID, LPSTR lpErrorText)
{
    QString dbg = QString("Webcam Error: ") + QString::number(nErrID) + " " + lpErrorText;
    QApplication::postEvent(wcMainWidget, new WebcamEvent(WebcamEvent::WebcamErrorEv, dbg));
    return (LRESULT) true;
}

LRESULT CALLBACK Webcam::StatusCallbackProc(HWND hWnd, int nID, LPSTR lpStatusText)
{
    QString dbg = QString("Webcam Status: ") + QString::number(nID) + " " + lpStatusText;
    QApplication::postEvent(wcMainWidget, new WebcamEvent(WebcamEvent::WebcamDebugEv, dbg));
    return (LRESULT) true;
}

#endif



void Webcam::readCaps()
{
#ifndef WIN32
    if (hDev > 0)
    {
        ioctl(hDev, VIDIOCGCAP, &vCaps);
        ioctl(hDev, VIDIOCGWIN, &vWin);
        ioctl(hDev, VIDIOCGPICT, &vPic);
    }
#else
    capCaptureGetSetup(hwndCap, &capParams, sizeof(capParams));
    capGetVideoFormat(hwndCap, &bitmapInfo, sizeof(bitmapInfo));
#endif
}


void Webcam::SetSize(int width, int height)
{
    // Note you can't call this whilst the webcam is open because all the buffers will be the wrong size
#ifndef WIN32
    memset(&vWin, 0, sizeof(struct video_window));
    vWin.width = width;
    vWin.height = height;

    if (ioctl(hDev, VIDIOCSWIN, &vWin) == -1)
        cerr << "Webcam: Error setting capture size " << width << "x" << height << endl;
#else
    bitmapInfo.bmiHeader.biHeight = height;
    bitmapInfo.bmiHeader.biWidth = width;
    capSetVideoFormat(hwndCap, &bitmapInfo, sizeof(bitmapInfo));
#endif

    readCaps();
}


bool Webcam::SetPalette(unsigned int palette)
{
    int depth;

    switch(palette)
    {
    case VIDEO_PALETTE_YUV420P: depth = 12;  break;
    case VIDEO_PALETTE_YUV422:  depth = 16;  break;
    case VIDEO_PALETTE_YUV422P: depth = 16;  break;
    case VIDEO_PALETTE_RGB32:   depth = 32;  break;
    case VIDEO_PALETTE_RGB24:   depth = 24;  break;
    default:                    depth = 0;   break;
    }

#ifndef WIN32
    vPic.palette = palette;
    vPic.depth = depth;
    ioctl(hDev, VIDIOCSPICT, &vPic);
#else
    int winPalette = 0;
    switch(palette)
    {
    default:                    
    case VIDEO_PALETTE_YUV420P: winPalette = (MAKEFOURCC('I', 'Y', 'U', 'V'));   break;
    case VIDEO_PALETTE_YUV422:  winPalette = (MAKEFOURCC('U', 'Y', 'V', 'Y'));   break;
    case VIDEO_PALETTE_YUV422P: winPalette = (MAKEFOURCC('Y', 'V', '1', '6'));   break;
    case VIDEO_PALETTE_RGB32:   winPalette = 0;                                  break;
    case VIDEO_PALETTE_RGB24:   winPalette = 0;                                  break;
    }
    bitmapInfo.bmiHeader.biCompression = winPalette;
    bitmapInfo.bmiHeader.biBitCount = depth;
    capSetVideoFormat(hwndCap, &bitmapInfo, sizeof(bitmapInfo));
#endif

    readCaps();

#ifndef WIN32
    return (vPic.palette == palette ? true : false);
#else
    return ((bitmapInfo.bmiHeader.biCompression == winPalette) && (bitmapInfo.bmiHeader.biBitCount == depth) ? true : false);
#endif
}


unsigned int Webcam::GetPalette(void) 
{
#ifndef WIN32
    return (vPic.palette);
#else
    int winPalette = 0;
    switch(bitmapInfo.bmiHeader.biCompression)
    {
    case MAKEFOURCC('I', 'Y', 'U', 'V'):    return VIDEO_PALETTE_YUV420P;
    case MAKEFOURCC('U', 'Y', 'V', 'Y'):    return VIDEO_PALETTE_YUV422;
    case MAKEFOURCC('Y', 'V', '1', '6'):    return VIDEO_PALETTE_YUV422P;
    default:                    
    case 0:
        if (bitmapInfo.bmiHeader.biBitCount == 24)
            return VIDEO_PALETTE_RGB24;
        else if (bitmapInfo.bmiHeader.biBitCount == 32)
            return VIDEO_PALETTE_RGB32;
    }
    return 0;
#endif
}


#ifndef WIN32

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

#endif


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

void Webcam::GetMaxSize(int *x, int *y)
{
#ifdef WIN32
    *y=bitmapInfo.bmiHeader.biHeight; *x=bitmapInfo.bmiHeader.biWidth;
#else
    *y=vCaps.maxheight; *x=vCaps.maxwidth; 
#endif
};

void Webcam::GetMinSize(int *x, int *y)
{
#ifdef WIN32
    *y=bitmapInfo.bmiHeader.biHeight; *x=bitmapInfo.bmiHeader.biWidth;
#else
    *y=vCaps.minheight; *x=vCaps.minwidth; 
#endif
};

void Webcam::GetCurSize(int *x, int *y)
{
    *y = WCHEIGHT; 
    *x = WCWIDTH;
};

int Webcam::isGreyscale()
{
#ifdef WIN32
    return false;
#else
    return ((vCaps.type & VID_TYPE_MONOCHROME) ? true : false); 
#endif
};



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
    client->framesDelivered = 0;

    switch (format)
    {
    case VIDEO_PALETTE_RGB24:   client->frameSize = RGB24_LEN(WCWIDTH, WCHEIGHT);   client->format = PIX_FMT_BGR24;      break;
    case VIDEO_PALETTE_RGB32:   client->frameSize = RGB32_LEN(WCWIDTH, WCHEIGHT);   client->format = PIX_FMT_RGBA32;     break;
    case VIDEO_PALETTE_YUV420P: client->frameSize = YUV420P_LEN(WCWIDTH, WCHEIGHT); client->format = PIX_FMT_YUV420P;    break;
    case VIDEO_PALETTE_YUV422P: client->frameSize = YUV422P_LEN(WCWIDTH, WCHEIGHT); client->format = PIX_FMT_YUV422P;    break;
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

void Webcam::ChangeClientFps(wcClient *client, int fps)
{
    if (client == 0)
        return;
        
    if (fps == 0)
    {
        fps = 10;
        cerr << "Webcam requested fps of zero\n";
    }

    WebcamLock.lock();
    client->fps = fps;
    client->actualFps = fps;
    client->interframeTime = 1000/fps;
    WebcamLock.unlock();
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
#ifndef WIN32
    killWebcamThread = false;
    start();
#endif
}

void Webcam::KillThread()
{
#ifndef WIN32
    if (!killWebcamThread) // Is the thread even running?
    {
        killWebcamThread = true;
        if (!wait(2000))
        {
            terminate();
            wait();
            cout << "SIP Webcam thread failed to terminate gracefully and was killed\n";
        }
    }
#endif
}

void Webcam::run(void)
{
    WebcamThreadWorker();
}

void Webcam::WebcamThreadWorker()
{
#ifndef WIN32
    int len=0;

    while((!killWebcamThread) && (hDev > 0))
    {
        if ((len = read(hDev, picbuff1, frameSize)) == frameSize)
        {
            if (killWebcamThread)
                break;
                
            ProcessFrame(picbuff1, frameSize);
        }
        else
            cerr << "Error reading from webcam; got " << len << " bytes; expected " << frameSize << endl;
    }
#endif
}

void Webcam::ProcessFrame(unsigned char *frame, int fSize)
{
    static unsigned char tempBuffer[MAX_RGB_704_576];

    WebcamLock.lock(); // Prevent changes to client registration structures whilst processing

    // Capture info to work out camera FPS
    if (frameCount++ > 0)
        totalCaptureMs += cameraTime.msecsTo(QTime::currentTime());
    cameraTime = QTime::currentTime();
    if (totalCaptureMs != 0)
        actualFps = (frameCount*1000)/totalCaptureMs;

    // Check if the webcam needs flipped (some webcams deliver pics upside down)
    if (wcFlip)
    {
        switch(wcFormat)
        {
        case PIX_FMT_YUV420P:   
            flipYuv420pImage(frame, WCWIDTH, WCHEIGHT, tempBuffer);     
            frame = tempBuffer;
            break;
        case PIX_FMT_YUV422P:   
            flipYuv422pImage(frame, WCWIDTH, WCHEIGHT, tempBuffer);     
            frame = tempBuffer;
            break;
        case PIX_FMT_RGBA32:    
            flipRgb32Image(frame, WCWIDTH, WCHEIGHT, tempBuffer);   
            frame = tempBuffer;
            break;
        case PIX_FMT_BGR24:    
        case PIX_FMT_RGB24:    
            flipRgb24Image(frame, WCWIDTH, WCHEIGHT, tempBuffer);   
            frame = tempBuffer;
            break;
        default:
            cout << "No routine to flip this type\n";
            break;
        }
    }

    // Format convert for each registered client.  Note this is optimised for not having
    // multiple clients requesting the same format, as that is unexpected
    wcClient *it;
    for (it=wcClientList.first(); it; it=wcClientList.next())
    {
        // Meet the FPS rate of the requesting client
        if ((it->timeLastCapture).msecsTo(QTime::currentTime()) > it->interframeTime)
        {
            // Get a buffer for the frame. If no "free" buffers try and reused an old one
            unsigned char *buffer = it->BufferList.first();
            if (buffer != 0)
            {
                it->BufferList.remove(buffer);
                it->FullBufferList.append(buffer);
            }
            else
                buffer = it->FullBufferList.first();

            if (buffer != 0)
            {
                it->framesDelivered++;

                // Format conversion
                if (wcFormat != it->format)
                {                   
                    AVPicture image_in, image_out;
                    avpicture_fill(&image_in,  (uint8_t *)frame, wcFormat, WCWIDTH, WCHEIGHT);
                    avpicture_fill(&image_out, (uint8_t *)buffer, it->format, WCWIDTH, WCHEIGHT);
                    img_convert(&image_out, it->format, &image_in, wcFormat, WCWIDTH, WCHEIGHT);
                    
                    QApplication::postEvent(it->eventWindow, new WebcamEvent(WebcamEvent::FrameReady, it));
                }
                else
                {
                    memcpy(buffer, frame, fSize);
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

Webcam::~Webcam()
{
  if (hDev > 0)
    camClose();
}

