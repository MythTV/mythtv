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

#include "videoout_xvmc.h"
#include "../libmyth/util.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavcodec/xvmc_render.h"
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

struct XvMCData
{
    Window XJ_root;
    Window XJ_win;
    Window XJ_curwin;
    GC XJ_gc;
    Screen *XJ_screen;
    Display *XJ_disp;
    XShmSegmentInfo *XJ_SHMInfo;

    XvMCSurfaceInfo surface_info;
    XvMCContext ctx;
    XvMCBlockArray data_blocks[8];
    XvMCMacroBlockArray mv_blocks[8];

    xvmc_render_state_t *surface_render;
    xvmc_render_state_t *p_render_surface_to_show;
    xvmc_render_state_t *p_render_surface_visible;
};

#define GUID_I420_PLANAR 0x30323449
#define GUID_YV12_PLANAR 0x32315659

int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
  d = d; 
  xeev = xeev;
  return 0;
}

VideoOutputXvMC::VideoOutputXvMC(void)
{
    XJ_started = 0; 
    xv_port = -1; 

    data = new XvMCData();
    memset(data, 0, sizeof(XvMCData));
}

VideoOutputXvMC::~VideoOutputXvMC()
{
    Exit();
    delete data;
}

void VideoOutputXvMC::InputChanged(int width, int height, float aspect)
{
    pthread_mutex_lock(&lock);

    VideoOutput::InputChanged(width, height, aspect);

    DeleteXvMCBuffers();
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

bool VideoOutputXvMC::Init(int width, int height, float aspect, int num_buffers,
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

    int mc_surf_num = 0;
    XvMCSurfaceInfo *mc_surf_list;

    int mode_id = 0;

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

                for (unsigned long p = ai[i].base_id; p < ai[i].base_id +
                     ai[i].num_ports; p++)
                {
                    mc_surf_list = XvMCListSurfaceTypes(data->XJ_disp, p,
                                                        &mc_surf_num);
                    if (mc_surf_list == NULL || mc_surf_num == 0)
                        continue;

                    for (int s = 0; s < mc_surf_num; s++)
                    {
                        if (width > mc_surf_list[s].max_width)
                            continue;
                        if (height > mc_surf_list[s].max_height)
                            continue;
                        if (mc_surf_list[s].chroma_format != 
                            XVMC_CHROMA_FORMAT_420)
                            continue;

                        xv_port = p;
                        memcpy(&data->surface_info, &mc_surf_list[s],
                               sizeof(XvMCSurfaceInfo));
                        mode_id = mc_surf_list[s].surface_type_id;
                        break;
                    }

                    XFree(mc_surf_list);

                    if (xv_port > 0)
                        break;
                }
                if (xv_port > 0)
                    break;
            }
 
            if (p_num_adaptors > 0)
                XvFreeAdaptorInfo(ai);
        }
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

    ret = XvMCCreateContext(data->XJ_disp, xv_port, mode_id, width, height,
                            XVMC_DIRECT, &(data->ctx));

    if (ret != Success)
    {
        cerr << "Unable to create XvMC Context\n";
        return false;
    }

    int numblocks = ((width + 15) / 16) * ((height + 15) / 16);
    int blocks_per_macroblock = 6;
  
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
            XvMCDestroyContext(data->XJ_disp, &data->ctx);
        }  
    }

    data->XJ_curwin = data->XJ_win = winid;
    
    if (embedid > 0)
        data->XJ_curwin = data->XJ_win = embedid;

    old_handler = XSetErrorHandler(XJ_error_catcher);
    XSync(data->XJ_disp, 0);

    printf("Using XV port %d\n", xv_port);
    XvGrabPort(data->XJ_disp, xv_port, CurrentTime);

    data->XJ_gc = XCreateGC(data->XJ_disp, data->XJ_win, 0, 0);
    XJ_depth = DefaultDepthOfScreen(data->XJ_screen);

    if (!CreateXvMCBuffers())
            return false;

    XSetErrorHandler(old_handler);

    if (XJ_caught_error) 
    {
        return false;
    }

    MoveResize();

    Atom xv_atom;  
    XvAttribute *attributes;
    int colorkey;
    int attrib_count;
    bool needdrawcolor = true;

    attributes = XvQueryPortAttributes(data->XJ_disp, xv_port, &attrib_count);
    if (attributes)
    {
        for (int i = 0; i < attrib_count; i++)
        {
            if (!strcmp(attributes[i].name, "XV_AUTOPAINT_COLORKEY"))
            {
                xv_atom = XInternAtom(data->XJ_disp, "XV_AUTOPAINT_COLORKEY",
                                      False);
                if (xv_atom != None)
                {
                    ret = XvSetPortAttribute(data->XJ_disp, xv_port, xv_atom,
                                             1);
                    if (ret == Success)
                        needdrawcolor = false;
                }
            }
        }
        XFree(attributes);
    }

    xv_atom = XInternAtom(data->XJ_disp, "XV_COLORKEY", False);
    if (xv_atom != None)
    {
        ret = XvGetPortAttribute(data->XJ_disp, xv_port, xv_atom, &colorkey);
        if (ret == Success)
        {
            needdrawcolor = true;
        }
    }

    XSetForeground(data->XJ_disp, data->XJ_gc, colorkey);
    XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc, 0, 0, 
                   dispw, disph);

    XJ_started = true;

    return true;
}

