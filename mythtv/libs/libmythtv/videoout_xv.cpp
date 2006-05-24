/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */

// ANSI C headers
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cerrno>

#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/keysym.h>

#include <iostream>
using namespace std;

// MythTV OSD headers
#include "yuv2rgb.h"
#include "osd.h"
#include "osdsurface.h"
#include "osdxvmc.h"
#include "osdchromakey.h"

// MythTV X11 headers
#include "videoout_xv.h"
#include "util-x11.h"
#include "util-xv.h"
#include "util-xvmc.h"
#include "xvmctextures.h"

// MythTV General headers
#include "mythconfig.h"
#include "mythcontext.h"
#include "filtermanager.h"
#define IGNORE_TV_PLAY_REC
#include "tv.h"

#define LOC QString("VideoOutputXv: ")
#define LOC_ERR QString("VideoOutputXv Error: ")

extern "C" {
#define XMD_H 1
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/Xinerama.h>
    extern int      XShmQueryExtension(Display*);
    extern int      XShmGetEventBase(Display*);
    extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*,
                                      int, int, XShmSegmentInfo*);
}

#include "../libavcodec/avcodec.h"

#ifndef HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

#ifndef XVMC_CHROMA_FORMAT_420
#define XVMC_CHROMA_FORMAT_420 0x00000001
#endif

static void SetFromEnv(bool &useXvVLD, bool &useXvIDCT, bool &useXvMC,
                       bool &useXV, bool &useShm);
static void SetFromHW(Display *d, bool &useXvMC, bool &useXV, bool& useShm);


/** \class  VideoOutputXv
 * Supports common video output methods used with %X11 Servers.
 *
 * This class suppurts XVideo with VLD acceleration (XvMC-VLD), XVideo with
 * inverse discrete cosine transform (XvMC-IDCT) acceleration, XVideo with 
 * motion vector (XvMC) acceleration, and normal XVideo with color transform
 * and scaling acceleration only. When none of these will work, we also try 
 * to use X Shared memory, and if that fails we try standard Xlib output.
 *
 * \see VideoOutput, VideoBuffers
 *
 */
VideoOutputXv::VideoOutputXv(MythCodecID codec_id)
    : VideoOutput(),
      myth_codec_id(codec_id), video_output_subtype(XVUnknown),
      display_res(NULL), global_lock(true),

      XJ_root(0),  XJ_win(0), XJ_curwin(0), XJ_gc(0), XJ_screen(NULL),
      XJ_disp(NULL), XJ_screen_num(0), XJ_white(0), XJ_black(0), XJ_depth(0),
      XJ_screenx(0), XJ_screeny(0), XJ_screenwidth(0), XJ_screenheight(0),
      XJ_started(false),

      XJ_non_xv_image(0), non_xv_frames_shown(0), non_xv_show_frame(1),
      non_xv_fps(0), non_xv_av_format(PIX_FMT_NB), non_xv_stop_time(0),

      xvmc_buf_attr(new XvMCBufferSettings()),
      xvmc_chroma(XVMC_CHROMA_FORMAT_420), xvmc_ctx(NULL),
      xvmc_osd_lock(false),
      xvmc_tex(NULL),

      xv_port(-1),      xv_hue_base(0),
      xv_colorkey(0),   xv_draw_colorkey(false),
      xv_chroma(0),     xv_color_conv_buf(NULL),

      chroma_osd(NULL)
{
    VERBOSE(VB_PLAYBACK, LOC + "ctor");
    bzero(&av_pause_frame, sizeof(av_pause_frame));

    // If using custom display resolutions, display_res will point
    // to a singleton instance of the DisplayRes class
    if (gContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes();
}

VideoOutputXv::~VideoOutputXv()
{
    VERBOSE(VB_PLAYBACK, LOC + "dtor");
    if (XJ_started) 
    {
        X11L;
        XSetForeground(XJ_disp, XJ_gc, XJ_black);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc,
                       display_visible_rect.left(),
                       display_visible_rect.top(),
                       display_visible_rect.width(),
                       display_visible_rect.height());
        X11U;

        m_deinterlacing = false;
    }

    DeleteBuffers(VideoOutputSubType(), true);

    // ungrab port...
    if (xv_port >= 0)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Closing XVideo port " << xv_port);
        X11L;
        XvUngrabPort(XJ_disp, xv_port, CurrentTime);
        del_open_xv_port(xv_port);
        X11U;
        xv_port = -1;
    }

    if (XJ_started) 
    {
        XJ_started = false;

        X11L;
        XFreeGC(XJ_disp, XJ_gc);
        XCloseDisplay(XJ_disp);
        X11U;
    }

    // Switch back to desired resolution for GUI
    if (display_res)
        display_res->SwitchToGUI();

    if (xvmc_tex)
        delete xvmc_tex;
}

// this is documented in videooutbase.cpp
void VideoOutputXv::Zoom(int direction)
{
    QMutexLocker locker(&global_lock);
    VideoOutput::Zoom(direction);
    MoveResize();
}

// this is documented in videooutbase.cpp
void VideoOutputXv::MoveResize(void)
{
    QMutexLocker locker(&global_lock);
    VideoOutput::MoveResize();
    if (chroma_osd)
    {
        chroma_osd->Reset();
        needrepaint = true;
    }
}

// documented in videooutbase.cpp
void VideoOutputXv::InputChanged(int width, int height, float aspect,
                                 MythCodecID av_codec_id)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3)")
            .arg(width).arg(height).arg(aspect));

    QMutexLocker locker(&global_lock);

    bool cid_changed = (myth_codec_id != av_codec_id);
    QSize new_res = QSize(width, height);
    bool res_changed = new_res != video_dim;
    bool asp_changed = aspect != video_aspect;

    VideoOutput::InputChanged(width, height, aspect, av_codec_id);

    if (!res_changed && !cid_changed)
    {
        if (VideoOutputSubType() == XVideo)
            clear_xv_buffers(vbuffers, video_dim.width(), video_dim.height(),
                             xv_chroma);
        if (asp_changed)
            MoveResize();
        return;
    }

    bool ok = true;

    DeleteBuffers(VideoOutputSubType(), cid_changed);
    ResizeForVideo((uint) width, (uint) height);
    if (cid_changed)
    {
        myth_codec_id = av_codec_id;

        // ungrab port...
        if (xv_port >= 0)
        {
            VERBOSE(VB_PLAYBACK, LOC + "Closing XVideo port " << xv_port);
            X11L;
            XvUngrabPort(XJ_disp, xv_port, CurrentTime);
            del_open_xv_port(xv_port);
            X11U;
            xv_port = -1;
        }

        ok = InitSetupBuffers();
    }
    else
        ok = CreateBuffers(VideoOutputSubType());

    MoveResize();

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "InputChanged(): "
                "Failed to recreate buffers");
        errored = true;
    }
}

// documented in videooutbase.cpp
QRect VideoOutputXv::GetVisibleOSDBounds(
    float &visible_aspect, float &font_scaling) const
{
    if (!chroma_osd)
        return VideoOutput::GetVisibleOSDBounds(visible_aspect, font_scaling);

    float dispPixelAdj  = GetDisplayAspect() * display_visible_rect.height();
    dispPixelAdj       /= display_visible_rect.width();
    visible_aspect = 1.3333f/dispPixelAdj;
    font_scaling   = 1.0f;
    return QRect(QPoint(0,0), display_visible_rect.size());
}

// documented in videooutbase.cpp
QRect VideoOutputXv::GetTotalOSDBounds(void) const
{
    QSize sz = (chroma_osd) ? display_visible_rect.size() : video_dim;
    return QRect(QPoint(0,0), sz);
}

/**
 * \fn VideoOutputXv::GetRefreshRate(void)
 *
 * This uses the XFree86 xf86vmode extension to query the mode line
 * It then uses the mode line to guess at the refresh rate.
 *
 * \bug This works for all user specified mode lines, but sometimes
 * fails for autogenerated mode lines.
 *
 * \return integer approximation of monitor refresh rate.
 */

int VideoOutputXv::GetRefreshRate(void)
{
    if (!XJ_started)
        return -1;

    XF86VidModeModeLine mode_line;
    int dot_clock;

    int ret = False;
    X11S(ret = XF86VidModeGetModeLine(XJ_disp, XJ_screen_num,
                                      &dot_clock, &mode_line));
    if (!ret)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GetRefreshRate(): "
                "X11 ModeLine query failed");
        return -1;
    }

    double rate = (double)((double)(dot_clock * 1000.0) /
                           (double)(mode_line.htotal * mode_line.vtotal));

    // Assume 60Hz if we can't otherwise determine it.
    if (rate == 0)
        rate = 60;

    if (rate < 20 || rate > 200)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Unreasonable refresh rate %1Hz "
                                           "reported by X").arg(rate));
        rate = 60;
    }

    rate = 1000000.0 / rate;

    return (int)rate;
}

/**
 * \fn VideoOutputXv::ResizeForVideo(uint width, uint height)
 * Sets display parameters based on video resolution. 
 *
 * If we are using DisplayRes support we use the video size to
 * determine the desired screen size and refresh rate.
 * If we are also not using "GuiSizeForTV" we also resize
 * the video output window.
 *
 * \param width,height Resolution of the video we will be playing
 */
void VideoOutputXv::ResizeForVideo(uint width, uint height)
{
    if (width == 1920 && height == 1088)
        height = 1080; // ATSC 1920x1080

    if (display_res && display_res->SwitchToVideo(width, height))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        display_dim = QSize(display_res->GetPhysicalWidth(),
                            display_res->GetPhysicalHeight());
        display_aspect = display_res->GetAspectRatio();

        bool fullscreen = !gContext->GetNumSetting("GuiSizeForTV", 0);
        
        // if width && height are zero users expect fullscreen playback
        if (!fullscreen)
        {
            int gui_width = 0, gui_height = 0;
            gContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        if (fullscreen)
        {
            QSize sz(display_res->GetWidth(), display_res->GetHeight());
            display_visible_rect = QRect(QPoint(0,0), sz);

            // Resize X window to fill new resolution
            X11S(XMoveResizeWindow(XJ_disp, XJ_win,
                                   display_visible_rect.left(),
                                   display_visible_rect.top(),
                                   display_visible_rect.width(),
                                   display_visible_rect.height()));
        }
    }
}

/** 
 * \fn VideoOutputXv::InitDisplayMeasurements(uint width, uint height)
 * \brief Init display measurements based on database settings and
 *        actual screen parameters.
 */
void VideoOutputXv::InitDisplayMeasurements(uint width, uint height)
{
    if (display_res)
    {
        // The very first Resize needs to be the maximum possible
        // desired res, because X will mask off anything outside
        // the initial dimensions
        X11S(XMoveResizeWindow(XJ_disp, XJ_win, 0, 0,
                               display_res->GetMaxWidth(),
                               display_res->GetMaxHeight()));
        ResizeForVideo(width, height);
    }
    else
    {
        display_dim = QSize(DisplayWidthMM(XJ_disp, XJ_screen_num),
                            DisplayHeightMM(XJ_disp, XJ_screen_num));

        if (db_display_dim.width() > 0 && db_display_dim.height() > 0)
            display_dim = db_display_dim;

        // Get default (possibly user selected) screen resolution from context
        float wmult, hmult;
        gContext->GetScreenSettings(XJ_screenx, XJ_screenwidth, wmult,
                                    XJ_screeny, XJ_screenheight, hmult);
    }

    // Fetch pixel width and height of the display
    int xbase, ybase, w, h;
    gContext->GetScreenBounds(xbase, ybase, w, h);

    // Determine window dimensions in pixels
    int window_w = w, window_h = h;
    if (gContext->GetNumSetting("GuiSizeForTV", 0))
        gContext->GetResolutionSetting("Gui", window_w,  window_h);
    else
        gContext->GetScreenBounds(xbase, ybase, window_w, window_h);
    window_w = (window_w) ? window_w : w;
    window_h = (window_h) ? window_h : h;
    float pixel_aspect = ((float)w) / ((float)h);

    VERBOSE(VB_PLAYBACK, LOC + QString(
                "Pixel dimensions: Screen %1x%2, window %3x%4")
            .arg(w).arg(h).arg(window_w).arg(window_h));

    // Determine if we are using Xinerama
    int event_base, error_base;
    bool usingXinerama = false;
    X11S(usingXinerama = 
         (XineramaQueryExtension(XJ_disp, &event_base, &error_base) &&
          XineramaIsActive(XJ_disp)));

    // If the dimensions are invalid, assume square pixels and 17" screen.
    // Only print warning if this isn't Xinerama, we will fix Xinerama later.
    if (((display_dim.width() <= 0) || (display_dim.height() <= 0)) &&
        !usingXinerama)
    {
        VERBOSE(VB_GENERAL, LOC + "Physical size of display unknown."
                "\n\t\t\tAssuming 17\" monitor with square pixels.");
        display_dim.setHeight(300);
        display_dim.setWidth((int) round(300 * pixel_aspect));
    }

    // If we are using XRandR, use the aspect ratio from it instead...
    if (display_res)
        display_aspect = display_res->GetAspectRatio();

    // If we are using Xinerama the display dimensions can not be trusted.
    // We need to use the Xinerama monitor aspect ratio from the DB to set
    // the physical screen width. This assumes the height is correct, which
    // is more or less true in the typical side-by-side monitor setup.
    if (usingXinerama)
    {
        float displayAspect = gContext->GetFloatSettingOnHost(
            "XineramaMonitorAspectRatio",
            gContext->GetHostName(), pixel_aspect);
        int height = display_dim.height();
        if (height <= 0)
            display_dim.setHeight(height = 300);
        display_dim.setWidth((int) round(height * displayAspect));
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Estimated display dimensions: %1x%2 mm  Aspect: %3")
            .arg(display_dim.width()).arg(display_dim.height())
            .arg(((float) display_dim.width()) / display_dim.height()));

    // We must now scale the display measurements to our window size.
    // If we are running fullscreen this is a no-op.
    display_dim = QSize((display_dim.width()  * window_w) / w,
                        (display_dim.height() * window_h) / h);

    // Now that we know the physical monitor size, we can
    // calculate the display aspect ratio pretty simply...
    display_aspect = ((float)display_dim.width()) / display_dim.height();

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Estimated window dimensions: %1x%2 mm  Aspect: %3")
            .arg(display_dim.width()).arg(display_dim.height())
            .arg(display_aspect));
}

