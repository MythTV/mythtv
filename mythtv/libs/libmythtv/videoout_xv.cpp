#include "NuppelVideoPlayer.h"

/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */

// ANSI C headers
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cerrno>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/keysym.h>

#include <algorithm>
#include <iostream>
using namespace std;

// Qt headers
#include <QApplication>
#include <QDesktopWidget>
#include <QTextStream>

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
#include "mythcontext.h"
#include "mythverbose.h"
#include "filtermanager.h"
#include "videodisplayprofile.h"
#define IGNORE_TV_PLAY_REC
#include "tv.h"
#include "fourcc.h"
#include "mythmainwindow.h"

#ifdef USING_VDPAU
#include "util-vdpau.h"
#endif

// MythTV OpenGL headers
#include "openglcontext.h"
#include "openglvideo.h"
#include "mythuihelper.h"

#define LOC      QString("VideoOutputXv: ")
#define LOC_WARN QString("VideoOutputXv Warning: ")
#define LOC_ERR  QString("VideoOutputXv Error: ")

extern "C" {
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/Xinerama.h>
#ifndef _XSHM_H_
    extern int      XShmQueryExtension(Display*);
    extern int      XShmGetEventBase(Display*);
#endif // silences warning when these are already defined
    extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*,
                                      int, int, XShmSegmentInfo*);
}

#ifndef HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

#ifndef XVMC_CHROMA_FORMAT_420
#define XVMC_CHROMA_FORMAT_420 0x00000001
#endif

#define NUM_VDPAU_BUFFERS 17

static QStringList allowed_video_renderers(
    MythCodecID codec_id, Display *display, int screen, Window curwin);

static void SetFromEnv(bool &useXvVLD, bool &useXvIDCT, bool &useXvMC,
                       bool &useXV, bool &useShm, bool &useOpenGL,
                       bool &useVDPAU);
static void SetFromHW(Display *d, int screen, Window curwin,
                      bool &useXvMC, bool &useXV,
                      bool &useShm, bool &useXvMCOpenGL,
                      bool &useOpenGL, bool &useVDPAU,
                      MythCodecID myth_codec_id);
static int calc_hue_base(const QString &adaptor_name);

const char *vr_str[] =
{
    "unknown", "xlib", "xshm", "opengl", "xv-blit", "vdpau", "xvmc", "xvmc",
    "xvmc",
};

class GLContextCreator : public QThread
{
  public:
    GLContextCreator(VideoOutputXv *parent,
                     const QRect &display_visible_rect,
                     bool map_window) :
        m_parent(parent), m_rect(display_visible_rect),
        m_map_window(map_window), m_gl_context(NULL)
    {
        start();
    }

    virtual void run(void)
    {
        m_gl_context = m_parent->CreateGLContext(m_rect, m_map_window);
        m_parent->GLContextCreatedNotify();
    }

    VideoOutputXv    *m_parent;
    QRect             m_rect;
    bool              m_map_window;
    OpenGLContextGLX *m_gl_context;
};

/** \class  VideoOutputXv
 *  \brief Supports common video output methods used with %X11 Servers.
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
      display_res(NULL), global_lock(QMutex::Recursive),

      XJ_root(0),  XJ_win(0), XJ_curwin(0), XJ_gc(0), XJ_screen(NULL),
      XJ_disp(NULL), XJ_screen_num(0),
      XJ_white(0), XJ_black(0), XJ_letterbox_colour(0), XJ_depth(0),
      XJ_started(false), XJ_monitor_sz(640,480), XJ_monitor_dim(400,300),

      XJ_non_xv_image(0), non_xv_frames_shown(0), non_xv_show_frame(1),
      non_xv_fps(0), non_xv_av_format(PIX_FMT_NB), non_xv_stop_time(0),

      xvmc_buf_attr(new XvMCBufferSettings()),
      xvmc_chroma(XVMC_CHROMA_FORMAT_420), xvmc_ctx(NULL),
      xvmc_osd_lock(),
      xvmc_tex(NULL),

#ifdef USING_VDPAU
      vdpau(NULL),
#endif
      vdpau_use_osd(false), vdpau_use_pip(true), vdpau_use_colorkey(false),

      xv_port(-1),      xv_hue_base(0),
      xv_colorkey(0),   xv_draw_colorkey(false),
      xv_chroma(0),

      gl_context_lock(QMutex::Recursive),
      gl_context_creator(NULL), gl_context(NULL),
      gl_videochain(NULL), gl_pipchain_active(NULL),
      gl_osdchain(NULL),

      gl_use_osd_opengl2(false),
      gl_osd_ready(false),


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

    const QRect tmp_display_visible_rect =
        windows[0].GetTmpDisplayVisibleRect();

    if (windows[0].GetPIPState() == kPIPStandAlone &&
        !tmp_display_visible_rect.isEmpty())
    {
        windows[0].SetDisplayVisibleRect(tmp_display_visible_rect);
    }

    if (XJ_started)
    {
        const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
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

    // Delete the video buffers
    DeleteBuffers(VideoOutputSubType(), true);

    gl_context_creator_lock.lock();
    if (gl_context_creator)
    {
        if (!gl_context)
        {
            gl_context_creator_lock.unlock();
            GLContextCreatedWait();
            gl_context_creator_lock.lock();
        }
        gl_context_creator->deleteLater();
        gl_context_creator = NULL;
    }
    gl_context_creator_lock.unlock();

    if (gl_context)
    {
        QMutexLocker locker(&gl_context_lock);
        delete gl_context;
        gl_context = NULL;
    }

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

    if (xvmc_buf_attr)
        delete xvmc_buf_attr;

    // Switch back to desired resolution for GUI
    if (display_res)
        display_res->SwitchToGUI();
}

// this is documented in videooutbase.cpp
void VideoOutputXv::Zoom(ZoomDirection direction)
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
        windows[0].SetNeedRepaint(true);
    }

    if (gl_videochain && gl_context)
    {
        OpenGLContextLocker ctx_lock(gl_context);
        gl_videochain->SetVideoRect(windows[0].GetDisplayVideoRect(),
                                    windows[0].GetVideoRect());
    }
}

void VideoOutputXv::WindowResized(const QSize &new_size)
{
    QMutexLocker locker(&global_lock);

    windows[0].SetDisplayVisibleRect(QRect(QPoint(0, 0), new_size));

    const QSize display_dim = QSize(
        (XJ_monitor_dim.width()  * new_size.width()) / XJ_monitor_sz.width(),
        (XJ_monitor_dim.height() * new_size.height())/ XJ_monitor_sz.height());
    windows[0].SetDisplayDim(display_dim);

    windows[0].SetDisplayAspect(
        ((float)display_dim.width()) / display_dim.height());

    MoveResize();
}

// documented in videooutbase.cpp
bool VideoOutputXv::InputChanged(const QSize &input_size,
                                 float        aspect,
                                 MythCodecID  av_codec_id,
                                 void        *codec_private)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3) '%4'->'%5'")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(myth_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&global_lock);

    bool cid_changed = (myth_codec_id != av_codec_id);
    bool res_changed = input_size     != windows[0].GetVideoDispDim();
    bool asp_changed = aspect         != windows[0].GetVideoAspect();
    bool gpu_deint   = (VideoOutputSubType() == OpenGL ||
                        VideoOutputSubType() == XVideoVDPAU);

    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private);

    if (!res_changed && !cid_changed)
    {
        if (VideoOutputSubType() == XVideo)
            vbuffers.Clear(xv_chroma);
        if (asp_changed)
            MoveResize();
        return true;
    }

    bool delete_pause_frame = cid_changed || (OpenGL == VideoOutputSubType());
    DeleteBuffers(VideoOutputSubType(), delete_pause_frame);

    const QSize video_disp_dim = windows[0].GetVideoDispDim();
    ResizeForVideo((uint) video_disp_dim.width(),
                   (uint) video_disp_dim.height());

    bool ok = true;
    if (cid_changed ||
        XVideoVDPAU == VideoOutputSubType() || OpenGL == VideoOutputSubType())
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
    {
        ok = CreateBuffers(VideoOutputSubType());
    }

    MoveResize();

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "InputChanged(): "
                "Failed to recreate buffers");
        errored = true;
    }
    else
    {
        if ((VideoOutputSubType() == OpenGL ||
             VideoOutputSubType() == XVideoVDPAU) ||
             gpu_deint)
        {
            BestDeint();
        }
    }

    return ok;
}

// documented in videooutbase.cpp
QRect VideoOutputXv::GetVisibleOSDBounds(
    float &visible_aspect, float &font_scaling, float themeaspect) const
{
    QRect dvr = GetTotalVisibleRect();

    // This rounding works for I420 video...
    QSize dvr2 = QSize(dvr.width()  & ~0x3,
                       dvr.height() & ~0x1);

    if (!hasFullScreenOSD())
    {
        return VideoOutput::GetVisibleOSDBounds(
            visible_aspect, font_scaling, themeaspect);
    }

    float dispPixelAdj = 1.0f;
    if (dvr2.height() && dvr2.width())
        dispPixelAdj = (GetDisplayAspect() * dvr2.height()) / dvr2.width();
    visible_aspect = themeaspect / dispPixelAdj;
    font_scaling   = 1.0f;
    return QRect(QPoint(0,0), dvr2);
}

/**
 *\brief Returns total visible window space.
 */
QRect VideoOutputXv::GetTotalVisibleRect(void) const
{
    const QRect tvr = windows[0].GetDisplayVisibleRect();
    QString osdrenderer = db_vdisp_profile->GetOSDRenderer();
    return tvr;
}

// documented in videooutbase.cpp
QRect VideoOutputXv::GetTotalOSDBounds(void) const
{
    QRect dvr = GetTotalVisibleRect();

    QSize dvr2 = QSize(dvr.width()  & ~0x3,
                       dvr.height() & ~0x1);

    QSize sz = hasFullScreenOSD() ? dvr2 : windows[0].GetVideoDispDim();
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
 * \return integer approximation of monitor refresh time (microseconds)
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

    double rate = mode_line.htotal * mode_line.vtotal;

    // Catch bad data from video drivers (divide by zero causes return of NaN)
    if (rate == 0.0 || dot_clock == 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GetRefreshRate(): "
                "X11 ModeLine query returned zeroes");
        return -1;
    }

    rate = (dot_clock * 1000.0) / rate;

    // Assume 60Hz if rate isn't good:
    if (rate < 20 || rate > 200)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Unreasonable refresh rate %1Hz "
                                           "reported by X").arg(rate));
        rate = 60;
    }

    rate = 1000000.0 / rate;

    return (int)rate;
}

void VideoOutputXv::ResizeForVideo(void) 
{
    const QSize video_disp_dim = windows[0].GetVideoDispDim();
    ResizeForVideo(video_disp_dim.width(), video_disp_dim.height());
}

