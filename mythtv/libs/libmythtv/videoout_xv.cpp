/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/keysym.h>

#include <iostream>

#include "yuv2rgb.h"
#include "osd.h"
#include "osdsurface.h"
#include "osdxvmc.h"
#include "mythcontext.h"
#include "filtermanager.h"
#include "videoout_xv.h"
#include "XvMCSurfaceTypes.h"
#include "util-x11.h"

extern "C" {
#include "../libavcodec/avcodec.h"

#define XMD_H 1
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/Xinerama.h>
    extern int      XShmQueryExtension(Display*);
    extern int      XShmGetEventBase(Display*);
    extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*,
                                      int, int, XShmSegmentInfo*);
}

static QString xvflags2str(int flags);

#ifdef USING_XVMC
#   define AGGRESSIVE_BUFFER_MANAGEMENT
#   define XVMC_OSD_NUM       2
#   define XVMC_OSD_RES_NUM   2
#   define XVMC_MIN_SURF_NUM  8
#   define XVMC_MAX_SURF_NUM 16
    static inline xvmc_render_state_t *GetRender(VideoFrame *frame);

#   if defined(USING_XVMCW)
        extern "C" Status XvMCPutSlice2(Display*,XvMCContext*,char*,int,int);
#   elif !defined(USING_XVMC_VLD) && !defined(USING_XVMCW)
        Status XvMCPutSlice2(Display*, XvMCContext*, char*, int, int)
            { return XvMCBadSurface; }
#   endif
    static inline QString ErrorStringXvMC(int);
#endif // USING_XVMC

#define GUID_I420_PLANAR 0x30323449
#define GUID_YV12_PLANAR 0x32315659

static void SetFromEnv(bool &useXvVLD, bool &useXvIDCT, bool &useXvMC,
                       bool &useXV, bool &useShm);
static void SetFromHW(Display *d, bool &useXvMC, bool &useXV, bool& useShm);

/** \class  VideoOutputXv
 * Supports common video output methods used with X11 Servers.
 *
 * This class suppurts XVideo with VLD acceleration (XvMC-VLD), XVideo with
 * inverse discrete cosine transform (XvMC-IDCT) acceleration, XVideo with 
 * motion vector (XvMC) acceleration, and normal XVideo with color transform
 * and scaling acceleration only. When none of these will work, we also try 
 * to use X Shared memory, and if that fails standard Xlib output.
 *
 * \see VideoOutput, VideoBuffers
 *
 */
VideoOutputXv::VideoOutputXv(MythCodecID codec_id)
    : VideoOutput(),
      XJ_root(0),  XJ_win(0), XJ_curwin(0), XJ_gc(0), XJ_screen(NULL),
      XJ_disp(NULL), XJ_screen_num(0), XJ_white(0), XJ_black(0), XJ_depth(0),
      XJ_screenx(0), XJ_screeny(0), XJ_screenwidth(0), XJ_screenheight(0),
      XJ_started(false),

      XJ_non_xv_image(0), non_xv_frames_shown(0), non_xv_show_frame(1),
      non_xv_fps(0), non_xv_av_format(PIX_FMT_NB), non_xv_stop_time(0),

      xv_port(-1), xv_colorkey(0), xv_draw_colorkey(false), xv_chroma(0),
      xv_color_conv_buf(NULL),
#ifdef USING_XVMC
      xvmc_surf_type(0), xvmc_chroma(XVMC_CHROMA_FORMAT_420),
      xvmc_osd_lock(false),
#endif
      video_output_subtype(XVUnknown), display_res(NULL),
      display_aspect(1.0), global_lock(true),
      myth_codec_id(codec_id)
{
    VERBOSE(VB_PLAYBACK, "VideoOutputXv()");
    bzero(&av_pause_frame, sizeof(av_pause_frame));

    // If using custom display resolutions, display_res will point
    // to a singleton instance of the DisplayRes class
    if (gContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes();
}

VideoOutputXv::~VideoOutputXv()
{
    VERBOSE(VB_PLAYBACK, "~VideoOutputXv()");
    if (XJ_started) 
    {
        X11L;
        XSetForeground(XJ_disp, XJ_gc, XJ_black);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc,
                       dispx, dispy, dispw, disph);
        X11U;

        m_deinterlacing = false;
    }

    DeleteBuffers(VideoOutputSubType(), true);

    // ungrab port...
    if (xv_port >= 0)
    {
        X11S(XvUngrabPort(XJ_disp, xv_port, CurrentTime));
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
}

// this is documented in videooutbase.cpp
void VideoOutputXv::Zoom(int direction)
{
    global_lock.lock();

    VideoOutput::Zoom(direction);
    MoveResize();

    global_lock.unlock();
}

//#define KILLER_DEBUG
void VideoOutputXv::InputChanged(int width, int height, float aspect)
{
    VERBOSE(VB_PLAYBACK, "InputChanged()");
    global_lock.lock();

    VideoOutput::InputChanged(width, height, aspect);
    DeleteBuffers(VideoOutputSubType(), false);
    ResizeForVideo((uint) width, (uint) height);
    bool ok = CreateBuffers(VideoOutputSubType());
    MoveResize();

#ifdef KILLER_DEBUG
    // to make sure NVP reports error
    static int kill_buf_dbg = 0;
    kill_buf_dbg++;
    if (3 == kill_buf_dbg)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Killed Buffers -- you should see an error dialog")
                .arg(kill_buf_dbg));
        ok = false;
        DeleteBuffers(VideoOutputSubType(), false);
    }
    else
        VERBOSE(VB_IMPORTANT, QString("Kill Buffers dbg %1 != 3")
                .arg(kill_buf_dbg));
#endif

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::InputChanged(): "
                "Failed to recreate buffers");
        errored = true;
    }

    global_lock.unlock();
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
    X11S(ret = XF86VidModeGetModeLine(XJ_disp, XJ_screen_num, &dot_clock, &mode_line));
    if (!ret)
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::GetRefreshRate() -- "
                "failed to query mode line");
        return -1;
    }

    double rate = (double)((double)(dot_clock * 1000.0) /
                           (double)(mode_line.htotal * mode_line.vtotal));

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
    if (display_res && display_res->SwitchToVideo(width, height))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        w_mm = display_res->GetPhysicalWidth();
        h_mm = display_res->GetPhysicalHeight();
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
            dispx = dispy = 0;
            dispw = display_res->GetWidth();
            disph = display_res->GetHeight();
            // Resize X window to fill new resolution
            X11S(XMoveResizeWindow(XJ_disp, XJ_win,
                                   dispx, dispy, dispw, disph));
        }
    }
}

