/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <time.h>

#include <map>
#include <iostream>
using namespace std;

#include "videoout_xv.h"
#include "../libmyth/util.h"

extern "C" {
#include "../libavcodec/avcodec.h"
}

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#define XMD_H 1
#include <X11/extensions/xf86vmode.h>

#include "yuv2rgb.h"

extern "C" {
extern int      XShmQueryExtension(Display*);
extern int      XShmGetEventBase(Display*);
extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*, int, int, XShmSegmentInfo*);
}

struct XvData
{
    Window XJ_root;
    Window XJ_win;
    Window XJ_curwin;
    GC XJ_gc;
    Screen *XJ_screen;
    Display *XJ_disp;
    XShmSegmentInfo *XJ_SHMInfo;

    map<unsigned char *, XvImage *> buffers;

    XImage *fallbackImage;
};

#define GUID_I420_PLANAR 0x30323449
#define GUID_YV12_PLANAR 0x32315659

int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
  d = d; 
  xeev = xeev;
  return 0;
}

VideoOutputXv::VideoOutputXv(void)
{
    XJ_started = 0; 
    xv_port = -1; 
    scratchspace = NULL; 

    data = new XvData();
}

VideoOutputXv::~VideoOutputXv()
{
    Exit();
    delete data;
}

void VideoOutputXv::InputChanged(int width, int height, float aspect)
{
    pthread_mutex_lock(&lock);

    VideoOutput::InputChanged(width, height, aspect);

    if (xv_port != -1)
    {
        DeleteXvBuffers();
        CreateXvBuffers();
        XFlush(data->XJ_disp);
    }
    else if (use_shm)
    {
        DeleteShmBuffers();
        CreateShmBuffers();
    }
    else
    {
        DeleteXBuffers();
        CreateShmBuffers();
    }

    MoveResize();
    pthread_mutex_unlock(&lock);
}

int VideoOutputXv::GetRefreshRate(void)
{
    if (!XJ_started)
        return -1;

    XF86VidModeModeLine mode_line;
    int dot_clock;

    if (!XF86VidModeGetModeLine(data->XJ_disp, XJ_screen_num, &dot_clock,
                                &mode_line))
        return -1;

    double rate = (double)((double)(dot_clock * 1000.0) /
                           (double)(mode_line.htotal * mode_line.vtotal));

    rate = 1000000.0 / rate;

    return (int)rate;
}