void VideoOutputXv::ResizeForGui(void)
{
    if (display_res)
        display_res->SwitchToGUI();
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
    if ((width == 1920 || width == 1440) && height == 1088)
        height = 1080; // ATSC 1920x1080

    if (display_res && display_res->SwitchToVideo(width, height))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        windows[0].SetDisplayDim(QSize(display_res->GetPhysicalWidth(),
                                       display_res->GetPhysicalHeight()));
        windows[0].SetDisplayAspect(display_res->GetAspectRatio());

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
            const QRect display_visible_rect = QRect(QPoint(0,0), sz);
            windows[0].SetDisplayVisibleRect(display_visible_rect);

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
        // The very first Resize needs to be the maximum possible
        // desired res, because X will mask off anything outside
        // the initial dimensions
        QSize sz1 = MythXGetDisplaySize(XJ_disp, XJ_screen_num);
        QSize sz2 = qApp->desktop()->
            availableGeometry(gContext->GetMainWindow()).size();
        int max_w = max(sz1.width(),  sz2.width());
        int max_h = max(sz1.height(), sz2.height());
        X11S(XMoveResizeWindow(XJ_disp, XJ_win, 0, 0, max_w, max_h));

        if (db_display_dim.width() > 0 && db_display_dim.height() > 0)
        {
            windows[0].SetDisplayDim(db_display_dim);
        }
        else
        {
            windows[0].SetDisplayDim(
                MythXGetDisplayDimensions(XJ_disp, XJ_screen_num));
        }
    }

    // Fetch pixel width and height of the display
    int xbase, ybase, w, h;
    GetMythUI()->GetScreenBounds(xbase, ybase, w, h);

    // Determine window dimensions in pixels
    int window_w = w, window_h = h;
    if (gContext->GetNumSetting("GuiSizeForTV", 0))
        gContext->GetResolutionSetting("Gui", window_w,  window_h);
    else
        GetMythUI()->GetScreenBounds(xbase, ybase, window_w, window_h);
    window_w = (window_w) ? window_w : w;
    window_h = (window_h) ? window_h : h;
    float pixel_aspect = ((float)w) / ((float)h);

    VERBOSE(VB_PLAYBACK, LOC + QString(
                "Pixel dimensions: Screen %1x%2, window %3x%4")
            .arg(w).arg(h).arg(window_w).arg(window_h));

    // Determine if we are using Xinerama
    bool usingXinerama = (GetNumberOfXineramaScreens() > 1);

    // If the dimensions are invalid, assume square pixels and 17" screen.
    // Only print warning if this isn't Xinerama, we will fix Xinerama later.
    QSize display_dim = windows[0].GetDisplayDim();
    if (((display_dim.width() <= 0) || (display_dim.height() <= 0)) &&
        !usingXinerama)
    {
        VERBOSE(VB_GENERAL, LOC + "Physical size of display unknown."
                "\n\t\t\tAssuming 17\" monitor with square pixels.");
        display_dim.setHeight(300);
        display_dim.setWidth((int) round(300 * pixel_aspect));
    }

    // If we are using Xinerama the display dimensions can not be trusted.
    // We need to use the Xinerama monitor aspect ratio from the DB to set
    // the physical screen width. This assumes the height is correct, which
    // is more or less true in the typical side-by-side monitor setup.
    if (usingXinerama)
    {
        float displayAspect = gContext->GetFloatSettingOnHost(
            "XineramaMonitorAspectRatio",
            gContext->GetHostName(), pixel_aspect);
        int displayHeight = display_dim.height();
        if (displayHeight <= 0)
            display_dim.setHeight(displayHeight = 300);
        display_dim.setWidth((int) round(displayHeight * displayAspect));
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Estimated display dimensions: %1x%2 mm  Aspect: %3")
            .arg(display_dim.width()).arg(display_dim.height())
            .arg(((float) display_dim.width()) / display_dim.height()));

    XJ_monitor_sz  = QSize(w, h);
    XJ_monitor_dim = display_dim;

    // We must now scale the display measurements to our window size.
    // If we are running fullscreen this is a no-op.
    display_dim = QSize((display_dim.width()  * window_w) / w,
                        (display_dim.height() * window_h) / h);

    // Now make these changes stick
    windows[0].SetDisplayDim(display_dim);

    // If we are using XRandR, use the aspect ratio from it, otherwise
    // calculate the display aspect ratio pretty using physical monitor size
    if (display_res)
    {
        windows[0].SetDisplayAspect(display_res->GetAspectRatio());
    }
    else
    {
        windows[0].SetDisplayAspect(
            ((float)display_dim.width()) / display_dim.height());
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Estimated window dimensions: %1x%2 mm  Aspect: %3")
            .arg(display_dim.width()).arg(display_dim.height())
            .arg(windows[0].GetDisplayAspect()));
}

class XvAttributes
{
  public:
    XvAttributes() :
        description(QString::null), xv_flags(0), feature_flags(0) {}
    XvAttributes(const QString &a, uint b, uint c) :
        description(a), xv_flags(b), feature_flags(c)
        { description.detach(); }

  public:
    QString description;
    uint    xv_flags;
    uint    feature_flags;

    static const uint kFeatureNone      = 0x0000;
    static const uint kFeatureXvMC      = 0x0001;
    static const uint kFeatureVLD       = 0x0002;
    static const uint kFeatureIDCT      = 0x0004;
    static const uint kFeatureChromakey = 0x0008;
    static const uint kFeaturePicCtrl   = 0x0010;
};

/**
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
    // avoid compiler warnings
    (void)xvmc_chroma; (void)xvmc_surf_info;

    if (adaptor_name)
        *adaptor_name = QString::null;

    // figure out what basic kind of surface we want..
    int stream_type = 0;
    bool need_mc   = false, need_idct = false;
    bool need_vld  = false, need_xv   = false;
    switch (mcodecid)
    {
        case kCodec_MPEG1_XVMC: stream_type = 1; need_mc   = true; break;
        case kCodec_MPEG2_XVMC: stream_type = 2; need_mc   = true; break;
        case kCodec_H263_XVMC:  stream_type = 3; need_mc   = true; break;
        case kCodec_MPEG4_XVMC: stream_type = 4; need_mc   = true; break;

        case kCodec_MPEG1_IDCT: stream_type = 1; need_idct = true; break;
        case kCodec_MPEG2_IDCT: stream_type = 2; need_idct = true; break;
        case kCodec_H263_IDCT:  stream_type = 3; need_idct = true; break;
        case kCodec_MPEG4_IDCT: stream_type = 4; need_idct = true; break;

        case kCodec_MPEG1_VLD:  stream_type = 1; need_vld  = true; break;
        case kCodec_MPEG2_VLD:  stream_type = 2; need_vld  = true; break;
        case kCodec_H263_VLD:   stream_type = 3; need_vld  = true; break;
        case kCodec_MPEG4_VLD:  stream_type = 4; need_vld  = true; break;

        default:
            need_xv = true;
            break;
    }

    // create list of requirements to check in order..
    vector<XvAttributes> req;
    if (need_vld)
    {
        req.push_back(XvAttributes(
                          "XvMC surface found with VLD support on port %1",
                          XvInputMask, XvAttributes::kFeatureXvMC |
                          XvAttributes::kFeatureVLD));
    }

    if (need_idct)
    {
        req.push_back(XvAttributes(
                          "XvMC surface found with IDCT support on port %1",
                          XvInputMask, XvAttributes::kFeatureXvMC |
                          XvAttributes::kFeatureIDCT));
    }

    if (need_mc)
    {
        req.push_back(XvAttributes(
                          "XvMC surface found with MC support on port %1",
                          XvInputMask, XvAttributes::kFeatureXvMC));
    }
 
    if (need_xv)
    {
        req.push_back(XvAttributes( 
                          "XVideo surface found on port %1",
                          XvInputMask | XvImageMask,
                          XvAttributes::kFeatureNone));
    }

    // try to get an adapter with picture attributes
    if (true)
    {
        uint end = req.size();
        for (uint i = 0; i < end; i++)
        {
            req.push_back(req[i]);
            req[i].feature_flags |=  XvAttributes::kFeaturePicCtrl;
        }
    }

    // figure out if we want chromakeying..
    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    bool check_for_colorkey = (vdp.GetOSDRenderer() == "chromakey");

    // if we want colorkey capability try to get an adapter with them
    if (check_for_colorkey)
    {
        uint end = req.size();
        for (uint i = 0; i < end; i++)
        {
            req.push_back(req[i]);
            req[i].feature_flags |= XvAttributes::kFeatureChromakey;
        }
    }

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

    QString lastAdaptorName = QString::null;
    int port = -1;

    // find an Xv port
    for (uint j = 0; j < req.size(); ++j)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("@ j=%1 Looking for flag[s]: %2 %3")
                .arg(j).arg(xvflags2str(req[j].xv_flags))
                .arg(req[j].feature_flags,0,16));

        for (int i = 0; i < (int)p_num_adaptors && (port == -1); ++i)
        {
            lastAdaptorName = ai[i].name;
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Adaptor#%1: %2 has flag[s]: %3")
                    .arg(i).arg(lastAdaptorName).arg(xvflags2str(ai[i].type)));

            if ((ai[i].type & req[j].xv_flags) != req[j].xv_flags)
            {
                VERBOSE(VB_PLAYBACK, LOC + "Missing XVideo flags, rejecting.");
                continue;
            }
            else
                VERBOSE(VB_PLAYBACK, LOC + "Has XVideo flags...");

            const XvPortID firstPort = ai[i].base_id;
            const XvPortID lastPort = ai[i].base_id + ai[i].num_ports - 1;
            XvPortID p = 0;

            if ((req[j].feature_flags & XvAttributes::kFeaturePicCtrl) &&
                (!xv_is_attrib_supported(disp, firstPort, "XV_BRIGHTNESS")))
            {
                VERBOSE(VB_PLAYBACK, LOC +
                        "Missing XV_BRIGHTNESS, rejecting.");
                continue;
            }
            else if (req[j].feature_flags & XvAttributes::kFeaturePicCtrl)
                VERBOSE(VB_PLAYBACK, LOC + "Has XV_BRIGHTNESS...");

            if ((req[j].feature_flags & XvAttributes::kFeatureChromakey) &&
                (!xv_is_attrib_supported(disp, firstPort, "XV_COLORKEY")))
            {
                VERBOSE(VB_PLAYBACK, LOC +
                        "Missing XV_COLORKEY, rejecting.");
                continue;
            }
            else if (req[j].feature_flags & XvAttributes::kFeatureChromakey)
                VERBOSE(VB_PLAYBACK, LOC + "Has XV_COLORKEY...");

            VERBOSE(VB_PLAYBACK, LOC + "Here...");

            if (req[j].feature_flags & XvAttributes::kFeatureXvMC)
            {
#ifdef USING_XVMC
                int surfNum;
                XvMCSurfaceTypes::find(
                    width, height, xvmc_chroma,
                    req[j].feature_flags & XvAttributes::kFeatureVLD,
                    req[j].feature_flags & XvAttributes::kFeatureIDCT,
                    stream_type,
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
            VERBOSE(VB_PLAYBACK, LOC + req[j].description.arg(port));
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
 * Creates an extra frame for pause.
 *
 * This creates a pause frame by copies the scratch frame settings, a
 * and allocating a databuffer, so a scratch must already exist.
 * XvMC does not use this pause frame facility so this only creates
 * a pause buffer for the other output methods.
 *
 * \sideeffect sets av_pause_frame.
 */
void VideoOutputXv::CreatePauseFrame(VOSType subtype)
{
    // All methods but XvMC use a pause frame, create it if needed
    if ((subtype <= XVideo) || (subtype == OpenGL))
    {
        vbuffers.LockFrame(&av_pause_frame, "CreatePauseFrame");

        if (av_pause_frame.buf)
        {
            delete [] av_pause_frame.buf;
            av_pause_frame.buf = NULL;
        }

        init(&av_pause_frame, FMT_YV12,
             new unsigned char[vbuffers.GetScratchFrame()->size + 128],
             vbuffers.GetScratchFrame()->width,
             vbuffers.GetScratchFrame()->height,
             vbuffers.GetScratchFrame()->bpp,
             vbuffers.GetScratchFrame()->size);

        av_pause_frame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

        clear(&av_pause_frame, xv_chroma);

        vbuffers.UnlockFrame(&av_pause_frame, "CreatePauseFrame");
    }
}