/**
 * \fn VideoOutputXv::GrabSuitableXvPort(Display*,Window,MythCodecID,uint,uint,int,XvMCSurfaceInfo*)
 * Internal function used to grab a XVideo port with the desired properties.
 *
 * \return port number if it succeeds, else -1.
 */
int VideoOutputXv::GrabSuitableXvPort(Display* disp, Window root,
                                      MythCodecID mcodecid,
                                      uint width, uint height,
                                      int xvmc_chroma,
                                      XvMCSurfaceInfo* xvmc_surf_info,
                                      QString *adaptor_name)
{
    uint neededFlags[] = { XvInputMask,
                           XvInputMask,
                           XvInputMask,
                           XvInputMask | XvImageMask };
    bool useXVMC[] = { true,  true,  true,  false };
    bool useVLD[]  = { true,  false, false, false };
    bool useIDCT[] = { false, true,  false, false };

    // avoid compiler warnings
    (void)width; (void)height; (void)xvmc_chroma; (void)xvmc_surf_info;
    (void)useVLD[0]; (void)useIDCT[0];

    QString msg[] =
    {
        "XvMC surface found with VLD support on port %1",
        "XvMC surface found with IDCT support on port %1",
        "XvMC surface found with MC support on port %1",
        "XVideo surface found on port %1"
    };

    if (adaptor_name)
        *adaptor_name = QString::null;

    // get the list of Xv ports
    XvAdaptorInfo *ai = NULL;
    uint p_num_adaptors = 0;
    int ret = Success;
    X11S(ret = XvQueryAdaptors(disp, root, &p_num_adaptors, &ai));
    if (Success != ret) 
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "XVideo supported, but no free Xv ports found."
                "\n\t\t\tYou may need to reload video driver.");
        return -1;
    }

    // find an Xv port
    int port = -1, stream_type = 0;
    uint begin = 0, end = 4;
    switch (mcodecid)
    {
        case kCodec_MPEG1_XVMC: (stream_type = 1),(begin = 2),(end = 3); break;
        case kCodec_MPEG2_XVMC: (stream_type = 2),(begin = 2),(end = 3); break;
        case kCodec_H263_XVMC:  (stream_type = 3),(begin = 2),(end = 3); break;
        case kCodec_MPEG4_XVMC: (stream_type = 4),(begin = 2),(end = 3); break;

        case kCodec_MPEG1_IDCT: (stream_type = 1),(begin = 1),(end = 2); break;
        case kCodec_MPEG2_IDCT: (stream_type = 2),(begin = 1),(end = 2); break;
        case kCodec_H263_IDCT:  (stream_type = 3),(begin = 1),(end = 2); break;
        case kCodec_MPEG4_IDCT: (stream_type = 4),(begin = 1),(end = 2); break;

        case kCodec_MPEG1_VLD:  (stream_type = 1),(begin = 0),(end = 1); break;
        case kCodec_MPEG2_VLD:  (stream_type = 2),(begin = 0),(end = 1); break;
        case kCodec_H263_VLD:   (stream_type = 3),(begin = 0),(end = 1); break;
        case kCodec_MPEG4_VLD:  (stream_type = 4),(begin = 0),(end = 1); break;

        default:
            begin = 3; end = 4;
            break;
    }

    QString lastAdaptorName = QString::null;
    for (uint j = begin; j < end; ++j)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("@ j=%1 Looking for flag[s]: %2")
                .arg(j).arg(xvflags2str(neededFlags[j])));

        for (uint i = 0; i < p_num_adaptors && (port == -1); ++i) 
        {
            lastAdaptorName = ai[i].name;
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Adaptor#%1: %2 has flag[s]: %3")
                    .arg(i).arg(lastAdaptorName).arg(xvflags2str(ai[i].type)));

            if ((ai[i].type & neededFlags[j]) != neededFlags[j])
                continue;
            
            const XvPortID firstPort = ai[i].base_id;
            const XvPortID lastPort = ai[i].base_id + ai[i].num_ports - 1;
            XvPortID p = 0;
            if (useXVMC[j])
            {
#ifdef USING_XVMC
                int surfNum;
                XvMCSurfaceTypes::find(width, height, xvmc_chroma,
                                       useVLD[j], useIDCT[j], stream_type,
                                       0, 0,
                                       disp, firstPort, lastPort,
                                       p, surfNum);
                if (surfNum<0)
                    continue;
                
                XvMCSurfaceTypes surf(disp, p);
                
                if (!surf.size())
                    continue;

                X11L;
                ret = XvGrabPort(disp, p, CurrentTime);
                if (Success == ret)
                {
                    VERBOSE(VB_PLAYBACK, LOC + "Grabbed xv port "<<p);
                    port = p;
                    add_open_xv_port(disp, p);
                }
                X11U;
                if (Success != ret)
                {
                    VERBOSE(VB_PLAYBACK,  LOC + "Failed to grab xv port "<<p);
                    continue;
                }
                
                if (xvmc_surf_info)
                    surf.set(surfNum, xvmc_surf_info);
#endif // USING_XVMC
            }
            else
            {
                for (p = firstPort; (p <= lastPort) && (port == -1); ++p)
                {
                    X11L;
                    ret = XvGrabPort(disp, p, CurrentTime);
                    if (Success == ret)
                    {
                        VERBOSE(VB_PLAYBACK,  LOC + "Grabbed xv port "<<p);
                        port = p;
                        add_open_xv_port(disp, p);
                    }
                    X11U;
                }
            }
        }
        if (port != -1)
        {
            VERBOSE(VB_PLAYBACK, LOC + msg[j].arg(port));
            break;
        }
    }
    if (port == -1)
        VERBOSE(VB_PLAYBACK, LOC + "No suitable XVideo port found");

    // free list of Xv ports
    if (ai)
        X11S(XvFreeAdaptorInfo(ai));

    if ((port != -1) && adaptor_name)
        *adaptor_name = lastAdaptorName;

    return port;
}

/**
 * \fn VideoOutputXv::CreatePauseFrame(void)
 * Creates an extra frame for pause.
 * 
 * This creates a pause frame by copies the scratch frame settings, a
 * and allocating a databuffer, so a scratch must already exist.
 * XvMC does not use this pause frame facility so this only creates
 * a pause buffer for the other output methods.
 *
 * \sideeffect sets av_pause_frame.
 */
void VideoOutputXv::CreatePauseFrame(void)
{
    // All methods but XvMC use a pause frame, create it if needed
    if (VideoOutputSubType() <= XVideo)
    {
        vbuffers.LockFrame(&av_pause_frame, "CreatePauseFrame");

        if (av_pause_frame.buf)
        {
            delete [] av_pause_frame.buf;
            av_pause_frame.buf = NULL;
        }
        av_pause_frame.height       = vbuffers.GetScratchFrame()->height;
        av_pause_frame.width        = vbuffers.GetScratchFrame()->width;
        av_pause_frame.bpp          = vbuffers.GetScratchFrame()->bpp;
        av_pause_frame.size         = vbuffers.GetScratchFrame()->size;
        av_pause_frame.frameNumber  = vbuffers.GetScratchFrame()->frameNumber;
        av_pause_frame.buf          = new unsigned char[av_pause_frame.size];
        av_pause_frame.qscale_table = NULL;
        av_pause_frame.qstride      = 0;

        vbuffers.UnlockFrame(&av_pause_frame, "CreatePauseFrame");
    }
}

/**
 * \fn VideoOutputXv::InitVideoBuffers(MythCodecID,bool,bool)
 * Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \bug Extra buffers are pre-allocated here for XVMC_VLD
 *      due to a bug somewhere else, see comment in code.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitVideoBuffers(MythCodecID mcodecid,
                                     bool use_xv, bool use_shm)
{
    (void)mcodecid;

    bool done = false;
    // If use_xvmc try to create XvMC buffers
#ifdef USING_XVMC
    if (mcodecid > kCodec_NORMAL_END)
    {
        // Create ffmpeg VideoFrames    
        bool vld, idct, mc;
        myth2av_codecid(myth_codec_id, vld, idct, mc);

        if (vld)
            xvmc_buf_attr->SetNumSurf(16);

        vbuffers.Init(xvmc_buf_attr->GetNumSurf(),
                      false /* create an extra frame for pause? */,
                      xvmc_buf_attr->GetFrameReserve(),
                      xvmc_buf_attr->GetPreBufferGoal(),
                      xvmc_buf_attr->GetPreBufferGoal(),
                      xvmc_buf_attr->GetNeededBeforeDisplay(),
                      true /*use_frame_locking*/);
        
        
        done = InitXvMC(mcodecid);

        if (!done)
            vbuffers.Reset();
    }
#endif // USING_XVMC

    // Create ffmpeg VideoFrames    
    if (!done)
        vbuffers.Init(31, true, 1, 12, 4, 2, false);

    // Fall back to XVideo if there is an xv_port
    if (!done && use_xv)
        done = InitXVideo();

    // Fall back to shared memory, if we are allowed to use it
    if (!done && use_shm)
        done = InitXShm();
 
    // Fall back to plain old X calls
    if (!done)
        done = InitXlib();

    return done;
}

/**
 * \fn VideoOutputXv::InitXvMC(MythCodecID)
 *  Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitXvMC(MythCodecID mcodecid)
{
    (void)mcodecid;
#ifdef USING_XVMC
    QString adaptor_name = QString::null;
    xv_port = GrabSuitableXvPort(XJ_disp, XJ_root, mcodecid,
                                 video_dim.width(), video_dim.height(),
                                 xvmc_chroma, &xvmc_surf_info, &adaptor_name);
    if (xv_port == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not find suitable XvMC surface.");
        return false;
    }

    InstallXErrorHandler(XJ_disp);

    // create XvMC buffers
    bool ok = CreateXvMCBuffers();
    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    if (!ok || errs.size())
    {
        PrintXErrors(XJ_disp, errs);
        DeleteBuffers(XVideoMC, false);
        ok = false;
    }

    if (ok)
    {
        video_output_subtype = XVideoMC;
        if (XVMC_IDCT == (xvmc_surf_info.mc_type & XVMC_IDCT))
            video_output_subtype = XVideoIDCT; 
        if (XVMC_VLD == (xvmc_surf_info.mc_type & XVMC_VLD))
            video_output_subtype = XVideoVLD;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create XvMC Buffers.");

        xvmc_osd_lock.lock();
        for (uint i=0; i<xvmc_osd_available.size(); i++)
            delete xvmc_osd_available[i];
        xvmc_osd_available.clear();
        xvmc_osd_lock.unlock();
        VERBOSE(VB_PLAYBACK, LOC + "Closing XVideo port " << xv_port);
        X11L;
        XvUngrabPort(XJ_disp, xv_port, CurrentTime);
        del_open_xv_port(xv_port);
        X11U;
        xv_port = -1;
    }

    return ok;
#else // USING_XVMC
    return false;
#endif // USING_XVMC
}

/**
 * \fn VideoOutputXv::InitXVideo()
 * Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitXVideo()
{
    QString adaptor_name = QString::null;
    xv_port = GrabSuitableXvPort(XJ_disp, XJ_root, kCodec_MPEG2,
                                 video_dim.width(), video_dim.height(),
                                 0, NULL, &adaptor_name);
    if (xv_port == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not find suitable XVideo surface.");
        return false;
    }

    VERBOSE(VB_IMPORTANT, LOC + QString("XVideo Adaptor Name: '%1'")
            .arg(adaptor_name));

    xv_hue_base = ((adaptor_name == "ATI Radeon Video Overlay") ||
                   (adaptor_name == "XV_SWOV" /* VIA 10K & 12K */)) ? 50 : 0;

    InstallXErrorHandler(XJ_disp);

    bool foundimageformat = false;
    int formats = 0;
    XvImageFormatValues *fo;
    X11S(fo = XvListImageFormats(XJ_disp, xv_port, &formats));
    for (int i = 0; i < formats; i++)
    {
        if ((fo[i].id == GUID_I420_PLANAR) ||
            (fo[i].id == GUID_IYUV_PLANAR))
        {
            foundimageformat = true;
            xv_chroma = GUID_I420_PLANAR;
        }
    }

    if (!foundimageformat)
    {
        for (int i = 0; i < formats; i++)
        {
            if (fo[i].id == GUID_YV12_PLANAR)
            {
                foundimageformat = true;
                xv_chroma = GUID_YV12_PLANAR;
            }
        }
    }

    for (int i = 0; i < formats; i++)
    {
        char *chr = (char*) &(fo[i].id);
        VERBOSE(VB_PLAYBACK, LOC + QString("XVideo Format #%1 is '%2%3%4%5'")
                .arg(i).arg(chr[0]).arg(chr[1]).arg(chr[2]).arg(chr[3]));
    }

    if (fo)
        X11S(XFree(fo));

    if (foundimageformat)
    {
        char *chr = (char*) &xv_chroma;
        VERBOSE(VB_PLAYBACK, LOC + QString("Using XVideo Format '%1%2%3%4'")
                .arg(chr[0]).arg(chr[1]).arg(chr[2]).arg(chr[3]));
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Couldn't find the proper XVideo image format.");
        VERBOSE(VB_PLAYBACK, LOC + "Closing XVideo port " << xv_port);
        X11L;
        XvUngrabPort(XJ_disp, xv_port, CurrentTime);
        del_open_xv_port(xv_port);
        X11U;
        xv_port = -1;
    }

    bool ok = xv_port >= 0;
    if (ok)
        ok = CreateBuffers(XVideo);

    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    if (!ok || errs.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create XVideo Buffers.");
        DeleteBuffers(XVideo, false);
        VERBOSE(VB_PLAYBACK, LOC + "Closing XVideo port " << xv_port);
        X11L;
        XvUngrabPort(XJ_disp, xv_port, CurrentTime);
        del_open_xv_port(xv_port);
        X11U;
        xv_port = -1;
        ok = false;
    }
    else
        video_output_subtype = XVideo;
    
    return ok;
}

