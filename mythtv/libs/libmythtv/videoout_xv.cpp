/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cmath>
#include <ctime>

#include <map>
#include <iostream>
using namespace std;

#include "videoout_xv.h"
#include "osdsurface.h"
#include "../libmyth/util.h"
#include "mythcontext.h"
#include "filtermanager.h"

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
#include <X11/extensions/Xinerama.h>

    extern int      XShmQueryExtension(Display*);
    extern int      XShmGetEventBase(Display*);
    extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*, int, int, XShmSegmentInfo*);
}

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

struct XvData
{
    Window XJ_root;
    Window XJ_win;
    Window XJ_curwin;
    GC XJ_gc;
    Screen *XJ_screen;
    Display *XJ_disp;
    float display_aspect;
    XShmSegmentInfo *XJ_SHMInfo;

    map<unsigned char *, XvImage *> buffers;

    XImage *fallbackImage;

    int colorkey;
    bool needdrawcolor;
    bool initialdraw;
};

#define GUID_I420_PLANAR 0x30323449
#define GUID_YV12_PLANAR 0x32315659

static int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
    d = d; 
    xeev = xeev;
    return 0;
}

VideoOutputXv::VideoOutputXv(void)
             : VideoOutput()
{
    XJ_started = 0; 
    xv_port = -1; 
    scratchspace = NULL; 

    pauseFrame.buf = NULL;
    pauseFrame.qscale_table = NULL;

    data = new XvData();

    framesShown = 0;
    showFrame = 1;
    fps = 0;

    // If using custom display resolutions, display_res will point
    // to a singleton instance of the DisplayRes class
    display_res = DisplayRes::getDisplayRes();
}

VideoOutputXv::~VideoOutputXv()
{
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    if (pauseFrame.qscale_table)
    {
        delete [] pauseFrame.qscale_table;
        pauseFrame.qscale_table = NULL;
    }

    Exit();
    delete data;

    if (display_res)
        // Switch back to desired resolution for GUI
        display_res->switchToGUI();
}

void VideoOutputXv::AspectChanged(float aspect)
{
    pthread_mutex_lock(&lock);

    VideoOutput::AspectChanged(aspect);
    MoveResize();

    pthread_mutex_unlock(&lock);
}

void VideoOutputXv::Zoom(int direction)
{
    pthread_mutex_lock(&lock);

    VideoOutput::Zoom(direction);
    MoveResize();

    pthread_mutex_unlock(&lock);
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
        CreateXBuffers();
    }

    if (display_res && display_res->switchToVid(width, height))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        w_mm =  display_res->Width_mm();
        h_mm =  display_res->Height_mm();

        data->display_aspect = display_res->GetAspectRatio();

        if (!gContext->GetNumSetting("GuiSizeForTV", 0))
        {
            dispx = dispy = 0;
            dispw = display_res->Width();
            disph = display_res->Height();
            // Resize X window to fill new resolution
            XMoveResizeWindow(data->XJ_disp, data->XJ_win,
                              dispx, dispy, dispw, disph);
        }
    }

    MoveResize();

    scratchFrame = &(vbuffers[kNumBuffers]);

    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width = scratchFrame->width;
    pauseFrame.bpp = scratchFrame->bpp;
    pauseFrame.size = scratchFrame->size;
    pauseFrame.buf = new unsigned char[pauseFrame.size];
    pauseFrame.qscale_table = scratchFrame->qscale_table = NULL;
    pauseFrame.qstride = scratchFrame->qstride = 0;
    pauseFrame.frameNumber = scratchFrame->frameNumber;

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