/**
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
                                     bool use_xv, bool use_shm,
                                     bool use_opengl, bool use_vdpau)
{
    (void)mcodecid;

    bool done = false;

#ifdef USING_VDPAU
    if (((kCodec_VDPAU_BEGIN < mcodecid) && (mcodecid < kCodec_VDPAU_END)) ||
         mcodecid < kCodec_NORMAL_END)
    {
        if (use_vdpau)
        {
            vbuffers.Init(NUM_VDPAU_BUFFERS, false, 1, 4, 4, 1, false);
            done = InitVDPAU(mcodecid);
            if (!done)
                vbuffers.Reset();
        }
    }
#endif

    // If use_xvmc try to create XvMC buffers
#ifdef USING_XVMC
    if (!done && (kCodec_STD_XVMC_BEGIN < mcodecid) &&
        (mcodecid < kCodec_VLD_END))
    {
        // Create ffmpeg VideoFrames
        bool vld, idct, mc, vdpau;
        myth2av_codecid(myth_codec_id, vld, idct, mc, vdpau);

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

    if (!done && mcodecid >= kCodec_NORMAL_END)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to initialize buffers for codec %1")
                .arg(toString(mcodecid)));
        return false;
    }

    // Create ffmpeg VideoFrames
    if (!done)
        vbuffers.Init(31, true, 1, 12, 4, 2, false);

    if (!done && use_opengl)
        done = InitOpenGL();

    // Fall back to XVideo if there is an xv_port
    if (!done && use_xv)
        done = InitXVideo();

    // only HW accel allowed for PIP and PBP
    if (!done && windows[0].GetPIPState() > kPIPOff)
        return done;

    // Fall back to shared memory, if we are allowed to use it
    if (!done && use_shm)
        done = InitXShm();

    // Fall back to plain old X calls
    if (!done)
        done = InitXlib();

    if (done)
    {
        QString tmp = vr_str[VideoOutputSubType()];
        QString rend = db_vdisp_profile->GetVideoRenderer();
        if ((tmp == "xvmc") && (rend.left(4) == "xvmc"))
            tmp = rend;
        db_vdisp_profile->SetVideoRenderer(tmp);
    }

    return done;
}

OpenGLContextGLX *VideoOutputXv::CreateGLContext(
    const QRect &display_rect, bool map_window)
{
    OpenGLContextGLX *m_context = NULL;
#ifdef USING_OPENGL
    bool ok = false;
    
    gl_context_lock.lock();

    m_context = new OpenGLContextGLX(&gl_context_lock);

    ok = m_context->Create(
            XJ_disp, XJ_win, XJ_screen_num,
            display_rect, db_use_picture_controls, map_window);

    gl_context_lock.unlock();
#else
    (void)display_rect;
    (void)map_window;
#endif  

    return m_context;
}

void VideoOutputXv::GLContextCreatedNotify(void)
{
    QMutexLocker locker(&gl_context_creator_lock);
    if (gl_context_creator)
    {
        gl_context = gl_context_creator->m_gl_context;
        gl_context_wait.wakeAll();
    }
}

void VideoOutputXv::GLContextCreatedWait(void)
{
    MythTimer t; t.start();
    QMutexLocker locker(&gl_context_creator_lock);

    if (!gl_context_creator)
        return;

    while (!gl_context_creator->m_gl_context)
        gl_context_wait.wait(&gl_context_creator_lock, 100);

    if (t.elapsed() > 5)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("GLContextCreatedWait: Waited %1 ms")
                .arg(t.elapsed()));
    }
}

bool VideoOutputXv::InitOpenGL(void)
{
    bool ok = false;

#ifdef USING_OPENGL_VIDEO
    ok = gl_context;

    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
    if (!ok)
        GLContextCreatedWait();

    if (gl_context)
    {
        gl_context->MapWindow(); // make GL window visible

        if (windows[0].GetPIPState() >= kPIPStandAlone)
        {
            QRect tmprect = QRect(QPoint(0,0), display_visible_rect.size());
            ResizeDisplayWindow(tmprect, true);
        }
        OpenGLContextLocker ctx_lock(gl_context);
        gl_context->Show();
        gl_videochain = new OpenGLVideo();
        ok = gl_videochain->Init(gl_context, db_use_picture_controls,
                                 windows[0].GetVideoDim(),
                                 GetTotalVisibleRect(),
                                 windows[0].GetDisplayVisibleRect(),
                                 windows[0].GetVideoRect(), true, GetFilters(),
                                 false, db_letterbox_colour);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to allocate opengl context.");
    }

    if (ok)
    {
        OpenGLContextLocker ctx_lock(gl_context);
        InstallXErrorHandler(XJ_disp);

        ok = CreateBuffers(OpenGL);

        vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
        if (!ok || errs.size())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to allocate video buffers.");

            PrintXErrors(XJ_disp, errs);
            DeleteBuffers(OpenGL, false);
            ok = false;
        }
        else
        {
            video_output_subtype = OpenGL;

            // ensure deinterlacing is re-enabled after input change
            bool temp_deinterlacing = m_deinterlacing;

            if (!m_deintfiltername.isEmpty() &&
                !m_deintfiltername.contains("opengl"))
            {
                gl_videochain->SetSoftwareDeinterlacer(m_deintfiltername);
            }

            SetDeinterlacingEnabled(true);

            if (!temp_deinterlacing)
            {
                SetDeinterlacingEnabled(false);
            }
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to initialize GL video renderer.");
    }

    if (!ok)
    {
        QMutexLocker locker(&gl_context_lock);
        if (gl_videochain)
        {
            delete gl_videochain;
            gl_videochain = NULL;
        }

        if (gl_context)
        {
            delete gl_context;
            gl_context = NULL;
        }
    }
#endif // USING_OPENGL_VIDEO

    return ok;
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
    const QSize video_dim = windows[0].GetVideoDim();
    xv_port = GrabSuitableXvPort(XJ_disp, XJ_root, mcodecid,
                                 video_dim.width(), video_dim.height(),
                                 xvmc_chroma, &xvmc_surf_info, &adaptor_name);
    if (xv_port == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not find suitable XvMC surface.");
        return false;
    }

    VERBOSE(VB_IMPORTANT, LOC + QString("XvMC Adaptor Name: '%1'")
            .arg(adaptor_name));

    xv_hue_base = calc_hue_base(adaptor_name);

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
        windows[0].SetAllowPreviewEPG(true);

        GLContextCreatedWait();
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
 * \fn VideoOutputXv::InitVDPAU(MythCodecID)
 *  Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitVDPAU(MythCodecID mcodecid)
{
    (void)mcodecid;
#ifdef USING_VDPAU
    vdpau = new VDPAUContext();

    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
    bool ok = vdpau->Init(XJ_disp, XJ_screen_num, XJ_curwin,
                          display_visible_rect.size(),
                          db_use_picture_controls, mcodecid);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "Unable to init VDPAU");
        vdpau->Deinit();
        delete vdpau;
        vdpau = NULL;
        return ok;
    }

    ok = CreateVDPAUBuffers();
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "Unable to create VDPAU buffers");
        DeleteBuffers(XVideoVDPAU, false);
        return ok;
    }
    else
    {
        VERBOSE(VB_PLAYBACK, LOC +
            QString("Created VDPAU context (%1 decode)")
            .arg((mcodecid < kCodec_NORMAL_END) ? "software" : "GPU"));
    }

    video_output_subtype = XVideoVDPAU;            
    windows[0].SetAllowPreviewEPG(false);
    return ok;
#else // USING_VDPAU
    return false;
#endif // USING_VDPAU
}

static bool has_format(XvImageFormatValues *formats, int format_cnt, int id)
{
    for (int i = 0; i < format_cnt; i++)
    {
        if ((formats[i].id == id))
            return true;
    }

    return false;
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
    const QSize video_dim = windows[0].GetVideoDim();
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

    xv_hue_base = calc_hue_base(adaptor_name);

    InstallXErrorHandler(XJ_disp);

    bool foundimageformat = false;
    int ids[] = { GUID_YV12_PLANAR, GUID_I420_PLANAR, GUID_IYUV_PLANAR, };
    int format_cnt = 0;
    XvImageFormatValues *formats;
    X11S(formats = XvListImageFormats(XJ_disp, xv_port, &format_cnt));

    for (int i = 0; i < format_cnt; i++)
    {
        char *chr = (char*) &(formats[i].id);
        VERBOSE(VB_PLAYBACK, LOC + QString("XVideo Format #%1 is '%2%3%4%5'")
                .arg(i).arg(chr[0]).arg(chr[1]).arg(chr[2]).arg(chr[3]));
    }

    for (uint i = 0; i < sizeof(ids)/sizeof(int); i++)
    {
        if (has_format(formats, format_cnt, ids[i]))
        {
            xv_chroma = ids[i];
            foundimageformat = true;
            break;
        }
    }

    // IYUV is bit identical to I420, just pretend we saw I420
    xv_chroma = (GUID_IYUV_PLANAR == xv_chroma) ? GUID_I420_PLANAR : xv_chroma;

    if (formats)
        X11S(XFree(formats));

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
    {
        video_output_subtype = XVideo;
        windows[0].SetAllowPreviewEPG(true);

        GLContextCreatedWait();
    }

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
    {
        video_output_subtype = XShm;
        windows[0].SetAllowPreviewEPG(false);
    }

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
    {
        video_output_subtype = Xlib;
        windows[0].SetAllowPreviewEPG(false);
    }

    return ok;
}

/**
 *  \return MythCodecID for the best supported codec on the main display.
 */
MythCodecID VideoOutputXv::GetBestSupportedCodec(
    uint width,       uint height,
    uint osd_width,   uint osd_height,
    uint stream_type, int xvmc_chroma,
    bool test_surface, bool force_xv)
{
    (void)width, (void)height, (void)osd_width, (void)osd_height;
    (void)stream_type, (void)xvmc_chroma, (void)test_surface;

    if (force_xv)
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

#if defined(USING_XVMC) || defined(USING_VDPAU)
    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    QString dec = vdp.GetDecoder();
    if ((dec == "libmpeg2") || (dec == "ffmpeg"))
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

    // Disable features based on environment and DB values.
    bool use_xvmc_vld = false, use_xvmc_idct = false, use_xvmc = false;
    bool use_xv = true, use_shm = true, use_opengl = true;
    bool use_vdpau = false;

    if (dec == "xvmc")
        use_xvmc_idct = use_xvmc = true;
    else if (dec == "xvmc-vld")
        use_xvmc_vld = use_xvmc = true;
    else if (dec == "vdpau")
        use_vdpau = true;

    SetFromEnv(use_xvmc_vld, use_xvmc_idct, use_xvmc, use_xv,
               use_shm, use_opengl, use_vdpau);

    // Disable features based on hardware capabilities.
    bool use_xvmc_opengl = use_xvmc;
    Display *disp = MythXOpenDisplay();
    X11L;
    int screen  = DefaultScreen(disp);
    Window root = DefaultRootWindow(disp);
    X11U;
    SetFromHW(disp, screen, root, use_xvmc, use_xv, use_shm,
              use_xvmc_opengl, use_opengl, use_vdpau,
              (MythCodecID)(kCodec_MPEG1_VDPAU + (stream_type-1)));

    MythCodecID ret = (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
#ifdef USING_XVMC
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
        XvMCSurfaceInfo info;

        ok = false;
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
    }
#endif // USING_XVMC

    X11S(XCloseDisplay(disp));

#ifdef USING_VDPAU
    if (use_vdpau)
        ret = (MythCodecID)(kCodec_MPEG1_VDPAU + (stream_type-1));
#endif // USING_VDPAU

    return ret;
#endif // defined(USING_XVMC) || defined(USING_VDPAU)
}