/**
 * \fn VideoOutputXv::InitXShm()
 * Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitXShm()
{
    InstallXErrorHandler(XJ_disp);

    VERBOSE(VB_IMPORTANT, LOC +
            "Falling back to X shared memory video output."
            "\n\t\t\t      *** May be slow ***");

    bool ok = CreateBuffers(XShm);

    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    if (!ok || errs.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate X shared memory.");
        PrintXErrors(XJ_disp, errs);
        DeleteBuffers(XShm, false);
        ok = false;
    }
    else
        video_output_subtype = XShm;
    
    return ok;
}

/**
 * \fn VideoOutputXv::InitXlib()
 * Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitXlib()
{ 
    InstallXErrorHandler(XJ_disp);

    VERBOSE(VB_IMPORTANT, LOC +
            "Falling back to X11 video output over a network socket."
            "\n\t\t\t      *** May be very slow ***");

    bool ok = CreateBuffers(Xlib);

    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    if (!ok || errs.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create X buffers.");
        PrintXErrors(XJ_disp, errs);
        DeleteBuffers(Xlib, false);
        ok = false;
    }
    else
        video_output_subtype = Xlib;

    return ok;
}

/** \fn VideoOutputXv::GetBestSupportedCodec(uint,uint,uint,uint,uint,int,bool)
 *
 *  \return MythCodecID for the best supported codec on the main display.
 */
MythCodecID VideoOutputXv::GetBestSupportedCodec(
    uint width,       uint height,
    uint osd_width,   uint osd_height,
    uint stream_type, int xvmc_chroma,    bool test_surface)
{
    (void)width, (void)height, (void)osd_width, (void)osd_height;
    (void)stream_type, (void)xvmc_chroma, (void)test_surface;

#ifdef USING_XVMC
    Display *disp;
    X11S(disp = XOpenDisplay(NULL));

    // Disable features based on environment and DB values.
    bool use_xvmc_vld = false, use_xvmc_idct = false, use_xvmc = false;
    bool use_xv = true, use_shm = true;

    QString dec = gContext->GetSetting("PreferredMPEG2Decoder", "ffmpeg");
    if (dec != "libmpeg2" && height < 720 && 
        gContext->GetNumSetting("UseXvMCForHDOnly", 0))
        dec = "ffmpeg";
    if (dec == "xvmc")
        use_xvmc_idct = use_xvmc = true;
    else if (dec == "xvmc-vld")
        use_xvmc_vld = use_xvmc = true;

    SetFromEnv(use_xvmc_vld, use_xvmc_idct, use_xvmc, use_xv, use_shm);
    SetFromHW(disp, use_xvmc, use_xv, use_shm);

    MythCodecID ret = (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
    if (use_xvmc_vld &&
        XvMCSurfaceTypes::has(disp, XvVLD, stream_type, xvmc_chroma,
                              width, height, osd_width, osd_height))
    {
        ret = (MythCodecID)(kCodec_MPEG1_VLD + (stream_type-1));
    }
    else if (use_xvmc_idct &&
        XvMCSurfaceTypes::has(disp, XvIDCT, stream_type, xvmc_chroma,
                              width, height, osd_width, osd_height))
    {
        ret = (MythCodecID)(kCodec_MPEG1_IDCT + (stream_type-1));
    }
    else if (use_xvmc &&
             XvMCSurfaceTypes::has(disp, XvMC, stream_type, xvmc_chroma,
                                   width, height, osd_width, osd_height))
    {
        ret = (MythCodecID)(kCodec_MPEG1_XVMC + (stream_type-1));
    }

    bool ok = true;
    if (test_surface && ret > kCodec_NORMAL_END)
    {
        Window root;
        XvMCSurfaceInfo info;

        ok = false;
        X11S(root = DefaultRootWindow(disp));
        int port = GrabSuitableXvPort(disp, root, ret, width, height,
                                      xvmc_chroma, &info);
        if (port >= 0)
        {
            XvMCContext *ctx =
                CreateXvMCContext(disp, port, info.surface_type_id,
                                  width, height);
            ok = NULL != ctx;
            DeleteXvMCContext(disp, ctx);
            VERBOSE(VB_PLAYBACK, LOC + "Closing XVideo port " << port);
            X11L;
            XvUngrabPort(disp, port, CurrentTime);
            del_open_xv_port(port);
            X11U;
        }
    }
    X11S(XCloseDisplay(disp));
    X11S(ok |= cnt_open_xv_port() > 0); // also ok if we already opened port..

    if (!ok)
    {
        QString msg = LOC_ERR + "Could not open XvMC port...\n"
                "\n"
                "\t\t\tYou may wish to verify that your DISPLAY\n"
                "\t\t\tenvironment variable does not use an external\n"
                "\t\t\tnetwork connection.\n";
#ifdef USING_XVMCW
        msg +=  "\n"
                "\t\t\tYou may also wish to verify that\n"
                "\t\t\t/etc/X11/XvMCConfig contains the correct\n"
                "\t\t\tvendor's XvMC library.\n";
#endif // USING_XVMCW
        VERBOSE(VB_IMPORTANT, msg);
        ret = (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
    }

    return ret;
#else // if !USING_XVMC
    return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
#endif // !USING_XVMC
}

#define XV_INIT_FATAL_ERROR_TEST(test,msg) \
do { \
    if (test) \
    { \
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg << " Exiting playback."); \
        errored = true; \
        return false; \
    } \
} while (false)

bool VideoOutputXv::InitSetupBuffers(void)
{
   // Set use variables...
    bool vld, idct, mc, xv, shm;
    myth2av_codecid(myth_codec_id, vld, idct, mc);
    xv = shm = !vld && !idct;
    SetFromEnv(vld, idct, mc, xv, shm);
    SetFromHW(XJ_disp, mc, xv, shm);
    bool use_chroma_key_osd = gContext->GetNumSettingOnHost(
        "UseChromaKeyOSD", gContext->GetHostName(), 0);
    use_chroma_key_osd &= (xv || vld || idct || mc);

    xvmc_tex = XvMCTextures::Create(XJ_disp, XJ_curwin, XJ_screen_num,
                                    video_dim, display_visible_rect.size());
    if (xvmc_tex)
    {
        VERBOSE(VB_IMPORTANT, LOC + "XvMCTex: Init succeeded");
        xvmc_buf_attr->SetOSDNum(0); // disable XvMC blending OSD
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC + "XvMCTex: Init failed");
    }

    // create chroma key osd structure if needed
    if (!xvmc_tex && use_chroma_key_osd &&
        ((32 == XJ_depth) || (24 == XJ_depth)))
    {
        chroma_osd = new ChromaKeyOSD(this);
#ifdef USING_XVMC
        xvmc_buf_attr->SetOSDNum(0); // disable XvMC blending OSD
#endif // USING_XVMC
    }
    else if (use_chroma_key_osd)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString(
                    "Number of bits per pixel is %1, \n\t\t\t"
                    "but we only support ARGB 32 bbp for ChromaKeyOSD.")
                .arg(XJ_depth));
    }

    // Create video buffers
    bool ok = InitVideoBuffers(myth_codec_id, xv, shm);
    XV_INIT_FATAL_ERROR_TEST(!ok, "Failed to get any video output");

    if (!xvmc_tex && video_output_subtype >= XVideo)
        InitColorKey(true);

    // Deal with the nVidia 6xxx & 7xxx cards which do
    // not support chromakeying with the latest drivers
    if (!xv_colorkey && chroma_osd)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Ack! Disabling ChromaKey OSD"
                "\n\t\t\tWe can't use ChromaKey OSD "
                "if chromakeying is not supported!");

#ifdef USING_XVMC
        // Delete the buffers we allocated before
        DeleteBuffers(VideoOutputSubType(), true);
        if (xv_port >= 0)
        {
            VERBOSE(VB_PLAYBACK, LOC + "Closing XVideo port " << xv_port);
            X11L;
            XvUngrabPort(XJ_disp, xv_port, CurrentTime);
            del_open_xv_port(xv_port);
            X11U;
            xv_port = -1;
        }
#endif // USING_XVMC

        // Get rid of the chromakey osd..
        delete chroma_osd;
        chroma_osd = NULL;

#ifdef USING_XVMC
        // Recreate video buffers
        xvmc_buf_attr->SetOSDNum(1);
        ok = InitVideoBuffers(myth_codec_id, xv, shm);
        XV_INIT_FATAL_ERROR_TEST(!ok, "Failed to get any video output (nCK)");
#endif // USING_XVMC
    }

    // The XVideo output methods sometimes allow the picture to
    // be adjusted, if the chroma keying color can be discovered.
    if (VideoOutputSubType() >= XVideo && xv_colorkey)
        InitPictureAttributes();

    return true;
}

/**
 * \fn VideoOutputXv::Init(int,int,float,WId,int,int,int,int,WId)
 * Initializes class for video output.
 *
 * \return success or failure.
 */
bool VideoOutputXv::Init(
    int width, int height, float aspect, 
    WId winid, int winx, int winy, int winw, int winh, WId embedid)
{
    needrepaint = true;

    XV_INIT_FATAL_ERROR_TEST(winid <= 0, "Invalid Window ID.");

    X11S(XJ_disp = XOpenDisplay(NULL));
    XV_INIT_FATAL_ERROR_TEST(!XJ_disp, "Failed to open display.");

    // Initialize X stuff
    X11L;
    XJ_screen     = DefaultScreenOfDisplay(XJ_disp);
    XJ_screen_num = DefaultScreen(XJ_disp);
    XJ_white      = XWhitePixel(XJ_disp, XJ_screen_num);
    XJ_black      = XBlackPixel(XJ_disp, XJ_screen_num);
    XJ_curwin     = winid;
    XJ_win        = winid;
    XJ_root       = DefaultRootWindow(XJ_disp);
    XJ_gc         = XCreateGC(XJ_disp, XJ_win, 0, 0);
    XJ_depth      = DefaultDepthOfScreen(XJ_screen);
    X11U;

    // Basic setup
    VideoOutput::Init(width, height, aspect,
                      winid, winx, winy, winw, winh,
                      embedid);

    // Set resolution/measurements (check XRandR, Xinerama, config settings)
    InitDisplayMeasurements(width, height);

    // Set embedding window id
    if (embedid > 0)
        XJ_curwin = XJ_win = embedid;

    if (!InitSetupBuffers())
        return false;

    MoveResize(); 

    XJ_started = true;

    return true;
}
#undef XV_INIT_FATAL_ERROR_TEST

/**
 * \fn VideoOutputXv::InitColorKey(bool)
 * Initializes color keying support used by XVideo output methods.
 *
 * \param turnoffautopaint turn off or on XV_AUTOPAINT_COLORKEY property.
 */
void VideoOutputXv::InitColorKey(bool turnoffautopaint)
{
    int ret = Success, xv_val=0;
    xv_draw_colorkey = true;
    xv_colorkey = 0; // set to invalid value as a sentinel

    Atom xv_atom;
    XvAttribute *attributes;
    int attrib_count;

    X11S(attributes = XvQueryPortAttributes(XJ_disp, xv_port, &attrib_count));
    for (int i = (attributes) ? 0 : attrib_count; i < attrib_count; i++)
    {
        if (!strcmp(attributes[i].name, "XV_AUTOPAINT_COLORKEY"))
        {
            X11S(xv_atom = XInternAtom(XJ_disp, "XV_AUTOPAINT_COLORKEY", False));
            if (xv_atom == None)
                continue;

            X11L;
            if (turnoffautopaint)
                ret = XvSetPortAttribute(XJ_disp, xv_port, xv_atom, 0);
            else
                ret = XvSetPortAttribute(XJ_disp, xv_port, xv_atom, 1);

            ret = XvGetPortAttribute(XJ_disp, xv_port, xv_atom, &xv_val);
            // turn of colorkey drawing if autopaint is on
            if (Success == ret && xv_val)
                xv_draw_colorkey = false;
            X11U;
        }
    }
    if (attributes)
        X11S(XFree(attributes));

    if (xv_draw_colorkey)
    {
        X11S(xv_atom = XInternAtom(XJ_disp, "XV_COLORKEY", False));
        if (xv_atom != None)
        {
            X11S(ret = XvGetPortAttribute(XJ_disp, xv_port, xv_atom, 
                                          &xv_colorkey));

            if (ret == Success && xv_colorkey == 0)
            {
                const int default_colorkey = 1;
                X11S(ret = XvSetPortAttribute(XJ_disp, xv_port, xv_atom,
                                              default_colorkey));
                if (ret == Success)
                {
                    VERBOSE(VB_PLAYBACK, LOC +
                            "0,0,0 is the only bad color key for MythTV, "
                            "using "<<default_colorkey<<" instead.");
                    xv_colorkey = default_colorkey;
                }
                ret = Success;
            }

            if (ret != Success)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Couldn't get the color key color,"
                        "\n\t\t\tprobably due to a driver bug or limitation."
                        "\n\t\t\tYou might not get any video, "
                        "but we'll try anyway.");
                xv_colorkey = 0;
            }
        }
    }
}

// documented in videooutbase.cpp
bool VideoOutputXv::SetDeinterlacingEnabled(bool enable)
{
    bool deint = VideoOutput::SetDeinterlacingEnabled(enable);
    xv_need_bobdeint_repaint = (m_deintfiltername == "bobdeint");
    return deint;
}

