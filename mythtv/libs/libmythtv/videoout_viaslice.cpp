#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cmath>
#include <ctime>

#include <map>
#include <iostream>
using namespace std;

#include "videoout_viaslice.h"
#include "../libmyth/mythcontext.h"
#include "../libmyth/util.h"

extern "C" {
#include "via_videosdk.h"
#include "../libavcodec/avcodec.h"
#include "../libavcodec/viaslice.h"
#include "../libavcodec/via_mpegsdk.h"
}

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define XMD_H 1
#include <X11/extensions/xf86vmode.h>

extern "C" {
#include <X11/extensions/Xinerama.h>
}

const int kNumBuffers = 7;
const int kPrebufferFrames = 1;
const int kNeedFreeFrames = 2;
const int kKeepPrebuffer = 1;

struct ViaData
{
    Window XJ_root;
    Window XJ_win;
    Window XJ_curwin;
    GC XJ_gc;
    Screen *XJ_screen;
    Display *XJ_disp;
    float display_aspect;

    via_slice_state_t decode_buffers[kNumBuffers];
    via_slice_state_t overlay_buffer;

    unsigned char *buffer;
    unsigned char *current;

    DDLOCK ddLock;
    VIASUBPICT VIASubPict;
    int fd;
    unsigned char *lpSubSurface;

    unsigned char reorder_palette[64];

    DDSURFACEDESC ddSurfaceDesc;
    DDUPDATEOVERLAY ddUpdateOverlay;

    VIAMPGSURFACE VIAMPGSurface;

    VideoFrame *display_buffer;
    int display_frame;
};

int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
  d = d; 
  xeev = xeev;
  return 0;
}

VideoOutputVIA::VideoOutputVIA(void)
              : VideoOutput()
{
    XJ_started = 0; 

    data = new ViaData();
    memset(data, 0, sizeof(ViaData));
}

VideoOutputVIA::~VideoOutputVIA()
{
    Exit();
    delete data;
}

void VideoOutputVIA::AspectChanged(float aspect)
{
    pthread_mutex_lock(&lock);

    VideoOutput::AspectChanged(aspect);
    MoveResize();

    pthread_mutex_unlock(&lock);
}

void VideoOutputVIA::Zoom(int direction)
{
    pthread_mutex_lock(&lock);

    VideoOutput::Zoom(direction);
    MoveResize();

    pthread_mutex_unlock(&lock);
}

void VideoOutputVIA::InputChanged(int width, int height, float aspect)
{
    pthread_mutex_lock(&lock);

    VideoOutput::InputChanged(width, height, aspect);

    DeleteViaBuffers();
    CreateViaBuffers();

    MoveResize();
    pthread_mutex_unlock(&lock);
}

int VideoOutputVIA::GetRefreshRate(void)
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

bool VideoOutputVIA::Init(int width, int height, float aspect,
                          WId winid, int winx, int winy, int winw, 
                          int winh, WId embedid)
{
    bool usingXinerama;
    int event_base, error_base;

    pthread_mutex_init(&lock, NULL);

    VideoOutput::InitBuffers(kNumBuffers, false, kNeedFreeFrames,
                             kPrebufferFrames, kKeepPrebuffer);
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

    if (myth_dsw != 0)
        w_mm = myth_dsw;
    else
        w_mm = DisplayWidthMM(data->XJ_disp, XJ_screen_num);

    if (myth_dsh != 0)
        h_mm = myth_dsh;
    else
        h_mm = DisplayHeightMM(data->XJ_disp, XJ_screen_num);

    usingXinerama = (XineramaQueryExtension(data->XJ_disp, &event_base, 
                                            &error_base) &&
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
        int gui_w = gContext->GetNumSetting("GuiWidth", w);
        int gui_h = gContext->GetNumSetting("GuiHeight", h);

                if (gui_w)
            w_mm = w_mm * gui_w / w;
                if (gui_h)
            h_mm = h_mm * gui_h / h;

        data->display_aspect = (float)w_mm/h_mm;
    }
    else
        data->display_aspect = (float)w_mm / h_mm;

    XJ_white = XWhitePixel(data->XJ_disp, XJ_screen_num);
    XJ_black = XBlackPixel(data->XJ_disp, XJ_screen_num);
  
    XJ_fullscreen = 0;
  
    data->XJ_root = DefaultRootWindow(data->XJ_disp);

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

    data->XJ_gc = XCreateGC(data->XJ_disp, data->XJ_win, 0, 0);
    XJ_depth = DefaultDepthOfScreen(data->XJ_screen);

    data->current = data->buffer = new unsigned char[1920 * 1080 * 2];

    if (VIADriverProc(CREATEDRIVER, NULL))
    {
        cerr << "Unable to initialize VIA CLE266 HW Driver, "
                "please check your installation.\n";
        exit(1);
    }

    if (!CreateViaBuffers())
        return false;

    MoveResize();

    XSync(data->XJ_disp, false);

    XJ_started = true;
    cout << "Using VIA CLE266 Hardware Decoding\n";
    
    return true;
}