bool VideoOutputXv::InitOSD(const QString &osd_renderer)
{
#ifdef USING_VDPAU
    if (osd_renderer == "vdpau" && vdpau)
    {
        vdpau_use_osd = true;
        if (!vdpau->InitOSD(GetTotalOSDBounds().size()))
        {
            vdpau_use_osd = false;
            VERBOSE(VB_IMPORTANT, LOC + "Init VDPAU osd failed.");
        }
        return vdpau_use_osd;
    }
#endif
    if (osd_renderer == "opengl")
    {
        xvmc_tex = XvMCTextures::Create(
            XJ_disp, XJ_curwin, XJ_screen_num,
            windows[0].GetVideoDim(), GetTotalOSDBounds().size());

        if (xvmc_tex)
        {
            VERBOSE(VB_IMPORTANT, LOC + "XvMCTex: Init succeeded");
            xvmc_buf_attr->SetOSDNum(0); // disable XvMC blending OSD
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + "XvMCTex: Init failed");
        }

        return xvmc_tex;
    }

    if (osd_renderer == "opengl2" && gl_context)
    {
        OpenGLContextLocker ctx_lock(gl_context);
        gl_use_osd_opengl2 = true;

        gl_osdchain = new OpenGLVideo();
        if (!gl_osdchain->Init(
                gl_context, db_use_picture_controls,
                GetTotalOSDBounds().size(),
                GetTotalOSDBounds(), GetTotalVisibleRect(), 
                QRect(QPoint(0, 0), GetTotalOSDBounds().size()),
                false, GetFilters(), true))
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + 
                    "InitOSD(): Failed to create OpenGL2 OSD");
            delete gl_osdchain;
            gl_osdchain = NULL;
            gl_use_osd_opengl2 = false;
        }
        else if (gl_videochain)
        {
            gl_osdchain->SetMasterViewport(gl_videochain->GetViewPort());
        }
    }

    if (osd_renderer == "chromakey")
    {
        // TODO Make sure that we are using chroma-keying
        //      before allowing chromakey OSD rendering.

        // create chroma key osd structure if needed
        if ((32 == XJ_depth) || (24 == XJ_depth))
        {
            chroma_osd = new ChromaKeyOSD(this);
            xvmc_buf_attr->SetOSDNum(0); // disable XvMC blending OSD
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + QString(
                        "Number of bits per pixel is %1, \n\t\t\t"
                        "but we only support ARGB 32 bbp for ChromaKeyOSD.")
                    .arg(XJ_depth));
        }
        return chroma_osd;
    }

    // Other OSD's don't require initialization here...
    return true;
}

bool VideoOutputXv::CheckOSDInit(void)
{
    // Deal with the nVidia 6xxx & 7xxx cards which do
    // not support chromakeying with the latest drivers
    if (xv_colorkey || !chroma_osd)
        return true;

    VERBOSE(VB_IMPORTANT, LOC + "Ack! Disabling ChromaKey OSD"
            "\n\t\t\tWe can't use ChromaKey OSD "
            "if chromakeying is not supported!");

    // Get rid of the chromakey osd..
    delete chroma_osd;
    chroma_osd = NULL;

    if (VideoOutputSubType() >= XVideoMC)
    {
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

        xvmc_buf_attr->SetOSDNum(1);
        return false;
    }

    return true;
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

static QString toCommaList(const QStringList &list)
{
    QString ret = "";
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        ret += *it + ",";

    if (ret.length())
        return ret.left(ret.length()-1);

    return "";
}

bool VideoOutputXv::InitSetupBuffers(void)
{
    // Figure out what video renderer to use
    db_vdisp_profile->SetInput(windows[0].GetVideoDim());
    QStringList renderers = allowed_video_renderers(
        myth_codec_id, XJ_disp, XJ_screen_num, XJ_curwin);
    QString     renderer  = QString::null;

    QString tmp = db_vdisp_profile->GetVideoRenderer();
    VERBOSE(VB_PLAYBACK, LOC + "InitSetupBuffers() "
            <<QString("render: %1, allowed: %2")
            .arg(tmp).arg(toCommaList(renderers)));

    if (renderers.contains(tmp))
        renderer = tmp;
    else if (renderers.empty())
        XV_INIT_FATAL_ERROR_TEST(false, "Failed to find a video renderer");
    else
    {
        // HACK HACK HACK -- begin  See #4792 for more elegant solutions
        if ((db_vdisp_profile->GetVideoRenderer() == "xvmc-blit") &&
            (renderers.indexOf("xv-blit") > 0))
        {
            swap(renderers[0], renderers[renderers.indexOf("xv-blit")]);
        }
        else if ((db_vdisp_profile->GetVideoRenderer() == "xvmc-opengl") &&
                 (renderers.indexOf("opengl") > 0))
        {
            swap(renderers[0], renderers[renderers.indexOf("opengl")]);
        }
        // HACK HACK HACK -- end

        QString tmp;
        QStringList::const_iterator it = renderers.begin();
        for (; it != renderers.end(); ++it)
            tmp += *it + ",";

        renderer = renderers[0];

        VERBOSE(VB_IMPORTANT, LOC + QString(
                    "Desired video renderer '%1' not available.\n\t\t\t"
                    "codec '%2' makes '%3' available, using '%4' instead.")
                .arg(db_vdisp_profile->GetVideoRenderer())
                .arg(toString(myth_codec_id)).arg(tmp).arg(renderer));
        db_vdisp_profile->SetVideoRenderer(renderer);
    }

    // Create video buffers
    bool use_xv     = (renderer.left(2) == "xv");
    bool use_shm    = (renderer == "xshm");
    bool use_opengl = (renderer == "opengl");
    bool use_vdpau  = (renderer == "vdpau");
    bool ok = InitVideoBuffers(myth_codec_id, use_xv, use_shm,
                               use_opengl, use_vdpau);
    if (!ok && windows[0].GetPIPState() == kPIPOff)
    {
        use_xv     |= (bool) renderers.contains("xv-blit");
        use_shm    |= (bool) renderers.contains("xshm");
        use_opengl |= (bool) renderers.contains("opengl");
        use_vdpau  |= (bool) renderers.contains("vdpau");
        ok = InitVideoBuffers(myth_codec_id, use_xv, use_shm,
                              use_opengl, use_vdpau);
    }
    XV_INIT_FATAL_ERROR_TEST(!ok, "Failed to get any video output");

    QString osdrenderer = db_vdisp_profile->GetOSDRenderer();

    // Initialize the OSD, if we need to
    InitOSD(osdrenderer);

    // Initialize chromakeying, if we need to
    if (!xvmc_tex && video_output_subtype >= XVideo)
        InitColorKey(true);

    // Check if we can actually use the OSD we want to use...
    if (!CheckOSDInit())
    {
        ok = InitVideoBuffers(myth_codec_id, use_xv, use_shm,
                              use_opengl, use_vdpau);
        XV_INIT_FATAL_ERROR_TEST(!ok, "Failed to get any video output (nCK)");
    }

    // Initialize the picture controls if we need to..
    if (db_use_picture_controls)
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
    windows[0].SetNeedRepaint(true);

    XV_INIT_FATAL_ERROR_TEST(winid <= 0, "Invalid Window ID.");

    XJ_disp = MythXOpenDisplay();
    XV_INIT_FATAL_ERROR_TEST(!XJ_disp, "Failed to open display.");

    // HACK -- begin
    //usleep(50 * 1000);
    // HACK -- end

    // Initialize X stuff
    X11L;
    XJ_screen     = DefaultScreenOfDisplay(XJ_disp);
    XJ_screen_num = DefaultScreen(XJ_disp);
    XJ_white      = XWhitePixel(XJ_disp, XJ_screen_num);
    XJ_black      = XBlackPixel(XJ_disp, XJ_screen_num);
    XJ_curwin     = winid;
    XJ_win        = winid;
    XJ_root       = DefaultRootWindow(XJ_disp);
    X11U;

    VERBOSE(VB_PLAYBACK, LOC + "Creating XJ_gc");
    InstallXErrorHandler(XJ_disp);
    X11S(XJ_gc    = XCreateGC(XJ_disp, XJ_win, 0, NULL));
    vector<XErrorEvent> errs = UninstallXErrorHandler(XJ_disp);
    PrintXErrors(XJ_disp, errs);

    VERBOSE(VB_PLAYBACK, LOC + "XJ_screen:     '"<<XJ_screen<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_screen_num: '"<<XJ_screen_num<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_curwin:     '"<<XJ_curwin<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_win:        '"<<XJ_win<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_root:       '"<<XJ_root<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_gc:         '"<<XJ_gc<<"'");

    XV_INIT_FATAL_ERROR_TEST(errs.size(), "Failed to create GC.");

    X11L;
    XJ_depth      = DefaultDepthOfScreen(XJ_screen);

    // The letterbox color..
    XJ_letterbox_colour = XJ_black;
    Colormap cmap = XDefaultColormap(XJ_disp, XJ_screen_num);
    XColor colour, colour_exact;
    QString name = toXString(db_letterbox_colour);
    QByteArray ascii_name =  name.toAscii();
    const char *cname = ascii_name.constData();
    if (XAllocNamedColor(XJ_disp, cmap, cname, &colour, &colour_exact))
        XJ_letterbox_colour = colour.pixel;

    X11U;

    // Basic setup
    VideoOutput::Init(width, height, aspect,
                      winid, winx, winy, winw, winh,
                      embedid);

#if defined(USING_OPENGL_VIDEO) || defined(USING_OPENGL_VSYNC)
    gl_context_creator = new GLContextCreator(
        this, windows[0].GetDisplayVisibleRect(), false);
    // HACK -- begin, see #6036
    GLContextCreatedWait();
    // HACK -- end 
#endif  

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
    if (VideoOutputSubType() == XVideoVDPAU)
    {
        if (getenv("USE_VDPAU_COLORKEY"))
            vdpau_use_colorkey = true;
        VERBOSE(VB_PLAYBACK, LOC + QString("VDPAU colorkeying %1.")
            .arg(vdpau_use_colorkey ? "enabled" : "disabled"));
        return;
    }

    static const char *attr_autopaint = "XV_AUTOPAINT_COLORKEY";
    int xv_val=0;

    // handle autopaint.. Normally we try to disable it so that bob-deint 
    // doesn't actually bob up the top and bottom borders up and down...
    xv_draw_colorkey = true;
    if (xv_is_attrib_supported(XJ_disp, xv_port, attr_autopaint, &xv_val))
    {
        if (turnoffautopaint && xv_val)
        {
            xv_set_attrib(XJ_disp, xv_port, attr_autopaint, 0);
            if (!xv_get_attrib(XJ_disp, xv_port, attr_autopaint, xv_val) ||
                xv_val)
            {
                VERBOSE(VB_IMPORTANT, "Failed to disable autopaint");
                xv_draw_colorkey = false;
            }
        }
        else if (!turnoffautopaint && !xv_val)
        {
            xv_set_attrib(XJ_disp, xv_port, attr_autopaint, 1);
            if (!xv_get_attrib(XJ_disp, xv_port, attr_autopaint, xv_val) ||
                !xv_val)
            {
                VERBOSE(VB_IMPORTANT, "Failed to enable autopaint");
            }
        }
        else if (!turnoffautopaint && xv_val)
        {
            xv_draw_colorkey = false;
        }
    }

    // Check that we have a colorkey attribute and make sure it is not
    // the same color as the MythTV letterboxing (currently Black).
    // This avoids avoid bob-deint actually bobbing the borders of
    // the video up and down..
    int letterbox_color = 0;
    static const char *attr_chroma = "XV_COLORKEY";
    if (!xv_is_attrib_supported(XJ_disp, xv_port, attr_chroma, &xv_colorkey))
    {
        // set to MythTV letterbox color as a sentinel
        xv_colorkey = letterbox_color;
    }
    else if (xv_colorkey == letterbox_color)
    {
        // if it is a valid attribute and set to the letterbox color, change it
        xv_set_attrib(XJ_disp, xv_port, attr_chroma, 1);

        if (xv_get_attrib(XJ_disp, xv_port, attr_chroma, xv_val) &&
            xv_val != letterbox_color)
        {
            xv_colorkey = xv_val;
        }
    }

    if (xv_colorkey == letterbox_color)
    {
        VERBOSE(VB_PLAYBACK, LOC +
                "Chromakeying not possible with this XVideo port.");
    }
}

// documented in videooutbase.cpp
bool VideoOutputXv::SetDeinterlacingEnabled(bool enable)
{
    if (VideoOutputSubType() == XVideoVDPAU)
        return SetDeinterlacingEnabledVDPAU(enable);

    if (VideoOutputSubType() == OpenGL)
        return SetDeinterlacingEnabledOpenGL(enable);

    bool deint = VideoOutput::SetDeinterlacingEnabled(enable);
    xv_need_bobdeint_repaint = (m_deintfiltername == "bobdeint");
    return deint;
}

bool VideoOutputXv::SetupDeinterlace(bool interlaced,
                                     const QString& overridefilter)
{
    if (VideoOutputSubType() == XVideoVDPAU)
        return SetupDeinterlaceVDPAU(interlaced, overridefilter);

    if (VideoOutputSubType() == OpenGL)
        return SetupDeinterlaceOpenGL(interlaced, overridefilter);

    bool deint = VideoOutput::SetupDeinterlace(interlaced, overridefilter);
    windows[0].SetNeedRepaint(true);
    return deint;
}

bool VideoOutputXv::SetDeinterlacingEnabledVDPAU(bool enable)
{
    (void)enable;
#ifdef USING_VDPAU
    if (!vdpau)
        return false;

    if (vdpau->GetDeinterlacer() != m_deintfiltername)
            return SetupDeinterlace(enable);

    m_deinterlacing = vdpau->SetDeinterlacing(enable);
#endif // USING_VDPAU
    return m_deinterlacing;
}

bool VideoOutputXv::SetupDeinterlaceVDPAU(
    bool interlaced, const QString &overridefilter)
{
    (void)interlaced;
    (void)overridefilter;
#ifdef USING_VDPAU
    // clear any software filters
    if (m_deintFiltMan)
    {
        delete m_deintFiltMan;
        m_deintFiltMan = NULL;
    }
    if (m_deintFilter)
    {
        delete m_deintFilter;
        m_deintFilter = NULL;
    }

    if (!vdpau)
        return false;

    m_deintfiltername = db_vdisp_profile->GetFilteredDeint(overridefilter);
    if (!m_deintfiltername.contains("vdpau"))
        return false;

    vdpau->SetDeinterlacer(m_deintfiltername);
    m_deinterlacing = vdpau->SetDeinterlacing(interlaced);
#endif// USING_VDPAU
    return m_deinterlacing;
}

bool VideoOutputXv::SetDeinterlacingEnabledOpenGL(bool enable)
{
    (void) enable;

    if (!gl_videochain || !gl_context)
        return false;

    if (enable && m_deinterlacing && (OpenGL != VideoOutputSubType()))
        return m_deinterlacing;

    OpenGLContextLocker ctx_lock(gl_context);

    if (enable)
    {
        if (m_deintfiltername == "")
            return SetupDeinterlace(enable);
        if (m_deintfiltername.contains("opengl"))
        {
            if (gl_videochain->GetDeinterlacer() == "")
                return SetupDeinterlace(enable);
        }
        else if (!m_deintfiltername.contains("opengl"))
        {
            // make sure opengl deinterlacing is disabled
            gl_videochain->SetDeinterlacing(false);

            if (!m_deintFiltMan || !m_deintFilter)
                return VideoOutput::SetupDeinterlace(enable);
        }
    }

    if (gl_videochain)
        gl_videochain->SetDeinterlacing(enable);

    m_deinterlacing = enable;

    return m_deinterlacing;
}

bool VideoOutputXv::SetupDeinterlaceOpenGL(
    bool interlaced, const QString &overridefilter)
{
    (void) interlaced;
    (void) overridefilter;

    if (!gl_videochain || !gl_context)
        return false;

    OpenGLContextLocker ctx_lock(gl_context);

    m_deintfiltername = db_vdisp_profile->GetFilteredDeint(overridefilter);

    if (!m_deintfiltername.contains("opengl"))
    {
        gl_videochain->SetDeinterlacing(false);

        gl_videochain->SetSoftwareDeinterlacer(QString::null);

        VideoOutput::SetupDeinterlace(interlaced, overridefilter);
        if (m_deinterlacing)
            gl_videochain->SetSoftwareDeinterlacer(m_deintfiltername);

        return m_deinterlacing;
    }

    // clear any non opengl filters
    if (m_deintFiltMan)
    {
        delete m_deintFiltMan;
        m_deintFiltMan = NULL;
    }
    if (m_deintFilter)
    {
        delete m_deintFilter;
        m_deintFilter = NULL;
    }

    if (m_deinterlacing == interlaced && (OpenGL != VideoOutputSubType()))
        return m_deinterlacing;
    m_deinterlacing = interlaced;

    if (!gl_videochain)
        return false;

    if (m_deinterlacing && !m_deintfiltername.isEmpty()) 
    {
        if (gl_videochain->GetDeinterlacer() != m_deintfiltername)
        {
            if (!gl_videochain->AddDeinterlacer(m_deintfiltername))
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        QString("Couldn't load deinterlace filter %1")
                        .arg(m_deintfiltername));
                m_deinterlacing = false;
                m_deintfiltername = "";
            }
            else
            {
                VERBOSE(VB_PLAYBACK, LOC +
                        QString("Using deinterlace method %1")
                   .arg(m_deintfiltername));
            }
        }
    }

    gl_videochain->SetDeinterlacing(m_deinterlacing);

    return m_deinterlacing;
}