/** 
 * \fn VideoOutputXv::InitDisplayMeasurements(uint width, uint height)
 * Initialized Display Measurements based on database settings and
 * actual screen parameters.
 *
 * \todo display measurements are broken for Xinerama screens, and 
 * non-fullscreen display.
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
        w_mm = (myth_dsw != 0) ?
            myth_dsw : DisplayWidthMM(XJ_disp, XJ_screen_num);

        h_mm = (myth_dsh != 0) ?
            myth_dsh : DisplayHeightMM(XJ_disp, XJ_screen_num);

        // Get default (possibly user selected) screen resolution from context
        float wmult, hmult;
        gContext->GetScreenSettings(XJ_screenx, XJ_screenwidth, wmult,
                                    XJ_screeny, XJ_screenheight, hmult);
    }

    // TODO these calculations are not quite right
    int event_base, error_base;
    bool usingXinerama = false;
    X11S(usingXinerama = 
         (XineramaQueryExtension(XJ_disp, &event_base, &error_base) &&
          XineramaIsActive(XJ_disp)));
    if (w_mm == 0 || h_mm == 0 || usingXinerama)
    {
        w_mm = (int)(300 * XJ_aspect);
        h_mm = 300;
        display_aspect = XJ_aspect;
    }
    else if (gContext->GetNumSetting("GuiSizeForTV", 0))
    {
        int w = DisplayWidth(XJ_disp, XJ_screen_num);
        int h = DisplayHeight(XJ_disp, XJ_screen_num);
        int gui_w = w, gui_h = h;
        gContext->GetResolutionSetting("Gui", gui_w,  gui_h);
        if (gui_w)
            w_mm = w_mm * gui_w / w;
        if (gui_h)
            h_mm = h_mm * gui_h / h;

        display_aspect = (float)w_mm/h_mm;
    }
    else
        display_aspect = (float)w_mm/h_mm;

    if (display_res)
        display_aspect = display_res->GetAspectRatio();
}

/**
 * \fn VideoOutputXv::GrabSuitableXvPort(VOSType)
 * Internal function used to grab a XVideo port with the desired properties.
 *
 * \return port number if it succeeds, else -1.
 */