bool VideoOutputXv::SetupDeinterlace(bool interlaced,
                                     const QString& overridefilter)
{
    QString f = (VideoOutputSubType() > XVideo) ? "bobdeint" : overridefilter;
    bool deint = VideoOutput::SetupDeinterlace(interlaced, f);
    needrepaint = true;
    return deint;
}

/**
 * \fn VideoOutput::NeedsDoubleFramerate() const
 * Approves bobdeint filter for XVideo and XvMC surfaces,
 * rejects other filters for XvMC, and defers to
 * VideoOutput::ApproveDeintFilter(const QString&)
 * otherwise.
 *
 * \return whether current video output supports a specific filter.
 */
bool VideoOutputXv::ApproveDeintFilter(const QString& filtername) const
{
    // TODO implement bobdeint for non-Xv[MC]
    VOSType vos = VideoOutputSubType();
    if (filtername == "bobdeint" && vos >= XVideo)
        return true;
    else if (vos > XVideo)
        return false;
    else
        return VideoOutput::ApproveDeintFilter(filtername);
}

XvMCContext* VideoOutputXv::CreateXvMCContext(
    Display* disp, int port, int surf_type, int width, int height)
{
    (void)disp; (void)port; (void)surf_type; (void)width; (void)height;
#ifdef USING_XVMC
    int ret = Success;
    XvMCContext *ctx = new XvMCContext;
    X11S(ret = XvMCCreateContext(disp, port, surf_type, width, height,
                                 XVMC_DIRECT, ctx));
    if (ret != Success)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to create XvMC Context, status(%1): %2")
                .arg(ret).arg(ErrorStringXvMC(ret)));

        delete ctx;
        ctx = NULL;
    }
    return ctx;
#else // if !USING_XVMC 
    return NULL;
#endif // !USING_XVMC
}

void VideoOutputXv::DeleteXvMCContext(Display* disp, XvMCContext*& ctx)
{
    (void)disp; (void)ctx;
#ifdef USING_XVMC
    if (ctx)
    {
        X11S(XvMCDestroyContext(disp, ctx));
        delete ctx;
        ctx = NULL;
    }
#endif // !USING_XVMC
}

bool VideoOutputXv::CreateXvMCBuffers(void)
{
#ifdef USING_XVMC
    xvmc_ctx = CreateXvMCContext(XJ_disp, xv_port,
                                 xvmc_surf_info.surface_type_id,
                                 video_dim.width(), video_dim.height());
    if (!xvmc_ctx)
        return false;

    bool createBlocks = !(XVMC_VLD == (xvmc_surf_info.mc_type & XVMC_VLD));
    xvmc_surfs = CreateXvMCSurfaces(xvmc_buf_attr->GetMaxSurf(), createBlocks);
    if (xvmc_surfs.size() < xvmc_buf_attr->GetMinSurf())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to create XvMC Surfaces");
        DeleteBuffers(XVideoMC, false);
        return false;
    }

    bool ok = vbuffers.CreateBuffers(video_dim.width(), video_dim.height(),
                                     XJ_disp, xvmc_ctx,
                                     &xvmc_surf_info, xvmc_surfs);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to create XvMC Buffers");
        DeleteBuffers(XVideoMC, false);
        return false;
    }

    xvmc_osd_lock.lock();
    for (uint i=0; i < xvmc_buf_attr->GetOSDNum(); i++)
    {
        XvMCOSD *xvmc_osd =
            new XvMCOSD(XJ_disp, xv_port, xvmc_surf_info.surface_type_id,
                        xvmc_surf_info.flags);
        xvmc_osd->CreateBuffer(*xvmc_ctx,
                               video_dim.width(), video_dim.height());
        xvmc_osd_available.push_back(xvmc_osd);
    }
    xvmc_osd_lock.unlock();


    X11S(XSync(XJ_disp, False));

    return true;
#else
    return false;
#endif // USING_XVMC
}

vector<void*> VideoOutputXv::CreateXvMCSurfaces(uint num, bool create_xvmc_blocks)
{
    (void)num;
    (void)create_xvmc_blocks;

    vector<void*> surfaces;
#ifdef USING_XVMC
    uint blocks_per_macroblock = calcBPM(xvmc_chroma);
    uint num_mv_blocks   = (((video_dim.width()  + 15) / 16) *
                            ((video_dim.height() + 15) / 16));
    uint num_data_blocks = num_mv_blocks * blocks_per_macroblock;

    // create needed XvMC stuff
    bool ok = true;
    for (uint i = 0; i < num; i++)
    {
        xvmc_vo_surf_t *surf = new xvmc_vo_surf_t;
        bzero(surf, sizeof(xvmc_vo_surf_t));

        X11L;

        int ret = XvMCCreateSurface(XJ_disp, xvmc_ctx, &(surf->surface));
        ok &= (Success == ret);

        if (create_xvmc_blocks && ok)
        {
            ret = XvMCCreateBlocks(XJ_disp, xvmc_ctx, num_data_blocks,
                                   &(surf->blocks));
            if (Success != ret)
            {
                XvMCDestroySurface(XJ_disp, &(surf->surface));
                ok = false;
            }
        }

        if (create_xvmc_blocks && ok)
        {
            ret = XvMCCreateMacroBlocks(XJ_disp, xvmc_ctx, num_mv_blocks,
                                        &(surf->macro_blocks));
            if (Success != ret)
            {
                XvMCDestroyBlocks(XJ_disp, &(surf->blocks));
                XvMCDestroySurface(XJ_disp, &(surf->surface));
                ok = false;
            }
        }

        X11U;

        if (!ok)
        {
            delete surf;
            break;
        }
        surfaces.push_back(surf);
    }
#endif // USING_XVMC
    return surfaces;
}

/**
 * \fn VideoOutputXv::CreateShmImages(uint num, bool use_xv)
 * \brief Creates Shared Memory Images.
 *
 *  Each XvImage/XImage created is added to xv_buffers, and shared
 *  memory info is added to XJ_shm_infos.
 * 
 * \param  num      number of buffers to create
 * \param  use_xv   use XvShmCreateImage instead of XShmCreateImage
 * \return vector containing image data for each buffer created
 */
vector<unsigned char*> VideoOutputXv::CreateShmImages(uint num, bool use_xv)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("CreateShmImages(%1): video_dim: %2x%3")
            .arg(num).arg(video_dim.width()).arg(video_dim.height()));

    vector<unsigned char*> bufs;
    XShmSegmentInfo blank;
    // for now make reserve big enough to avoid realloc.. 
    // we should really have vector of pointers...
    XJ_shm_infos.reserve(max(num + 32, (uint)128));
    for (uint i = 0; i < num; i++)
    {
        XJ_shm_infos.push_back(blank);
        void *image = NULL;
        int size = 0;
        int desiredsize = 0;

        X11L;

        if (use_xv)
        {
            image = XvShmCreateImage(XJ_disp, xv_port, xv_chroma, 0, 
                                     video_dim.width(), video_dim.height(),
                                     &XJ_shm_infos[i]);
            size = ((XvImage*)image)->data_size + 64;
            desiredsize = video_dim.width() * video_dim.height() * 3 / 2;

            if (image && size < desiredsize)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "CreateXvShmImages(): "
                        "XvShmCreateImage() failed to create image of the "
                        "requested size.");
                XFree(image);
                image = NULL;
            }
        }
        else
        {
            XImage *img =
                XShmCreateImage(XJ_disp, DefaultVisual(XJ_disp, XJ_screen_num),
                                XJ_depth, ZPixmap, 0, &XJ_shm_infos[i],
                                display_visible_rect.width(),
                                display_visible_rect.height());
            size = img->bytes_per_line * img->height + 64;
            image = img;
            desiredsize = (display_visible_rect.width() *
                           display_visible_rect.height() * 3 / 2);
            if (image && size < desiredsize)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "CreateXvShmImages(): "
                        "XShmCreateImage() failed to create image of the "
                        "requested size.");
                XDestroyImage((XImage *)image);
                image = NULL;
            }
        }

        X11U;

        if (image)
        {
            XJ_shm_infos[i].shmid = shmget(IPC_PRIVATE, size, IPC_CREAT|0777);
            if (XJ_shm_infos[i].shmid >= 0)
            {
                XJ_shm_infos[i].shmaddr = (char*) shmat(XJ_shm_infos[i].shmid, 0, 0);
                if (use_xv)
                    ((XvImage*)image)->data = XJ_shm_infos[i].shmaddr;
                else
                    ((XImage*)image)->data = XJ_shm_infos[i].shmaddr;
                xv_buffers[(unsigned char*) XJ_shm_infos[i].shmaddr] = image;
                XJ_shm_infos[i].readOnly = False;

                X11L;
                XShmAttach(XJ_disp, &XJ_shm_infos[i]);
                XSync(XJ_disp, False); // needed for FreeBSD?
                X11U;

                // Mark for delete immediately.
                // It won't actually be removed until after we detach it.
                shmctl(XJ_shm_infos[i].shmid, IPC_RMID, 0);

                bufs.push_back((unsigned char*) XJ_shm_infos[i].shmaddr);
            }
            else
            { 
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "CreateXvShmImages(): shmget() failed." + ENO);
                break;
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "CreateXvShmImages(): "
                    "XvShmCreateImage() failed to create image.");
            break;
        }
    }
    return bufs;
}

bool VideoOutputXv::CreateBuffers(VOSType subtype)
{
    bool ok = false;

    if (subtype > XVideo && xv_port >= 0)
        ok = CreateXvMCBuffers();
    else if (subtype == XVideo && xv_port >= 0)
    {
        vector<unsigned char*> bufs = 
            CreateShmImages(vbuffers.allocSize(), true);
        ok = vbuffers.CreateBuffers(
            video_dim.width(), video_dim.height(), bufs);

        clear_xv_buffers(vbuffers, video_dim.width(), video_dim.height(),
                         xv_chroma);

        X11S(XSync(XJ_disp, False));
        if (xv_chroma != GUID_I420_PLANAR)
            xv_color_conv_buf = new unsigned char[
                video_dim.width() * video_dim.height() * 3 / 2];
    }
    else if (subtype == XShm || subtype == Xlib)
    {
        if (subtype == XShm)
        {
            CreateShmImages(1, false);
            XJ_non_xv_image = (XImage*) xv_buffers.begin()->second;
        }
        else
        {

            X11L;

            int bytes_per_line = XJ_depth / 8 * display_visible_rect.width();
            int scrn = DefaultScreen(XJ_disp);
            Visual *visual = DefaultVisual(XJ_disp, scrn);
            XJ_non_xv_image = XCreateImage(XJ_disp, visual, XJ_depth,
                                           ZPixmap, /*offset*/0, /*data*/0,
                                           display_visible_rect.width(),
                                           display_visible_rect.height(),
                                           /*bitmap_pad*/0,
                                           bytes_per_line);

            X11U;

            if (!XJ_non_xv_image)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "XCreateImage failed: "
                        <<"XJ_disp("<<XJ_disp<<") visual("<<visual<<") "<<endl
                        <<"                        "
                        <<"XJ_depth("<<XJ_depth<<") "
                        <<"WxH("<<display_visible_rect.width()
                        <<"x"<<display_visible_rect.height()<<") "
                        <<"bpl("<<bytes_per_line<<")");
                return false;
            }
            XJ_non_xv_image->data = (char*) malloc(
                bytes_per_line * display_visible_rect.height());
        }

        switch (XJ_non_xv_image->bits_per_pixel)
        {   // only allow these three output formats for non-xv videout
            case 16: non_xv_av_format = PIX_FMT_RGB565; break;
            case 24: non_xv_av_format = PIX_FMT_RGB24;  break;
            case 32: non_xv_av_format = PIX_FMT_RGBA32; break;
            default: non_xv_av_format = PIX_FMT_NB;
        }
        if (PIX_FMT_NB == non_xv_av_format)
        {
            QString msg = QString(
                "Non XVideo modes only support displays with 16,\n\t\t\t"
                "24, or 32 bits per pixel. But you have a %1 bpp display.")
                .arg(XJ_depth*8);
            
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
        }
        else
            ok = vbuffers.CreateBuffers(video_dim.width(), video_dim.height());

    }

    if (ok)
        CreatePauseFrame();

    return ok;
}

void VideoOutputXv::DeleteBuffers(VOSType subtype, bool delete_pause_frame)
{
    (void) subtype;
    DiscardFrames(true);

#ifdef USING_XVMC
    // XvMC buffers
    for (uint i=0; i<xvmc_surfs.size(); i++)
    {
        xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*) xvmc_surfs[i];
        X11S(XvMCHideSurface(XJ_disp, &(surf->surface)));
    }
    DiscardFrames(true);
    for (uint i=0; i<xvmc_surfs.size(); i++)
    {
        xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*) xvmc_surfs[i];

        X11L;

        XvMCDestroySurface(XJ_disp, &(surf->surface));
        XvMCDestroyMacroBlocks(XJ_disp, &(surf->macro_blocks));
        XvMCDestroyBlocks(XJ_disp, &(surf->blocks));

        X11U;
    }
    xvmc_surfs.clear();

    // OSD buffers
    xvmc_osd_lock.lock();
    for (uint i=0; i<xvmc_osd_available.size(); i++)
    {
        xvmc_osd_available[i]->DeleteBuffer();
        delete xvmc_osd_available[i];
    }
    xvmc_osd_available.clear();
    xvmc_osd_lock.unlock();

    if (xvmc_tex)
    {
        delete xvmc_tex;
        xvmc_tex = NULL;
    }