/**
 * \fn VideoOutput::ApproveDeintFilter(const QString&) const
 * Approves bobdeint filter for XVideo and XvMC surfaces,
 * rejects other filters for XvMC, and defers to
 * VideoOutput::ApproveDeintFilter(const QString&)
 * otherwise.
 *
 * \return whether current video output supports a specific filter.
 */
bool VideoOutputXv::ApproveDeintFilter(const QString &filtername) const
{
    // TODO implement bobdeint for non-Xv[MC]
    VOSType vos = VideoOutputSubType();

    if (XVideoVDPAU == vos)
        return filtername.contains("vdpau");

    if ((OpenGL == vos) && filtername.contains("opengl"))
        return true;

    if (filtername == "bobdeint" && (vos >= OpenGL) && (XVideoVDPAU != vos))
        return true;

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

bool VideoOutputXv::CreateVDPAUBuffers(void)
{
#ifdef USING_VDPAU
    if (!vdpau)
        return false;

    const QSize video_dim = windows[0].GetVideoDim();
    if (!vdpau->InitBuffers(video_dim.width(),
                            video_dim.height(),
                            NUM_VDPAU_BUFFERS))
    {
        vdpau->FreeBuffers();
        return false;
    }

    bool ok = false;

    if (myth_codec_id > kCodec_VDPAU_BEGIN &&
        myth_codec_id < kCodec_VDPAU_END)
    {
        ok = vbuffers.CreateBuffers(video_dim.width(), 
                               video_dim.height(), vdpau);
    }
    else if (myth_codec_id < kCodec_NORMAL_END)
    {
        ok = vbuffers.CreateBuffers(video_dim.width(), video_dim.height());
    }

    if (!ok)
    {
        DeleteBuffers(XVideoVDPAU, false);
        return ok;
    }

    return true;
#else
    return false;
#endif
}

bool VideoOutputXv::CreateXvMCBuffers(void)
{
#ifdef USING_XVMC
    const QSize video_dim = windows[0].GetVideoDim();
    xvmc_ctx = CreateXvMCContext(XJ_disp, xv_port,
                                 xvmc_surf_info.surface_type_id,
                                 video_dim.width(), video_dim.height());
    if (!xvmc_ctx)
        return false;

    bool surface_has_vld = (XVMC_VLD == (xvmc_surf_info.mc_type & XVMC_VLD));
    xvmc_surfs = CreateXvMCSurfaces(xvmc_buf_attr->GetMaxSurf(), 
                                    surface_has_vld);

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

vector<void*> VideoOutputXv::CreateXvMCSurfaces(uint num, bool surface_has_vld)
{
    (void)num;
    (void)surface_has_vld;

    vector<void*> surfaces;
#ifdef USING_XVMC
    uint blocks_per_macroblock = calcBPM(xvmc_chroma);
    const QSize video_dim = windows[0].GetVideoDim();
    uint num_mv_blocks   = (((video_dim.width()  + 15) / 16) *
                            ((video_dim.height() + 15) / 16));
    uint num_data_blocks = num_mv_blocks * blocks_per_macroblock;

    // need the equivalent of 5 extra surfaces for VLD decoding -- Tegue
    if (surface_has_vld)
        num += 5;

    // create needed XvMC stuff
    bool ok = true;
    for (uint i = 0; i < num; i++)
    {
        xvmc_vo_surf_t *surf = new xvmc_vo_surf_t;
        bzero(surf, sizeof(xvmc_vo_surf_t));

        X11L;

        int ret = XvMCCreateSurface(XJ_disp, xvmc_ctx, &(surf->surface));
        ok &= (Success == ret);

        if (ok && !surface_has_vld)
        {
            ret = XvMCCreateBlocks(XJ_disp, xvmc_ctx, num_data_blocks,
                                   &(surf->blocks));
            if (Success != ret)
            {
                XvMCDestroySurface(XJ_disp, &(surf->surface));
                ok = false;
            }
        }

        if (ok && !surface_has_vld)
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

    // For VLD decoding: the last 5 surface were allocated just to make 
    // sure we had enough space.  now, deallocate/destroy them. -- Tegue

    if (surface_has_vld)
    {
        VERBOSE(VB_PLAYBACK, LOC + 
                QString("VLD - Allocated %1 surfaces, "
                        "now destroying 5 of them.").arg(surfaces.size()));

        X11L;
        for (uint i = 0; i < 5; i++)
        {
            xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*)surfaces.back();
            surfaces.pop_back();
            XvMCDestroySurface(XJ_disp, &(surf->surface));
            delete surf;
        }
        X11U;
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
    const QSize video_dim = windows[0].GetVideoDim();
    VERBOSE(VB_PLAYBACK, LOC +
            QString("CreateShmImages(%1): video_dim: %2x%3")
            .arg(num).arg(video_dim.width()).arg(video_dim.height()));

    vector<unsigned char*> bufs;
    for (uint i = 0; i < num; i++)
    {
        XShmSegmentInfo *info = new XShmSegmentInfo;
        void *image = NULL;
        int size = 0;
        int desiredsize = 0;

        X11L;

        if (use_xv)
        {
            XvImage *img =
                XvShmCreateImage(XJ_disp, xv_port, xv_chroma, 0,
                                 video_dim.width(), video_dim.height(), info);
            size = img->data_size + 64;
            image = img;
            desiredsize = video_dim.width() * video_dim.height() * 3 / 2;

            if (image && size < desiredsize)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "CreateXvShmImages(): "
                        "XvShmCreateImage() failed to create image of the "
                        "requested size.");
                XFree(image);
                image = NULL;
                delete info;
            }

            if (image && (3 == img->num_planes))
            {
                XJ_shm_infos.push_back(info);
                YUVInfo tmp(img->width, img->height, img->data_size,
                            img->pitches, img->offsets);
                if (xv_chroma == GUID_YV12_PLANAR)
                {
                    swap(tmp.pitches[1], tmp.pitches[2]);
                    swap(tmp.offsets[1], tmp.offsets[2]);
                }

                XJ_yuv_infos.push_back(tmp);
            }
            else if (image)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "CreateXvShmImages(): "
                        "XvShmCreateImage() failed to create image "
                        "with the correct number of pixel planes.");
                XFree(image);
                image = NULL;
                delete info;
            }
        }
        else
        {
            const QRect display_visible_rect =
                windows[0].GetDisplayVisibleRect();
            XImage *img =
                XShmCreateImage(XJ_disp, DefaultVisual(XJ_disp, XJ_screen_num),
                                XJ_depth, ZPixmap, 0, info,
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
                delete info;
            }

            if (image)
            {
                YUVInfo tmp(img->width, img->height,
                            img->bytes_per_line * img->height, NULL, NULL);
                XJ_yuv_infos.push_back(tmp);
                XJ_shm_infos.push_back(info);
            }
        }

        X11U;

        if (image)
        {
            XJ_shm_infos[i]->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT|0777);
            if (XJ_shm_infos[i]->shmid >= 0)
            {
                XJ_shm_infos[i]->shmaddr = (char*)
                    shmat(XJ_shm_infos[i]->shmid, 0, 0);
                if (use_xv)
                    ((XvImage*)image)->data = XJ_shm_infos[i]->shmaddr;
                else
                    ((XImage*)image)->data = XJ_shm_infos[i]->shmaddr;
                xv_buffers[(unsigned char*) XJ_shm_infos[i]->shmaddr] = image;
                XJ_shm_infos[i]->readOnly = False;

                X11L;
                XShmAttach(XJ_disp, XJ_shm_infos[i]);
                XSync(XJ_disp, False); // needed for FreeBSD?
                X11U;

                // Mark for delete immediately.
                // It won't actually be removed until after we detach it.
                shmctl(XJ_shm_infos[i]->shmid, IPC_RMID, 0);

                bufs.push_back((unsigned char*) XJ_shm_infos[i]->shmaddr);
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

    const QSize video_dim = windows[0].GetVideoDim();
    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();

    if (subtype == XVideoVDPAU)
        ok = CreateVDPAUBuffers();
    else if (subtype > XVideo && xv_port >= 0)
        ok = CreateXvMCBuffers();
    else if (subtype == XVideo && xv_port >= 0)
    {
        vector<unsigned char*> bufs =
            CreateShmImages(vbuffers.allocSize(), true);

        ok = (bufs.size() >= vbuffers.allocSize()) &&
            vbuffers.CreateBuffers(video_dim.width(), video_dim.height(),
                                   bufs, XJ_yuv_infos);

        X11S(XSync(XJ_disp, False));
    }
    else if (subtype == XShm || subtype == Xlib)
    {
        if (subtype == XShm)
        {
            vector<unsigned char*> bufs = CreateShmImages(1, false);
            if (bufs.empty())
                return false;
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
    else if (subtype == OpenGL)
    {
        QSize size = gl_videochain->GetVideoSize();
        ok = vbuffers.CreateBuffers(size.width(), size.height());
    }

    if (ok)
        CreatePauseFrame(subtype);

    return ok;
}

void VideoOutputXv::DeleteBuffers(VOSType subtype, bool delete_pause_frame)
{
    (void) subtype;
    DiscardFrames(true);

#ifdef USING_VDPAU
    if (vdpau)
    {
        vdpau->FreeBuffers();
        vdpau->Deinit();
        delete vdpau;
        vdpau = NULL;
        vdpau_use_osd = false;
    }
#endif

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

    // OpenGL stuff
    if (gl_context)
        gl_context->MakeCurrent(true);

    if (gl_videochain)
    {
        delete gl_videochain;
        gl_videochain = NULL;
    }
    while (!gl_pipchains.empty())
    {
        delete *gl_pipchains.begin();
        gl_pipchains.erase(gl_pipchains.begin());
    }
    if (gl_osdchain)
    {
        delete gl_osdchain;
        gl_osdchain = NULL;
    }

    if (gl_context)
    {
        gl_context->Hide();
        gl_context->MakeCurrent(false);
    }

    gl_use_osd_opengl2 = false;
    gl_pip_ready.clear();
    gl_osd_ready = false;

    // end OpenGL stuff

    vbuffers.DeleteBuffers();

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

    for (uint i = 0; i < XJ_shm_infos.size(); i++)
    {
        X11S(XShmDetach(XJ_disp, XJ_shm_infos[i]));
        XvImage *image = (XvImage*)
            xv_buffers[(unsigned char*) XJ_shm_infos[i]->shmaddr];
        if (image)
        {
            if ((XImage*)image == (XImage*)XJ_non_xv_image)
                X11S(XDestroyImage((XImage*)XJ_non_xv_image));
            else
                X11S(XFree(image));
        }
        if (XJ_shm_infos[i]->shmaddr)
            shmdt(XJ_shm_infos[i]->shmaddr);
        if (XJ_shm_infos[i]->shmid > 0)
            shmctl(XJ_shm_infos[i]->shmid, IPC_RMID, 0);
        delete XJ_shm_infos[i];
    }
    XJ_shm_infos.clear();
    xv_buffers.clear();
    XJ_yuv_infos.clear();
    XJ_non_xv_image = NULL;

#ifdef USING_XVMC
    DeleteXvMCContext(XJ_disp, xvmc_ctx);
#endif // USING_XVMC
}

void VideoOutputXv::EmbedInWidget(int x, int y, int w, int h)
{
    QMutexLocker locker(&global_lock);

    if (!windows[0].IsEmbedding())
        VideoOutput::EmbedInWidget(x, y, w, h);
    MoveResize();
}

void VideoOutputXv::StopEmbedding(void)
{
    if (!windows[0].IsEmbedding())
        return;

    QMutexLocker locker(&global_lock);

    VideoOutput::StopEmbedding();
    MoveResize();
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
#ifdef USING_VDPAU
    if (vdpau && VideoOutputSubType() == XVideoVDPAU)
    {
        displaying = vdpau->IsBeingUsed(frame);
    }
#endif // USING_VDPAU
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
    if (VideoOutputSubType() >= XVideoMC)
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
    if (VideoOutputSubType() <= XVideoVDPAU)
    {
#ifdef USING_VDPAU
        if (vdpau && VideoOutputSubType() == XVideoVDPAU)
        {
            CheckFrameStates();
            vdpau->ClearReferenceFrames();
        }
#endif // USING_VDPAU
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

#ifdef USING_VDPAU
    if (vdpau && VideoOutputSubType() == XVideoVDPAU)
    {
        if (vbuffers.size(kVideoBuffer_used))
        {
            VideoFrame *frame = vbuffers.head(kVideoBuffer_used);
            DiscardFrame(frame);
        }
        CheckFrameStates();
        return;
    }
#endif // USING_VDPAU
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

void VideoOutputXv::PrepareFrameVDPAU(VideoFrame *frame, FrameScanType scan)
{
    (void)frame;
    (void)scan;

    bool pause = (frame == NULL);
    // select the correct still frame on certain dvds
    if (pause && vbuffers.size(kVideoBuffer_used))
        frame = vbuffers.head(kVideoBuffer_used);

    if (frame)
        framesPlayed = frame->frameNumber + 1;

#ifdef USING_VDPAU
    if (!vdpau)
        return;

    vdpau->PrepareVideo(
        frame, windows[0].GetVideoRect(), windows[0].GetDisplayVideoRect(),
    	windows[0].GetDisplayVisibleRect().size(), scan, pause);

#endif

    if (pause)
        vbuffers.SetLastShownFrameToScratch();
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
        {
            xvmc_tex->PrepareFrame(
                render->p_surface, windows[0].GetDisplayVideoRect(),scan);
        }
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

    if (vbuffers.GetScratchFrame() == frame)
        vbuffers.SetLastShownFrameToScratch();
}

void VideoOutputXv::PrepareFrameOpenGL(VideoFrame *buffer, FrameScanType t)
{
    (void) t;

    if (!gl_videochain || !gl_context)
        return;

    OpenGLContextLocker ctx_lock(gl_context);

    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    framesPlayed = buffer->frameNumber + 1;

    // TODO should cope with YUV422P, rgb24, argb32 etc
    if (buffer->codec != FMT_YV12)
        return;

    gl_videochain->SetVideoRect(windows[0].GetDisplayVideoRect(),
                                windows[0].GetVideoRect());
    gl_videochain->PrepareFrame(t, m_deinterlacing, framesPlayed, false);

    QMap<NuppelVideoPlayer*,OpenGLVideo*>::iterator it = gl_pipchains.begin();
    for (; it != gl_pipchains.end(); ++it)
    {
        if (gl_pip_ready[it.key()])
        {
            bool active = gl_pipchain_active == *it;
            (*it)->PrepareFrame(t, m_deinterlacing, framesPlayed, active);
        }
    }

    if (gl_osd_ready && gl_osdchain)
        gl_osdchain->PrepareFrame(t, m_deinterlacing, framesPlayed, false);

    gl_context->Flush(false);

    if (vbuffers.GetScratchFrame() == buffer)
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

    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();

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

    if (VideoOutputSubType() == XVideoVDPAU)
        PrepareFrameVDPAU(buffer, scan);
    else if (VideoOutputSubType() > XVideo)
        PrepareFrameXvMC(buffer, scan);
    else if (VideoOutputSubType() == XVideo)
        PrepareFrameXv(buffer);
    else if (VideoOutputSubType() == OpenGL)
        PrepareFrameOpenGL(buffer, scan);
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

    if ((kScan_Interlaced != scan) && (kScan_Intr2ndField != scan))
        return;

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

void VideoOutputXv::SetNextFrameDisplayTimeOffset(int delayus)
{
    (void)delayus;
#ifdef USING_VDPAU
    if (!vdpau)
        return;

    vdpau->SetNextFrameDisplayTimeOffset(delayus);
#endif
}

void VideoOutputXv::ShowVDPAU(FrameScanType scan)
{
    (void)scan;
#ifdef USING_VDPAU
    if (!vdpau)
        return;

    if (vdpau->IsErrored())
    {
        errored = true;
        return;
    }

    vdpau->DisplayNextFrame();
    CheckFrameStates();
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
        xvmc_tex->Show(scan);

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
    const QRect video_rect         = windows[0].GetVideoRect();
    const QRect display_video_rect = windows[0].GetDisplayVideoRect();
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

    const QRect video_rect         = windows[0].GetVideoRect();
    const QRect display_video_rect = windows[0].GetDisplayVideoRect();
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

    if ((windows[0].IsRepaintNeeded() || xv_need_bobdeint_repaint) &&
        (VideoOutputSubType() >= XVideo) && !windows[0].IsEmbedding())
    {
        DrawUnusedRects(/* don't do a sync*/false);
    }

    if (VideoOutputSubType() == XVideoVDPAU)
        ShowVDPAU(scan);
    else if (VideoOutputSubType() > XVideo)
        ShowXvMC(scan);
    else if (VideoOutputSubType() == XVideo)
        ShowXVideo(scan);
    else if (VideoOutputSubType() == OpenGL && gl_context)
        gl_context->SwapBuffers();

    X11S(XSync(XJ_disp, False));
}

void VideoOutputXv::ShowPIP(VideoFrame        *frame,
                            NuppelVideoPlayer *pipplayer,
                            PIPLocation        loc)
{
    if (VideoOutputSubType() >= XVideoMC &&
        VideoOutputSubType() <= XVideoVLD)
    {
        return;
    }

    if (VideoOutputSubType() != OpenGL &&
        VideoOutputSubType() != XVideoVDPAU)
    {
        VideoOutput::ShowPIP(frame, pipplayer, loc);
        return;
    }

    (void) frame;

    if (!pipplayer)
        return;

    int pipw, piph;
    VideoFrame *pipimage       = pipplayer->GetCurrentFrame(pipw, piph);
    const bool  pipActive      = pipplayer->IsPIPActive();
    const bool  pipVisible     = pipplayer->IsPIPVisible();
    const float pipVideoAspect = pipplayer->GetVideoAspect();
    const QSize pipVideoDim    = pipplayer->GetVideoBufferSize();
    const uint  pipVideoWidth  = pipVideoDim.width();
    const uint  pipVideoHeight = pipVideoDim.height();

    // If PiP is not initialized to values we like, silently ignore the frame.
    if ((pipVideoAspect <= 0) || !pipimage || 
        !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    if (!pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);
    QRect dvr = GetTotalVisibleRect();
 
#ifdef USING_VDPAU
    if (vdpau && VideoOutputSubType() == XVideoVDPAU)
    {
        if (vdpau_use_pip && vdpau->InitPIPLayer(dvr.size()))
            vdpau_use_pip = vdpau->ShowPIP(pipplayer, pipimage,
                                           position, pipActive);
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }
#endif // USING_VDPAU

    gl_pip_ready[pipplayer] = false;
    OpenGLVideo *gl_pipchain = gl_pipchains[pipplayer];
    if (!gl_pipchain)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Initialise PiP.");
        gl_pipchains[pipplayer] = gl_pipchain = new OpenGLVideo();
        bool success = gl_pipchain->Init(gl_context, db_use_picture_controls,
                     QSize(pipVideoWidth, pipVideoHeight),
                     dvr, position,
                     QRect(0, 0, pipVideoWidth, pipVideoHeight),
                     false, GetFilters());
        gl_pipchain->SetMasterViewport(gl_videochain->GetViewPort());
        if (!success)
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }
    }

    QSize current = gl_pipchain->GetVideoSize();
    if ((uint)current.width()  != pipVideoWidth ||
        (uint)current.height() != pipVideoHeight)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Re-initialise PiP.");
        delete gl_pipchain;
        gl_pipchains[pipplayer] = gl_pipchain = new OpenGLVideo();
        bool success = gl_pipchain->Init(
            gl_context, db_use_picture_controls,
            QSize(pipVideoWidth, pipVideoHeight),
            dvr, position,
            QRect(0, 0, pipVideoWidth, pipVideoHeight),
            false, GetFilters());

        gl_pipchain->SetMasterViewport(gl_videochain->GetViewPort());
        if (!success)
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }

    }
    gl_pipchain->SetVideoRect(position,
                              QRect(0, 0, pipVideoWidth, pipVideoHeight));
    gl_pipchain->UpdateInputFrame(pipimage);

    gl_pip_ready[pipplayer] = true;

    if (pipActive)
        gl_pipchain_active = gl_pipchain;

    pipplayer->ReleaseCurrentFrame(pipimage);
}

void VideoOutputXv::RemovePIP(NuppelVideoPlayer *pipplayer)
{
    if (VideoOutputSubType() == OpenGL)
    {
        if (!gl_pipchains.contains(pipplayer))
            return;

        OpenGLContextLocker ctx_lock(gl_context);

        OpenGLVideo *gl_pipchain = gl_pipchains[pipplayer];
        if (gl_pipchain)
            delete gl_pipchain;
        gl_pip_ready.remove(pipplayer);
        gl_pipchains.remove(pipplayer);
    }
#ifdef USING_VDPAU
    else if (vdpau && VideoOutputSubType() == XVideoVDPAU)
    {
        vdpau->DeinitPIP(pipplayer);
    }
#endif // USING_VDPAU
}

void VideoOutputXv::DrawUnusedRects(bool sync)
{
    if (VideoOutputSubType() == OpenGL)
        return;

    // boboff assumes the smallest interlaced resolution is 480 lines - 5%
    bool use_bob   = (m_deinterlacing && m_deintfiltername == "bobdeint");
    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
    const QRect display_video_rect   = windows[0].GetDisplayVideoRect();
    int boboff_raw = (int)round(((double)display_video_rect.height()) /
                                456 - 0.00001);
    int boboff     = use_bob ? boboff_raw : 0;

    if (XVideoVDPAU == VideoOutputSubType())
    {
        if (!vdpau_use_colorkey)
            return;
        X11L;
        XSetForeground(XJ_disp, XJ_gc, 0x020202);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc,
                       display_visible_rect.left(),
                       display_visible_rect.top(),
                       display_visible_rect.width(),
                       display_visible_rect.height());
        X11U;
        return;
    }

    xv_need_bobdeint_repaint |= windows[0].IsRepaintNeeded();

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

        windows[0].SetNeedRepaint(false);
        xv_need_bobdeint_repaint = false;
        return;
    }

    X11L;

    // This is used to avoid drawing the colorkey when embedding and
    // not using overlay. This is needed because we don't paint this
    // in the vertical retrace period when calling this from the EPG.
    bool clrdraw = xv_colorkey || !windows[0].IsEmbedding();

    if (xv_draw_colorkey && windows[0].IsRepaintNeeded() && clrdraw)
    {
        XSetForeground(XJ_disp, XJ_gc, xv_colorkey);
        XFillRectangle(XJ_disp, XJ_curwin, XJ_gc,
                       display_visible_rect.left(),
                       display_visible_rect.top() + boboff,
                       display_visible_rect.width(),
                       display_visible_rect.height() - 2 * boboff);
    }
    else if (xv_draw_colorkey && xv_need_bobdeint_repaint && clrdraw)
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

    windows[0].SetNeedRepaint(false);
    xv_need_bobdeint_repaint = false;

    // Set colour for masked areas
    XSetForeground(XJ_disp, XJ_gc, XJ_letterbox_colour); 

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

#ifdef USING_VDPAU
    if (VideoOutputSubType() == XVideoVDPAU)
    {
        if (!vdpau)
            return;
        vdpau->Decode(frame);
        return;
    }
#endif

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

    VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame() " + vbuffers.GetStatus());

    if ((VideoOutputSubType() <= XVideo) ||
        (VideoOutputSubType() == OpenGL) || xvmc_tex)
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
#ifdef USING_VDPAU
    else if (VideoOutputSubType() == XVideoVDPAU)
    {
        return;
    }
#endif
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

void VideoOutputXv::ProcessFrameVDPAU(VideoFrame *frame, OSD *osd,
                                      const PIPMap &pipPlayers)
{
    if (vdpau_use_osd && osd)
        DisplayOSD(frame, osd);
    ShowPIPs(frame, pipPlayers);
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
        if (!windows[0].IsEmbedding() && osd)
        {
            bool needrepaint = chroma_osd->ProcessOSD(osd);
            if (!windows[0].IsRepaintNeeded() && needrepaint)
                windows[0].SetNeedRepaint(true);
        }
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
    if (!windows[0].IsEmbedding() && osd)
        xvmc_osd = GetAvailableOSD();

    if (xvmc_osd && xvmc_osd->IsValid())
    {
        VideoFrame *osdframe = NULL;
        int ret = DisplayOSD(xvmc_osd->OSDFrame(), osd, -1,
                             xvmc_osd->GetRevision());
        OSDSurface *osdsurf = osd->GetDisplaySurface();
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

void VideoOutputXv::ProcessFrameOpenGL(VideoFrame *frame, OSD *osd,
                                       FilterChain *filterList,
                                       const PIPMap &pipPlayers)
{
    (void) osd;
    (void) filterList;
    (void) pipPlayers;

    if (!gl_videochain || !gl_context)
        return;

    OpenGLContextLocker ctx_lock(gl_context);

    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &av_pause_frame);
        pauseframe = true;
    }

    if (filterList)
        filterList->ProcessFrame(frame);

    bool safepauseframe = pauseframe && !IsBobDeint();
    if (m_deinterlacing && m_deintFilter != NULL &&
        m_deinterlaceBeforeOSD && (!pauseframe || safepauseframe))
    {
        m_deintFilter->ProcessFrame(frame);
    }
    
    if (!windows[0].IsEmbedding())
    {
        gl_pipchain_active = NULL;
        ShowPIPs(frame, pipPlayers);
        if (osd)
            DisplayOSD(frame, osd);
    }

    bool soft_bob = m_deinterlacing && (m_deintfiltername == "bobdeint");

    if (gl_videochain)
        gl_videochain->UpdateInputFrame(frame, soft_bob);
}

void VideoOutputXv::ProcessFrameMem(VideoFrame *frame, OSD *osd,
                                    FilterChain *filterList,
                                    const PIPMap &pipPlayers)
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

    bool safepauseframe = pauseframe && !IsBobDeint();
    if (!pauseframe || safepauseframe)
    {
        if (filterList)
            filterList->ProcessFrame(frame);

        if (deint_proc && m_deinterlaceBeforeOSD)
            m_deintFilter->ProcessFrame(frame);
    }

    ShowPIPs(frame, pipPlayers);

    if (osd && !windows[0].IsEmbedding())
    {
        if (!chroma_osd)
            DisplayOSD(frame, osd);
        else
        {
            QMutexLocker locker(&global_lock);
            windows[0].SetNeedRepaint(
                chroma_osd->ProcessOSD(osd) || windows[0].IsRepaintNeeded());
        }
    }

    if ((!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame);
    }

    vbuffers.UnlockFrame(frame, "ProcessFrameMem");
}

// this is documented in videooutbase.cpp
void VideoOutputXv::ProcessFrame(VideoFrame *frame, OSD *osd,
                                 FilterChain *filterList,
                                 const PIPMap &pipPlayers)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() in ProcessFrame()");
        return;
    }

    if (VideoOutputSubType() == XVideoVDPAU)
        ProcessFrameVDPAU(frame, osd, pipPlayers);
    else if (VideoOutputSubType() == OpenGL)
        ProcessFrameOpenGL(frame, osd, filterList, pipPlayers);
    else if (VideoOutputSubType() <= XVideo)
        ProcessFrameMem(frame, osd, filterList, pipPlayers);
    else
        ProcessFrameXvMC(frame, osd);
}