int VideoOutputXv::GrabSuitableXvPort(VOSType type)
{
    uint neededFlags[] = { XvInputMask,
                           XvInputMask,
                           XvInputMask,
                           XvInputMask | XvImageMask };
    bool useXVMC[] = { true,  true,  true,  false };
    bool useVLD[]  = { true,  false, false, false };
    bool useIDCT[] = { false, true,  false, false };

    (void)useVLD[0]; (void)useIDCT[0]; // avoid compiler warning

    QString msg[] =
    {
        "XvMC surface found with VLD support on port %1",
        "XvMC surface found with IDCT support on port %1",
        "XvMC surface found with MC support on port %1",
        "XVideo surface found on port %1"
    };

    // get the list of Xv ports
    XvAdaptorInfo *ai = NULL;
    uint p_num_adaptors = 0;
    int ret = Success;
    X11S(ret = XvQueryAdaptors(XJ_disp, XJ_root, &p_num_adaptors, &ai));
    if (Success != ret) 
    {
        VERBOSE(VB_IMPORTANT, "XVideo supported, but no free Xv ports "
                "found. You may need to reload video driver.");
        return -1;
    }

    // find an Xv port
    int port = -1;
    uint begin = 0, end = 4;
    switch (type)
    {
        case XVideo:
            begin = 3; end = 4;
            break;
        case XVideoMC:
            begin = 2; end = 3;
            break;
        case XVideoIDCT:
            begin = 1; end = 2;
            break;
        case XVideoVLD:
            begin = 0; end = 1;
            break;
        default:
            break;
    }
    VERBOSE(VB_IMPORTANT, "begin("<<begin<<") end("<<end<<")");

    for (uint j = begin; j < end; ++j)
    {
        VERBOSE(VB_PLAYBACK, QString("@ j=%1 Looking for flag[s]: %2")
                .arg(j).arg(xvflags2str(neededFlags[j])));
        for (uint i = 0; i < p_num_adaptors && (port == -1); ++i) 
        {
            bool hasNF = (ai[i].type & neededFlags[j]) == neededFlags[j];
            VERBOSE(VB_PLAYBACK, QString("Adaptor: %1 has flag[s]: %2")
                    .arg(i).arg(xvflags2str(ai[i].type)));
            if (!hasNF)
                continue;
            
            const XvPortID firstPort = ai[i].base_id;
            const XvPortID lastPort = ai[i].base_id + ai[i].num_ports - 1;
            XvPortID p = 0;
            if (useXVMC[j])
            {
#ifdef USING_XVMC
                int surfNum;
                XvMCSurfaceTypes::find(XJ_width, min(XJ_height,1), xvmc_chroma,
                                       useVLD[j], useIDCT[j], 2, 0, 0,
                                       XJ_disp, firstPort, lastPort,
                                       p, surfNum);
                if (surfNum<0)
                    continue;
                
                XvMCSurfaceTypes surf(XJ_disp, p);
                
                if (!surf.size())
                    continue;

                X11S(ret = XvGrabPort(XJ_disp, p, CurrentTime));
                if (Success != ret)
                    continue;
                
                surf.set(surfNum, &xvmc_surf_info);
                xvmc_surf_type = surf.surfaceTypeID(surfNum);
                port = p;
#endif // USING_XVMC
            }
            else
            {
                for (p = firstPort; (p <= lastPort) && (port == -1); ++p)
                {
                    X11S(ret = XvGrabPort(XJ_disp, p, CurrentTime));
                    if (Success == ret)
                        port = p;
                }
            }
        }
        if (port != -1)
        {
            VERBOSE(VB_PLAYBACK, msg[j].arg(port));
            break;
        }
    }
    if (port == -1)
        VERBOSE(VB_PLAYBACK, "No suitible XVideo port found");

    // free list of Xv ports
    if (ai)
        X11S(XvFreeAdaptorInfo(ai));

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
 * \fn VideoOutputXv::InitVideoBuffers(bool,bool,bool,bool,bool)
 * Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitVideoBuffers(VOSType xvmc_type,
                                     bool use_xv, bool use_shm)
{
    (void)xvmc_type;

    bool done = false;
    // If use_xvmc try to create XvMC buffers
#ifdef USING_XVMC
    if (xvmc_type > XVideo)
    {
        // Create ffmpeg VideoFrames    
        vbuffers.Init(8 /*numdecode*/, false /*extra_for_pause*/,
                      1+XVMC_OSD_RES_NUM /*need_free*/,
                      5-XVMC_OSD_RES_NUM /*needprebuffer_normal*/,
                      5-XVMC_OSD_RES_NUM /*needprebuffer_small*/,
                      1 /*keepprebuffer*/, true /*use_frame_locking*/);
        done = InitXvMC(xvmc_type);

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

    // XVideo & XvMC output methods allow the picture to be adjusted
    if (done && VideoOutputSubType() >= XVideo &&
        gContext->GetNumSetting("UseOutputPictureControls", 0))
    {
        ChangePictureAttribute(kPictureAttribute_Brightness, brightness);
        ChangePictureAttribute(kPictureAttribute_Contrast, contrast);
        ChangePictureAttribute(kPictureAttribute_Colour, colour);
        ChangePictureAttribute(kPictureAttribute_Hue, hue);
    }

    return done;
}

/**
 * \fn VideoOutputXv::InitXvMC(VOSType)
 *  Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitXvMC(VOSType type)
{
#ifdef USING_XVMC
    xv_port = GrabSuitableXvPort(type);
    if (xv_port == -1)
    {
        VERBOSE(VB_IMPORTANT, "Could not find suitable XvMC surface.");
        return false;
    }

    InstallXErrorHandler(XJ_disp);

    for (uint i=0; i<XVMC_OSD_NUM; i++)
    {
        XvMCOSD *xvmc_osd =
            new XvMCOSD(XJ_disp, xv_port, xvmc_surf_info.surface_type_id,
                        xvmc_surf_info.flags);
        xvmc_osd_available.push_back(xvmc_osd);
    }

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
        VERBOSE(VB_IMPORTANT, "Failed to create XvMC Buffers.");
        for (uint i=0; i<xvmc_osd_available.size(); i++)
            delete xvmc_osd_available[i];
        xvmc_osd_available.clear();
        X11S(XvUngrabPort(XJ_disp, xv_port, CurrentTime));
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
    xv_port = GrabSuitableXvPort(XVideo);
    if (xv_port == -1)
    {
        VERBOSE(VB_IMPORTANT, "Could not find suitable XVideo surface.");
        return false;
    }

    InstallXErrorHandler(XJ_disp);

    bool foundimageformat = false;
    int formats = 0;
    XvImageFormatValues *fo;
    X11S(fo = XvListImageFormats(XJ_disp, xv_port, &formats));
    for (int i = 0; i < formats; i++)
    {
        if (fo[i].id == GUID_I420_PLANAR)
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

    if (fo)
        X11S(XFree(fo));

    if (!foundimageformat)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't find the proper XVideo image format.");
        X11S(XvUngrabPort(XJ_disp, xv_port, CurrentTime));
        xv_port = -1;
    }

    bool ok = xv_port >= 0;
    if (ok)
        ok = CreateBuffers(XVideo);

    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    if (!ok || errs.size())
    {
        VERBOSE(VB_IMPORTANT, "Failed to create XVideo Buffers.");
        DeleteBuffers(XVideo, false);
        X11S(XvUngrabPort(XJ_disp, xv_port, CurrentTime));
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

    VERBOSE(VB_IMPORTANT, "Falling back to X shared memory.");
    VERBOSE(VB_IMPORTANT, "      *** May be slow ***");
    bool ok = CreateBuffers(XShm);

    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    if (!ok || errs.size())
    {
        VERBOSE(VB_IMPORTANT, "Failed to allocate X shared memory.");
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

    VERBOSE(VB_IMPORTANT, "Falling back to X11 network display.");
    VERBOSE(VB_IMPORTANT, "    *** May be very slow ***");
    bool ok = CreateBuffers(Xlib);

    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    if (!ok || errs.size())
    {
        VERBOSE(VB_IMPORTANT, "Failed to create X buffers.");
        PrintXErrors(XJ_disp, errs);
        DeleteBuffers(Xlib, false);
        ok = false;
    }
    else
        video_output_subtype = Xlib;

    return ok;
}

/** \fn VideoOutputXv::GetBestSupportedCodec(uint,uint,uint,uint,uint)
 *
 * \return MythCodecID for the best supported codec on the main display.
 */
MythCodecID VideoOutputXv::GetBestSupportedCodec(uint width, uint height,
                                                 uint osd_width, uint osd_height,
                                                 uint stream_type, int xvmc_chroma)
{
#ifdef USING_XVMC
    Display *disp;
    X11S(disp = XOpenDisplay(NULL));

    // Disable features based on environment and DB values.
    bool use_xvmc_vld = false, use_xvmc_idct = false, use_xvmc = false;
    bool use_xv = true, use_shm = true;
#ifdef USING_XVMC_VLD
    use_xvmc_vld = use_xvmc = gContext->GetNumSetting("UseXvMcVld", 1);
#endif
    if (!use_xvmc)
        use_xvmc_idct = use_xvmc = gContext->GetNumSetting("UseXVMC", 1);
    else if (!use_xvmc_idct)
        use_xvmc_idct = gContext->GetNumSetting("UseXVMC", 1);

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

    X11L;
    XCloseDisplay(disp);
    X11U;
    return ret;
#else
    return kCodec_MPEG1 + (stream_type-1);
#endif
}

#define XV_INIT_FATAL_ERROR_TEST(test,msg) \
do { \
    if (test) \
    { \
        VERBOSE(VB_IMPORTANT, msg << " Exiting playback."); \
        errored = true; \
        return false; \
    } \
} while (false)

/**
 * \fn VideoOutputXv::Init()
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

    // Set use variables...
    bool vld, idct, mc;
    myth2av_codecid(myth_codec_id, vld, idct, mc);
    bool xv = !vld && !idct, shm = xv;
    SetFromEnv(vld, idct, mc, xv, shm);
    SetFromHW(XJ_disp, mc, xv, shm);
    VOSType xvmc_type = vld ? XVideoVLD : 
        (idct ? XVideoIDCT : (mc ? XVideoMC : XVideo));

    // Set embedding window id
    if (embedid > 0)
        XJ_curwin = XJ_win = embedid;

    // Create video buffers
    bool ok = InitVideoBuffers(xvmc_type, xv, shm);
    XV_INIT_FATAL_ERROR_TEST(!ok, "Failed to get any video output");

    if (video_output_subtype >= XVideo)
        InitColorKey(true);

    MoveResize(); 

    XJ_started = true;

    return true;
}
#undef XV_INIT_FATAL_ERROR_TEST

/**
 * \fn VideoOutputXv::InitColorKey()
 * Initializes color keying support used by XVideo output methods.
 *
 * \param turnoffautopaint turn off or on XV_AUTOPAINT_COLORKEY property.
 */
void VideoOutputXv::InitColorKey(bool turnoffautopaint)
{
    int ret = Success, xv_val=0;
    xv_draw_colorkey = true;

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
                    VERBOSE(VB_PLAYBACK, "0 is the only bad color key for "
                            "MythTV, using "<<default_colorkey<<" instead.");
                    xv_colorkey = default_colorkey;
                }
                ret = Success;
            }

            if (ret != Success)
            {
                VERBOSE(VB_IMPORTANT,
                        "Couldn't get the color key color, and we need it.\n"
                        "You likely won't get any video.");
                xv_colorkey = 0;
            }
        }
    }
}