bool VideoOutputXv::Init(int width, int height, float aspect, 
                         WId winid, int winx, int winy, int winw, 
                         int winh, WId embedid)
{
    pthread_mutex_init(&lock, NULL);

    needrepaint = true;
    data->initialdraw = true;

    int (*old_handler)(Display *, XErrorEvent *);
    int i, ret;
    XJ_caught_error = 0;

    unsigned int p_version, p_release, p_request_base, p_event_base, 
                 p_error_base;
    int p_num_adaptors;
    bool usingXinerama;
    int event_base, error_base;

    XvAdaptorInfo *ai;

    VideoOutput::InitBuffers(kNumBuffers, true, kNeedFreeFrames,
                             kPrebufferFramesNormal, kPrebufferFramesSmall, 
                             kKeepPrebuffer);
    VideoOutput::Init(width, height, aspect, winid, winx, winy, winw, winh, 
                      embedid);

    data->XJ_disp = XOpenDisplay(NULL);
    if (!data->XJ_disp) 
    {
        printf("open display failed\n"); 
        return false;
    }

    data->XJ_screen = DefaultScreenOfDisplay(data->XJ_disp);
    XJ_screen_num = DefaultScreen(data->XJ_disp);

    if (winid <= 0)
    {
        cerr << "Bad winid given to output\n";
        return false;
    }

    data->XJ_curwin = data->XJ_win = winid;

    if (display_res)
    {
        // The very first Resize needs to be the maximum possible
        // desired res, because X will mask off anything outside
        // the initial dimensions
        XMoveResizeWindow(data->XJ_disp, data->XJ_win, 0, 0,
                          display_res->GetMaxWidth(),
                          display_res->GetMaxHeight());

        if (display_res->switchToVid(width, height))
        {
            // Switching to custom display resolution succeeded
            // Make a note of the new size
            w_mm = display_res->Width_mm();
            h_mm = display_res->Height_mm();
            
            if (!gContext->GetNumSetting("GuiSizeForTV", 0))
            {
                dispx = dispy = 0;
                dispw = display_res->Width();
                disph = display_res->Height();
                // Resize X window to fill new resolution
                XMoveResizeWindow(data->XJ_disp, data->XJ_win,
                                  dispx, dispy, dispw, disph);
            }
        }
    }
    else
    {
        if (myth_dsw != 0)
            w_mm = myth_dsw;
        else
            w_mm = DisplayWidthMM(data->XJ_disp, XJ_screen_num);

        if (myth_dsh != 0)
            h_mm = myth_dsh;
        else
            h_mm = DisplayHeightMM(data->XJ_disp, XJ_screen_num);
    }

    usingXinerama = 
        (XineramaQueryExtension(data->XJ_disp, &event_base, &error_base) &&
         XineramaIsActive(data->XJ_disp));
    if (w_mm == 0 || h_mm == 0 || usingXinerama)
    {
        w_mm = (int)(300 * XJ_aspect);
        h_mm = 300;
        data->display_aspect = XJ_aspect;
    }
    else if (gContext->GetNumSetting("GuiSizeForTV", 0))
    {
        int w = DisplayWidth(data->XJ_disp, XJ_screen_num);
        int h = DisplayHeight(data->XJ_disp, XJ_screen_num);
        int gui_w = w, gui_h = h;
        gContext->GetResolutionSetting("Gui", gui_w,  gui_h);

        if (gui_w)
            w_mm = w_mm * gui_w / w;
        if (gui_h)
            h_mm = h_mm * gui_h / h;

        data->display_aspect = (float)w_mm/h_mm;
    }
    else
        data->display_aspect = (float)w_mm/h_mm;

    if (display_res)
        data->display_aspect = display_res->GetAspectRatio();

    XJ_white = XWhitePixel(data->XJ_disp, XJ_screen_num);
    XJ_black = XBlackPixel(data->XJ_disp, XJ_screen_num);

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
    else if (ai)
    {
        for (i = 0; (i < p_num_adaptors) && (xv_port == -1); i++) 
        {
            if ((ai[i].type & XvInputMask) &&
                (ai[i].type & XvImageMask))
            {
                for (unsigned int p = ai[i].base_id;
                    p < (ai[i].base_id + ai[i].num_ports);
                    p++)
                {
                    if (XvGrabPort(data->XJ_disp, p, CurrentTime) == Success)
                    {
                        xv_port = p;
                        VERBOSE(VB_GENERAL,
                                QString("Using XV port %1").arg(xv_port));
                        break;
                    }
                }
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

    if (display_res)
    {
        // Using custom, full-screen display resolution
        XJ_screenx = XJ_screeny = 0;
        XJ_screenwidth = display_res->Width();
        XJ_screenheight = display_res->Height();
    }
    else
    {
        // Get default (possibly user selected) screen resolution from context
        float wmult, hmult;

        gContext->GetScreenSettings(XJ_screenx, XJ_screenwidth, wmult,
                                    XJ_screeny, XJ_screenheight, hmult);
    }

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

        if (fo)
            XFree(fo);

        if (!foundimageformat)
        {
            VERBOSE(VB_IMPORTANT, "Couldn't find the proper Xv image format");
            errored = true;
            return false;
        }
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

    scratchFrame = &(vbuffers[kNumBuffers]);

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width = scratchFrame->width;
    pauseFrame.bpp = scratchFrame->bpp;
    pauseFrame.size = scratchFrame->size;
    pauseFrame.buf = new unsigned char[pauseFrame.size];
    pauseFrame.qscale_table = NULL;
    pauseFrame.qstride = 0;
    pauseFrame.frameNumber = scratchFrame->frameNumber;

    InitColorKey(true);
   
    MoveResize();
 
    if (gContext->GetNumSetting("UseOutputPictureControls", 0))
    {
        ChangePictureAttribute(kPictureAttribute_Brightness, brightness);
        ChangePictureAttribute(kPictureAttribute_Contrast, contrast);
        ChangePictureAttribute(kPictureAttribute_Colour, colour);
        ChangePictureAttribute(kPictureAttribute_Hue, hue);
    }

    XJ_started = true;

    return true;
}

void VideoOutputXv::InitColorKey(bool turnoffautopaint)
{
    int ret = Success;
    data->needdrawcolor = true;

    Atom xv_atom;
    XvAttribute *attributes;
    int attrib_count;

    attributes = XvQueryPortAttributes(data->XJ_disp, xv_port, &attrib_count);
    if (attributes)
    {
        for (int i = 0; i < attrib_count; i++)
        {
            if (!strcmp(attributes[i].name, "XV_AUTOPAINT_COLORKEY"))
            {
                xv_atom = XInternAtom(data->XJ_disp, "XV_AUTOPAINT_COLORKEY",
                                      False);
                if (xv_atom == None)
                    continue;

                if (turnoffautopaint)
                    XvSetPortAttribute(data->XJ_disp, xv_port, xv_atom, 0);
                else
                    XvSetPortAttribute(data->XJ_disp, xv_port, xv_atom, 1);
            }
        }
        XFree(attributes);
    }

    if (data->needdrawcolor)
    {
        xv_atom = XInternAtom(data->XJ_disp, "XV_COLORKEY", False);
        if (xv_atom != None)
        {
            ret = XvGetPortAttribute(data->XJ_disp, xv_port, xv_atom, 
                                     &data->colorkey);
            if (ret != Success)
            {
                VERBOSE(VB_IMPORTANT,
                        "Couldn't get the color key color, and we need it.\n"
                        "You likely won't get any video.");
                data->colorkey = 0;
            }
        }
    }
}

bool VideoOutputXv::SetupDeinterlace(bool interlaced)
{
    bool deint = VideoOutput::SetupDeinterlace(interlaced);
    return deint;
}

bool VideoOutputXv::ApproveDeintFilter(const QString& filtername) const
{
    if (filtername == "bobdeint")
        return (xv_port != -1); // FIXME, implement split-frame bob for non-Xv
    return true;
}
bool VideoOutputXv::CreateXvBuffers(void)
{
    data->XJ_SHMInfo = new XShmSegmentInfo[numbuffers + 1];
    for (int i = 0; i < numbuffers + 1; i++)
    {
        XvImage *image = XvShmCreateImage(data->XJ_disp, xv_port,
                                          colorid, 0, XJ_width, XJ_height,
                                          &(data->XJ_SHMInfo)[i]);

        (data->XJ_SHMInfo)[i].shmid = shmget(IPC_PRIVATE, 
                                             image->data_size + 64,
                                             IPC_CREAT|0777);

        if ((data->XJ_SHMInfo)[i].shmid < 0)
        {
            perror("shmget failed:");
            return false;
        }

        image->data = (data->XJ_SHMInfo)[i].shmaddr =
                      (char *)shmat((data->XJ_SHMInfo)[i].shmid, 0, 0);

        data->buffers[(unsigned char *)image->data] = image;

        (data->XJ_SHMInfo)[i].readOnly = False;

        XShmAttach(data->XJ_disp, &(data->XJ_SHMInfo)[i]);

        XSync(data->XJ_disp, 0); // needed for FreeBSD?
        // mark for delete immediately - it won't be removed until detach
        shmctl((data->XJ_SHMInfo)[i].shmid, IPC_RMID, 0);


        vbuffers[i].buf = (unsigned char *)image->data;
        vbuffers[i].height = XJ_height;
        vbuffers[i].width = XJ_width;
        vbuffers[i].bpp = 12;
        vbuffers[i].size = XJ_height * XJ_width * 3 / 2;
        vbuffers[i].codec = FMT_YV12;
        vbuffers[i].qscale_table = NULL;
        vbuffers[i].qstride = 0;
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

    for (int i = 0; i < numbuffers + 1; i++)
    {
        vbuffers[i].height = XJ_height;
        vbuffers[i].width = XJ_width;
        vbuffers[i].bpp = 12;
        vbuffers[i].size = XJ_height * XJ_width * 3 / 2;
        vbuffers[i].codec = FMT_YV12;
        vbuffers[i].buf = new unsigned char[vbuffers[i].size + 64];
        vbuffers[i].qscale_table = NULL;
        vbuffers[i].qstride = 0;
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

    for (int i = 0; i < numbuffers + 1; i++)
    {
        vbuffers[i].height = XJ_height;
        vbuffers[i].width = XJ_width;
        vbuffers[i].bpp = 12;
        vbuffers[i].size = XJ_height * XJ_width * 3 / 2;
        vbuffers[i].codec = FMT_YV12;
        vbuffers[i].buf = new unsigned char[vbuffers[i].size + 64];
        vbuffers[i].qscale_table = NULL;
        vbuffers[i].qstride = 0;
    }

    XSync(data->XJ_disp, 0);
    return true;
}

void VideoOutputXv::Exit(void)
{
    if (XJ_started) 
    {
        XSetForeground(data->XJ_disp, data->XJ_gc, XJ_black);
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc,
                       dispx, dispy, dispw, disph);

        m_deinterlacing = false;
        InitColorKey(false);

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

    for (int i=0; iter != data->buffers.end(); ++iter, ++i)
    {
        XShmDetach(data->XJ_disp, &(data->XJ_SHMInfo)[i]);

        if ((data->XJ_SHMInfo)[i].shmaddr)
            shmdt((data->XJ_SHMInfo)[i].shmaddr);

        XFree(iter->second);
    }

    for (int i = 0; i < numbuffers + 1; i++)
    {
        if (vbuffers[i].qscale_table)
        {
            delete [] vbuffers[i].qscale_table;
            vbuffers[i].qscale_table = NULL;
        }
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

    for (int i = 0; i < numbuffers + 1; i++)
    {
        delete [] vbuffers[i].buf;
        vbuffers[i].buf = NULL;

        if (vbuffers[i].qscale_table)
        {
            delete [] vbuffers[i].qscale_table;
            vbuffers[i].qscale_table = NULL;
        }
    }
}

void VideoOutputXv::DeleteXBuffers()
{
    XFree(data->fallbackImage);

    for (int i = 0; i < numbuffers + 1; i++)
    {
        delete [] vbuffers[i].buf;
        vbuffers[i].buf = NULL;

        if (vbuffers[i].qscale_table)
        {
            delete [] vbuffers[i].qscale_table;
            vbuffers[i].qscale_table = NULL;
        }
    }
}

void VideoOutputXv::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    pthread_mutex_lock(&lock);

    if (embedding)
    {
        MoveResize();
        pthread_mutex_unlock(&lock);
        return;
    }

    data->XJ_curwin = wid;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);

    if (display_res)
    {
        // Switch to resolution of widget
        XWindowAttributes   attr;

        XGetWindowAttributes(data->XJ_disp, wid, &attr);
        display_res->switchToCustom(attr.width, attr.height);
    }

    pthread_mutex_unlock(&lock);
}

void VideoOutputXv::StopEmbedding(void)
{
    if (!embedding)
        return;

    pthread_mutex_lock(&lock);
    data->XJ_curwin = data->XJ_win;
    VideoOutput::StopEmbedding();

    if (display_res)
    {
        // Switch back to resolution for full screen video
        display_res->switchToVid(display_res->vidWidth(),
                                 display_res->vidHeight());
    }

    pthread_mutex_unlock(&lock);
}

void VideoOutputXv::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::PrepareFrame() called while IsErrored is true.");
        return;
    }

    if (!buffer)
        buffer = scratchFrame;

    framesPlayed = buffer->frameNumber + 1;

    if (xv_port != -1)
    {
        pthread_mutex_lock(&lock);

        XvImage *image = data->buffers[buffer->buf];

        if (!image)
        {
            pthread_mutex_unlock(&lock);
            return;
        }

        if (needrepaint)
        {
            data->initialdraw = false;
            DrawUnusedRects(false);
            needrepaint = false;
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

        int src_x = imgx, src_y = imgy, src_w = imgw, src_h = imgh;
        int dest_y = dispyoff;
        bool drawn = false;

        if (m_deinterlacing && (m_deintfiltername == "bobdeint"))
        {
            if ((t == kScan_Interlaced && buffer->top_field_first == 1) ||
                (t == kScan_Intr2ndField && buffer->top_field_first == 0))
            {
                // Show top field
                src_y = imgy / 2;
                src_h = imgh / 2;
                XvShmPutImage(data->XJ_disp, xv_port, data->XJ_curwin,
                              data->XJ_gc, image, src_x, src_y, src_w, src_h,
                              dispxoff, dest_y, dispwoff, disphoff, False);
                drawn = true;
            }
            else if ((t == kScan_Interlaced && buffer->top_field_first == 0) ||
                     (t == kScan_Intr2ndField && buffer->top_field_first == 1))
            {
                // Show bottom field
                src_y = (buffer->height + imgy) / 2;
                src_h = imgh / 2;
                int halfLineSrc = (int)round(((float)disphoff) / imgh - 0.001f);
                src_h -= (halfLineSrc) ? 1 : 0;

//                VERBOSE(VB_PLAYBACK,
//                        QString("XvPut img_h(%1-%6) dest y(%2+%4) h(%3-%5)")
//                        .arg(imgh).arg(dest_y).arg(disphoff)
//                        .arg(halfLineSrc).arg(2 * halfLineSrc)
//                        .arg(2*(halfLineSrc ? 1 : 0))); // 2* for imgh coords

                XvShmPutImage(data->XJ_disp, xv_port, data->XJ_curwin,
                              data->XJ_gc, image, src_x, src_y, src_w, src_h,
                              dispxoff, dest_y   + halfLineSrc,
                              dispwoff, disphoff - 2 * halfLineSrc, False);
                drawn = true;
            }
        }

        if (!drawn)
        {
            XvShmPutImage(data->XJ_disp, xv_port, data->XJ_curwin, data->XJ_gc,
                          image, src_x, src_y, src_w, src_h, dispxoff, dest_y,
                          dispwoff, disphoff, False);
        }

        pthread_mutex_unlock(&lock);
    }
    else
    {
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

            img_resample_close(scontext);
        }

        switch (data->fallbackImage->bits_per_pixel)
        {
            case 16: av_format = PIX_FMT_RGB565; break;
            case 24: av_format = PIX_FMT_RGB24;  break;
            case 32: av_format = PIX_FMT_RGBA32; break;
            default: 
                VERBOSE(VB_IMPORTANT, "Non Xv mode only supports 16, 24, and 32 bpp displays");
                errored = true;
                return;
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

void VideoOutputXv::Show(FrameScanType )
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::Show() called while IsErrored istrue.");
        return;
    }

    pthread_mutex_lock(&lock);
    XSync(data->XJ_disp, False);
    pthread_mutex_unlock(&lock);
}

void VideoOutputXv::DrawUnusedRects(bool sync)
{
    // boboff assumes the smallest interlaced resolution is 480 lines
    int boboff = (int)round(((float)disphoff) / 480 - 0.001f);
    boboff = (m_deinterlacing && m_deintfiltername == "bobdeint") ? boboff : 0;
    //VERBOSE(VB_PLAYBACK, QString("disphoff(%1) boboff(%2) ndc(%3)")
    //        .arg(disphoff).arg(boboff).arg((data->needdrawcolor?"yes":"no")));

    if (data->needdrawcolor && !data->initialdraw)
    {
        XSetForeground(data->XJ_disp, data->XJ_gc, data->colorkey);
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc, dispx, 
                       dispy + boboff, dispw, disph - 2 * boboff);
    }

    // Draw black in masked areas
    XSetForeground(data->XJ_disp, data->XJ_gc, XJ_black);

    if (dispxoff > dispx) // left
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc, 
                       dispx, dispy, dispxoff - dispx, disph);
    if (dispxoff + dispwoff < dispx + dispw) // right
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc, 
                       dispxoff + dispwoff, dispy, 
                       (dispx + dispw) - (dispxoff + dispwoff), disph);
    if (dispyoff + boboff > dispy) // top of screen
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc, 
                       dispx, dispy, dispw, dispyoff + boboff - dispy);
    if (dispyoff + disphoff < dispy + disph + boboff) // bottom of screen
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc, 
                       dispx, dispyoff + disphoff - boboff, 
                       dispw, (dispy + disph + boboff) - (dispyoff + disphoff));

    if (sync)
        XSync(data->XJ_disp, false);
}