bool VideoOutputXv::Init(int width, int height, float aspect, int num_buffers, 
                         VideoFrame *out_buffers, unsigned int winid,
                         int winx, int winy, int winw, int winh, 
                         unsigned int embedid)
{
    pthread_mutex_init(&lock, NULL);

    int (*old_handler)(Display *, XErrorEvent *);
    int i, ret;
    XJ_caught_error = 0;
  
    unsigned int p_version, p_release, p_request_base, p_event_base, 
                 p_error_base;
    int p_num_adaptors;

    XvAdaptorInfo *ai;

    VideoOutput::Init(width, height, aspect, num_buffers, out_buffers, winid,
                      winx, winy, winw, winh, embedid);

    data->XJ_disp = XOpenDisplay(NULL);
    if (!data->XJ_disp) 
    {
        printf("open display failed\n"); 
        return false;
    }
 
    data->XJ_screen = DefaultScreenOfDisplay(data->XJ_disp);
    XJ_screen_num = DefaultScreen(data->XJ_disp);

    XJ_white = XWhitePixel(data->XJ_disp, XJ_screen_num);
    XJ_black = XBlackPixel(data->XJ_disp, XJ_screen_num);
  
    XJ_fullscreen = 0;
  
    data->XJ_root = DefaultRootWindow(data->XJ_disp);

    ret = XvQueryExtension(data->XJ_disp, &p_version, &p_release, 
                           &p_request_base, &p_event_base, &p_error_base);
    if (ret != Success) 
    {
        printf("XvQueryExtension failed.\n");
    }

    ai = NULL;
    xv_port = -1;
    ret = XvQueryAdaptors(data->XJ_disp, data->XJ_root,
                          (unsigned int *)&p_num_adaptors, &ai);

    if (ret != Success) 
    {
        printf("XvQueryAdaptors failed.\n");
        ai = NULL;
    }
    else
        if ( ai )
        {
            for (i = 0; i < p_num_adaptors; i++) 
            {
                if ((ai[i].type & XvInputMask) &&
                    (ai[i].type & XvImageMask))
                {
                    xv_port = ai[i].base_id;
                    break;
                }
            }
 
            if (p_num_adaptors > 0)
                XvFreeAdaptorInfo(ai);
        }

    use_shm = 0;
    char *dispname = DisplayString(data->XJ_disp);

    if ((dispname) &&
        (*dispname == ':'))
        use_shm = XShmQueryExtension(data->XJ_disp);

    // can be used to force non-Xv mode as well as non-Xv/non-Shm mode
    if (getenv("NO_XV"))
    {
        xv_port = -1;
    }
    if (getenv("NO_SHM"))
    {
        xv_port = -1;
        use_shm = 0;
    }

#ifndef QWS
    GetMythTVGeometry(data->XJ_disp, XJ_screen_num,
                      &XJ_screenx, &XJ_screeny, 
                      &XJ_screenwidth, &XJ_screenheight);
#endif

    if (winid <= 0)
    {
        cerr << "Bad winid given to output\n";
        return false;
    }

    data->XJ_curwin = data->XJ_win = winid;
    
    if (embedid > 0)
        data->XJ_curwin = data->XJ_win = embedid;

    old_handler = XSetErrorHandler(XJ_error_catcher);
    XSync(data->XJ_disp, 0);

    if (xv_port == -1)
    {
        printf("***\n");
        printf("* Couldn't find Xv support, falling back to non-Xv mode.\n");
        printf("* MythTV performance will be much slower since color\n");
        printf("* conversion and scaling will be done in software.\n");
        printf("* Consider upgrading your video card or X server if\n");
        printf("* you would like better performance.\n");

        if (!use_shm)
        {
            printf("***\n" );
            printf("* No XShm support found, MythTV may be very slow and "
                    "consume lots of cpu.\n");
        }
    }

    int formats;
    XvImageFormatValues *fo;
    bool foundimageformat = false;

    if (xv_port != -1)
    {
        fo = XvListImageFormats(data->XJ_disp, xv_port, &formats);
        for (i = 0; i < formats; i++)
        {
            if (fo[i].id == GUID_I420_PLANAR)
            {
                foundimageformat = true;
                colorid = GUID_I420_PLANAR;
            }
        }

        if (!foundimageformat)
        {
            for (i = 0; i < formats; i++)
            {
                if (fo[i].id == GUID_YV12_PLANAR)
                {
                    foundimageformat = true;
                    colorid = GUID_YV12_PLANAR;
                }
            }
        } 

        if (!foundimageformat)
        {
            printf("Couldn't find the proper Xv image format\n");
            exit(0);
        }

        if (fo)
            XFree(fo);

        printf("Using XV port %d\n", xv_port);
        XvGrabPort(data->XJ_disp, xv_port, CurrentTime);
    }

    data->XJ_gc = XCreateGC(data->XJ_disp, data->XJ_win, 0, 0);
    XJ_depth = DefaultDepthOfScreen(data->XJ_screen);

    if (xv_port != -1)
    {
        if (!CreateXvBuffers())
            return false;
    }
    else if (use_shm)
    {
        if (!CreateShmBuffers())
            return false;
    }
    else
    {
        if (!CreateXBuffers())
            return false;
    }

    XSetErrorHandler(old_handler);

    if (XJ_caught_error) 
    {
        return false;
    }

    MoveResize();
  
    XJ_started = true;

    return true;
}

bool VideoOutputXv::CreateXvBuffers(void)
{
    data->XJ_SHMInfo = new XShmSegmentInfo[numbuffers];
    for (int i = 0; i < numbuffers; i++)
    {
        XvImage *image = XvShmCreateImage(data->XJ_disp, xv_port,
                                          colorid, 0, XJ_width, XJ_height,
                                          &(data->XJ_SHMInfo)[i]);

        (data->XJ_SHMInfo)[i].shmid = shmget(IPC_PRIVATE, image->data_size,
                                             IPC_CREAT|0777);

        if ((data->XJ_SHMInfo)[i].shmid < 0)
        {
            perror("shmget failed:");
            return false;
        }

        image->data = (data->XJ_SHMInfo)[i].shmaddr =
                               (char *)shmat((data->XJ_SHMInfo)[i].shmid, 0, 0);

        // mark for delete immediately - it won't be removed until detach
        shmctl((data->XJ_SHMInfo)[i].shmid, IPC_RMID, 0);

        data->buffers[(unsigned char *)image->data] = image;

        (data->XJ_SHMInfo)[i].readOnly = False;

        XShmAttach(data->XJ_disp, &(data->XJ_SHMInfo)[i]);

        videoframes[i].buf = (unsigned char *)image->data;
        videoframes[i].height = XJ_height;
        videoframes[i].width = XJ_width;
        videoframes[i].bpp = 12;
        videoframes[i].size = XJ_height * XJ_width * 3 / 2;
        videoframes[i].codec = FMT_YV12;
    }

    XSync(data->XJ_disp, 0);

    if (colorid != GUID_I420_PLANAR)
        scratchspace = new unsigned char[XJ_width * XJ_height * 3 / 2];
    else
        scratchspace = NULL;

    return true;
}