bool VideoOutputXv::SetupDeinterlace(bool interlaced)
{
    if (VideoOutputSubType() > XVideo)
        m_deintfiltername == "bobdeint";
    bool deint = VideoOutput::SetupDeinterlace(interlaced);
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

#ifdef USING_XVMC
static uint calcBPM(int chroma)
{
    int ret;
    switch (chroma)
    {
        case XVMC_CHROMA_FORMAT_420: ret = 6;
        case XVMC_CHROMA_FORMAT_422: ret = 4+2;
        case XVMC_CHROMA_FORMAT_444: ret = 4+4;
        default: ret = 6;
        // default unless gray, then 4 is the right number,
        // a bigger number just wastes a little memory.
    }
    return ret;
}
#endif

bool VideoOutputXv::CreateXvMCBuffers(void)
{
#ifdef USING_XVMC
    int ret = Success;
    X11S(ret = XvMCCreateContext(XJ_disp, xv_port, xvmc_surf_type, 
                                 XJ_width, XJ_height, XVMC_DIRECT,
                                 &(xvmc_ctx)));
    if (ret != Success)
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to create XvMC Context, "
                "status(%1): %2").arg(ret).arg(ErrorStringXvMC(ret)));
        return false;
    }

    bool createBlocks = !(XVMC_VLD == (xvmc_surf_info.mc_type & XVMC_VLD));
    xvmc_surfs = CreateXvMCSurfaces(XVMC_MAX_SURF_NUM, createBlocks);
    if (xvmc_surfs.size() < XVMC_MIN_SURF_NUM)
    {
        VERBOSE(VB_IMPORTANT, "Unable to create XvMC Surfaces");
        DeleteBuffers(XVideoMC, false);
        return false;
    }

    bool ok = vbuffers.CreateBuffers(XJ_width, XJ_height, XJ_disp, &xvmc_ctx,
                                     &xvmc_surf_info, xvmc_surfs);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "Unable to create XvMC Buffers");
        DeleteBuffers(XVideoMC, false);
        return false;
    }

    for (uint i=0; i<xvmc_osd_available.size(); i++)
        xvmc_osd_available[i]->CreateBuffer(xvmc_ctx, XJ_width, XJ_height);

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
    uint num_mv_blocks   = ((XJ_width + 15) / 16) * ((XJ_height + 15) / 16);
    uint num_data_blocks = num_mv_blocks * blocks_per_macroblock;

    // create needed XvMC stuff
    bool ok = true;
    for (uint i = 0; i < num; i++)
    {
        xvmc_vo_surf_t *surf = new xvmc_vo_surf_t;
        bzero(surf, sizeof(xvmc_vo_surf_t));

        X11L;

        int ret = XvMCCreateSurface(XJ_disp, &xvmc_ctx, &(surf->surface));
        ok &= (Success == ret);

        if (create_xvmc_blocks && ok)
        {
            ret = XvMCCreateBlocks(XJ_disp, &xvmc_ctx, num_data_blocks,
                                   &(surf->blocks));
            if (Success != ret)
            {
                XvMCDestroySurface(XJ_disp, &(surf->surface));
                ok = false;
            }
        }

        if (create_xvmc_blocks && ok)
        {
            ret = XvMCCreateMacroBlocks(XJ_disp, &xvmc_ctx, num_mv_blocks,
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

        X11L;

        if (use_xv)
        {
            image = XvShmCreateImage(XJ_disp, xv_port, xv_chroma, 0, 
                                     XJ_width, XJ_height, &XJ_shm_infos[i]);
            size = ((XvImage*)image)->data_size + 64;
        }
        else
        {
            XImage *img =
                XShmCreateImage(XJ_disp, DefaultVisual(XJ_disp, XJ_screen_num),
                                XJ_depth, ZPixmap, 0, &XJ_shm_infos[i],
                                dispw, disph);
            size = img->bytes_per_line * img->height + 64;
            image = img;
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

                // mark for delete immediately - it won't be removed until detach
                shmctl(XJ_shm_infos[i].shmid, IPC_RMID, 0);

                bufs.push_back((unsigned char*) XJ_shm_infos[i].shmaddr);
            }
            else
            { 
                VERBOSE(VB_IMPORTANT, "CreateXvShmImages() shmget() failed: "
                        <<strerror(errno));
                break;
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "CreateXvShmImages() XvShmCreateImage()");
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
        ok = vbuffers.CreateBuffers(XJ_width, XJ_height, bufs);
        X11S(XSync(XJ_disp, False));
        if (xv_chroma != GUID_I420_PLANAR)
            xv_color_conv_buf = new unsigned char[XJ_width * XJ_height * 3 / 2];
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

            int bytes_per_line = XJ_depth / 8 * dispw;
            int scrn = DefaultScreen(XJ_disp);
            Visual *visual = DefaultVisual(XJ_disp, scrn);
            XJ_non_xv_image = XCreateImage(XJ_disp, visual, XJ_depth,
                                           ZPixmap, /*offset*/0, /*data*/0,
                                           dispw, disph, /*bitmap_pad*/0,
                                           bytes_per_line);

            X11U;

            if (!XJ_non_xv_image)
            {
                VERBOSE(VB_IMPORTANT, "XCreateImage failed: "
                        <<"XJ_disp("<<XJ_disp<<") visual("<<visual<<") "<<endl
                        <<"                        "
                        <<"XJ_depth("<<XJ_depth<<") "
                        <<"WxH("<<dispw<<"x"<<disph<<") "
                        <<"bpl("<<bytes_per_line<<")");
                return false;
            }
            XJ_non_xv_image->data = (char*) malloc(bytes_per_line * disph);
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
            VERBOSE(VB_IMPORTANT, "Non XVideo mode only supports "
                    <<"16, 24, and 32 bit per pixel displays\n"
                    <<"                        "
                    <<"But you have a "<<(XJ_depth*8)<<" bpp display");
        }
        else
            ok = vbuffers.CreateBuffers(XJ_width, XJ_height);

    }

    if (ok)
        CreatePauseFrame();

    return ok;
}