bool VideoOutputXvMC::CreateXvMCBuffers(void)
{
    int numblocks = ((XJ_width + 15) / 16) * ((XJ_height + 15) / 16);
    int blocks_per_macroblock = 6;

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
        render->mc_type = data->surface_info.mc_type & (~XVMC_IDCT);
        render->idct = (data->surface_info.mc_type & XVMC_IDCT) == XVMC_IDCT;
        render->chroma_format = data->surface_info.chroma_format;
        render->unsigned_intra = (data->surface_info.flags & 
                                  XVMC_INTRA_UNSIGNED) == XVMC_INTRA_UNSIGNED;
        render->p_surface = surface;
        render->state = 0;

        videoframes[i].buf = (unsigned char *)render;
        videoframes[i].priv[0] = (unsigned char *)&(data->data_blocks[i]);
        videoframes[i].priv[1] = (unsigned char *)&(data->mv_blocks[i]);

        videoframes[i].height = XJ_height;
        videoframes[i].width = XJ_width;
        videoframes[i].bpp = -1;
        videoframes[i].size = sizeof(XvMCSurface);
        videoframes[i].codec = FMT_XVMC_IDCT_MPEG2;
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
        XJ_started = false;

        DeleteXvMCBuffers();
        XvUngrabPort(data->XJ_disp, xv_port, CurrentTime);

        XFreeGC(data->XJ_disp, data->XJ_gc);
        XCloseDisplay(data->XJ_disp);
    }
}

void VideoOutputXvMC::DeleteXvMCBuffers()
{
    for (int i = 0; i < numbuffers; i++)
    {
        XvMCDestroyMacroBlocks(data->XJ_disp, &data->mv_blocks[i]);
        XvMCDestroyBlocks(data->XJ_disp, &data->data_blocks[i]);
    }

    for (int i = 0; i < numbuffers; i++)
    {
        xvmc_render_state_t *render = (xvmc_render_state_t *)(videoframes[i].buf);
        if (!render)
            continue;

        XvMCHideSurface(data->XJ_disp, render->p_surface);
        XvMCDestroySurface(data->XJ_disp, render->p_surface);

        delete render->p_surface;
        delete render;

        videoframes[i].buf = NULL;
    }

    XvMCDestroyContext(data->XJ_disp, &data->ctx);
}

void VideoOutputXvMC::EmbedInWidget(unsigned long wid, int x, int y, int w, 
                                    int h)
{
    if (embedding)
        return;

    pthread_mutex_lock(&lock);
    data->XJ_curwin = wid;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);

    pthread_mutex_unlock(&lock);
}
 
void VideoOutputXvMC::StopEmbedding(void)
{
    if (!embedding)
        return;

    pthread_mutex_lock(&lock);

    data->XJ_curwin = data->XJ_win;
    VideoOutput::StopEmbedding();

    pthread_mutex_unlock(&lock);
}

static void SyncSurface(Display *disp, XvMCSurface *surf)
{
    int res = 0, status = 0;

    res = XvMCGetSurfaceStatus(disp, surf, &status);
    if (status & XVMC_RENDERING)
        XvMCSyncSurface(disp, surf);
}

void VideoOutputXvMC::PrepareFrame(VideoFrame *buffer)
{
    pthread_mutex_lock(&lock);

    xvmc_render_state_t *render = (xvmc_render_state_t *)buffer->buf;

    render->state |= MP_XVMC_STATE_DISPLAY_PENDING;
    data->p_render_surface_to_show = render;

    pthread_mutex_unlock(&lock);
}

void VideoOutputXvMC::Show()
{
    if (data->p_render_surface_to_show == NULL)
        return;

    XvMCSurface *surf = data->p_render_surface_to_show->p_surface;

    pthread_mutex_lock(&lock);

    SyncSurface(data->XJ_disp, surf);

    if (data->p_render_surface_visible != NULL)
        data->p_render_surface_visible->state &= ~MP_XVMC_STATE_DISPLAY_PENDING;

    XvMCPutSurface(data->XJ_disp, surf, data->XJ_curwin, imgx, imgy, imgw, 
                   imgh, dispxoff, dispyoff, dispwoff, disphoff, 3);

    if (data->p_render_surface_visible)
    {
        surf = data->p_render_surface_visible->p_surface;

        int status = XVMC_DISPLAYING;

        while (status & XVMC_DISPLAYING)
            XvMCGetSurfaceStatus(data->XJ_disp, surf, &status);
    }

    data->p_render_surface_visible = data->p_render_surface_to_show;
    data->p_render_surface_to_show = NULL;

    pthread_mutex_unlock(&lock);
}

void VideoOutputXvMC::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    int status = XVMC_DISPLAYING;

    xvmc_render_state_t *render = (xvmc_render_state_t *)frame->buf;

    pthread_mutex_lock(&lock);

    XvMCGetSurfaceStatus(data->XJ_disp, render->p_surface,
                         &status);

    if (y == 0 && status & XVMC_DISPLAYING)
        cerr << "Bad: still displaying frame\n";

    if (render->p_past_surface != NULL)
        SyncSurface(data->XJ_disp, render->p_past_surface);
    if (render->p_future_surface != NULL)
        SyncSurface(data->XJ_disp, render->p_future_surface);

    int res = XvMCRenderSurface(data->XJ_disp, &data->ctx, 
                                render->picture_structure, render->p_surface,
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

    XvMCFlushSurface(data->XJ_disp, render->p_surface);

    pthread_mutex_unlock(&lock);

    render->start_mv_blocks_num = 0;
    render->filled_mv_blocks_num = 0;
    render->next_free_data_block_num = 0;
}