// this is documented in videooutbase.cpp
int VideoOutputXv::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    if (!supported_attributes)
        return -1;

    if (VideoOutputSubType() == XVideoVDPAU)
    {
#ifdef USING_VDPAU
        if (vdpau)
        {
            newValue = min(max(newValue, 0), 100);
            newValue = vdpau->SetPictureAttribute(attribute, newValue);
            if (newValue >= 0)
                SetPictureAttributeDBValue(attribute, newValue);
            return newValue;
        }
#endif // USING_VDPAU
    }

    if (VideoOutputSubType() == OpenGL && gl_context)
    {
        newValue = min(max(newValue, 0), 100);
        newValue = gl_context->SetPictureAttribute(attribute, newValue);
        if (newValue >= 0)
            SetPictureAttributeDBValue(attribute, newValue);
        return newValue;
    }

    QString attrName = toXVString(attribute);
    QByteArray ascii_attr_name =  attrName.toAscii();
    const char *cname = ascii_attr_name.constData();

    int valAdj = (kPictureAttribute_Hue == attribute) ? xv_hue_base : 0;

    if (attrName.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "\n\n\n attrName.isEmpty() \n\n\n");
        return -1;
    }

    if (0 == (toMask(attribute) & supported_attributes))
    {
        VERBOSE(VB_IMPORTANT, "\n\n\n unsupported attribute \n\n\n");
        return -1;
    }

    newValue = min(max(newValue, 0), 100);
    if (kPictureAttribute_Hue == attribute)
    {
        int oldValue = GetPictureAttribute(attribute);
        newValue = (0 == newValue && oldValue > 0 && oldValue < 5) ?
            100 : ((100 == newValue && oldValue > 95 && oldValue < 100) ?
                   0 : newValue);
    }

    int port_min = xv_attribute_min[attribute];
    int port_max = xv_attribute_max[attribute];
    int range    = port_max - port_min;

    int tmpval2 = (newValue + valAdj) % 100; 
    int tmpval3 = (int) roundf(range * 0.01f * tmpval2); 
    int value   = min(tmpval3 + port_min, port_max); 

    xv_set_attrib(XJ_disp, xv_port, cname, value);