void VideoOutputXv::DeleteBuffers(VOSType subtype, bool delete_pause_frame)
{
    (void) subtype;
    DiscardFrames();

#ifdef USING_XVMC
    // XvMC buffers
    for (uint i=0; i<xvmc_surfs.size(); i++)
    {
        xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*) xvmc_surfs[i];
        X11S(XvMCHideSurface(XJ_disp, &(surf->surface)));
    }
    DiscardFrames();
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
    for (uint i=0; i<xvmc_osd_available.size(); i++)
    {
        xvmc_osd_available[i]->DeleteBuffer();
        delete xvmc_osd_available[i];
    }
    xvmc_osd_available.clear();
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
                X11S(XFree(XJ_non_xv_image));
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
    if (XVideo < subtype)
        X11S(XvMCDestroyContext(XJ_disp, &xvmc_ctx));
#endif // USING_XVMC
}

void VideoOutputXv::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    global_lock.lock();
    if (embedding)
    {
        MoveResize();
        global_lock.unlock();
        return;
    }

    XJ_curwin = wid;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);

    if (display_res)
    {
        // Switch to resolution of widget
        XWindowAttributes   attr;

        X11S(XGetWindowAttributes(XJ_disp, wid, &attr));
        display_res->SwitchToCustomGUI(attr.width, attr.height);
    }

    global_lock.unlock();
}

void VideoOutputXv::StopEmbedding(void)
{
    if (!embedding)
        return;

    global_lock.lock();
    XJ_curwin = XJ_win;
    VideoOutput::StopEmbedding();

    // Switch back to resolution for full screen video
    if (display_res)
        display_res->SwitchToVideo(XJ_width, XJ_height);

    global_lock.unlock();
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
        VideoFrame* osdframe = vbuffers.GetOSDFrame(frame);
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

#define DQ_COPY(DST, SRC) \
    do { \
        DST.insert(DST.end(), vbuffers.begin_lock(SRC), vbuffers.end(SRC)); \
        vbuffers.end_lock(); \
    } while (0)

void VideoOutputXv::DiscardFrames(void)
{ 
    if (VideoOutputSubType() <= XVideo)
    {
        vbuffers.DiscardFrames();
        return;
    }

#ifdef USING_XVMC
    frame_queue_t::iterator it;
    frame_queue_t syncs;
    frame_queue_t ula;
    frame_queue_t discards;

    {
        vbuffers.begin_lock(kVideoBuffer_displayed); // Lock X
        VERBOSE(VB_PLAYBACK, QString("VideoOutputXv::DiscardFrames() 1: %1")
                .arg(vbuffers.GetStatus()));
        vbuffers.end_lock(); // Lock X
    }

    CheckDisplayedFramesForAvailability();

    {
        vbuffers.begin_lock(kVideoBuffer_displayed); // Lock Y

        DQ_COPY(syncs, kVideoBuffer_displayed);
        DQ_COPY(syncs, kVideoBuffer_pause);
        for (it = syncs.begin(); it != syncs.end(); ++it)
        {
            SyncSurface(*it, -1); // sync past
            SyncSurface(*it, +1); // sync future
            SyncSurface(*it,  0); // sync current
            //GetRender(*it)->p_past_surface   = NULL;
            //GetRender(*it)->p_future_surface = NULL;
        }
        VERBOSE(VB_PLAYBACK, QString("VideoOutputXv::DiscardFrames() 2: %1")
                .arg(vbuffers.GetStatus()));
#if 0
        // Remove inheritence of all frames not in displayed or pause
        DQ_COPY(ula, kVideoBuffer_used);
        DQ_COPY(ula, kVideoBuffer_limbo);
        DQ_COPY(ula, kVideoBuffer_avail);
        
        for (it = ula.begin(); it != ula.end(); ++it)
            vbuffers.RemoveInheritence(*it);
#endif

        VERBOSE(VB_PLAYBACK, QString("VideoOutputXv::DiscardFrames() 3: %1")
                .arg(vbuffers.GetStatus()));
        // create discard frame list
        DQ_COPY(discards, kVideoBuffer_used);
        DQ_COPY(discards, kVideoBuffer_limbo);

        vbuffers.end_lock(); // Lock Y
    }

    for (it = discards.begin(); it != discards.end(); ++it)
        DiscardFrame(*it);

    {
        vbuffers.begin_lock(kVideoBuffer_displayed); // Lock Z

        syncs.clear();
        DQ_COPY(syncs, kVideoBuffer_displayed);
        DQ_COPY(syncs, kVideoBuffer_pause);
        for (it = syncs.begin(); it != syncs.end(); ++it)
        {
            SyncSurface(*it, -1); // sync past
            SyncSurface(*it, +1); // sync future
            SyncSurface(*it,  0); // sync current
            //GetRender(*it)->p_past_surface   = NULL;
            //GetRender(*it)->p_future_surface = NULL;
        }

        VERBOSE(VB_PLAYBACK,
                QString("VideoOutputXv::DiscardFrames() 4: %1 -- done() ")
                .arg(vbuffers.GetStatus()));
        
        vbuffers.end_lock(); // Lock Z
    }
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
    if (VideoOutputSubType() <= XVideo)
    {
        vbuffers.DoneDisplayingFrame();
        return;
    }
#ifdef USING_XVMC
    if (vbuffers.size(kVideoBuffer_used))
    {
        VideoFrame *frame = vbuffers.head(kVideoBuffer_used);
        DiscardFrame(frame);
        VideoFrame *osdframe = vbuffers.GetOSDFrame(frame);
        if (osdframe)
            DiscardFrame(osdframe);
    }
    CheckDisplayedFramesForAvailability();
#endif
}

/**
 * \fn VideoOutputXv::PrepareFrameXvMC(VideoFrame *frame)
 *  
 *  
 */
void VideoOutputXv::PrepareFrameXvMC(VideoFrame *frame)
{
    (void)frame;
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

    global_lock.lock(); vbuffers.LockFrame(frame, "PrepareFrameXv");
    framesPlayed = frame->frameNumber + 1;
    XvImage *image = (XvImage*) xv_buffers[frame->buf];
    vbuffers.UnlockFrame(frame, "PrepareFrameXv"); global_lock.unlock();

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
 * \fn VideoOutputXv::PrepareFrameMem(VideoFrame *frame)
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
            VERBOSE(VB_IMPORTANT, 
                    "***\n"
                    "* Your system is not capable of displaying the\n"
                    "* full framerate at "
                    <<dispw<<"x"<<disph<<" resolution.  Frames\n"
                    "* will be skipped in order to keep the audio and\n"
                    "* video in sync.\n");
        }
    }

    non_xv_frames_shown++;

    if ((non_xv_show_frame != 1) && (non_xv_frames_shown % non_xv_show_frame))
        return;

    if (!XJ_non_xv_image)
    {
        VERBOSE(VB_IMPORTANT, "ERROR: XJ_non_xv_image == NULL");
        return;
    }

    unsigned char *sbuf = new unsigned char[dispw * disph * 3 / 2];
    AVPicture image_in, image_out;
    ImgReSampleContext *scontext;

    avpicture_fill(&image_out, (uint8_t *)sbuf, PIX_FMT_YUV420P,
                   dispw, disph);

    vbuffers.LockFrame(buffer, "PrepareFrameMem");
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
    vbuffers.UnlockFrame(buffer, "PrepareFrameMem");

    avpicture_fill(&image_in, (uint8_t *)XJ_non_xv_image->data, 
                   non_xv_av_format, dispw, disph);

    img_convert(&image_in, non_xv_av_format, &image_out, PIX_FMT_YUV420P,
                dispw, disph);

    global_lock.lock();
 
    X11L;
    if (XShm == video_output_subtype)
        XShmPutImage(XJ_disp, XJ_curwin, XJ_gc, XJ_non_xv_image,
                     0, 0, 0, 0, dispw, disph, False);
    else
        XPutImage(XJ_disp, XJ_curwin, XJ_gc, XJ_non_xv_image, 
                  0, 0, 0, 0, dispw, disph);
    X11U;

    global_lock.unlock();

    delete [] sbuf;
}