#endif // USING_XVMC

    vbuffers.DeleteBuffers();

    if (xv_color_conv_buf)
    {
        delete [] xv_color_conv_buf;
        xv_color_conv_buf = NULL;
    }

    if (delete_pause_frame)
    {
        if (av_pause_frame.buf)
        {
            delete [] av_pause_frame.buf;
            av_pause_frame.buf = NULL;
        }
        if (av_pause_frame.qscale_table)
        {
            delete [] av_pause_frame.qscale_table;
            av_pause_frame.qscale_table = NULL;
        }
    }

    for (uint i=0; i<XJ_shm_infos.size(); ++i)
    {
        X11S(XShmDetach(XJ_disp, &(XJ_shm_infos[i])));
        XvImage *image = (XvImage*) 
            xv_buffers[(unsigned char*)XJ_shm_infos[i].shmaddr];
        if (image)
        {
            if ((XImage*)image == (XImage*)XJ_non_xv_image)
                X11S(XDestroyImage((XImage*)XJ_non_xv_image));
            else
                X11S(XFree(image));
        }
        if (XJ_shm_infos[i].shmaddr)
            shmdt(XJ_shm_infos[i].shmaddr);
        if (XJ_shm_infos[i].shmid > 0)
            shmctl(XJ_shm_infos[0].shmid, IPC_RMID, 0);
    }
    XJ_shm_infos.clear();
    xv_buffers.clear();
    XJ_non_xv_image = NULL;

#ifdef USING_XVMC
    DeleteXvMCContext(XJ_disp, xvmc_ctx);
#endif // USING_XVMC
}

void VideoOutputXv::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    QMutexLocker locker(&global_lock);

    if (embedding)
    {
        MoveResize();
        return;
    }

    XJ_curwin = wid;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);

    // Switch to GUI size
    if (display_res)
        display_res->SwitchToGUI();
}

void VideoOutputXv::StopEmbedding(void)
{
    if (!embedding)
        return;

    QMutexLocker locker(&global_lock);

    XJ_curwin = XJ_win;
    VideoOutput::StopEmbedding();

    // Switch back to resolution for full screen video
    if (display_res)
        display_res->SwitchToVideo(video_dim.width(), video_dim.height());
}

VideoFrame *VideoOutputXv::GetNextFreeFrame(bool /*allow_unsafe*/)
{
    return vbuffers.GetNextFreeFrame(false, false);
}

/**
 * \fn VideoOutputXv::DiscardFrame(VideoFrame *frame)
 *  Frame is ready to be reused by decoder added to the
 *  done or available list.
 *
 * \param frame to discard.
 */
void VideoOutputXv::DiscardFrame(VideoFrame *frame)
{
    bool displaying = false;
    if (!frame)
        return;

#ifdef USING_XVMC
    vbuffers.LockFrame(frame, "DiscardFrame -- XvMC display check");
    if (frame && VideoOutputSubType() >= XVideoMC)
    {
        // Check display status
        VideoFrame* pframe = NULL;
        VideoFrame* osdframe = NULL;
        if (xvmc_buf_attr->GetOSDNum())
            osdframe = vbuffers.GetOSDFrame(frame);

        if (osdframe)
            vbuffers.SetOSDFrame(frame, NULL);
        else
            pframe = vbuffers.GetOSDParent(frame);

        SyncSurface(frame);
        displaying = IsDisplaying(frame);
        vbuffers.UnlockFrame(frame, "DiscardFrame -- XvMC display check A");

        SyncSurface(osdframe);
        displaying |= IsDisplaying(osdframe);

        if (!displaying && pframe)
            vbuffers.SetOSDFrame(frame, NULL);
    }
    else
        vbuffers.UnlockFrame(frame, "DiscardFrame -- XvMC display check B");
#endif

    if (displaying || vbuffers.HasChildren(frame))
        vbuffers.safeEnqueue(kVideoBuffer_displayed, frame);
    else
    {
        vbuffers.LockFrame(frame,   "DiscardFrame -- XvMC not displaying");
#ifdef USING_XVMC
        if (frame && VideoOutputSubType() >= XVideoMC)
        {
            GetRender(frame)->p_past_surface   = NULL;
            GetRender(frame)->p_future_surface = NULL;
        }
#endif
        vbuffers.UnlockFrame(frame, "DiscardFrame -- XvMC not displaying");
        vbuffers.RemoveInheritence(frame);
        vbuffers.DiscardFrame(frame);
    }
}

void VideoOutputXv::ClearAfterSeek(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "ClearAfterSeek()");
    DiscardFrames(false);
#ifdef USING_XVMC
    if (VideoOutputSubType() > XVideo)
    {
        for (uint i=0; i<xvmc_surfs.size(); i++)
        {
            xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*) xvmc_surfs[i];
            X11S(XvMCHideSurface(XJ_disp, &(surf->surface)));
        }
        DiscardFrames(true);
    }
#endif
}

#define DQ_COPY(DST, SRC) \
    do { \
        DST.insert(DST.end(), vbuffers.begin_lock(SRC), vbuffers.end(SRC)); \
        vbuffers.end_lock(); \
    } while (0)

void VideoOutputXv::DiscardFrames(bool next_frame_keyframe)
{
    VERBOSE(VB_PLAYBACK, LOC + "DiscardFrames("<<next_frame_keyframe<<")");
    if (VideoOutputSubType() <= XVideo)
    {
        vbuffers.DiscardFrames(next_frame_keyframe);
        VERBOSE(VB_PLAYBACK, LOC + QString("DiscardFrames() 3: %1 -- done()")
                .arg(vbuffers.GetStatus()));
        return;
    }

#ifdef USING_XVMC
    frame_queue_t::iterator it;
    frame_queue_t syncs;

    // Print some debugging
    vbuffers.begin_lock(kVideoBuffer_displayed); // Lock X
    VERBOSE(VB_PLAYBACK, LOC + QString("DiscardFrames() 1: %1")
            .arg(vbuffers.GetStatus()));
    vbuffers.end_lock(); // Lock X

    // Finish rendering all these surfaces and move them
    // from the used queue to the displayed queue.
    // This allows us to reuse these surfaces, if they
    // get moved to the used list in CheckFrameStates().
    // This will only happen if avlib isn't using them
    // either and they are not currently being displayed.
    vbuffers.begin_lock(kVideoBuffer_displayed); // Lock Y
    DQ_COPY(syncs, kVideoBuffer_used);
    for (it = syncs.begin(); it != syncs.end(); ++it)
    {
        SyncSurface(*it, -1); // sync past
        SyncSurface(*it, +1); // sync future
        SyncSurface(*it,  0); // sync current
        vbuffers.safeEnqueue(kVideoBuffer_displayed, *it);
    }
    syncs.clear();
    vbuffers.end_lock(); // Lock Y

    CheckFrameStates();

    // If the next frame is a keyframe we can clear out a lot more...
    if (next_frame_keyframe)
    {
        vbuffers.begin_lock(kVideoBuffer_displayed); // Lock Z

        // Move all the limbo and pause frames to displayed
        DQ_COPY(syncs, kVideoBuffer_limbo);
        for (it = syncs.begin(); it != syncs.end(); ++it)
        {
            SyncSurface(*it, -1); // sync past
            SyncSurface(*it, +1); // sync future
            SyncSurface(*it,  0); // sync current
            vbuffers.safeEnqueue(kVideoBuffer_displayed, *it);
        }

        VERBOSE(VB_PLAYBACK, LOC + QString("DiscardFrames() 2: %1")
                .arg(vbuffers.GetStatus()));

        vbuffers.end_lock(); // Lock Z

        // Now call CheckFrameStates() to remove inheritence and
        // move the surfaces to the used list if possible (i.e.
        // if avlib is not using them and they are not currently
        // being displayed on screen).
        CheckFrameStates();
    }
    VERBOSE(VB_PLAYBACK, LOC + QString("DiscardFrames() 3: %1 -- done()")
            .arg(vbuffers.GetStatus()));
        
#endif // USING_XVMC
}

#undef DQ_COPY

/** 
 * \fn VideoOutputXv::DoneDisplayingFrame(void)
 *  This is used to tell this class that the NPV will not
 *  call Show() on this frame again.
 *
 *  If the frame is not referenced elsewhere or all
 *  frames referencing it are done rendering this
 *  removes last displayed frame from used queue
 *  and adds it to the available list. If the frame is 
 *  still being used then it adds it to a special
 *  done displaying list that is checked when
 *  more frames are needed than in the available
 *  list.
 *
 */
void VideoOutputXv::DoneDisplayingFrame(void)
{
    if (VideoOutputSubType() <= XVideo || xvmc_tex)
    {
        vbuffers.DoneDisplayingFrame();
        return;
    }
#ifdef USING_XVMC
    if (vbuffers.size(kVideoBuffer_used))
    {
        VideoFrame *frame = vbuffers.head(kVideoBuffer_used);
        DiscardFrame(frame);

        VideoFrame *osdframe = NULL;
        if (xvmc_buf_attr->GetOSDNum())
            osdframe = vbuffers.GetOSDFrame(frame);

        if (osdframe)
            DiscardFrame(osdframe);
    }
    CheckFrameStates();
#endif
}

/**
 * \fn VideoOutputXv::PrepareFrameXvMC(VideoFrame*,FrameScanType)
 *  
 *  
 */
void VideoOutputXv::PrepareFrameXvMC(VideoFrame *frame, FrameScanType scan)
{
    (void)frame;
    (void)scan;
#ifdef USING_XVMC
    xvmc_render_state_t *render = NULL, *osdrender = NULL;
    VideoFrame *osdframe = NULL;

    if (frame)
    {
        global_lock.lock();
        framesPlayed = frame->frameNumber + 1;
        global_lock.unlock();

        vbuffers.LockFrame(frame, "PrepareFrameXvMC");
        SyncSurface(frame);
        render = GetRender(frame);
        render->state |= MP_XVMC_STATE_DISPLAY_PENDING;
        if (xvmc_tex)
            xvmc_tex->PrepareFrame(render->p_surface,display_video_rect,scan);
        else if (xvmc_buf_attr->GetOSDNum())
            osdframe = vbuffers.GetOSDFrame(frame);
        vbuffers.UnlockFrame(frame, "PrepareFrameXvMC");
    }

    if (osdframe)
    {
        vbuffers.LockFrame(osdframe, "PrepareFrameXvMC -- osd");
        SyncSurface(osdframe);
        osdrender = GetRender(osdframe);
        osdrender->state |= MP_XVMC_STATE_DISPLAY_PENDING;
        vbuffers.UnlockFrame(osdframe, "PrepareFrameXvMC -- osd");
    }
#endif // USING_XVMC
}

/**
 * \fn VideoOutputXv::PrepareFrameXv(VideoFrame *frame)
 *  
 *  
 */
void VideoOutputXv::PrepareFrameXv(VideoFrame *frame)
{
    if (!frame)
        frame = vbuffers.GetScratchFrame();

    XvImage *image = NULL;
    {
        QMutexLocker locker(&global_lock);
        vbuffers.LockFrame(frame, "PrepareFrameXv");
        framesPlayed = frame->frameNumber + 1;
        image        = (XvImage*) xv_buffers[frame->buf];
        vbuffers.UnlockFrame(frame, "PrepareFrameXv");
    }

    if (image && (GUID_YV12_PLANAR == xv_chroma))
    {
        vbuffers.LockFrame(frame, "PrepareFrameXv -- color conversion");
        int width = frame->width;
        int height = frame->height;

        memcpy(xv_color_conv_buf, (unsigned char *)image->data + 
               (width * height), width * height / 4);
        memcpy((unsigned char *)image->data + (width * height),
               (unsigned char *)image->data + (width * height) * 5 / 4,
               width * height / 4);
        memcpy((unsigned char *)image->data + (width * height) * 5 / 4,
               xv_color_conv_buf, width * height / 4);
        vbuffers.UnlockFrame(frame, "PrepareFrameXv -- color conversion");
    }

    if (vbuffers.GetScratchFrame() == frame)
        vbuffers.SetLastShownFrameToScratch();
}

/**
 * \fn VideoOutputXv::PrepareFrameMem(VideoFrame*, FrameScanType)
 *  
 *  
 */
void VideoOutputXv::PrepareFrameMem(VideoFrame *buffer, FrameScanType /*scan*/)
{
    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    vbuffers.LockFrame(buffer, "PrepareFrameMem");

    framesPlayed = buffer->frameNumber + 1;
    int width = buffer->width;
    int height = buffer->height;

    vbuffers.UnlockFrame(buffer, "PrepareFrameMem");

    // bad way to throttle frame display for non-Xv mode.
    // calculate fps we can do and skip enough frames so we don't exceed.
    if (non_xv_frames_shown == 0)
        non_xv_stop_time = time(NULL) + 4;

    if ((!non_xv_fps) && (time(NULL) > non_xv_stop_time))
    {
        non_xv_fps = (int)(non_xv_frames_shown / 4);

        if (non_xv_fps < 25)
        {
            non_xv_show_frame = 120 / non_xv_frames_shown + 1;
            VERBOSE(VB_IMPORTANT, LOC_ERR + "\n"
                    "***\n"
                    "* Your system is not capable of displaying the\n"
                    "* full framerate at "
                    <<display_visible_rect.width()<<"x"
                    <<display_visible_rect.height()<<" resolution.  Frames\n"
                    "* will be skipped in order to keep the audio and\n"
                    "* video in sync.\n");
        }
    }

    non_xv_frames_shown++;

    if ((non_xv_show_frame != 1) && (non_xv_frames_shown % non_xv_show_frame))
        return;

    if (!XJ_non_xv_image)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "XJ_non_xv_image == NULL");
        return;
    }

    unsigned char *sbuf = new unsigned char[
        display_visible_rect.width() * display_visible_rect.height() * 3 / 2];
    AVPicture image_in, image_out;
    ImgReSampleContext *scontext;

    avpicture_fill(&image_out, (uint8_t *)sbuf, PIX_FMT_YUV420P,
                   display_visible_rect.width(),
                   display_visible_rect.height());

    vbuffers.LockFrame(buffer, "PrepareFrameMem");
    if ((display_visible_rect.width() == width) &&
        (display_visible_rect.height() == height))
    {
        memcpy(sbuf, buffer->buf, width * height * 3 / 2);
    }
    else
    {
        avpicture_fill(&image_in, buffer->buf, PIX_FMT_YUV420P,
                       width, height);
        scontext = img_resample_init(display_visible_rect.width(),
                                     display_visible_rect.height(),
                                     width, height);
        img_resample(scontext, &image_out, &image_in);

        img_resample_close(scontext);
    }
    vbuffers.UnlockFrame(buffer, "PrepareFrameMem");

    avpicture_fill(&image_in, (uint8_t *)XJ_non_xv_image->data, 
                   non_xv_av_format, display_visible_rect.width(),
                   display_visible_rect.height());

    img_convert(&image_in, non_xv_av_format, &image_out, PIX_FMT_YUV420P,
                display_visible_rect.width(), display_visible_rect.height());

    {
        QMutexLocker locker(&global_lock);
        X11L;
        if (XShm == video_output_subtype)
            XShmPutImage(XJ_disp, XJ_curwin, XJ_gc, XJ_non_xv_image,
                         0, 0, 0, 0, display_visible_rect.width(),
                         display_visible_rect.height(), False);
        else
            XPutImage(XJ_disp, XJ_curwin, XJ_gc, XJ_non_xv_image, 
                      0, 0, 0, 0, display_visible_rect.width(),
                      display_visible_rect.height());
        X11U;
    }

    delete [] sbuf;
}