#ifdef USING_XVMC
    // Needed for VIA XvMC to commit change immediately.
    if (video_output_subtype > XVideo)
    {
        Atom xv_atom;
        X11S(xv_atom = XInternAtom(XJ_disp, cname, False));
        if (xv_atom != None)
            X11S(XvMCSetAttribute(XJ_disp, xvmc_ctx, xv_atom, value));
    }
#endif

    SetPictureAttributeDBValue(attribute, newValue);
    return newValue;
}

void VideoOutputXv::InitPictureAttributes(void)
{
    supported_attributes = kPictureAttributeSupported_None;

    if (VideoOutputSubType() == XVideoVDPAU)
    {
#ifdef USING_VDPAU
        if (vdpau)
            supported_attributes = vdpau->GetSupportedPictureAttributes();
#endif //USING_VDPAU
    }
    else if (VideoOutputSubType() == OpenGL && gl_context)
    {
        supported_attributes = gl_context->GetSupportedPictureAttributes();
    }
    else if (VideoOutputSubType() >= XVideo)
    {
        int val, min_val, max_val;
        for (uint i = 0; i < kPictureAttribute_MAX; i++)
        {
            QString attrName = toXVString((PictureAttribute)i);
            QByteArray ascii_attr_name =  attrName.toAscii();
            const char *cname = ascii_attr_name.constData();

            if (attrName.isEmpty())
                continue;

            if (xv_is_attrib_supported(XJ_disp, xv_port, cname,
                                       &val, &min_val, &max_val))
            {
                supported_attributes = (PictureAttributeSupported)
                    (supported_attributes | toMask((PictureAttribute)i));
                xv_attribute_min[(PictureAttribute)i] = min_val;
                xv_attribute_max[(PictureAttribute)i] = max_val;
            }
        }
    }
    else
    {
        return;
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("PictureAttributes: %1")
            .arg(toString(supported_attributes)));

    VideoOutput::InitPictureAttributes();
}