// this is documented in videooutbase.cpp
void VideoOutputXv::PrepareFrame(VideoFrame *buffer, FrameScanType scan)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::PrepareFrame() called "
                "while IsErrored is true.");
        return;
    }

    if (VideoOutputSubType() > XVideo)
        PrepareFrameXvMC(buffer);
    else if (VideoOutputSubType() == XVideo)
        PrepareFrameXv(buffer);
    else
        PrepareFrameMem(buffer, scan);
}

static void calc_bob(FrameScanType scan, int imgh, int disphoff,
                    int imgy, int dispyoff,
                    int frame_height, int top_field_first,
                    int &field, int &src_y, int &dest_y, int& xv_src_y_incr)
{
    int dst_half_line_in_src = 0, dest_y_incr = 0, src_y_incr = 0;
    // a negative offset y gives us bobbing, so adjust...
    if (dispyoff < 0)
    {
        dest_y_incr = -dispyoff;
        src_y_incr = (int) (dest_y_incr * imgh * 0.5 / disphoff);
    }

    if ((scan == kScan_Interlaced && top_field_first == 1) ||
        (scan == kScan_Intr2ndField && top_field_first == 0))
    {
        field = 1;
        xv_src_y_incr = imgy / 2;
    }
    else if ((scan == kScan_Interlaced && top_field_first == 0) ||
             (scan == kScan_Intr2ndField && top_field_first == 1))
    {
        field = 2;
        xv_src_y_incr = (frame_height + imgy) / 2;
        dst_half_line_in_src = (int) round(((float)disphoff)/imgh - 0.001f);
    }

    src_y += src_y_incr;
    dest_y += dst_half_line_in_src + dest_y_incr;
}

void VideoOutputXv::ShowXvMC(FrameScanType scan)
{
    (void)scan;
#ifdef USING_XVMC
    VideoFrame *frame = NULL;
    bool using_pause_frame = false;

    vbuffers.begin_lock(kVideoBuffer_pause);
    if (vbuffers.size(kVideoBuffer_pause))
    {
        frame = vbuffers.head(kVideoBuffer_pause);
        VERBOSE(VB_PLAYBACK, QString("use pause frame: %1 ShowXvMC")
                .arg(DebugString(frame)));
        using_pause_frame = true;
    }
    else if (vbuffers.size(kVideoBuffer_used))
        frame = vbuffers.head(kVideoBuffer_used);
    vbuffers.end_lock();

    if (!frame)
    {
        VERBOSE(VB_PLAYBACK, "ShowXvMC -- called with no frame to show");
        return;
    }

    vbuffers.LockFrame(frame, "ShowXvMC");

    // calculate bobbing params
    int field = 3, src_y = imgy, dest_y = dispyoff, xv_src_y_incr = 0;
    if (m_deinterlacing)
    {
        calc_bob(scan, imgh, disphoff, imgy, dispyoff,
                 frame->height, frame->top_field_first,
                 field, src_y, dest_y, xv_src_y_incr);
    }
    if (hasVLDAcceleration())
    {   // don't do bob-adjustment for VLD drivers
        src_y = imgy;
        dest_y = dispyoff;
    }

    // get and try to lock OSD frame, if it exists
    VideoFrame *osdframe = vbuffers.GetOSDFrame(frame);
    if (osdframe && !vbuffers.TryLockFrame(osdframe, "ShowXvMC -- osd"))
    {
        VERBOSE(VB_IMPORTANT, "ShowXvMC() -- unable to get OSD lock");
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
                   imgx, src_y, imgw, imgh,
                   dispxoff, dest_y, dispwoff, disphoff, field);
    XFlush(XJ_disp); // send XvMCPutSurface call to X11 server
    X11U;

    // if not using_pause_frame, clear old process buffer
    if (!using_pause_frame)
    {
        while (vbuffers.size(kVideoBuffer_pause))
            DiscardFrame(vbuffers.dequeue(kVideoBuffer_pause));
    }
    // clear any displayed frames not on screen
    CheckDisplayedFramesForAvailability();

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

    int field = 3, src_y = imgy, dest_y = dispyoff, xv_src_y_incr = 0;
    if (m_deinterlacing && (m_deintfiltername == "bobdeint"))
    {
        calc_bob(scan, imgh, disphoff, imgy, dispyoff,
                 frame->height, frame->top_field_first,
                 field, src_y, dest_y, xv_src_y_incr);
        src_y += xv_src_y_incr;
    }

    vbuffers.UnlockFrame(frame, "ShowXVideo");
    global_lock.lock();
    vbuffers.LockFrame(frame, "ShowXVideo");

    X11S(XvShmPutImage(XJ_disp, xv_port, XJ_curwin,
                       XJ_gc, image, imgx, src_y, imgw,
                       (3 != field) ? (imgh/2) : imgh,
                       dispxoff, dest_y, dispwoff, disphoff, False));

    vbuffers.UnlockFrame(frame, "ShowXVideo");
    global_lock.unlock();
}