bool VideoOutputVIA::CreateViaBuffers(void)
{
    data->ddSurfaceDesc.dwWidth = XJ_width;
    data->ddSurfaceDesc.dwHeight = XJ_height;
    data->ddSurfaceDesc.dwBackBufferCount = kNumBuffers;
    data->ddSurfaceDesc.dwFourCC = FOURCC_VIA;
    if (VIADriverProc(CREATESURFACE, &data->ddSurfaceDesc))
        return false;

    data->ddSurfaceDesc.dwWidth  = XJ_width;
    data->ddSurfaceDesc.dwHeight = XJ_height;
    data->ddSurfaceDesc.dwBackBufferCount = 1;
    data->ddSurfaceDesc.dwFourCC = FOURCC_SUBP;
    if (VIADriverProc(CREATESURFACE, &data->ddSurfaceDesc))
        return false;

    data->ddLock.dwFourCC = FOURCC_SUBP;
    if (VIADriverProc(LOCKSURFACE, &data->ddLock))
        return false;

    data->fd = open("/dev/mem", O_RDWR);
    if (data->fd >= 0)
    {
        data->lpSubSurface = (unsigned char *)mmap(0, 16 * 1024 * 1024,
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, data->fd, 
                                           (off_t)data->ddLock.dwPhysicalBase);
        if (data->lpSubSurface == MAP_FAILED)
        {
            perror("MMAP ViaSlice Surface failed");
            cerr << "Couldn't map overlay surface\n";
            cerr << "You should run MythTV as root when using ViaSlice\n";
            exit(1);
        }

        data->lpSubSurface += data->ddLock.SubDev.dwSUBPhysicalAddr[0];

        close(data->fd);

        for (int i = 0; i < 16; i++)
        {
            data->reorder_palette[i * 4] = i * 16;
            data->reorder_palette[i * 4 + 1] = 127;
            data->reorder_palette[i * 4 + 2] = 127;

            data->VIASubPict.dwRamTab[i] = 
                           *(unsigned long *)(data->reorder_palette + (i * 4));
        }

        data->VIASubPict.dwTaskType = VIA_TASK_SP_RAMTAB;
        VIASUBPicture(&data->VIASubPict);
    }
    else
    {
        perror("Open /dev/mem failed");
        cerr << "Couldn't open /dev/mem to map overlay surface\n";
        cerr << "You should run MythTV as root when using ViaSlice\n";
        exit(1);
    }
     
    for (int i = 0; i < kNumBuffers; i++)
    {
        data->decode_buffers[i].image_number = i;
        data->decode_buffers[i].slice_data = NULL;
        data->decode_buffers[i].slice_datalen = 0;
        data->decode_buffers[i].lastcode = 0;
        data->decode_buffers[i].slicecount = 1;

        vbuffers[i].buf = (unsigned char *)&(data->decode_buffers[i]);
        vbuffers[i].height = XJ_height;
        vbuffers[i].width = XJ_width;
        vbuffers[i].bpp = -1;
        vbuffers[i].size = sizeof(via_slice_state_t);
        vbuffers[i].codec = FMT_VIA_HWSLICE;
    }

    return true;
}