bool VideoOutputXv::CreateShmBuffers(void)
{
    data->XJ_SHMInfo = new XShmSegmentInfo[1];

    XImage *image = XShmCreateImage(data->XJ_disp,
                                    DefaultVisual(data->XJ_disp, XJ_screen_num),
                                    XJ_depth, ZPixmap, 0, 
                                    &(data->XJ_SHMInfo)[0],
                                    dispw, disph);

    (data->XJ_SHMInfo)[0].shmid = shmget(IPC_PRIVATE, image->bytes_per_line
                                         * image->height, IPC_CREAT|0777);

    if ((data->XJ_SHMInfo)[0].shmid < 0)
    {
        perror("shmget failed:");
        return false;
    }

    image->data = (data->XJ_SHMInfo)[0].shmaddr =
                              (char *)shmat((data->XJ_SHMInfo)[0].shmid, 0, 0);

    // mark for delete immediately - it won't be removed until detach
    shmctl((data->XJ_SHMInfo)[0].shmid, IPC_RMID, 0);

    data->fallbackImage = image;

    (data->XJ_SHMInfo)[0].readOnly = False;

    if (!XShmAttach(data->XJ_disp, &(data->XJ_SHMInfo)[0]))
    {
        perror("XShmAttach() failed:");
        return false;
    }

    for (int i = 0; i < numbuffers; i++)
    {
        videoframes[i].height = XJ_height;
        videoframes[i].width = XJ_width;
        videoframes[i].bpp = 12;
        videoframes[i].size = XJ_height * XJ_width * 3 / 2;
        videoframes[i].codec = FMT_YV12;
        videoframes[i].buf = new unsigned char[videoframes[i].size];
    }

    XSync(data->XJ_disp, 0);

    return true;
}

bool VideoOutputXv::CreateXBuffers(void)
{
    char *sbuf = new char[XJ_depth / 8 * XJ_screenwidth * XJ_screenheight];

    XImage *image = XCreateImage(data->XJ_disp,
                                 DefaultVisual(data->XJ_disp, 0),
                                 XJ_depth, ZPixmap, 0, sbuf,
                                 dispw, disph, XJ_depth, 0);

    data->fallbackImage = image;

    for (int i = 0; i < numbuffers; i++)
    {
        videoframes[i].height = XJ_height;
        videoframes[i].width = XJ_width;
        videoframes[i].bpp = 12;
        videoframes[i].size = XJ_height * XJ_width * 3 / 2;
        videoframes[i].codec = FMT_YV12;
        videoframes[i].buf = new unsigned char[videoframes[i].size];
    }

    XSync(data->XJ_disp, 0);
    return true;
}

void VideoOutputXv::Exit(void)
{
    if (XJ_started) 
    {
        XJ_started = false;

        if ( xv_port != -1 )
        {
            DeleteXvBuffers();
            XvUngrabPort(data->XJ_disp, xv_port, CurrentTime);
        }
        else if (use_shm)
            DeleteShmBuffers();
        else
            DeleteXBuffers();

        XFreeGC(data->XJ_disp, data->XJ_gc);
        XCloseDisplay(data->XJ_disp);
    }
}

void VideoOutputXv::DeleteXvBuffers()
{
    map<unsigned char *, XvImage *>::iterator iter = data->buffers.begin();

    for(int i=0; iter != data->buffers.end(); ++iter, ++i)
    {
        XShmDetach(data->XJ_disp, &(data->XJ_SHMInfo)[i]);

        if ((data->XJ_SHMInfo)[i].shmaddr)
            shmdt((data->XJ_SHMInfo)[i].shmaddr);

        XFree(iter->second);
    }

    delete [] (data->XJ_SHMInfo);

    if (scratchspace)
        delete [] scratchspace;
    scratchspace = NULL;

    data->buffers.clear();
}

void VideoOutputXv::DeleteShmBuffers()
{
    XShmDetach(data->XJ_disp, &(data->XJ_SHMInfo)[0]);
    if ((data->XJ_SHMInfo)[0].shmaddr)
        shmdt((data->XJ_SHMInfo)[0].shmaddr);
    if ((data->XJ_SHMInfo)[0].shmid > 0)
        shmctl((data->XJ_SHMInfo)[0].shmid, IPC_RMID, 0);
    XFree(data->fallbackImage);

    delete [] (data->XJ_SHMInfo);

    for (int i = 0; i < numbuffers; i++)
    {
        delete [] videoframes[i].buf;
        videoframes[i].buf = NULL;
    }
}