// this is documented in videooutbase.cpp
void VideoOutputXv::PrepareFrame(VideoFrame *buffer, FrameScanType scan)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() in PrepareFrame()");
        return;
    }

    if (VideoOutputSubType() > XVideo)
        PrepareFrameXvMC(buffer, scan);
    else if (VideoOutputSubType() == XVideo)
        PrepareFrameXv(buffer);
    else
        PrepareFrameMem(buffer, scan);
}

static void calc_bob(FrameScanType scan, int imgh, int disphoff,
                     int imgy, int dispyoff,
                     int frame_height, int top_field_first,
                     int &field, int &src_y, int &dest_y,
                     int& xv_src_y_incr, int &xv_dest_y_incr)
{
    int dst_half_line_in_src = 0, dest_y_incr = 0, src_y_incr = 0;
    field = 3;
    src_y = imgy;
    dest_y = dispyoff;
    xv_src_y_incr = 0;
    // a negative offset y gives us bobbing, so adjust...
    if (dispyoff < 0)
    {
        dest_y_incr = -dispyoff;
        src_y_incr = (int) (dest_y_incr * imgh / disphoff);
        xv_src_y_incr -= (int) (0.5 * dest_y_incr * imgh / disphoff);
    }

    if ((scan == kScan_Interlaced && top_field_first == 1) ||
        (scan == kScan_Intr2ndField && top_field_first == 0))
    {
        field = 1;
        xv_src_y_incr += - imgy / 2;
    }
    else if ((scan == kScan_Interlaced && top_field_first == 0) ||
             (scan == kScan_Intr2ndField && top_field_first == 1))
    {
        field = 2;
        xv_src_y_incr += (frame_height - imgy) / 2;

        dst_half_line_in_src =
            max((int) round((((double)disphoff)/imgh) - 0.00001), 0);
    }
    src_y += src_y_incr;
    dest_y += dest_y_incr;

#define NVIDIA_6629
#ifdef NVIDIA_6629
    xv_dest_y_incr = dst_half_line_in_src;
    // nVidia v 66.29, does proper compensation when imgh==frame_height
    // but we need to compensate when the difference is >= 5%
    int mod = 0;
    if (frame_height>=(int)(imgh+(0.05*frame_height)) && 2==field)
    {
        //int nrml = (int) round((((double)disphoff)/frame_height) - 0.00001);
        mod = -dst_half_line_in_src;
        dest_y += mod;
        xv_dest_y_incr -= mod;
    }
#else
    dest_y += dst_half_line_in_src;
#endif

    // DEBUG
#if 0
    static int last_dest_y_field[3] = { -1000, -1000, -1000, };
    int last_dest_y = last_dest_y_field[field];

    if (last_dest_y != dest_y)
    {
        cerr<<"####### Field "<<field<<" #######"<<endl;
        cerr<<"         src_y: "<<src_y<<endl;
        cerr<<"        dest_y: "<<dest_y<<endl;
        cerr<<" xv_src_y_incr: "<<xv_src_y_incr<<endl;
        cerr<<"xv_dest_y_incr: "<<xv_dest_y_incr<<endl;
        cerr<<"      disphoff: "<<disphoff<<endl;
        cerr<<"          imgh: "<<imgh<<endl;
        cerr<<"           mod: "<<mod<<endl;
        cerr<<endl;
    }
    last_dest_y_field[field] = dest_y;
#endif
}

void VideoOutputXv::ShowXvMC(FrameScanType scan)
{
    (void)scan;
#ifdef USING_XVMC
    VideoFrame *frame = NULL;
    bool using_pause_frame = false;

    if (xvmc_tex)
    {
        xvmc_tex->Show();

        // clear any displayed frames not on screen
        CheckFrameStates();
        return;
    }

    vbuffers.begin_lock(kVideoBuffer_pause);
    if (vbuffers.size(kVideoBuffer_pause))
    {
        frame = vbuffers.head(kVideoBuffer_pause);
#ifdef DEBUG_PAUSE
        VERBOSE(VB_PLAYBACK, LOC + QString("use pause frame: %1 ShowXvMC")
                .arg(DebugString(frame)));
#endif // DEBUG_PAUSE
        using_pause_frame = true;
    }
    else if (vbuffers.size(kVideoBuffer_used))
        frame = vbuffers.head(kVideoBuffer_used);
    vbuffers.end_lock();

    if (!frame)
    {
        VERBOSE(VB_PLAYBACK, LOC + "ShowXvMC(): No frame to show");
        return;
    }

    vbuffers.LockFrame(frame, "ShowXvMC");

    // calculate bobbing params
    int field = 3, src_y = video_rect.top(), dest_y = display_video_rect.top();
    int xv_src_y_incr = 0, xv_dest_y_incr = 0;
    if (m_deinterlacing)
    {
        calc_bob(scan,
                 video_rect.height(), display_video_rect.height(),
                 video_rect.top(),    display_video_rect.top(),
                 frame->height, frame->top_field_first,
                 field, src_y, dest_y, xv_src_y_incr, xv_dest_y_incr);
    }
    if (hasVLDAcceleration())
    {   // don't do bob-adjustment for VLD drivers
        src_y  = video_rect.top();
        dest_y = display_video_rect.top();
    }

    // get and try to lock OSD frame, if it exists
    VideoFrame *osdframe = vbuffers.GetOSDFrame(frame);
    if (osdframe && !vbuffers.TryLockFrame(osdframe, "ShowXvMC -- osd"))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "ShowXvMC(): Unable to get OSD lock");
        vbuffers.safeEnqueue(kVideoBuffer_displayed, osdframe);
        osdframe = NULL;
    }

    // set showing surface, depending on existance of osd
    xvmc_render_state_t *showingsurface = (osdframe) ?
        GetRender(osdframe) : GetRender(frame);
    XvMCSurface *surf = showingsurface->p_surface;

    // actually display the frame 
    X11L;
    XvMCPutSurface(XJ_disp, surf, XJ_curwin,
                   video_rect.left(), src_y,
                   video_rect.width(), video_rect.height(),
                   display_video_rect.left(), dest_y,
                   display_video_rect.width(),
                   display_video_rect.height(), field);
    XFlush(XJ_disp); // send XvMCPutSurface call to X11 server
    X11U;

    // if not using_pause_frame, clear old process buffer
    if (!using_pause_frame)
    {
        while (vbuffers.size(kVideoBuffer_pause))
            DiscardFrame(vbuffers.dequeue(kVideoBuffer_pause));
    }
    // clear any displayed frames not on screen
    CheckFrameStates();

    // unlock the frame[s]
    vbuffers.UnlockFrame(osdframe, "ShowXvMC -- OSD");
    vbuffers.UnlockFrame(frame, "ShowXvMC");

    // make sure osdframe is eventually added to available
    vbuffers.safeEnqueue(kVideoBuffer_displayed, osdframe);
#endif // USING_XVMC
}

void VideoOutputXv::ShowXVideo(FrameScanType scan)
{
    VideoFrame *frame = GetLastShownFrame();

    vbuffers.LockFrame(frame, "ShowXVideo");

    XvImage *image = (XvImage*) xv_buffers[frame->buf];
    if (!image)
    {
        vbuffers.UnlockFrame(frame, "ShowXVideo");
        return;
    }

    int field = 3, src_y = video_rect.top(), dest_y = display_video_rect.top(),
        xv_src_y_incr = 0, xv_dest_y_incr = 0;
    if (m_deinterlacing && (m_deintfiltername == "bobdeint"))
    {
        calc_bob(scan,
                 video_rect.height(), display_video_rect.height(),
                 video_rect.top(),    display_video_rect.top(),
                 frame->height, frame->top_field_first,
                 field, src_y, dest_y, xv_src_y_incr, xv_dest_y_incr);
        src_y += xv_src_y_incr;
        dest_y += xv_dest_y_incr;
    }

    vbuffers.UnlockFrame(frame, "ShowXVideo");
    {
        QMutexLocker locker(&global_lock);
        vbuffers.LockFrame(frame, "ShowXVideo");
        int video_height = (3 != field) ?
            (video_rect.height()/2) : video_rect.height();
        X11S(XvShmPutImage(XJ_disp, xv_port, XJ_curwin,
                           XJ_gc, image,
                           video_rect.left(), src_y,
                           video_rect.width(), video_height,
                           display_video_rect.left(), dest_y,
                           display_video_rect.width(),
                           display_video_rect.height(), False));
        vbuffers.UnlockFrame(frame, "ShowXVideo");
    }
}

// this is documented in videooutbase.cpp
void VideoOutputXv::Show(FrameScanType scan)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() is true in Show()");
        return;
    }

    if ((needrepaint || xv_need_bobdeint_repaint) &&
        (VideoOutputSubType() >= XVideo))
    {
        DrawUnusedRects(/* don't do a sync*/false);
    }

    if (VideoOutputSubType() > XVideo)
        ShowXvMC(scan);
    else if (VideoOutputSubType() == XVideo)
        ShowXVideo(scan);

    X11S(XSync(XJ_disp, False));
}

void VideoOutputXv::DrawUnusedRects(bool sync)
{
    // boboff assumes the smallest interlaced resolution is 480 lines - 5%
    bool use_bob   = (m_deinterlacing && m_deintfiltername == "bobdeint");
    int boboff_raw = (int)round(((double)display_video_rect.height()) /
                                456 - 0.00001);
    int boboff     = use_bob ? boboff_raw : 0;

    xv_need_bobdeint_repaint |= needrepaint;

    if (chroma_osd && chroma_osd->GetImage() && xv_need_bobdeint_repaint)
    {
        X11L;
        XShmPutImage(XJ_disp, XJ_curwin, XJ_gc, chroma_osd->GetImage(),
                     0, 0, 0, 0,
                     display_visible_rect.width(),
                     display_visible_rect.height(), False);
        if (sync)
            XSync(XJ_disp, false);
        X11U;

        needrepaint = false;
        xv_need_bobdeint_repaint = false;
        return;
    }

    X11L;

    if (xv_draw_colorkey && needrepaint)
    {
        XSetForeground(XJ_disp, XJ_gc, xv_colorkey);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc,
                       display_visible_rect.left(), 
                       display_visible_rect.top() + boboff,
                       display_visible_rect.width(),
                       display_visible_rect.height() - 2 * boboff);
    }
    else if (xv_draw_colorkey && xv_need_bobdeint_repaint)
    {
        // if this is only for deinterlacing mode switching, draw
        // the border areas, presumably the main image is undamaged.
        XSetForeground(XJ_disp, XJ_gc, xv_colorkey);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc,
                       display_visible_rect.left(), 
                       display_visible_rect.top(),
                       display_visible_rect.width(),
                       boboff_raw);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc,
                       display_visible_rect.left(), 
                       display_visible_rect.height() - 2 * boboff_raw,
                       display_visible_rect.width(),
                       display_visible_rect.height());
    }

    needrepaint = false;
    xv_need_bobdeint_repaint = false;

    // Draw black in masked areas
    XSetForeground(XJ_disp, XJ_gc, XJ_black);

    if (display_video_rect.left() > display_visible_rect.left())
    { // left
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       display_visible_rect.left(),
                       display_visible_rect.top(),
                       display_video_rect.left() - display_visible_rect.left(),
                       display_visible_rect.height());
    }
    if (display_video_rect.left() + display_video_rect.width() <
        display_visible_rect.left() + display_visible_rect.width())
    { // right
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       display_video_rect.left() + display_video_rect.width(),
                       display_visible_rect.top(), 
                       (display_visible_rect.left() +
                        display_visible_rect.width()) -
                       (display_video_rect.left() +
                        display_video_rect.width()),
                       display_visible_rect.height());
    }
    if (display_video_rect.top() + boboff > display_visible_rect.top())
    { // top of screen
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       display_visible_rect.left(),
                       display_visible_rect.top(),
                       display_visible_rect.width(),
                       display_video_rect.top() + boboff -
                       display_visible_rect.top());
    }
    if (display_video_rect.top() + display_video_rect.height() <
        display_visible_rect.top() + display_visible_rect.height())
    { // bottom of screen
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       display_visible_rect.left(),
                       display_video_rect.top() + display_video_rect.height(), 
                       display_visible_rect.width(),
                       (display_visible_rect.top() +
                        display_visible_rect.height()) -
                       (display_video_rect.top() +
                        display_video_rect.height()));
    }

    if (sync)
        XSync(XJ_disp, false);

    X11U;
}