void VideoOutputVIA::Exit(void)
{
    if (XJ_started) 
    {
        XJ_started = false;

        DeleteViaBuffers();

        VIADriverProc(DESTROYDRIVER, NULL);
        
        XFreeGC(data->XJ_disp, data->XJ_gc);
        XCloseDisplay(data->XJ_disp);

        delete [] data->buffer;
    }
}

void VideoOutputVIA::DeleteViaBuffers()
{
     data->ddUpdateOverlay.dwFlags = DDOVER_HIDE;
     VIADriverProc(UPDATEOVERLAY, &data->ddUpdateOverlay);
     
     data->ddSurfaceDesc.dwFourCC = FOURCC_VIA;
     VIADriverProc(DESTROYSURFACE, &data->ddSurfaceDesc);

     data->ddSurfaceDesc.dwFourCC = FOURCC_SUBP;
     VIADriverProc(DESTROYSURFACE, &data->ddSurfaceDesc);
}

void VideoOutputVIA::EmbedInWidget(unsigned long wid, int x, int y, int w, 
                                   int h)
{
    if (embedding)
        return;

    pthread_mutex_lock(&lock);
    data->XJ_curwin = wid;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);

    pthread_mutex_unlock(&lock);
}
 
void VideoOutputVIA::StopEmbedding(void)
{
    if (!embedding)
        return;

    pthread_mutex_lock(&lock);

    data->XJ_curwin = data->XJ_win;
    VideoOutput::StopEmbedding();

    pthread_mutex_unlock(&lock);
}

void VideoOutputVIA::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    // pause update
    if (!buffer)
        return;

    data->display_buffer = buffer;
    via_slice_state_t *curdata = (via_slice_state_t *)buffer->buf;

    data->display_frame = curdata->image_number;

    if (needrepaint)
    {
        DrawUnusedRects();
        needrepaint = false;
    }
}

void VideoOutputVIA::Show(FrameScanType )
{
    data->VIAMPGSurface.dwTaskType = VIA_TASK_DISPLAY;
    data->VIAMPGSurface.dwDisplayBuffIndex = data->display_frame;

    via_slice_state_t *cur = (via_slice_state_t *)data->display_buffer->buf;

    if (cur->progressive_sequence)
    {
        data->VIAMPGSurface.dwDisplayPictStruct = VIA_PICT_STRUCT_FRAME;
        VIADisplayControl(DEV_MPEG, &data->VIAMPGSurface);
    }
    else
    {
        if (cur->top_field_first)
        {
            data->VIAMPGSurface.dwDisplayPictStruct = VIA_PICT_STRUCT_BOTTOM;
            VIADisplayControl(DEV_MPEG, &data->VIAMPGSurface);
            data->VIAMPGSurface.dwDisplayPictStruct = VIA_PICT_STRUCT_TOP;
            VIADisplayControl(DEV_MPEG, &data->VIAMPGSurface);
        }
        else
        {
            data->VIAMPGSurface.dwDisplayPictStruct = VIA_PICT_STRUCT_TOP;
            VIADisplayControl(DEV_MPEG, &data->VIAMPGSurface);
            data->VIAMPGSurface.dwDisplayPictStruct = VIA_PICT_STRUCT_BOTTOM;
            VIADisplayControl(DEV_MPEG, &data->VIAMPGSurface);
        }
    }
}

