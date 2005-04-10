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
#include <cassert>

#include <map>
#include <iostream>
using namespace std;

#include "XvMCSurfaceTypes.h"
#include "videoout_xvmc.h"
#include "../libmyth/util.h"
#include "../libmyth/mythcontext.h"

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#define XMD_H 1
#include <X11/extensions/xf86vmode.h>

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavcodec/xvmc_render.h"
}

/**********************************************************
 *  The DisplayRes module allows for the display resolution to be
 *  changed "on the fly", based on the video resolution.
 *
 *  I first implemented this in libs/libmythtv/NuppelVideoPlayer,
 *  where it worked very well, but was messy.
 *
 *  Moving the display resolution switching calls to the videoout_*
 *  routines has *really* cleaned up the code.  However, this has
 *   resulted in it not working as well for XvMC.
 *
 *  With this new implementation, the order of events is different, and
 *  that seems to confuse XvMC's rendering of the video sometimes.  For
 *  example, when bringing up the EPG while watching live tv, the
 *  "Preview" window just says "Loading Preview Video...", and never
 *  actually display the video.
 *
 *  Also, OSD can look really bad.  If the GUI display resolution is
 *  much lower than the video display resolution, the OSD fonts look
 *  like they are being very poorly scaled up.
 *
 *  I hope that someone with much better knowledge of XvMC can
 *  suggest a fix for these issues.
 *
 *  John Patrick Poet
 *
 *  02 Auguest 2004
 **********************************************************/

extern "C" {
#include <X11/extensions/Xinerama.h>

    extern int      XShmQueryExtension(Display*);
    extern int      XShmGetEventBase(Display*);
    extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*, int, int, XShmSegmentInfo*);
}

const int kNumBuffers = 8;
const int kPrebufferFramesNormal = 4;
const int kPrebufferFramesSmall = 4;
const int kNeedFreeFrames = 2;
const int kKeepPrebuffer = 1;

#define NO_SUBPICTURE 0
#define OVERLAY_SUBPICTURE 1
#define BLEND_SUBPICTURE 2
#define BACKEND_SUBPICTURE 3

#define GUID_IA44  0x34344941
#define GUID_AI44  0x34344149

struct XvMCData
{
    Window XJ_root;
    Window XJ_win;
    Window XJ_curwin;
    GC XJ_gc;
    Screen *XJ_screen;
    Display *XJ_disp;
    XShmSegmentInfo *XJ_SHMInfo;
    float display_aspect;

    XvMCSurfaceInfo surface_info;
    XvMCContext ctx;
    XvMCBlockArray data_blocks[8];
    XvMCMacroBlockArray mv_blocks[8];

    xvmc_render_state_t *surface_render;
    xvmc_render_state_t *p_render_surface_to_show;
    xvmc_render_state_t *p_render_surface_visible;
    VideoFrame *curosd;
    VideoFrame *lastframe;

    int mode_id;

    int subpicture_mode;
    bool subpicture_alloc;

    XvMCSubpicture subpicture;
    XvImageFormatValues subpicture_info;
    int subpicture_clear_color;

    bool ia44;

    XShmSegmentInfo shminfo;
    XvImage *xvimage;
    unsigned char *palette;

    int colorkey;
    bool needdrawcolor;
    bool initialdraw;
};


static int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
    d = d; 
    xeev = xeev;
    return 0;
}

VideoOutputXvMC::VideoOutputXvMC(void)
               : VideoOutput()
{
    XJ_started = 0; 
    xv_port = -1; 

    chroma = XVMC_CHROMA_FORMAT_420;

    data = new XvMCData();
    memset(data, 0, sizeof(XvMCData));

    // If using custom display resolutions, display_res will point
    // to a singleton instance of the DisplayRes class
    display_res = DisplayRes::getDisplayRes();
}

VideoOutputXvMC::~VideoOutputXvMC()
{
    Exit();
    delete data;

    if (display_res)
        // Switch back to desired resolution for GUI
        display_res->switchToGUI();
}

void VideoOutputXvMC::AspectChanged(float aspect)
{
    pthread_mutex_lock(&lock);

    VideoOutput::AspectChanged(aspect);
    MoveResize();

    pthread_mutex_unlock(&lock);
}