/**
 * \fn VideoOutputXv::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
 *  
 *  
 */
void VideoOutputXv::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)frame;
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    if (VideoOutputSubType() <= XVideo)
        return;

#ifdef USING_XVMC
    xvmc_render_state_t *render = GetRender(frame);
    // disable questionable ffmpeg surface munging
    if (render->p_past_surface == render->p_surface)
        render->p_past_surface = NULL;
    vbuffers.AddInheritence(frame);

    Status status;
    if (hasVLDAcceleration())
    {
        vbuffers.LockFrame(frame, "DrawSlice -- VLD");
        X11S(status = XvMCPutSlice2(XJ_disp, xvmc_ctx, 
                                    (char*)render->slice_data, 
                                    render->slice_datalen, 
                                    render->slice_code));
        if (Success != status)
            VERBOSE(VB_PLAYBACK, LOC_ERR + "XvMCPutSlice: "<<status);

#if 0
        // TODO are these three lines really needed???
        render->start_mv_blocks_num = 0;
        render->filled_mv_blocks_num = 0;
        render->next_free_data_block_num = 0;
#endif

        vbuffers.UnlockFrame(frame, "DrawSlice -- VLD");
    }
    else
    {
        vector<const VideoFrame*> locks;
        locks.push_back(vbuffers.PastFrame(frame));
        locks.push_back(vbuffers.FutureFrame(frame));
        locks.push_back(frame);
        vbuffers.LockFrames(locks, "DrawSlice");

        // Sync past & future I and P frames
        X11S(status =
             XvMCRenderSurface(XJ_disp, xvmc_ctx, 
                               render->picture_structure, 
                               render->p_surface,
                               render->p_past_surface, 
                               render->p_future_surface,
                               render->flags,
                               render->filled_mv_blocks_num,
                               render->start_mv_blocks_num,
                               (XvMCMacroBlockArray *)frame->priv[1], 
                               (XvMCBlockArray *)frame->priv[0]));

        if (Success != status)
            VERBOSE(VB_PLAYBACK, LOC_ERR +
                    QString("XvMCRenderSurface: %1 (%2)")
                    .arg(ErrorStringXvMC(status)).arg(status));
        else
            FlushSurface(frame);

        render->start_mv_blocks_num = 0;
        render->filled_mv_blocks_num = 0;
        render->next_free_data_block_num = 0;
        vbuffers.UnlockFrames(locks, "DrawSlice");
    }
#endif // USING_XVMC
}

// documented in videooutbase.cpp
void VideoOutputXv::VideoAspectRatioChanged(float aspect)
{
    QMutexLocker locker(&global_lock);
    VideoOutput::VideoAspectRatioChanged(aspect);
}

// documented in videooutbase.cpp
void VideoOutputXv::CopyFrame(VideoFrame *to, const VideoFrame *from)
{
    if (VideoOutputSubType() <= XVideo)
        VideoOutput::CopyFrame(to, from);
    else if (xvmc_tex)
    {
        global_lock.lock();
        int tmp = framesPlayed;
        global_lock.unlock();

        PrepareFrameXvMC((VideoFrame*)from, kScan_Interlaced);

        global_lock.lock();
        framesPlayed = tmp;
        global_lock.unlock();
    }
}

void VideoOutputXv::UpdatePauseFrame(void)
{
    QMutexLocker locker(&global_lock);

    if (VideoOutputSubType() <= XVideo || xvmc_tex)
    {
        // Try used frame first, then fall back to scratch frame.
        vbuffers.LockFrame(&av_pause_frame, "UpdatePauseFrame -- pause");

        vbuffers.begin_lock(kVideoBuffer_used);
        VideoFrame *used_frame = NULL;
        if (vbuffers.size(kVideoBuffer_used) > 0)
        {
            used_frame = vbuffers.head(kVideoBuffer_used);
            if (!vbuffers.TryLockFrame(used_frame, "UpdatePauseFrame -- used"))
                used_frame = NULL;
        }
        if (used_frame)
        {
            CopyFrame(&av_pause_frame, used_frame);
            vbuffers.UnlockFrame(used_frame, "UpdatePauseFrame -- used");
        }
        vbuffers.end_lock();

        if (!used_frame && !xvmc_tex &&
            vbuffers.TryLockFrame(vbuffers.GetScratchFrame(),
                                  "UpdatePauseFrame -- scratch"))
        {
            vbuffers.GetScratchFrame()->frameNumber = framesPlayed - 1;
            CopyFrame(&av_pause_frame, vbuffers.GetScratchFrame());
            vbuffers.UnlockFrame(vbuffers.GetScratchFrame(),
                                 "UpdatePauseFrame -- scratch");
        }
        vbuffers.UnlockFrame(&av_pause_frame, "UpdatePauseFrame - used");
    }
#ifdef USING_XVMC
    else
    {
        if (vbuffers.size(kVideoBuffer_pause)>1)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + "UpdatePauseFrame(): "
                    "Pause buffer size>1 check, " + QString("size = %1")
                    .arg(vbuffers.size(kVideoBuffer_pause)));
            while (vbuffers.size(kVideoBuffer_pause))
                DiscardFrame(vbuffers.dequeue(kVideoBuffer_pause));
            CheckFrameStates();
        } else if (1 == vbuffers.size(kVideoBuffer_pause))
        {
            VideoFrame *frame = vbuffers.dequeue(kVideoBuffer_used);
            if (frame)
            {
                while (vbuffers.size(kVideoBuffer_pause))
                    DiscardFrame(vbuffers.dequeue(kVideoBuffer_pause));
                vbuffers.safeEnqueue(kVideoBuffer_pause, frame);
                VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame(): "
                        "XvMC using NEW pause frame");
            }
            else
                VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame(): "
                        "XvMC using OLD pause frame");
            return;
        }

        frame_queue_t::iterator it =
            vbuffers.begin_lock(kVideoBuffer_displayed);

        VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame -- XvMC");
        if (vbuffers.size(kVideoBuffer_displayed))
        {
            VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame -- XvMC: "
                    "\n\t\t\tFound a pause frame in display");

            VideoFrame *frame = vbuffers.tail(kVideoBuffer_displayed);
            if (vbuffers.GetOSDParent(frame))
                frame = vbuffers.GetOSDParent(frame);
            vbuffers.safeEnqueue(kVideoBuffer_pause, frame);
        }
        vbuffers.end_lock();

        if (1 != vbuffers.size(kVideoBuffer_pause))
        {
            VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame -- XvMC: "
                    "\n\t\t\tDid NOT find a pause frame");
        }
    }
#endif
}

void VideoOutputXv::ProcessFrameXvMC(VideoFrame *frame, OSD *osd)
{
    (void)frame;
    (void)osd;
#ifdef USING_XVMC
    if (xvmc_tex)
    {
        xvmc_tex->ProcessOSD(osd);
        return;
    }

    // Handle Pause frame
    if (frame)
    {
        vbuffers.LockFrame(frame, "ProcessFrameXvMC");
        while (vbuffers.size(kVideoBuffer_pause))
            DiscardFrame(vbuffers.dequeue(kVideoBuffer_pause));
    }
    else
    {
        bool success = false;
        
        frame_queue_t::iterator it = vbuffers.begin_lock(kVideoBuffer_pause);
        if (vbuffers.size(kVideoBuffer_pause))
        {
            frame = vbuffers.head(kVideoBuffer_pause);
            success = vbuffers.TryLockFrame(
                frame, "ProcessFrameXvMC -- reuse");
        }
        vbuffers.end_lock();

        if (success)
        {
#ifdef DEBUG_PAUSE
            VERBOSE(VB_PLAYBACK, LOC + "ProcessFrameXvMC: " +
                    QString("Use pause frame: %1").arg(DebugString(frame)));
#endif // DEBUG_PAUSE
            vbuffers.SetOSDFrame(frame, NULL);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + "ProcessFrameXvMC: "
                    "Tried to reuse frame but failed");
            frame = NULL;
        }
    }

    // Handle ChromaKey OSD
    if (chroma_osd)
    {
        vbuffers.UnlockFrame(frame, "ProcessFrameXvMC");
        QMutexLocker locker(&global_lock);
        if (!embedding && osd)
            needrepaint |= chroma_osd->ProcessOSD(osd);
        return;
    }

    ////////////////////////////////////////////////////////////////////
    // Everything below this line is to support XvMC composite surface
    if (!frame)
    {
        VERBOSE(VB_IMPORTANT, LOC + "ProcessFrameXvMC: "
                "Called without frame");
        return;
    }

    if (!xvmc_buf_attr->GetOSDNum())
    {
        vbuffers.UnlockFrame(frame, "ProcessFrameXvMC");
        return;
    }

    VideoFrame * old_osdframe = vbuffers.GetOSDFrame(frame);
    if (old_osdframe)
    {
        VERBOSE(VB_IMPORTANT, LOC + "ProcessFrameXvMC:\n\t\t\t" +
                QString("Warning, %1 is still marked as the OSD frame of %2.")
                .arg(DebugString(old_osdframe, true))
                .arg(DebugString(frame, true)));

        vbuffers.SetOSDFrame(frame, NULL);
    }

    XvMCOSD* xvmc_osd = NULL;
    if (!embedding && osd)
        xvmc_osd = GetAvailableOSD();

    if (xvmc_osd && xvmc_osd->IsValid())
    {
        VideoFrame *osdframe = NULL;
        int ret = DisplayOSD(xvmc_osd->OSDFrame(), osd, -1,
                             xvmc_osd->GetRevision());
        OSDSurface *osdsurf = osd->Display();
        if (osdsurf)
            xvmc_osd->SetRevision(osdsurf->GetRevision());
        if (ret >= 0 && xvmc_osd->NeedFrame())
        {
            // If there are no available buffer, try to toss old
            // displayed frames.
            if (!vbuffers.size(kVideoBuffer_avail))
                CheckFrameStates();

            // If tossing doesn't work try hiding showing frames,
            // then tossing displayed frames.
            if (!vbuffers.size(kVideoBuffer_avail))
            {
                frame_queue_t::iterator it;
                it = vbuffers.begin_lock(kVideoBuffer_displayed);
                for (;it != vbuffers.end(kVideoBuffer_displayed); ++it)
                    if (*it != frame)
                        X11S(XvMCHideSurface(XJ_disp,
                                             GetRender(*it)->p_surface));
                vbuffers.end_lock();

                CheckFrameStates();
            }

            // If there is an available buffer grab it.
            if (vbuffers.size(kVideoBuffer_avail))
            {
                osdframe = vbuffers.GetNextFreeFrame(false, false);
                // Check for error condition..
                if (frame == osdframe)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            QString("ProcessFrameXvMC: %1 %2")
                            .arg(DebugString(frame, true))
                            .arg(vbuffers.GetStatus()));
                    osdframe = NULL;
                }
            }

            if (osdframe && vbuffers.TryLockFrame(
                    osdframe, "ProcessFrameXvMC -- OSD"))
            {
                vbuffers.SetOSDFrame(osdframe, NULL);
                xvmc_osd->CompositeOSD(frame, osdframe);
                vbuffers.UnlockFrame(osdframe, "ProcessFrameXvMC -- OSD");
                vbuffers.SetOSDFrame(frame, osdframe);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "ProcessFrameXvMC: "
                        "Failed to get OSD lock");
                DiscardFrame(osdframe);
            }
        }
        if (ret >= 0 && !xvmc_osd->NeedFrame())
        {
            xvmc_osd->CompositeOSD(frame);
        }
    }
    if (xvmc_osd)
        ReturnAvailableOSD(xvmc_osd);
    vbuffers.UnlockFrame(frame, "ProcessFrameXvMC");            
#endif // USING_XVMC
}

#ifdef USING_XVMC
XvMCOSD* VideoOutputXv::GetAvailableOSD()
{
    if (xvmc_buf_attr->GetOSDNum() > 1)
    {
        XvMCOSD *val = NULL;
        xvmc_osd_lock.lock();
        while (!xvmc_osd_available.size())
        {
            xvmc_osd_lock.unlock();
            usleep(50);
            xvmc_osd_lock.lock();
        }
        val = xvmc_osd_available.dequeue();
        xvmc_osd_lock.unlock();
        return val;
    }
    else if (xvmc_buf_attr->GetOSDNum() > 0)
    {
        xvmc_osd_lock.lock();
        return xvmc_osd_available.head();
    }
    return NULL;
}
#endif // USING_XVMC

#ifdef USING_XVMC
void VideoOutputXv::ReturnAvailableOSD(XvMCOSD *avail)
{
    if (xvmc_buf_attr->GetOSDNum() > 1)
    {
        xvmc_osd_lock.lock();
        xvmc_osd_available.push_front(avail);
        xvmc_osd_lock.unlock();
    }
    else if (xvmc_buf_attr->GetOSDNum() > 0)
    {
        xvmc_osd_lock.unlock();
    }
}
#endif // USING_XVMC

void VideoOutputXv::ProcessFrameMem(VideoFrame *frame, OSD *osd,
                                    FilterChain *filterList,
                                    NuppelVideoPlayer *pipPlayer)
{
    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL);
    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        vector<const VideoFrame*> locks;
        locks.push_back(frame);
        locks.push_back(&av_pause_frame);
        vbuffers.LockFrames(locks, "ProcessFrameMem -- pause");
        CopyFrame(frame, &av_pause_frame);
        vbuffers.UnlockFrames(locks, "ProcessFrameMem -- pause");
        pauseframe = true;
    }

    vbuffers.LockFrame(frame, "ProcessFrameMem");

    if (!pauseframe)
    {
        if (filterList)
            filterList->ProcessFrame(frame);
        
        if (deint_proc && m_deinterlaceBeforeOSD)
            m_deintFilter->ProcessFrame(frame);
    }

    ShowPip(frame, pipPlayer);

    if (osd && !embedding)
    {
        if (!chroma_osd)
            DisplayOSD(frame, osd);
        else
        {
            QMutexLocker locker(&global_lock);
            needrepaint |= chroma_osd->ProcessOSD(osd);
        }
    }

    if (!pauseframe && deint_proc && !m_deinterlaceBeforeOSD)
        m_deintFilter->ProcessFrame(frame);

    vbuffers.UnlockFrame(frame, "ProcessFrameMem");
}