void VideoOutputVIA::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    via_slice_state_t *curdata = (via_slice_state_t *)frame->buf;

    if (curdata->lastcode > curdata->code )
    {
#if 0  // Crashes the decoder?

        //Slice must be in next frame so flush last buffer first.
        VIASliceReceiveData(curdata->slicecount - 1, data->buffer);

        data->VIAMPGSurface.dwTaskType = VIA_TASK_DECODE;
        data->VIAMPGSurface.dwDisplayPictStruct = VIA_PICT_STRUCT_FRAME;
        data->VIAMPGSurface.dwDisplayBuffIndex = curdata->image_number;
        VIADisplayControl(DEV_MPEG, &data->VIAMPGSurface);
#endif
        curdata->slicecount = 1;
    }

    if (curdata->slicecount == 1)
        data->current = data->buffer;

    /* VIA Mpeg Slice Layout:
     *  uint32_t size
     *  uint32_t startcode
     *  uint8_t  slice_data[slice_datalen]
     *  uint8_t  padding[ 8 - (slice_datalen % 4) ]
     */

    uint8_t padding = 8 - (curdata->slice_datalen % 4);

    // XXX Overflow/Reallocate...

    *(uint32_t*)data->current = curdata->slice_datalen + padding + 4;
    data->current += 4;

    *(uint32_t*)data->current = 0x00010000 | curdata->code << 24;
    data->current += 4;

    memcpy(data->current, curdata->slice_data, curdata->slice_datalen);
    data->current += curdata->slice_datalen;

    memset(data->current, 0, padding);
    data->current += padding;

    if (curdata->code == curdata->maxcode)
    {
        // Send slices to HW Decoder
        VIASliceReceiveData(curdata->slicecount, data->buffer);

        data->VIAMPGSurface.dwTaskType = VIA_TASK_DECODE;
        data->VIAMPGSurface.dwDisplayPictStruct = VIA_PICT_STRUCT_FRAME;
        data->VIAMPGSurface.dwDisplayBuffIndex = curdata->image_number;
        VIADisplayControl(DEV_MPEG, &data->VIAMPGSurface);

        curdata->slicecount = 1;
        curdata->lastcode = 0;
    } 
    else
    {
        curdata->slicecount++;
        curdata->lastcode = curdata->code;
    }
}

void VideoOutputVIA::DrawUnusedRects(void)
{
    XSetForeground(data->XJ_disp, data->XJ_gc, XJ_black);

    // Clear all background, so that the guide also get's nice video.
    XFillRectangle(data->XJ_disp, data->XJ_curwin, data->XJ_gc,
                   dispx, dispy, dispw, disph);

    data->ddUpdateOverlay.rDest.left = dispxoff;
    data->ddUpdateOverlay.rDest.top = dispyoff;
    data->ddUpdateOverlay.rDest.right = dispxoff + dispwoff;
    data->ddUpdateOverlay.rDest.bottom = dispyoff + disphoff;

    data->ddUpdateOverlay.rSrc.left = imgx;
    data->ddUpdateOverlay.rSrc.top = imgy;
    data->ddUpdateOverlay.rSrc.right = imgx + imgw;
    data->ddUpdateOverlay.rSrc.bottom = imgy + imgh;

    data->ddUpdateOverlay.dwFlags = DDOVER_SHOW | DDOVER_KEYDEST;

    VIADriverProc(UPDATEOVERLAY, &data->ddUpdateOverlay);

    XSync(data->XJ_disp, false);
}

float VideoOutputVIA::GetDisplayAspect(void)
{
    return data->display_aspect;
}

void VideoOutputVIA::UpdatePauseFrame(void)
{
}

void VideoOutputVIA::ProcessFrame(VideoFrame *frame, OSD *osd,
                                  FilterChain *filterList,
                                  NuppelVideoPlayer *pipPlayer)
{
    (void)filterList;
    (void)frame;
    (void)pipPlayer;

    if (osd)
    {
        VideoFrame tmpframe;
        tmpframe.codec = FMT_IA44;
        tmpframe.buf = (unsigned char *)data->lpSubSurface;

        int ret = DisplayOSD(&tmpframe, osd, data->ddLock.SubDev.dwPitch);

        if (ret < 0)
        {
            data->VIASubPict.dwTaskType = VIA_TASK_SP_DISPLAY_DISABLE;
            VIASUBPicture(&data->VIASubPict);
            return;
        }

        if (ret > 0)
        {
            data->VIASubPict.dwSPLeft = 0;
            data->VIASubPict.dwSPTop = 0;
            data->VIASubPict.dwSPWidth = XJ_width;
            data->VIASubPict.dwSPHeight = XJ_height;
            data->VIASubPict.dwDisplayBuffIndex = 0;
            data->VIASubPict.dwTaskType = VIA_TASK_SP_DISPLAY;
            VIASUBPicture(&data->VIASubPict);
        }
    } 
}