void VideoOutputXvMC::Zoom(int direction)
{
    pthread_mutex_lock(&lock);

    VideoOutput::Zoom(direction);
    MoveResize();

    pthread_mutex_unlock(&lock);
}

void VideoOutputXvMC::InputChanged(int width, int height, float aspect)
{
    pthread_mutex_lock(&lock);

    VideoOutput::InputChanged(width, height, aspect);
    DeleteXvMCBuffers();

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

    CreateXvMCBuffers();
    XFlush(data->XJ_disp);

    MoveResize();
    pthread_mutex_unlock(&lock);
}

int VideoOutputXvMC::GetRefreshRate(void)
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

bool VideoOutputXvMC::Init(int width, int height, float aspect,
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

    VideoOutput::InitBuffers(kNumBuffers, false, kNeedFreeFrames,
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

    XJ_fullscreen = 0;

    data->XJ_root = DefaultRootWindow(data->XJ_disp);

    ret = XvQueryExtension(data->XJ_disp, &p_version, &p_release, 
                           &p_request_base, &p_event_base, &p_error_base);
    if (ret != Success) 
    {
        printf("XvQueryExtension failed.\n");
    }

    int mc_eventBase = 0, mc_errorBase = 0;
    if (True != XvMCQueryExtension(data->XJ_disp, &mc_eventBase, &mc_errorBase))
    {
        cerr << "No XvMC found\n";
        return false;
    }

    int mc_version, mc_release;
    if (Success == XvMCQueryVersion(data->XJ_disp, &mc_version, &mc_release))
    {
        printf("Using XvMC version: %d.%d\n", mc_version, mc_release);
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
    {
        if ( ai )
        {
            for (i = 0; i < p_num_adaptors; i++) 
            {
                if (ai[i].type == 0)
                    continue;

                XvPortID p = 0;
                int s;
                XvMCSurfaceTypes::find(width, height, chroma,
                                       true, false, 2, 0, 0,
                                       data->XJ_disp, ai[i].base_id,
                                       ai[i].base_id + ai[i].num_ports - 1,
                                       p, s);
                if (p != 0)
                    printf("XvMC found and using VLD surface\n");

                if (0 == p)
                {
                    // No VLD surface found, try to find IDCT surface
                    XvMCSurfaceTypes::find(width, height, chroma,
                                           false, true, 2, 0, 0,
                                           data->XJ_disp, ai[i].base_id,
                                           ai[i].base_id + ai[i].num_ports - 1,
                                           p, s);
                    if (p != 0)
                        printf("XvMC found and using IDCT surface\n");
                }

                if (0 == p) 
                {
                    // No IDCT surface found, try to find MC surface
                    XvMCSurfaceTypes::find(width, height, chroma,
                                           false, false, 2, 0, 0,
                                           data->XJ_disp, ai[i].base_id,
                                           ai[i].base_id + ai[i].num_ports - 1,
                                           p, s);
                    if (p != 0)
                        printf("XvMC found and using MC surface\n");
                }

                if (p != 0) 
                {
                    xv_port = p;
                    XvMCSurfaceTypes surf(data->XJ_disp, p);
                    assert(surf.size()>0);
                    surf.set(s, &data->surface_info);
                    data->mode_id = surf.surfaceTypeID(s);
                    break;
                }
            }

            if (p_num_adaptors > 0)
                XvFreeAdaptorInfo(ai);
        }
    }

    if (xv_port <= 0)
    {
        VERBOSE(VB_ALL, "Invalid xv port");
        return false;
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

    if (embedid > 0)
        data->XJ_curwin = data->XJ_win = embedid;

    old_handler = XSetErrorHandler(XJ_error_catcher);
    XSync(data->XJ_disp, 0);

    VERBOSE(VB_GENERAL, QString("Using XV port %1").arg(xv_port));
    XvGrabPort(data->XJ_disp, xv_port, CurrentTime);

    data->XJ_gc = XCreateGC(data->XJ_disp, data->XJ_win, 0, 0);
    XJ_depth = DefaultDepthOfScreen(data->XJ_screen);
    XFlush(data->XJ_disp);
    XSync(data->XJ_disp, false);

    XvImageFormatValues *xvfmv;
    int num_subpic;
    xvfmv = XvMCListSubpictureTypes(data->XJ_disp, xv_port, 
                                    data->surface_info.surface_type_id,
                                    &num_subpic);

    data->subpicture_mode = NO_SUBPICTURE;

    if (num_subpic != 0 && xvfmv != NULL)
    {
        for (int i = 0; i < num_subpic; i++)
        {
            if (xvfmv[i].id == GUID_IA44)
            {
                data->ia44 = true;
                data->subpicture_info = xvfmv[i];
                data->subpicture_mode = BLEND_SUBPICTURE;
                break;
            }
            else if (xvfmv[i].id == GUID_AI44)
            {
                data->ia44 = false;
                data->subpicture_info = xvfmv[i];
                data->subpicture_mode = BLEND_SUBPICTURE;
                break;
            }
        }

        XFree(xvfmv);
    }

    if ((data->subpicture_mode == BLEND_SUBPICTURE) &&
        (data->surface_info.flags & XVMC_BACKEND_SUBPICTURE))
        data->subpicture_mode = BACKEND_SUBPICTURE;

    if (!CreateXvMCBuffers())
        return false;

    XSetErrorHandler(old_handler);

    if (XJ_caught_error) 
    {
        return false;
    }

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

void VideoOutputXvMC::InitColorKey(bool turnoffautopaint)
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

bool VideoOutputXvMC::SetupDeinterlace(bool interlaced)
{
    bool deint = VideoOutput::SetupDeinterlace(interlaced);
    return deint;
}

bool VideoOutputXvMC::NeedsDoubleFramerate() const
{
    return m_deinterlacing; // always doing bob deint
}

bool VideoOutputXvMC::ApproveDeintFilter(const QString& filtername) const
{
    (void)filtername;
    VERBOSE(VB_PLAYBACK, "XvMC will use bob deinterlacing");
    return true; // yeah, sure, whatever.. we'll do bob deint ourselves, thanks
}

bool VideoOutputXvMC::CreateXvMCBuffers(void)
{
    int ret = XvMCCreateContext(data->XJ_disp, xv_port, data->mode_id, 
                                XJ_width, XJ_height, XVMC_DIRECT, &(data->ctx));

    if (ret != Success)
    {
        cerr << "Unable to create XvMC Context return status:" << ret ;
        switch (ret)
        {
            case XvBadPort: cerr << " XvBadPort"; break;
            case BadValue:  cerr << " BadValue" ; break;
            case BadMatch:  cerr << " BadMatch" ; break;
            case BadAlloc:  cerr << " BadAlloc" ; break;
            default:        cerr << " unrecognized return value"; break;
        }
        cerr << endl;
        return false;
    }

    int numblocks = ((XJ_width + 15) / 16) * ((XJ_height + 15) / 16);
    int blocks_per_macroblock = 6;

    if (!hasVLDAcceleration())
    {
        for (int i = 0; i < 8; i++)
        {
            ret = XvMCCreateBlocks(data->XJ_disp, &data->ctx,
                                   numblocks * blocks_per_macroblock,
                                   &(data->data_blocks[i]));
            if (ret != Success)
            {
                cerr << "Unable to create XvMC Blocks\n";
                XvMCDestroyContext(data->XJ_disp, &data->ctx);
                return false;
            }

            ret = XvMCCreateMacroBlocks(data->XJ_disp, &data->ctx, numblocks,
                                        &(data->mv_blocks[i]));
            if (ret != Success)
            {
                cerr << "Unable to create XvMC Macro Blocks\n";
                XvMCDestroyBlocks(data->XJ_disp,&(data->data_blocks[i]));
                XvMCDestroyContext(data->XJ_disp, &data->ctx);
                return false;
            }
        }
    }

    int rez = 0;
    for (int i = 0; i < numbuffers; i++)
    {
        XvMCSurface *surface = new XvMCSurface;

        rez = XvMCCreateSurface(data->XJ_disp, &data->ctx, surface);
        if (rez != Success)
        {
            cerr << "unable to create: " << i << " buffer\n";
            break;
        }

        xvmc_render_state_t *render = new xvmc_render_state_t;
        memset(render, 0, sizeof(xvmc_render_state_t));

        render->magic = MP_XVMC_RENDER_MAGIC;
        render->data_blocks = data->data_blocks[i].blocks;
        render->mv_blocks = data->mv_blocks[i].macro_blocks;
        render->total_number_of_mv_blocks = numblocks;
        render->total_number_of_data_blocks = numblocks * blocks_per_macroblock;
        render->mc_type = data->surface_info.mc_type;
        render->idct = (data->surface_info.mc_type & XVMC_IDCT) == XVMC_IDCT;
        render->chroma_format = data->surface_info.chroma_format;
        render->unsigned_intra = (data->surface_info.flags & 
                                  XVMC_INTRA_UNSIGNED) == XVMC_INTRA_UNSIGNED;
        render->p_surface = surface;
        render->state = 0;

        render->disp = data->XJ_disp;
        render->ctx = &data->ctx;

        vbuffers[i].buf = (unsigned char *)render;
        vbuffers[i].priv[0] = (unsigned char *)&(data->data_blocks[i]);
        vbuffers[i].priv[1] = (unsigned char *)&(data->mv_blocks[i]);

        vbuffers[i].height = XJ_height;
        vbuffers[i].width = XJ_width;
        vbuffers[i].bpp = -1;
        vbuffers[i].size = sizeof(XvMCSurface);
        vbuffers[i].codec = FMT_XVMC_IDCT_MPEG2;
    }

    data->subpicture_clear_color = 0;

    rez = XvMCCreateSubpicture(data->XJ_disp, &data->ctx, &data->subpicture,
                               XJ_width, XJ_height, data->subpicture_info.id);

    if (rez == Success)
    {
        data->subpicture_alloc = true;
        XvMCClearSubpicture(data->XJ_disp, &data->subpicture, 0, 0, XJ_width,
                            XJ_height, data->subpicture_clear_color);

        data->xvimage = XvShmCreateImage(data->XJ_disp, xv_port,
                                         data->subpicture_info.id, NULL,
                                         XJ_width, XJ_height, &data->shminfo);

        data->shminfo.shmid = shmget(IPC_PRIVATE, data->xvimage->data_size,
                                     IPC_CREAT | 0777);
        data->shminfo.shmaddr = (char *)shmat(data->shminfo.shmid, 0, 0);
        data->shminfo.readOnly = False;

        data->xvimage->data = data->shminfo.shmaddr;

        XShmAttach(data->XJ_disp, &data->shminfo);

        shmctl(data->shminfo.shmid, IPC_RMID, 0);

        if (data->subpicture.num_palette_entries > 0)
        {
            int snum = data->subpicture.num_palette_entries;
            int seb = data->subpicture.entry_bytes;

            data->palette = new unsigned char[snum * seb];

            for (int i = 0; i < snum; i++)
            {
                int Y = i * (1 << data->subpicture_info.y_sample_bits) / snum;
                int U = 1 << (data->subpicture_info.u_sample_bits - 1);
                int V = 1 << (data->subpicture_info.v_sample_bits - 1);
                for (int j = 0; j < seb; j++)
                {
                    switch (data->subpicture.component_order[j]) 
                    {
                        case 'U': data->palette[i * seb + j] = U; break;
                        case 'V': data->palette[i * seb + j] = V; break;
                        case 'Y': default:
                                  data->palette[i * seb + j] = Y; break;
                    }
                }
            }

            XvMCSetSubpicturePalette(data->XJ_disp, &data->subpicture, 
                                     data->palette);
        }
    }
    else
    {
        data->subpicture_mode = NO_SUBPICTURE;
        data->subpicture_alloc = false;
    }

    XSync(data->XJ_disp, 0);

    data->p_render_surface_to_show = NULL;
    data->p_render_surface_visible = NULL;
    return true;
}

void VideoOutputXvMC::Exit(void)
{
    if (XJ_started) 
    {
        XSetForeground(data->XJ_disp, data->XJ_gc, XJ_black);
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc,
                       dispx, dispy, dispw, disph);

        m_deinterlacing = false;
        InitColorKey(false);

        XJ_started = false;

        DeleteXvMCBuffers();
        XvUngrabPort(data->XJ_disp, xv_port, CurrentTime);

        XFreeGC(data->XJ_disp, data->XJ_gc);
        XCloseDisplay(data->XJ_disp);
    }
}

void VideoOutputXvMC::DeleteXvMCBuffers()
{
    if (!hasVLDAcceleration()) 
    {
        for (int i = 0; i < 8; i++)
        {
            XvMCDestroyMacroBlocks(data->XJ_disp, &data->mv_blocks[i]);
            XvMCDestroyBlocks(data->XJ_disp, &data->data_blocks[i]);
        }
    }

    for (int i = 0; i < numbuffers; i++)
    {
        xvmc_render_state_t *render = (xvmc_render_state_t *)(vbuffers[i].buf);
        if (!render)
            continue;

        XvMCHideSurface(data->XJ_disp, render->p_surface);
        XvMCDestroySurface(data->XJ_disp, render->p_surface);

        delete render->p_surface;
        delete render;

        vbuffers[i].buf = NULL;
    }

    if (data->subpicture_alloc)
    {
        XvMCDestroySubpicture(data->XJ_disp, &data->subpicture);

        XShmDetach(data->XJ_disp, &data->shminfo);
        shmdt(data->shminfo.shmaddr);

        data->subpicture_alloc = false;
        XFree(data->xvimage);
        XFlush(data->XJ_disp);
        XSync(data->XJ_disp, false);


        if (data->palette)
            delete [] data->palette;
    }

    XvMCDestroyContext(data->XJ_disp, &data->ctx);
}

void VideoOutputXvMC::EmbedInWidget(WId wid, int x, int y, int w, int h)
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

void VideoOutputXvMC::StopEmbedding(void)
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

static void SyncSurface(Display *disp, XvMCSurface *surf)
{
    if (!surf)
        return;

    int res = 0, status = 0;

    res = XvMCGetSurfaceStatus(disp, surf, &status);
    if (res)
        VERBOSE(VB_PLAYBACK, QString("Error XvMCGetSurfaceStatus %1").arg(res));
    if (status & XVMC_RENDERING)
        XvMCSyncSurface(disp, surf);
}

static void FlushSurface(Display *disp, XvMCSurface *surf)
{
    if (surf)
        XvMCFlushSurface(disp, surf);
}

void VideoOutputXvMC::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    (void)t;
    if (!buffer)
        buffer = data->lastframe;
    if (!buffer)
        return;

    framesPlayed = buffer->frameNumber + 1;

    pthread_mutex_lock(&lock);

    xvmc_render_state_t *render = reinterpret_cast<xvmc_render_state_t *>(buffer->buf);

    VideoFrame *osdframe = (VideoFrame *)(render->p_osd_target_surface_render);
    xvmc_render_state_t * osdrender =
        (osdframe) ? (xvmc_render_state_t *)(osdframe->buf) : 0;
    Display *display = data->XJ_disp;

    pthread_mutex_unlock(&lock);

    SyncSurface(display, render->p_surface);
    SyncSurface(display, (osdrender) ? osdrender->p_surface : 0);

    pthread_mutex_lock(&lock);

    render->state |= MP_XVMC_STATE_DISPLAY_PENDING;
    data->p_render_surface_to_show = render;

    data->lastframe = buffer;

    if (needrepaint)
    {
        data->initialdraw = false;
        DrawUnusedRects(false);
        needrepaint = false;
    }

    pthread_mutex_unlock(&lock);
}

void VideoOutputXvMC::Show(FrameScanType scan)
{
    int field = 3;
    int src_h = imgh;
    int halfLineSrc = 0;

    VideoFrame *buffer = data->lastframe;

    if (m_deinterlacing && buffer)
    {
        if ((scan == kScan_Interlaced && buffer->top_field_first == 1) ||
            (scan == kScan_Intr2ndField && buffer->top_field_first == 0))
        {
            field = 1;
        }
        else if ((scan == kScan_Interlaced && buffer->top_field_first == 0) ||
                 (scan == kScan_Intr2ndField && buffer->top_field_first == 1))
        {
            field = 2;
            halfLineSrc = (int) round(((float)disphoff)/imgh - 0.001f);
            //src_h -= (halfLineSrc) ? 2 : 0;
        }
    }

    pthread_mutex_lock(&lock);

    xvmc_render_state_t *render = data->p_render_surface_to_show;

    if (render == NULL)
    {
        pthread_mutex_unlock(&lock);
        return;
    }

    xvmc_render_state_t *osdren = NULL;

    VideoFrame *osdframe = (VideoFrame *)render->p_osd_target_surface_render;
    if (osdframe)
        osdren = (xvmc_render_state_t *)osdframe->buf;

    xvmc_render_state_t *showingsurface = (osdren) ? osdren : render;
    XvMCSurface *surf = showingsurface->p_surface;

    if (data->p_render_surface_visible != NULL)
        data->p_render_surface_visible->state &= ~MP_XVMC_STATE_DISPLAY_PENDING;

    // XXX Ugly hack.  With bobdeint, holding this lock takes too much time
    // away from the decoding process, and coming back from a pause can
    // result in a very low buffer situation, causing jerky playback.
    // Let's just hope that the XvMC driver's a bit threadsafe.
    //if (field != 3)
        pthread_mutex_unlock(&lock);

    int yoff = dispyoff;
    int hoff = disphoff;
    if (!hasVLDAcceleration())
    {
        yoff += halfLineSrc;
        hoff -= halfLineSrc;
    }

    XvMCPutSurface(data->XJ_disp, surf, data->XJ_curwin,
                   imgx, imgy, imgw, src_h,
                   dispxoff, yoff, dispwoff, hoff, field);

    //if (field != 3)
        pthread_mutex_lock(&lock);

    if (data->p_render_surface_visible && 
        (data->p_render_surface_visible != showingsurface))
    {
        surf = data->p_render_surface_visible->p_surface;

        int status = XVMC_DISPLAYING;

        XvMCGetSurfaceStatus(data->XJ_disp, surf, &status);
        while (status & XVMC_DISPLAYING)
        {
            pthread_mutex_unlock(&lock);
            usleep(1000);
            pthread_mutex_lock(&lock);
            XvMCGetSurfaceStatus(data->XJ_disp, surf, &status);
        }
    }

    data->p_render_surface_visible = data->p_render_surface_to_show;

    if (osdframe)
        data->p_render_surface_visible = osdren;
 
    if (!m_deinterlacing || (m_deinterlacing && scan != kScan_Interlaced))
    {
        data->p_render_surface_to_show = NULL;
        render->p_osd_target_surface_render = NULL;
    }

    pthread_mutex_unlock(&lock);
}

void FlushSlice(Display *display, XvMCSurface* current, int pict_type,
                XvMCSurface* past, XvMCSurface* future)
{
    // We always display I and P frames, so flush them
    if (!past && !future)
    { // I frame
        FlushSurface(display, current); // Flush I frame
        if (FF_I_TYPE!=pict_type)
            cerr<<"I"<<FF_I_TYPE<<":"<<pict_type;
    }
    else if (past && !future)
    { // P frame
        FlushSurface(display, past);    // Flush I/P frame
        FlushSurface(display, current); // Flush P frame
        if (FF_P_TYPE!=pict_type)
            cerr<<"P"<<FF_P_TYPE<<":"<<pict_type;
    }
    else
    { // B frame
        // We don't always display B frames, so forgo flushing...
        if (FF_B_TYPE!=pict_type) {
            cerr<<"B"<<FF_B_TYPE<<":"<<pict_type;
            // error? flush all!
            FlushSurface(display, past);
            FlushSurface(display, future);
            FlushSurface(display, current);
        }
    }
}

void VideoOutputXvMC::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    xvmc_render_state_t *render = (xvmc_render_state_t *)frame->buf;

    pthread_mutex_lock(&lock);

    if (hasVLDAcceleration())
    {
#ifdef USING_XVMC_VLD
        Status status = XvMCPutSlice2(data->XJ_disp, &data->ctx, 
                                      (char*)render->slice_data, 
                                      render->slice_datalen, 
                                      render->slice_code);
        if (status)
            printf("XvMCPutSlice: Error: %d\n", status);
#endif
    }
    else
    {
        if (render->p_past_surface && !render->p_future_surface)
            SyncSurface(data->XJ_disp, render->p_past_surface); // Sync prev-I frame

        int res = XvMCRenderSurface(data->XJ_disp, &data->ctx, 
                                    render->picture_structure, 
                                    render->p_surface,
                                    render->p_past_surface, 
                                    render->p_future_surface,
                                    render->flags, render->filled_mv_blocks_num,
                                    render->start_mv_blocks_num,
                                    (XvMCMacroBlockArray *)frame->priv[1], 
                                    (XvMCBlockArray *)frame->priv[0]);
        if (res != Success)
        {
            cerr << "XvMCRenderSurface error\n";
        }

        FlushSlice(data->XJ_disp, render->p_surface, render->pict_type, 
                   render->p_past_surface, render->p_future_surface);
    }

    pthread_mutex_unlock(&lock);

    render->start_mv_blocks_num = 0;
    render->filled_mv_blocks_num = 0;
    render->next_free_data_block_num = 0;
}

void VideoOutputXvMC::DrawUnusedRects(bool sync)
{
    // boboff assumes the smallest interlaced resolution is 480 lines
    int boboff = (int) round(((float)disphoff)/480 - 0.001f);
    boboff = (m_deinterlacing && m_deintfiltername == "bobdeint") ? boboff : 0;

    if (data->needdrawcolor && !data->initialdraw)
    {
        XSetForeground(data->XJ_disp, data->XJ_gc, data->colorkey);
        XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc,
                       dispx, dispy + boboff, dispw, disph - 2 * boboff);
    }

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

float VideoOutputXvMC::GetDisplayAspect(void)
{
    return data->display_aspect;
}

void VideoOutputXvMC::UpdatePauseFrame(void)
{
}

void VideoOutputXvMC::ProcessFrame(VideoFrame *frame, OSD *osd,
                                   FilterChain *filterList,
                                   NuppelVideoPlayer *pipPlayer)
{
    (void)filterList;
    (void)pipPlayer;

    if (!frame)
        frame = data->lastframe;
    if (!frame)
        return;

    xvmc_render_state_t *render = (xvmc_render_state_t *)frame->buf;
    render->p_osd_target_surface_render = NULL;

    if (data->curosd)
    {
        DiscardFrame(data->curosd);
        data->curosd = NULL;
    }

    if (osd)
    {
        if (data->subpicture_mode == BLEND_SUBPICTURE ||
            data->subpicture_mode == BACKEND_SUBPICTURE)
        {
            VideoFrame tmpframe;
            if (data->ia44)
                tmpframe.codec = FMT_IA44;
            else
                tmpframe.codec = FMT_AI44;
            tmpframe.buf = (unsigned char *)(data->xvimage->data);

            int ret = DisplayOSD(&tmpframe, osd);
            if (ret < 0)
                return;

            pthread_mutex_lock(&lock);

            if (ret > 0)
            {
                XvMCCompositeSubpicture(data->XJ_disp, &data->subpicture, 
                                        data->xvimage, 0, 0, XJ_width, 
                                        XJ_height, 0, 0);
                // delay sync until after getnextfreeframe...
                XvMCFlushSubpicture(data->XJ_disp, &data->subpicture);
            }

            if (data->subpicture_mode == BLEND_SUBPICTURE)
            {
                VideoFrame *newframe = GetNextFreeFrame();

                xvmc_render_state_t *osdren;
                osdren = (xvmc_render_state_t *)newframe->buf;

                XvMCSyncSubpicture(data->XJ_disp, &data->subpicture);
                XvMCBlendSubpicture2(data->XJ_disp, render->p_surface,
                                     osdren->p_surface, &data->subpicture,
                                     0, 0, XJ_width, XJ_height,
                                     0, 0, XJ_width, XJ_height);

                render->p_osd_target_surface_render = newframe;
                data->curosd = newframe;
            }
            else if (data->subpicture_mode == BACKEND_SUBPICTURE)
            {
                XvMCSyncSubpicture(data->XJ_disp, &data->subpicture);
                XvMCBlendSubpicture(data->XJ_disp, render->p_surface,
                                    &data->subpicture, 0, 0, XJ_width,
                                    XJ_height, 0, 0, XJ_width, XJ_height);
            }

            FlushSurface(data->XJ_disp, render->p_surface);
            
            pthread_mutex_unlock(&lock);
        }
    }
}

int VideoOutputXvMC::ChangePictureAttribute(int attributeType, int newValue)
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

            XvMCSetAttribute(data->XJ_disp, &data->ctx, attribute, value);

            pthread_mutex_unlock(&lock);

            return newValue;
        }
    }

    pthread_mutex_unlock(&lock);

    return -1;
}

// This is used to set the correct pixel format for the avdecoder.
bool VideoOutputXvMC::hasIDCTAcceleration() const 
{
    return XVMC_IDCT == (data->surface_info.mc_type & XVMC_IDCT);
}

bool VideoOutputXvMC::hasVLDAcceleration() const
{
#if USING_XVMC_VLD
    return XVMC_VLD == (data->surface_info.mc_type & XVMC_VLD);
#endif
    return 0;
}