void VideoOutputXv::CheckFrameStates(void)
{
#ifdef USING_VDPAU
    if (vdpau && VideoOutputSubType() == XVideoVDPAU)
    {
        frame_queue_t::iterator it;
        it = vbuffers.begin_lock(kVideoBuffer_displayed);
        while (it != vbuffers.end(kVideoBuffer_displayed))
        {
            VideoFrame* frame = *it;
            if (!vdpau->IsBeingUsed(frame))
            {
                if (vbuffers.contains(kVideoBuffer_decode, frame))
                {
                    VERBOSE(VB_PLAYBACK, LOC + QString(
                                "Frame %1 is in use by avlib and so is "
                                "being held for later discarding.")
                            .arg(DebugString(frame, true)));
                }
                else
                {
                    vbuffers.RemoveInheritence(frame);
                    vbuffers.safeEnqueue(kVideoBuffer_avail, frame);
                    vbuffers.end_lock();
                    it = vbuffers.begin_lock(kVideoBuffer_displayed);
                continue;
                }
            }
            ++it;
        }
        vbuffers.end_lock();
        return;
    }
#endif // USING_VDPAU
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
    if (!frame)
        return false;

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

QRect VideoOutputXv::GetPIPRect(PIPLocation        location,
                                NuppelVideoPlayer *pipplayer,
                                bool               do_pixel_adj) const
{
    (void)do_pixel_adj;

    if (!pipplayer || OpenGL == VideoOutputSubType() ||
        XVideoVDPAU == VideoOutputSubType() ||
        !pipplayer->UsingNullVideo())
    {
        return VideoOutput::GetPIPRect(location, pipplayer);
    }

    QRect position;
    const QSize video_disp_dim       = windows[0].GetVideoDispDim();
    const QRect video_rect           = windows[0].GetVideoRect();
    const QRect display_video_rect   = windows[0].GetDisplayVideoRect();
    const QRect display_visible_rect = windows[0].GetDisplayVisibleRect();
    float video_aspect               = windows[0].GetVideoAspect();
    if (video_aspect < 0.01f)
        video_aspect = 1.3333f;

    const float pip_size             = (float)windows[0].GetPIPSize();
    const float pipVideoAspect       = pipplayer->GetVideoAspect();

    // adjust for aspect override modes...
    int letterXadj = 0;
    int letterYadj = 0;
    float letterAdj = 1.0f;
    if (windows[0].GetAspectOverride() != kAspect_Off)
    {
        letterXadj = max(-display_video_rect.left(), 0);
        float xadj = (float)video_rect.width() / display_visible_rect.width();
        letterXadj = (int)(letterXadj * xadj);
        float yadj = (float)video_rect.height() / display_visible_rect.height();
        letterYadj = max(-display_video_rect.top(), 0);
        letterYadj = (int)(letterYadj * yadj);
        letterAdj  = windows[0].GetVideoAspect() /
            windows[0].GetOverridenVideoAspect();
    }
    // adjust for the relative aspect ratios of pip and main video
    float aspectAdj  = pipVideoAspect / video_aspect;

    int tmph = (int) ((float)video_disp_dim.height() * pip_size * 0.01f);
    int tmpw = (int) ((float)video_disp_dim.width() * pip_size * 0.01f *
                             aspectAdj * letterAdj);
    position.setWidth((tmpw >> 1) << 1);
    position.setHeight((tmph >> 1) << 1);

    int xoff = 30;
    int yoff = 40;

    switch (location)
    {
        default:
        case kPIPTopLeft:
            xoff += letterXadj;
            yoff += letterYadj;
            break;
        case kPIPBottomLeft:
            xoff += letterXadj;
            yoff = video_disp_dim.height() - position.height() - 
                yoff - letterYadj;
            break;
        case kPIPTopRight:
            xoff = video_disp_dim.width() - position.width() -
                    xoff - letterXadj;
            yoff = yoff + letterYadj;
            break;
        case kPIPBottomRight:
            xoff = video_disp_dim.width() - position.width() -
                    xoff - letterXadj;
            yoff = video_disp_dim.height() - position.height() -
                   yoff - letterYadj;
            break;
    }

    position.translate(xoff, yoff);
    return position;
}

void VideoOutputXv::ShutdownVideoResize(void)
{
    if (VideoOutputSubType() != OpenGL)
    {
        VideoOutput::ShutdownVideoResize();
        return;
    }

    if (!gl_osdchain)
    {
        VideoOutput::ShutdownVideoResize();
        return;
    }

    if (gl_videochain)
        gl_videochain->DisableVideoResize();

    vsz_enabled = false;
}

int VideoOutputXv::DisplayOSD(VideoFrame *frame, OSD *osd,
                              int stride, int revision)
{
    if (!gl_use_osd_opengl2 && !vdpau_use_osd)
        return VideoOutput::DisplayOSD(frame, osd, stride, revision);

    gl_osd_ready = false;

    if (!osd)
        return -1;

    OSDSurface *surface = osd->Display();
    if (!surface)
    {
#ifdef USING_VDPAU
        if (vdpau)
            vdpau->DisableOSD();
#endif
        return -1;
    }

    bool changed = (-1 == revision) ?
        surface->Changed() : (surface->GetRevision()!=revision);

    if (gl_use_osd_opengl2)
        gl_osd_ready = true;

    if (changed)
    {
        QSize visible = GetTotalOSDBounds().size();

        if (vdpau_use_osd)
        {
#ifdef USING_VDPAU
            if (!vdpau)
                return -1;

            void *offsets[3], *alpha[1];
            offsets[0] = surface->y;
            offsets[1] = surface->u;
            offsets[2] = surface->v;
            alpha[0] = surface->alpha;
            vdpau->UpdateOSD(offsets, visible, alpha);
#endif // USING_VDPAU
        }
        else if (gl_use_osd_opengl2)
        {
            if (!gl_osdchain)
                return -1;

            if (vsz_enabled && gl_videochain)
                gl_videochain->SetVideoResize(vsz_desired_display_rect);

            int offsets[3];
            offsets[0] = surface->y - surface->yuvbuffer;
            offsets[1] = surface->u - surface->yuvbuffer;
            offsets[2] = surface->v - surface->yuvbuffer;

            gl_osdchain->UpdateInput(surface->yuvbuffer, offsets,
				     FMT_YV12, visible, surface->alpha);
        }
    }
    return changed;
}

bool VideoOutputXv::hasFullScreenOSD(void) const
{
    return (vdpau_use_osd || gl_use_osd_opengl2 || chroma_osd);
}

QStringList VideoOutputXv::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;

    QStringList list;

    Display *disp = MythXOpenDisplay();

    if (!disp)
        return list;

    X11L;
    int screen    = DefaultScreen(disp);
    Window window = DefaultRootWindow(disp);
    X11U;

    list = allowed_video_renderers(myth_codec_id, disp, screen, window);

    XCloseDisplay(disp);

    return list;
}

static void SetFromEnv(bool &useXvVLD, bool &useXvIDCT, bool &useXvMC,
                       bool &useXVideo, bool &useShm, bool &useOpenGL,
                       bool &useVDPAU)
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
    if (getenv("NO_OPENGL"))
        useOpenGL = false;
    if (getenv("NO_VDPAU"))
        useVDPAU = false;
}

static void SetFromHW(Display *d,
                      int     screen,     Window  curwin,
                      bool    &useXvMC,   bool   &useXVideo,
                      bool    &useShm,    bool   &useXvMCOpenGL,
                      bool    &useOpenGL, bool   &useVDPAU,
                      MythCodecID vdpau_codec_id)
{
    (void)screen;
    (void)d;
    (void)curwin;
    (void) vdpau_codec_id;
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

    if (useXvMCOpenGL)
    {
        useXvMCOpenGL = false;
#ifdef USING_XVMC_OPENGL
        bool glx_1_3 = OpenGLContextGLX::IsGLXSupported(d, 1, 3);
        useXvMCOpenGL = (useXvMC && glx_1_3);
#endif // USING_XVMC_OPENGL
    }

    if (useOpenGL)
    {
        useOpenGL = false;
#ifdef USING_OPENGL_VIDEO
        useOpenGL = OpenGLContextGLX::IsGLXSupported(d, 1, 2);
#endif // USING_OPENGL_VIDEO
    }

    if (useVDPAU)
    {
        useVDPAU = false;
#ifdef USING_VDPAU
        if (vdpau_codec_id < kCodec_NORMAL_END)
        {
            useVDPAU = true;
        }
        else
        {
            useVDPAU = VDPAUContext::CheckCodecSupported(vdpau_codec_id);
        }
        if (useVDPAU)
        {
            // N.B. This only confirms another VDPAU context can be created.
            // Creating a second hardware decoder will still fail (180.25)
            // e.g. when attempting to use PBP.
            VDPAUContext *c = new VDPAUContext();
            useVDPAU = c->Init(d, screen, curwin, QSize(1920,1200),
                               false, vdpau_codec_id);
            c->Deinit();
            delete c;
        }

#endif // USING_VDPAU
    }
}

static QStringList allowed_video_renderers(
    MythCodecID myth_codec_id, Display *display, int screen, Window curwin)
{
    bool vld, idct, mc, xv, shm, xvmc_opengl, opengl, vdpau;

    myth2av_codecid(myth_codec_id, vld, idct, mc, vdpau);

    // allow vdpau rendering for software decode
    if (myth_codec_id < kCodec_NORMAL_END)
        vdpau = true;

    opengl = xv = shm = !vld && !idct;
    xvmc_opengl = vld || idct || mc;

    SetFromEnv(vld, idct, mc, xv, shm, opengl, vdpau);
    SetFromHW(display, screen, curwin, mc, xv, shm, xvmc_opengl,
              opengl, vdpau, myth_codec_id);
    idct &= mc;

    QStringList list;
    if (myth_codec_id < kCodec_NORMAL_END)
    {
        if (opengl)
            list += "opengl";
        if (xv)
            list += "xv-blit";
        if (shm)
            list += "xshm";
        if (vdpau)
            list += "vdpau";
        list += "xlib";
    }
    else if ((kCodec_VDPAU_BEGIN < myth_codec_id) && 
             (myth_codec_id < kCodec_VDPAU_END) && vdpau)
    {
        list += "vdpau";
    }
    else
    {
        if (vld || idct || mc)
            list += "xvmc-blit";
        if ((vld || idct || mc) && xvmc_opengl)
            list += "xvmc-opengl";
    }
    return list;
}

static int calc_hue_base(const QString &adaptor_name)
{
    if ((adaptor_name == "ATI Radeon Video Overlay") ||
        (adaptor_name == "XV_SWOV" /* VIA 10K & 12K */) ||
        (adaptor_name == "Savage Streams Engine" /* S3 Prosavage DDR-K */) ||
        (adaptor_name == "SIS 300/315/330 series Video Overlay"))
    {
        return 50;
    }

    return 0; //< nVidia normal
}