float VideoOutputXv::GetDisplayAspect(void)
{
    return data->display_aspect;
}

void VideoOutputXv::UpdatePauseFrame(void)
{
    pthread_mutex_lock(&lock);

    VideoFrame *pauseb = scratchFrame;
    if (usedVideoBuffers.count() > 0)
        pauseb = usedVideoBuffers.head();
    CopyFrame(&pauseFrame, pauseb);

    pthread_mutex_unlock(&lock);
}

void VideoOutputXv::ProcessFrame(VideoFrame *frame, OSD *osd,
                                 FilterChain *filterList,
                                 NuppelVideoPlayer *pipPlayer)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::ProcessFrame() called while IsErrored is true.");
        return;
    }

    pthread_mutex_lock(&lock);

    bool pauseframe = false;
    if (!frame)
    {
        frame = scratchFrame;
        CopyFrame(scratchFrame, &pauseFrame);
        pauseframe = true;
    }

    if (filterList)
        filterList->ProcessFrame(frame);

    if (m_deinterlacing && m_deintFilter != NULL && m_deinterlaceBeforeOSD &&
        !pauseframe)
    {
        m_deintFilter->ProcessFrame(frame);
    }

    ShowPip(frame, pipPlayer);
    DisplayOSD(frame, osd);

    if (m_deinterlacing && m_deintFilter != NULL && !m_deinterlaceBeforeOSD &&
        !pauseframe)
    {
        m_deintFilter->ProcessFrame(frame);
    }

    pthread_mutex_unlock(&lock);
}