// this is documented in videooutbase.cpp
void VideoOutputXv::Show(FrameScanType scan)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::Show() "
                "called while IsErrored is true.");
        return;
    }

    if (needrepaint && (VideoOutputSubType() >= XVideo))
        DrawUnusedRects(/* don't do a sync*/false);

    if (VideoOutputSubType() > XVideo)
        ShowXvMC(scan);
    else if (VideoOutputSubType() == XVideo)
        ShowXVideo(scan);

    X11S(XSync(XJ_disp, False));
}

/**
 * \fn VideoOutputXv::DrawUnusedRects(bool sync)
 *  
 *  
 */
void VideoOutputXv::DrawUnusedRects(bool sync)
{
    // boboff assumes the smallest interlaced resolution is 480 lines
    int boboff = (int)round(((float)disphoff) / 480 - 0.001f);
    boboff = (m_deinterlacing && m_deintfiltername == "bobdeint") ? boboff : 0;

    X11L;

    if (xv_draw_colorkey && needrepaint)
    {
        XSetForeground(XJ_disp, XJ_gc, xv_colorkey);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, dispx, 
                       dispy + boboff, dispw, disph - 2 * boboff);
        needrepaint = false;
    }

    // Draw black in masked areas
    XSetForeground(XJ_disp, XJ_gc, XJ_black);

    if (dispxoff > dispx) // left
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       dispx, dispy, dispxoff - dispx, disph);
    if (dispxoff + dispwoff < dispx + dispw) // right
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       dispxoff + dispwoff, dispy, 
                       (dispx + dispw) - (dispxoff + dispwoff), disph);
    if (dispyoff + boboff > dispy) // top of screen
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       dispx, dispy, dispw, dispyoff + boboff - dispy);
    if (dispyoff + disphoff < dispy + disph) // bottom of screen
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc, 
                       dispx, dispyoff + disphoff, 
                       dispw, (dispy + disph) - (dispyoff + disphoff));

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
        X11S(status = XvMCPutSlice2(XJ_disp, &xvmc_ctx, 
                                    (char*)render->slice_data, 
                                    render->slice_datalen, 
                                    render->slice_code));
        if (Success != status)
            VERBOSE(VB_PLAYBACK, "XvMCPutSlice, error: "<<status);

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
             XvMCRenderSurface(XJ_disp, &xvmc_ctx, 
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
            VERBOSE(VB_PLAYBACK, 
                    QString("XvMCRenderSurface, error: %1 (%2)")
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

void VideoOutputXv::AspectChanged(float aspect)
{
    global_lock.lock();

    VideoOutput::AspectChanged(aspect);
    MoveResize();

    global_lock.unlock();
}

float VideoOutputXv::GetDisplayAspect(void)
{
    return display_aspect;
}

void VideoOutputXv::UpdatePauseFrame(void)
{
    if (VideoOutputSubType() <= XVideo)
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

        if (!used_frame &&
            vbuffers.TryLockFrame(vbuffers.GetScratchFrame(),
                                  "UpdatePauseFrame -- scratch"))
        {
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
            VERBOSE(VB_PLAYBACK, QString("UpdatePauseFrame -- Error, pause "
                                         "buffer size>1, size = %1")
                    .arg(vbuffers.size(kVideoBuffer_pause)));
            while (vbuffers.size(kVideoBuffer_pause))
                DiscardFrame(vbuffers.dequeue(kVideoBuffer_pause));
            CheckDisplayedFramesForAvailability();
        } else if (1 == vbuffers.size(kVideoBuffer_pause))
        {
            VideoFrame *frame = vbuffers.dequeue(kVideoBuffer_used);
            if (frame)
            {
                while (vbuffers.size(kVideoBuffer_pause))
                    DiscardFrame(vbuffers.dequeue(kVideoBuffer_pause));
                vbuffers.safeEnqueue(kVideoBuffer_pause, frame);
                VERBOSE(VB_PLAYBACK,
                        "UpdatePauseFrame -- XvMC using new pause frame");
            }
            else
                VERBOSE(VB_PLAYBACK,
                        "UpdatePauseFrame -- XvMC using old pause frame");
            return;
        }

        frame_queue_t::iterator it = vbuffers.begin_lock(kVideoBuffer_displayed);
        VERBOSE(VB_PLAYBACK, "UpdatePauseFrame -- XvMC");
        if (vbuffers.size(kVideoBuffer_displayed))
        {
            VERBOSE(VB_PLAYBACK,
                    "UpdatePauseFrame -- XvMC found a pause frame in display");

            VideoFrame *frame = vbuffers.tail(kVideoBuffer_displayed);
            if (vbuffers.GetOSDParent(frame))
                frame = vbuffers.GetOSDParent(frame);
            vbuffers.safeEnqueue(kVideoBuffer_pause, frame);
        }
        vbuffers.end_lock();

        if (1 != vbuffers.size(kVideoBuffer_pause))
        {
            VERBOSE(VB_PLAYBACK,
                    "UpdatePauseFrame -- XvMC did NOT find a pause frame");
        }
    }
#endif
}

void VideoOutputXv::ProcessFrameXvMC(VideoFrame *frame, OSD *osd)
{
    (void)frame;
    (void)osd;
#ifdef USING_XVMC
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
            success = vbuffers.TryLockFrame(frame, "ProcessFrameXvMC -- reuse");
        }
        vbuffers.end_lock();

        if (success)
        {
            VERBOSE(VB_PLAYBACK, QString("use pause frame: %1 ProcessFrameXvMC")
                    .arg(DebugString(frame)));
            vbuffers.SetOSDFrame(frame, NULL);
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    "ProcessFrameXvMC tried to reuse frame but failed");
            frame = NULL;
        }
    }

    if (!frame)
    {
        VERBOSE(VB_IMPORTANT, "ProcessFrameXvMC -- called with no frame");
        return;
    }

    VideoFrame * old_osdframe = vbuffers.GetOSDFrame(frame);
    if (old_osdframe)
    {
        QString a = DebugString(old_osdframe, true), b = DebugString(frame, true);
        VERBOSE(VB_IMPORTANT, QString("ProcessFrameXvMC: Warning, %1 is still "
                                      "marked as the OSD frame of %2.")
                .arg(a).arg(b));
        vbuffers.SetOSDFrame(frame, NULL);
    }

    XvMCOSD* xvmc_osd = GetAvailableOSD();
    if (osd && xvmc_osd->IsValid())
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
                CheckDisplayedFramesForAvailability();

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

                CheckDisplayedFramesForAvailability();
            }

            // If there is an available buffer grab it.
            if (vbuffers.size(kVideoBuffer_avail))
            {
                osdframe = vbuffers.GetNextFreeFrame(false, false);
                // Check for error condition..
                if (frame == osdframe)
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("ProcessFrameXvMC: Error, %1 %2")
                            .arg(DebugString(frame, true))
                            .arg(vbuffers.GetStatus()));
                    osdframe = NULL;
                }
            }

            if (osdframe && vbuffers.TryLockFrame(osdframe, "ProcessFrameXvMC -- OSD"))
            {
                vbuffers.SetOSDFrame(osdframe, NULL);
                xvmc_osd->CompositeOSD(frame, osdframe);
                vbuffers.UnlockFrame(osdframe, "ProcessFrameXvMC -- OSD");
                vbuffers.SetOSDFrame(frame, osdframe);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "ProcessFrameXvMC -- Failed to get OSD lock");
                DiscardFrame(osdframe);
            }
        }
        if (ret >= 0 && !xvmc_osd->NeedFrame())
        {
            xvmc_osd->CompositeOSD(frame);
        }
    }
    ReturnAvailableOSD(xvmc_osd);
    vbuffers.UnlockFrame(frame, "ProcessFrameXvMC");            