void VideoOutputXv::DeleteXBuffers()
{
    XFree(data->fallbackImage);

    for (int i = 0; i < numbuffers; i++)
    {
        delete [] videoframes[i].buf;
        videoframes[i].buf = NULL;
    }
}

void VideoOutputXv::EmbedInWidget(unsigned long wid, int x, int y, int w, int h)
{
    if (embedding)
        return;

    pthread_mutex_lock(&lock);
    data->XJ_curwin = wid;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);

    pthread_mutex_unlock(&lock);
}
 
void VideoOutputXv::StopEmbedding(void)
{
    if (!embedding)
        return;

    pthread_mutex_lock(&lock);

    data->XJ_curwin = data->XJ_win;
    VideoOutput::StopEmbedding();

    pthread_mutex_unlock(&lock);
}

void VideoOutputXv::PrepareFrame(VideoFrame *buffer)
{
    if (xv_port != -1)
    {
        pthread_mutex_lock(&lock);

        XvImage *image = data->buffers[buffer->buf];

        if (!image)
        {
            pthread_mutex_unlock(&lock);
            return;
        }

        if (colorid == GUID_YV12_PLANAR)
        {
            int width = buffer->width;
            int height = buffer->height;

            memcpy(scratchspace, (unsigned char *)image->data + 
                   (width * height), width * height / 4);
            memcpy((unsigned char *)image->data + (width * height),
                   (unsigned char *)image->data + (width * height) * 5 / 4,
                   width * height / 4);
            memcpy((unsigned char *)image->data + (width * height) * 5 / 4,
                   scratchspace, width * height / 4);
        }

        XvShmPutImage(data->XJ_disp, xv_port, data->XJ_curwin, data->XJ_gc,
                      image, imgx, imgy, imgw, imgh, dispxoff, dispyoff,
                      dispwoff, disphoff, False);

        pthread_mutex_unlock(&lock);
    }
    else
    {
        static long long framesShown = 0;
        static int showFrame = 1;
        static int fps = 0;
        static time_t stop_time;

        // bad way to throttle frame display for non-Xv mode.
        // calculate fps we can do and skip enough frames so we don't exceed.
        if (framesShown == 0)
            stop_time = time(NULL) + 4;

        if ((!fps) && (time(NULL) > stop_time))
        {
            fps = (int)(framesShown / 4);

            if (fps < 25)
            {
                showFrame = 120 / framesShown + 1;
                printf("***\n" );
                printf("* Your system is not capable of displaying the\n");
                printf("* full framerate at %dx%d resolution.  Frames\n",
                       dispw, disph );
                printf("* will be skipped in order to keep the audio and\n");
                printf("* video in sync.\n");
            }
        }

        framesShown++;

        if ((showFrame != 1) && (framesShown % showFrame))
            return;

        unsigned char *sbuf = new unsigned char[dispw * disph * 3 / 2];
        AVPicture image_in, image_out;
        XImage *image = data->fallbackImage;
        ImgReSampleContext *scontext;
        int av_format;
        int width = buffer->width;
        int height = buffer->height;

        avpicture_fill(&image_out, (uint8_t *)sbuf, PIX_FMT_YUV420P,
                       dispw, disph);

        if ((dispw == width) && (disph == height))
        {
            memcpy(sbuf, buffer->buf, width * height * 3 / 2);
        }
        else
        {
            avpicture_fill(&image_in, buffer->buf, PIX_FMT_YUV420P,
                           width, height);
            scontext = img_resample_init(dispw, disph, width, height);
            img_resample(scontext, &image_out, &image_in);
        }

        switch (data->fallbackImage->bits_per_pixel)
        {
            case 16: av_format = PIX_FMT_RGB565; break;
            case 24: av_format = PIX_FMT_RGB24;  break;
            case 32: av_format = PIX_FMT_RGBA32; break;
            default: cerr << "Non Xv mode only supports 16, 24, and 32 bpp "
                         "displays\n";
                     exit(0);
        }

        avpicture_fill(&image_in, (uint8_t *)image->data, 
                       av_format, dispw, disph);

        img_convert(&image_in, av_format, &image_out, PIX_FMT_YUV420P,
                    dispw, disph);

        pthread_mutex_lock(&lock);

        if (use_shm)
            XShmPutImage(data->XJ_disp, data->XJ_curwin, data->XJ_gc, image,
                         0, 0, 0, 0, dispw, disph, False );
        else
            XPutImage(data->XJ_disp, data->XJ_curwin, data->XJ_gc, image, 
                      0, 0, 0, 0, dispw, disph );

        pthread_mutex_unlock(&lock);

        delete [] sbuf;
    }
}

void VideoOutputXv::Show()
{
    XSync(data->XJ_disp, False);
}