int VideoOutputXv::ChangePictureAttribute(int attributeType, int newValue)
{
    int value;
    int i, howmany, port_min, port_max, range;
    char *attrName = NULL;
    Atom attribute;
    XvAttribute *attributes;

    switch (attributeType) {
        case kPictureAttribute_Brightness:
            attrName = "XV_BRIGHTNESS";
            break;
        case kPictureAttribute_Contrast:
            attrName = "XV_CONTRAST";
            break;
        case kPictureAttribute_Colour:
            attrName = "XV_SATURATION";
            break;
        case kPictureAttribute_Hue:
            attrName = "XV_HUE";
            break;
    }

    if (!attrName)
        return -1;

    if (newValue < 0) newValue = 0;
    if (newValue >= 100) newValue = 99;

    pthread_mutex_lock(&lock);

    attribute = XInternAtom (data->XJ_disp, attrName, False);
    if (!attribute) {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    attributes = XvQueryPortAttributes(data->XJ_disp, xv_port, &howmany);
    if (!attributes) {
        pthread_mutex_unlock(&lock);
        return -1;
    }

    for (i = 0; i < howmany; i++) {
        if (!strcmp(attrName, attributes[i].name)) {
            port_min = attributes[i].min_value;
            port_max = attributes[i].max_value;
            range = port_max - port_min;

            value = (int) (port_min + (range/100.0) * newValue);

            XvSetPortAttribute(data->XJ_disp, xv_port, attribute, value);

            pthread_mutex_unlock(&lock);

            return newValue;
        }
    }

    pthread_mutex_unlock(&lock);

    return -1;
}