#endif // USING_XVMC
}

#ifdef USING_XVMC
#if XVMC_OSD_NUM == 1

XvMCOSD* VideoOutputXv::GetAvailableOSD()
{ 
    xvmc_osd_lock.lock();
    return xvmc_osd_available.head();
}

void VideoOutputXv::ReturnAvailableOSD(XvMCOSD*)
{
    xvmc_osd_lock.unlock();
}

#else // if XVMC_OSD_NUM != 1

XvMCOSD* VideoOutputXv::GetAvailableOSD()
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

void VideoOutputXv::ReturnAvailableOSD(XvMCOSD* avail)
{
    xvmc_osd_lock.lock();
    xvmc_osd_available.push_front(avail);
    xvmc_osd_lock.unlock();
}
#endif // if XVMC_OSD_NUM != 1

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

    if (osd)
    {
        DisplayOSD(frame, osd);
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
        VERBOSE(VB_IMPORTANT, "VideoOutputXv::ProcessFrame() called "
                "while IsErrored is true.");
        return;
    }

    if (VideoOutputSubType() <= XVideo)
        ProcessFrameMem(frame, osd, filterList, pipPlayer);
    else
        ProcessFrameXvMC(frame, osd);
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

    X11S(attribute = XInternAtom (XJ_disp, attrName, False));
    if (!attribute) {
        return -1;
    }

    X11S(attributes = XvQueryPortAttributes(XJ_disp, xv_port, &howmany));
    if (!attributes) {
        return -1;
    }

    for (i = 0; i < howmany; i++) {
        if (!strcmp(attrName, attributes[i].name)) {
            port_min = attributes[i].min_value;
            port_max = attributes[i].max_value;
            range = port_max - port_min;

            value = (int) (port_min + (range/100.0) * newValue);

            X11S(XvSetPortAttribute(XJ_disp, xv_port, attribute, value));

            return newValue;
        }
    }

    return -1;
}

void VideoOutputXv::CheckDisplayedFramesForAvailability(void)
{
#ifdef USING_XVMC
    frame_queue_t::iterator it;

#ifdef AGGRESSIVE_BUFFER_MANAGEMENT
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
#endif // AGGRESSIVE_BUFFER_MANAGEMENT

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
                VERBOSE(VB_PLAYBACK, "Frame "<<DebugString(pframe, true)<<" w/children: "
                        <<" "<<DebugString(children)<<" is being held for later discarding.");
                frame_queue_t::iterator cit;
                for (cit = children.begin(); cit != children.end(); ++cit)
                    if (vbuffers.contains(kVideoBuffer_avail, *cit))
                        VERBOSE(VB_IMPORTANT, "ERROR: child     "
                                <<DebugString(*cit)
                                <<" is already in available");
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
            VERBOSE(VB_PLAYBACK, QString("Error#2 XvMCGetSurfaceStatus %1").arg(res));
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
            VERBOSE(VB_PLAYBACK, QString("Error#1 XvMCGetSurfaceStatus %1").arg(res));
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
                VERBOSE(VB_PLAYBACK,
                        QString("Error#3 XvMCGetSurfaceStatus %1").arg(res));
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
            VERBOSE(VB_IMPORTANT, "XvMC output requested, "
                    "but is not supported by display.");
            useXvMC = false;
        }

        int mc_ver, mc_rel;
        X11S(ret = XvMCQueryVersion(d, &mc_ver, &mc_rel));
        if (Success == ret)
            VERBOSE(VB_PLAYBACK, "XvMC version: "<<mc_ver<<"."<<mc_rel);
#else // !USING_XVMC
        VERBOSE(VB_IMPORTANT, "XvMC output requested, "
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
            VERBOSE(VB_IMPORTANT, "XVideo output requested, "
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

static QString xvflags2str(int flags)
{
    QString str("");
    if (XvInputMask == (flags & XvInputMask))
        str.append("XvInputMask ");
    if (XvOutputMask == (flags & XvOutputMask))
        str.append("XvOutputMask ");
    if (XvVideoMask == (flags & XvVideoMask))
        str.append("XvVideoMask ");
    if (XvStillMask == (flags & XvStillMask))
        str.append("XvStillMask ");
    if (XvImageMask == (flags & XvImageMask))
        str.append("XvImageMask ");
    return str;
}

CodecID myth2av_codecid(MythCodecID codec_id,
                        bool& vld, bool& idct, bool& mc)
{
    vld = idct = mc = false;
    CodecID ret = CODEC_ID_NONE;
    switch (codec_id)
    {
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
        default:
            VERBOSE(VB_IMPORTANT, QString("Error: MythCodecID %1 has not been "
                                          "added to myth2av_codecid")
                    .arg(codec_id));
            break;
    } // switch(codec_id)
    return ret;
}

#ifdef USING_XVMC
static QString ErrorStringXvMC(int val)
{
    QString str = "unrecognized return value";
    switch (val)
    {
        case Success:   str = "Success"  ; break;
        case BadValue:  str = "BadValue" ; break;
        case BadMatch:  str = "BadMatch" ; break;
        case BadAlloc:  str = "BadAlloc" ; break;
    }
    return str;
}

static xvmc_render_state_t *GetRender(VideoFrame *frame)
{
    if (frame)
        return (xvmc_render_state_t*) frame->buf;
    return NULL;
}
#endif // USING_XVMC