// this is documented in videooutbase.cpp
void VideoOutputXv::ProcessFrame(VideoFrame *frame, OSD *osd,
                                 FilterChain *filterList,
                                 NuppelVideoPlayer *pipPlayer)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() in ProcessFrame()");
        return;
    }

    if (VideoOutputSubType() <= XVideo)
        ProcessFrameMem(frame, osd, filterList, pipPlayer);
    else
        ProcessFrameXvMC(frame, osd);
}

// this is documented in videooutbase.cpp
int VideoOutputXv::SetPictureAttribute(int attribute, int newValue)
{
    QString  attrName = QString::null;
    int      valAdj   = 0;

    if (kPictureAttribute_Brightness == attribute)
        attrName = "XV_BRIGHTNESS";
    else if (kPictureAttribute_Contrast == attribute)
        attrName = "XV_CONTRAST";
    else if (kPictureAttribute_Colour == attribute)
        attrName = "XV_SATURATION";
    else if (kPictureAttribute_Hue == attribute)
    {
        attrName = "XV_HUE";
        valAdj   = xv_hue_base;
    }

    if (attrName.isEmpty())
        return -1;

    newValue = min(max(newValue, 0), 100);

    Atom attributeAtom = None;
    X11S(attributeAtom = XInternAtom(XJ_disp, attrName.ascii(), False));
    if (attributeAtom == None)
        return -1;

    XvAttribute *attributes = NULL;
    int howmany;
    X11S(attributes = XvQueryPortAttributes(XJ_disp, xv_port, &howmany));
    if (!attributes)
        return -1;

    bool value_set = false;
    for (int i = 0; i < howmany; i++)
    {
        if (attrName != attributes[i].name)
            continue;

        int port_min = attributes[i].min_value;
        int port_max = attributes[i].max_value;
        int range    = port_max - port_min;

        int tmpval2 = (newValue + valAdj) % 100;
        int tmpval3 = (int) roundf(range * 0.01f * tmpval2);
        int value   = min(tmpval3 + port_min, port_max);
        value_set = true;

        X11L;
        XvSetPortAttribute(XJ_disp, xv_port, attributeAtom, value);
#ifdef USING_XVMC
        // Needed for VIA XvMC to commit change immediately.
        if (video_output_subtype > XVideo)
            XvMCSetAttribute(XJ_disp, xvmc_ctx, attributeAtom, value);
#endif
        X11U;
        break;
    }

    X11S(XFree(attributes));

    if (!value_set)
        return -1;

    SetPictureAttributeDBValue(attribute, newValue);
    return newValue;
}

void VideoOutputXv::CheckFrameStates(void)
{
#ifdef USING_XVMC
    frame_queue_t::iterator it;

    if (xvmc_buf_attr->IsAggressive())
    {
        it = vbuffers.begin_lock(kVideoBuffer_displayed);
        for (;it != vbuffers.end(kVideoBuffer_displayed); ++it)
        {
            VideoFrame* frame = *it;
            frame_queue_t c = vbuffers.Children(frame);
            frame_queue_t::iterator cit = c.begin();
            for (; cit != c.end(); ++cit)
            {
                VideoFrame *cframe = *cit;
                vbuffers.LockFrame(cframe, "CDFForAvailability 1");
                if (!IsRendering(cframe))
                {
                    GetRender(cframe)->p_past_surface   = NULL;
                    GetRender(cframe)->p_future_surface = NULL;
                    vbuffers.RemoveInheritence(cframe);
                    vbuffers.UnlockFrame(cframe, "CDFForAvailability 2");
                    if (!vbuffers.HasChildren(frame))
                        break;
                    else
                    {
                        c = vbuffers.Children(frame);
                        cit = c.begin();
                    }
                }
                else
                    vbuffers.UnlockFrame(cframe, "CDFForAvailability 3");
            }
        }
        vbuffers.end_lock();
    }

    it = vbuffers.begin_lock(kVideoBuffer_displayed);
    for (;it != vbuffers.end(kVideoBuffer_displayed); ++it)
        vbuffers.RemoveInheritence(*it);
    vbuffers.end_lock();

    it = vbuffers.begin_lock(kVideoBuffer_displayed);
    while (it != vbuffers.end(kVideoBuffer_displayed))
    {
        VideoFrame* pframe = *it;
        SyncSurface(pframe);
        if (!IsDisplaying(pframe))
        {
            frame_queue_t children = vbuffers.Children(pframe);
            if (!children.empty())
            {
#if 0
                VERBOSE(VB_PLAYBACK, LOC + QString(
                            "Frame %1 w/children: %2 is being held for later "
                            "discarding.")
                        .arg(DebugString(pframe, true))
                        .arg(DebugString(children)));
#endif
                frame_queue_t::iterator cit;
                for (cit = children.begin(); cit != children.end(); ++cit)
                {
                    if (vbuffers.contains(kVideoBuffer_avail, *cit))
                    {
                        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                                    "Child     %1 was already marked "
                                    "as available.").arg(DebugString(*cit)));
                    }
                }
            }
            else if (vbuffers.contains(kVideoBuffer_decode, pframe))
            {
                VERBOSE(VB_PLAYBACK, LOC + QString(
                            "Frame %1 is in use by avlib and so is "
                            "being held for later discarding.")
                        .arg(DebugString(pframe, true)));
            }
            else
            {
                vbuffers.RemoveInheritence(pframe);
                vbuffers.safeEnqueue(kVideoBuffer_avail, pframe);
                vbuffers.end_lock();
                it = vbuffers.begin_lock(kVideoBuffer_displayed);
                continue;
            }
        }
        ++it;
    }
    vbuffers.end_lock();

#endif // USING_XVMC
}

bool VideoOutputXv::IsDisplaying(VideoFrame* frame)
{
    (void)frame;
#ifdef USING_XVMC
    xvmc_render_state_t *render = GetRender(frame);
    if (render)
    {
        Display *disp     = render->disp;
        XvMCSurface *surf = render->p_surface;
        int res = 0, status = 0;
        if (disp && surf)
            X11S(res = XvMCGetSurfaceStatus(disp, surf, &status));
        if (Success == res)
            return (status & XVMC_DISPLAYING);
        else
            VERBOSE(VB_PLAYBACK, LOC_ERR + "IsDisplaying(): " +
                    QString("XvMCGetSurfaceStatus %1").arg(res));
    }
#endif // USING_XVMC
    return false;
}

bool VideoOutputXv::IsRendering(VideoFrame* frame)
{
    (void)frame;
#ifdef USING_XVMC
    xvmc_render_state_t *render = GetRender(frame);
    if (render)
    {
        Display *disp     = render->disp;
        XvMCSurface *surf = render->p_surface;
        int res = 0, status = 0;
        if (disp && surf)
            X11S(res = XvMCGetSurfaceStatus(disp, surf, &status));
        if (Success == res)
            return (status & XVMC_RENDERING);
        else
            VERBOSE(VB_PLAYBACK, LOC_ERR + "IsRendering(): " +
                    QString("XvMCGetSurfaceStatus %1").arg(res));
    }
#endif // USING_XVMC
    return false;
}

void VideoOutputXv::SyncSurface(VideoFrame* frame, int past_future)
{
    (void)frame;
    (void)past_future;
#ifdef USING_XVMC
    xvmc_render_state_t *render = GetRender(frame);
    if (render)
    {
        Display *disp     = render->disp;
        XvMCSurface *surf = render->p_surface;
        if (past_future == -1)
            surf = render->p_past_surface;
        else if (past_future == +1)
            surf = render->p_future_surface;

        if (disp && surf)
        {
            int status = 0, res = Success;

            X11S(res = XvMCGetSurfaceStatus(disp, surf, &status));

            if (res != Success)
                VERBOSE(VB_PLAYBACK, LOC_ERR + "SyncSurface(): " +
                        QString("XvMCGetSurfaceStatus %1").arg(res));
            if (status & XVMC_RENDERING)
            {
                X11S(XvMCFlushSurface(disp, surf));
                while (IsRendering(frame))
                    usleep(50);
            }
        }
    }
#endif // USING_XVMC
}

void VideoOutputXv::FlushSurface(VideoFrame* frame)
{ 
    (void)frame;
#ifdef USING_XVMC
    xvmc_render_state_t *render = GetRender(frame);
    if (render)
    {
        Display *disp     = render->disp;
        XvMCSurface *surf = render->p_surface;
        if (disp && IsRendering(frame))
            X11S(XvMCFlushSurface(disp, surf));
    }
#endif // USING_XVMC
}

static void SetFromEnv(bool &useXvVLD, bool &useXvIDCT, bool &useXvMC,
                       bool &useXVideo, bool &useShm)
{
    // can be used to force non-Xv mode as well as non-Xv/non-Shm mode
    if (getenv("NO_XVMC_VLD"))
        useXvVLD = false;
    if (getenv("NO_XVMC_IDCT"))
        useXvIDCT = false;
    if (getenv("NO_XVMC"))
        useXvVLD = useXvIDCT = useXvMC = false;
    if (getenv("NO_XV"))
        useXvVLD = useXvIDCT = useXvMC = useXVideo = false;
    if (getenv("NO_SHM"))
        useXVideo = useShm = false;
}

static void SetFromHW(Display *d, bool &useXvMC, bool &useXVideo, bool &useShm)
{
    // find out about XvMC support
    if (useXvMC)
    {
#ifdef USING_XVMC
        int mc_event, mc_err, ret;
        X11S(ret = XvMCQueryExtension(d, &mc_event, &mc_err));
        if (True != ret)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "XvMC output requested, "
                    "but is not supported by display.");
            useXvMC = false;
        }

        int mc_ver, mc_rel;
        X11S(ret = XvMCQueryVersion(d, &mc_ver, &mc_rel));
        if (Success == ret)
            VERBOSE(VB_PLAYBACK, LOC + "XvMC version: "<<mc_ver<<"."<<mc_rel);
#else // !USING_XVMC
        VERBOSE(VB_IMPORTANT, LOC_ERR + "XvMC output requested, "
                "but is not compiled into MythTV.");
        useXvMC = false;
#endif // USING_XVMC
    }

    // find out about XVideo support
    if (useXVideo)
    {
        uint p_ver, p_rel, p_req, p_event, p_err, ret;
        X11S(ret = XvQueryExtension(d, &p_ver, &p_rel,
                                    &p_req, &p_event, &p_err));
        if (Success != ret)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "XVideo output requested, "
                    "but is not supported by display.");
            useXVideo = false;
            useXvMC = false;
        }
    }

    if (useShm)
    {
        const char *dispname = DisplayString(d);
        if ((dispname) && (*dispname == ':'))
            X11S(useShm = (bool) XShmQueryExtension(d));
    }
}

CodecID myth2av_codecid(MythCodecID codec_id,
                        bool& vld, bool& idct, bool& mc)
{
    vld = idct = mc = false;
    CodecID ret = CODEC_ID_NONE;
    switch (codec_id)
    {
        case kCodec_NONE:
            ret = CODEC_ID_NONE;
            break;

        case kCodec_MPEG1:
            ret = CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2:
            ret = CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263:
            ret = CODEC_ID_H263;
            break;
        case kCodec_MPEG4:
            ret = CODEC_ID_MPEG4;
            break;
        case kCodec_H264:
            ret = CODEC_ID_H264;
            break;

        case kCodec_MPEG1_XVMC:
            mc = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC;
            break;
        case kCodec_MPEG2_XVMC:
            mc = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC;
            break;
        case kCodec_H263_XVMC:
            VERBOSE(VB_IMPORTANT, "Error: XvMC H263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_XVMC:
            VERBOSE(VB_IMPORTANT, "Error: XvMC MPEG4 not supported by ffmpeg");
            break;
        case kCodec_H264_XVMC:
            VERBOSE(VB_IMPORTANT, "Error: XvMC H264 not supported by ffmpeg");
            break;

        case kCodec_MPEG1_IDCT:
            idct = mc = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC;
            break;
        case kCodec_MPEG2_IDCT:
            idct = mc = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC;
            break;
        case kCodec_H263_IDCT:
            VERBOSE(VB_IMPORTANT, "Error: XvMC-IDCT H263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_IDCT:
            VERBOSE(VB_IMPORTANT, "Error: XvMC-IDCT MPEG4 not supported by ffmpeg");
            break;
        case kCodec_H264_IDCT:
            VERBOSE(VB_IMPORTANT, "Error: XvMC-IDCT H264 not supported by ffmpeg");
            break;

        case kCodec_MPEG1_VLD:
            vld = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC_VLD;
            break;
        case kCodec_MPEG2_VLD:
            vld = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC_VLD;
            break;
        case kCodec_H263_VLD:
            VERBOSE(VB_IMPORTANT, "Error: XvMC-VLD H263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_VLD:
            VERBOSE(VB_IMPORTANT, "Error: XvMC-VLD MPEG4 not supported by ffmpeg");
            break;
        case kCodec_H264_VLD:
            VERBOSE(VB_IMPORTANT, "Error: XvMC-VLD H264 not supported by ffmpeg");
            break;
        default:
            VERBOSE(VB_IMPORTANT, QString("Error: MythCodecID %1 has not been "
                                          "added to myth2av_codecid")
                    .arg(codec_id));
            break;
    } // switch(codec_id)
    return ret;
}
