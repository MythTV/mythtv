#include "mythplayer.h"

/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */

// ANSI C headers
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cerrno>

#if HAVE_MALLOC_H
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

#include "mythconfig.h"

// MythTV OSD headers
#include "yuv2rgb.h"
#include "osd.h"
#include "osdchromakey.h"

// MythTV X11 headers
#include "videoout_xv.h"
#include "mythxdisplay.h"
#include "util-xv.h"

// MythTV General headers
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "filtermanager.h"
#include "videodisplayprofile.h"
#define IGNORE_TV_PLAY_REC
#include "tv.h"
#include "fourcc.h"
#include "mythmainwindow.h"
#include "myth_imgconvert.h"
#include "mythuihelper.h"

#define LOC      QString("VideoOutputXv: ")

extern "C" {
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/Xinerama.h>
#ifndef _XSHM_H_
    extern int      XShmQueryExtension(Display*);
    extern int      XShmGetEventBase(Display*);
#endif // silences warning when these are already defined

#include "libswscale/swscale.h"
}

#if ! HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

static QStringList allowed_video_renderers(
    MythCodecID codec_id, MythXDisplay *display, Window curwin = 0);

static void SetFromEnv(bool &useXV, bool &useShm);
static void SetFromHW(MythXDisplay *d, Window curwin,
                      bool &useXV, bool &useShm);

const char *vr_str[] =
{
    "unknown", "xlib", "xshm", "xv-blit",
};

void VideoOutputXv::GetRenderOptions(render_opts &opts,
                                     QStringList &cpudeints)
{
    opts.renderers->append("xlib");
    opts.renderers->append("xshm");
    opts.renderers->append("xv-blit");

    opts.deints->insert("xlib", cpudeints);
    opts.deints->insert("xshm", cpudeints);
    opts.deints->insert("xv-blit", cpudeints);
    (*opts.deints)["xv-blit"].append("bobdeint");

    (*opts.osds)["xlib"].append("softblend");
    (*opts.osds)["xshm"].append("softblend");
    (*opts.osds)["xv-blit"].append("softblend");
    (*opts.osds)["xv-blit"].append("chromakey");

    (*opts.safe_renderers)["dummy"].append("xlib");
    (*opts.safe_renderers)["dummy"].append("xshm");
    (*opts.safe_renderers)["dummy"].append("xv-blit");
    (*opts.safe_renderers)["nuppel"].append("xlib");
    (*opts.safe_renderers)["nuppel"].append("xshm");
    (*opts.safe_renderers)["nuppel"].append("xv-blit");

    (*opts.render_group)["x11"].append("xlib");
    (*opts.render_group)["x11"].append("xshm");
    (*opts.render_group)["x11"].append("xv-blit");

    opts.priorities->insert("xlib", 20);
    opts.priorities->insert("xshm", 30);
    opts.priorities->insert("xv-blit", 90);

    if (opts.decoders->contains("ffmpeg"))
    {
        (*opts.safe_renderers)["ffmpeg"].append("xlib");
        (*opts.safe_renderers)["ffmpeg"].append("xshm");
        (*opts.safe_renderers)["ffmpeg"].append("xv-blit");
    }

    if (opts.decoders->contains("crystalhd"))
    {
        (*opts.safe_renderers)["crystalhd"].append("xlib");
        (*opts.safe_renderers)["crystalhd"].append("xshm");
        (*opts.safe_renderers)["crystalhd"].append("xv-blit");
    }
}

/** \class  VideoOutputXv
 *  \brief Supports common video output methods used with %X11 Servers.
 *
 * This class supports XVideo with color transform and scaling acceleration.
 * When this is not available, we also try to use X Shared memory, and if that
 * fails we try standard Xlib output.
 *
 * \see VideoOutput, VideoBuffers
 *
 */
VideoOutputXv::VideoOutputXv()
    : VideoOutput(),
      video_output_subtype(XVUnknown),
      global_lock(QMutex::Recursive),

      XJ_win(0), XJ_curwin(0), disp(NULL), XJ_letterbox_colour(0),
      XJ_started(false),

      XJ_non_xv_image(0), non_xv_frames_shown(0), non_xv_show_frame(1),
      non_xv_fps(0), non_xv_av_format(PIX_FMT_NB), non_xv_stop_time(0),

      xv_port(-1),      xv_hue_base(0),
      xv_colorkey(0),   xv_draw_colorkey(false),
      xv_chroma(0),     xv_set_defaults(false),
      xv_use_picture_controls(true),

      chroma_osd(NULL)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "ctor");
    memset(&av_pause_frame, 0, sizeof(av_pause_frame));

    if (gCoreContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes(true);
}

VideoOutputXv::~VideoOutputXv()
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "dtor");

    const QRect tmp_display_visible_rect =
        window.GetTmpDisplayVisibleRect();

    if (window.GetPIPState() == kPIPStandAlone &&
        !tmp_display_visible_rect.isEmpty())
    {
        window.SetDisplayVisibleRect(tmp_display_visible_rect);
    }

    if (XJ_started)
    {
        const QRect display_visible_rect = window.GetDisplayVisibleRect();
        disp->SetForeground(disp->GetBlack());
        disp->FillRectangle(XJ_curwin, display_visible_rect);
        m_deinterlacing = false;
    }

    // Delete the video buffers
    DeleteBuffers(VideoOutputSubType(), true);

    // ungrab port...
    if (xv_port >= 0 && XJ_started)
    {
        XLOCK(disp, XvStopVideo(disp->GetDisplay(), xv_port, XJ_curwin));
        UngrabXvPort(disp, xv_port);
        xv_port = -1;
    }

    if (XJ_started)
    {
        XJ_started = false;
        delete disp;
        disp = NULL;
    }
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
        window.SetNeedRepaint(true);
}

void VideoOutputXv::WindowResized(const QSize &new_size)
{
    // This is causing problems in association with xrandr/display resolution
    // switching. Disabling for 0.24 as this is the only videooutput class
    // that implements this method
    // see http://cvs.mythtv.org/trac/ticket/7408
    QMutexLocker locker(&global_lock);

    window.SetDisplayVisibleRect(QRect(QPoint(0, 0), new_size));

    const QSize display_dim = QSize(
        (monitor_dim.width()  * new_size.width()) / monitor_sz.width(),
        (monitor_dim.height() * new_size.height())/ monitor_sz.height());
    window.SetDisplayDim(display_dim);

    window.SetDisplayAspect(
        ((float)display_dim.width()) / display_dim.height());

    MoveResize();
}

// documented in videooutbase.cpp
bool VideoOutputXv::InputChanged(const QSize &input_size,
                                 float        aspect,
                                 MythCodecID  av_codec_id,
                                 void        *codec_private,
                                 bool        &aspect_only)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InputChanged(%1,%2,%3) '%4'->'%5'")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&global_lock);

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = input_size     != window.GetActualVideoDim();
    bool asp_changed = aspect         != window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        aspect_only = true;
        if (asp_changed)
        {
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private,
                              aspect_only);

    bool delete_pause_frame = cid_changed;
    DeleteBuffers(VideoOutputSubType(), delete_pause_frame);

    const QSize video_disp_dim = window.GetVideoDispDim();
    ResizeForVideo((uint) video_disp_dim.width(),
                   (uint) video_disp_dim.height());

    bool ok = true;
    if (cid_changed)
    {
        // ungrab port...
        if (xv_port >= 0)
        {
            UngrabXvPort(disp, xv_port);
            xv_port = -1;
        }

        ok = InitSetupBuffers();
    }
    else
    {
        ok = CreateBuffers(VideoOutputSubType());
    }

    InitColorKey(true);
    CreateOSD();

    MoveResize();

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + 
            "InputChanged(): Failed to recreate buffers");
        errorState = kError_Unknown;
    }

    return ok;
}

void VideoOutputXv::MoveResizeWindow(QRect new_rect)
{
    if (disp)
        disp->MoveResizeWin(XJ_win, new_rect);
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

    static const uint kFeatureNone      = 0x00;
    static const uint kFeatureChromakey = 0x01;
    static const uint kFeaturePicCtrl   = 0x02;
};

/**
 * Internal function used to release an XVideo port.
 */
void VideoOutputXv::UngrabXvPort(MythXDisplay *disp, int port)
{
    if (!disp)
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Closing XVideo port %1").arg(port));
    disp->Lock();
    restore_port_attributes(port);
    XvUngrabPort(disp->GetDisplay(), port, CurrentTime);
    del_open_xv_port(port);
    disp->Unlock();
}

/**
 * Internal function used to grab a XVideo port with the desired properties.
 *
 * \return port number if it succeeds, else -1.
 */
int VideoOutputXv::GrabSuitableXvPort(MythXDisplay* disp, Window root,
                                      MythCodecID mcodecid,
                                      uint width, uint height,
                                      bool &xvsetdefaults,
                                      QString *adaptor_name)
{
    if (adaptor_name)
        *adaptor_name = QString::null;

    // create list of requirements to check in order..
    vector<XvAttributes> req;
    req.push_back(XvAttributes(
                  "XVideo surface found on port %1",
                  XvInputMask | XvImageMask,
                  XvAttributes::kFeatureNone));

    // try to get an adapter with picture attributes
    uint end = req.size();
    for (uint i = 0; i < end; i++)
    {
        req.push_back(req[i]);
        req[i].feature_flags |=  XvAttributes::kFeaturePicCtrl;
    }

    // figure out if we want chromakeying..
    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    if (vdp.GetOSDRenderer() == "chromakey")
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
    XLOCK(disp, ret = XvQueryAdaptors(disp->GetDisplay(), root,
                                     &p_num_adaptors, &ai));
    if (Success != ret)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "XVideo supported, but no free Xv ports found."
                "\n\t\t\tYou may need to reload video driver.");
        return -1;
    }

    QString lastAdaptorName = QString::null;
    int port = -1;

    // find an Xv port
    for (uint j = 0; j < req.size(); ++j)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("@ j=%1 Looking for flag[s]: %2 %3")
                .arg(j).arg(xvflags2str(req[j].xv_flags))
                .arg(req[j].feature_flags,0,16));

        for (int i = 0; i < (int)p_num_adaptors && (port == -1); ++i)
        {
            lastAdaptorName = ai[i].name;
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Adaptor#%1: %2 has flag[s]: %3")
                    .arg(i).arg(lastAdaptorName).arg(xvflags2str(ai[i].type)));

            if ((ai[i].type & req[j].xv_flags) != req[j].xv_flags)
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    "Missing XVideo flags, rejecting.");
                continue;
            }
            else
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Has XVideo flags...");

            const XvPortID firstPort = ai[i].base_id;
            const XvPortID lastPort = ai[i].base_id + ai[i].num_ports - 1;
            XvPortID p = 0;

            if ((req[j].feature_flags & XvAttributes::kFeaturePicCtrl) &&
                (!xv_is_attrib_supported(disp, firstPort, "XV_BRIGHTNESS")))
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    "Missing XV_BRIGHTNESS, rejecting.");
                continue;
            }
            else if (req[j].feature_flags & XvAttributes::kFeaturePicCtrl)
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Has XV_BRIGHTNESS...");

            if ((req[j].feature_flags & XvAttributes::kFeatureChromakey) &&
                (!xv_is_attrib_supported(disp, firstPort, "XV_COLORKEY")))
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    "Missing XV_COLORKEY, rejecting.");
                continue;
            }
            else if (req[j].feature_flags & XvAttributes::kFeatureChromakey)
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Has XV_COLORKEY...");

            for (p = firstPort; (p <= lastPort) && (port == -1); ++p)
            {
                disp->Lock();
                ret = XvGrabPort(disp->GetDisplay(), p, CurrentTime);
                if (Success == ret)
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("Grabbed xv port %1").arg(p));
                    port = p;
                    xvsetdefaults = add_open_xv_port(disp, p);
                }
                disp->Unlock();
            }
        }

        if (port != -1)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + req[j].description.arg(port));
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("XV_SET_DEFAULTS is %1supported on this port")
                    .arg(xvsetdefaults ? "" : "not "));

            bool xv_vsync = xv_is_attrib_supported(disp, port,
                                                   "XV_SYNC_TO_VBLANK");
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("XV_SYNC_TO_VBLANK %1supported")
                    .arg(xv_vsync ? "" : "not "));
            if (xv_vsync)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("XVideo Sync to VBlank %1set")
                        .arg(xv_set_attrib(disp, port, "XV_SYNC_TO_VBLANK", 1) ?
                             "" : "NOT "));
            }

            break;
        }
    }

    if (port == -1)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "No suitable XVideo port found");

    // free list of Xv ports
    if (ai)
        XLOCK(disp, XvFreeAdaptorInfo(ai));

    if ((port != -1) && adaptor_name)
        *adaptor_name = lastAdaptorName;

    return port;
}

/**
 * Creates an extra frame for pause.
 *
 * This creates a pause frame by copies the scratch frame settings, a
 * and allocating a databuffer, so a scratch must already exist.
 *
 * \sideeffect sets av_pause_frame.
 */
void VideoOutputXv::CreatePauseFrame(VOSType subtype)
{
    if (av_pause_frame.buf)
    {
        delete [] av_pause_frame.buf;
        av_pause_frame.buf = NULL;
    }

    init(&av_pause_frame, FMT_YV12,
         new unsigned char[vbuffers.GetScratchFrame()->size + 128],
         vbuffers.GetScratchFrame()->width,
         vbuffers.GetScratchFrame()->height,
         vbuffers.GetScratchFrame()->size);

    av_pause_frame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    clear(&av_pause_frame);
}

/**
 * Creates and initializes video buffers.
 *
 * \sideeffect sets video_output_subtype if it succeeds.
 *
 * \return success or failure at creating any buffers.
 */
bool VideoOutputXv::InitVideoBuffers(bool use_xv, bool use_shm)
{
    bool done = false;

    // Create ffmpeg VideoFrames
    if (!done)
        vbuffers.Init(31, true, 1, 12, 4, 2);

    // Fall back to XVideo if there is an xv_port
    if (!done && use_xv)
        done = InitXVideo();

    // only HW accel allowed for PIP and PBP
    if (!done && window.GetPIPState() > kPIPOff)
        return done;

    // Fall back to shared memory, if we are allowed to use it
    if (!done && use_shm)
        done = InitXShm();

    // Fall back to plain old X calls
    if (!done)
        done = InitXlib();

    if (done)
        db_vdisp_profile->SetVideoRenderer(vr_str[VideoOutputSubType()]);

    return done;
}

static bool has_format(XvImageFormatValues *formats, int format_cnt, int id)
{
    for (int i = 0; i < format_cnt; i++)
    {
        if (formats[i].id == id)
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
    MythXLocker lock(disp);
    disp->StartLog();
    QString adaptor_name = QString::null;
    const QSize video_dim = window.GetVideoDim();
    xv_port = GrabSuitableXvPort(disp, disp->GetRoot(), kCodec_MPEG2,
                                 video_dim.width(), video_dim.height(),
                                 xv_set_defaults, &adaptor_name);
    if (xv_port == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Could not find suitable XVideo surface.");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("XVideo Adaptor Name: '%1'")
            .arg(adaptor_name));

    xv_hue_base = VideoOutput::CalcHueBase(adaptor_name);
    xv_use_picture_controls =
        adaptor_name != "Intel(R) Video Overlay";

    bool foundimageformat = false;
    int ids[] = { GUID_YV12_PLANAR, GUID_I420_PLANAR, GUID_IYUV_PLANAR, };
    int format_cnt = 0;
    XvImageFormatValues *formats;
    formats = XvListImageFormats(disp->GetDisplay(), xv_port, &format_cnt);

    for (int i = 0; i < format_cnt; i++)
    {
        char *chr = (char*) &(formats[i].id);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("XVideo Format #%1 is '%2%3%4%5'")
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
        XFree(formats);

    if (foundimageformat)
    {
        char *chr = (char*) &xv_chroma;
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Using XVideo Format '%1%2%3%4'")
                .arg(chr[0]).arg(chr[1]).arg(chr[2]).arg(chr[3]));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Couldn't find the proper XVideo image format.");
        UngrabXvPort(disp, xv_port);
        xv_port = -1;
    }

    bool ok = xv_port >= 0;
    if (ok)
        ok = CreateBuffers(XVideo);

    if (!disp->StopLog())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create XVideo Buffers.");
        DeleteBuffers(XVideo, false);
        UngrabXvPort(disp, xv_port);
        xv_port = -1;
        ok = false;
    }
    else
    {
        video_output_subtype = XVideo;
        window.SetAllowPreviewEPG(true);
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
    MythXLocker lock(disp);
    disp->StartLog();

    LOG(VB_GENERAL, LOG_ERR, LOC +
            "Falling back to X shared memory video output."
            "\n\t\t\t      *** May be slow ***");

    bool ok = CreateBuffers(XShm);

    if (!disp->StopLog())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate X shared memory.");
        DeleteBuffers(XShm, false);
        ok = false;
    }
    else
    {
        video_output_subtype = XShm;
        window.SetAllowPreviewEPG(false);
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
    MythXLocker lock(disp);
    disp->StartLog();

    LOG(VB_GENERAL, LOG_ERR, LOC +
            "Falling back to X11 video output over a network socket."
            "\n\t\t\t      *** May be very slow ***");

    bool ok = CreateBuffers(Xlib);

    if (!disp->StopLog())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create X buffers.");
        DeleteBuffers(Xlib, false);
        ok = false;
    }
    else
    {
        video_output_subtype = Xlib;
        window.SetAllowPreviewEPG(false);
    }

    return ok;
}

/**
 *  \return MythCodecID for the best supported codec on the main display.
 */
MythCodecID VideoOutputXv::GetBestSupportedCodec(uint stream_type)
{
    return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
}

bool VideoOutputXv::CreateOSD(void)
{
    QString osdrenderer = db_vdisp_profile->GetOSDRenderer();

    if (osdrenderer == "chromakey")
    {
        if ((xv_colorkey == (int)XJ_letterbox_colour) ||
            (video_output_subtype < XVideo))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + 
                "Disabling ChromaKeyOSD as colorkeying will not work.");
        }
        else if (!((32 == disp->GetDepth()) || (24 == disp->GetDepth())))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Number of bits per pixel is %1, \n\t\t\t"
                        "but we only support ARGB 32 bbp for ChromaKeyOSD.")
                    .arg(disp->GetDepth()));
        }
        else
        {
            chroma_osd = new ChromaKeyOSD(this);
        }
        return chroma_osd;
    }

    return true;
}

#define XV_INIT_FATAL_ERROR_TEST(test,msg) \
do { \
    if (test) \
    { \
        LOG(VB_GENERAL, LOG_ERR, LOC + msg + " Exiting playback."); \
        errorState = kError_Unknown; \
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
    db_vdisp_profile->SetInput(window.GetVideoDim());
    QStringList renderers = allowed_video_renderers(
                                video_codec_id, disp, XJ_curwin);
    QString     renderer  = QString::null;

    QString tmp = db_vdisp_profile->GetVideoRenderer();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "InitSetupBuffers() " +
        QString("render: %1, allowed: %2")
            .arg(tmp).arg(toCommaList(renderers)));

    if (renderers.contains(tmp))
        renderer = tmp;
    else if (renderers.empty())
        XV_INIT_FATAL_ERROR_TEST(false, "Failed to find a video renderer");
    else
    {
        QString tmp;
        QStringList::const_iterator it = renderers.begin();
        for (; it != renderers.end(); ++it)
            tmp += *it + ",";

        renderer = renderers[0];

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Desired video renderer '%1' not available.\n\t\t\t"
                    "codec '%2' makes '%3' available, using '%4' instead.")
                .arg(db_vdisp_profile->GetVideoRenderer())
                .arg(toString(video_codec_id)).arg(tmp).arg(renderer));
        db_vdisp_profile->SetVideoRenderer(renderer);
    }

    // Create video buffers
    bool use_xv     = (renderer.left(2) == "xv");
    bool use_shm    = (renderer == "xshm");
    bool ok = InitVideoBuffers(use_xv, use_shm);

    if (!ok && window.GetPIPState() == kPIPOff)
    {
        use_xv     |= (bool) renderers.contains("xv-blit");
        use_shm    |= (bool) renderers.contains("xshm");
        ok = InitVideoBuffers(use_xv, use_shm);
    }
    XV_INIT_FATAL_ERROR_TEST(!ok, "Failed to get any video output");

    if (xv_port && (VideoOutputSubType() >= XVideo))
        save_port_attributes(xv_port);

    // Initialize the picture controls if we need to..
    if (xv_use_picture_controls)
        InitPictureAttributes();

    return true;
}

/**
 * \fn VideoOutputXv::Init(int,int,float,WId,int,int,int,int,WId)
 * Initializes class for video output.
 *
 * \return success or failure.
 */
bool VideoOutputXv::Init(int width, int height, float aspect,
                         WId winid, const QRect &win_rect, MythCodecID codec_id)
{
    window.SetNeedRepaint(true);

    XV_INIT_FATAL_ERROR_TEST(winid <= 0, "Invalid Window ID.");

    disp = OpenMythXDisplay();
    XV_INIT_FATAL_ERROR_TEST(!disp, "Failed to open display.");

    // Initialize X stuff
    MythXLocker lock(disp);

    XJ_curwin     = winid;
    XJ_win        = winid;
    XV_INIT_FATAL_ERROR_TEST(!disp->CreateGC(XJ_win), "Failed to create GC.");

    // The letterbox color..
    XJ_letterbox_colour = disp->GetBlack();
    Colormap cmap = XDefaultColormap(disp->GetDisplay(), disp->GetScreen());
    XColor colour, colour_exact;
    QString name = toXString(db_letterbox_colour);
    QByteArray ascii_name =  name.toLatin1();
    const char *cname = ascii_name.constData();
    if (XAllocNamedColor(disp->GetDisplay(), cmap, cname, &colour, &colour_exact))
        XJ_letterbox_colour = colour.pixel;

    XJ_started = true;

    // Basic setup
    VideoOutput::Init(width, height, aspect,winid, win_rect,codec_id);

    // Set resolution/measurements (check XRandR, Xinerama, config settings)
    InitDisplayMeasurements(width, height, true);

    if (!InitSetupBuffers())
        return false;

    InitColorKey(true);
    CreateOSD();

    MoveResize();

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
    if (video_output_subtype < XVideo)
        return;

    static const char *attr_autopaint = "XV_AUTOPAINT_COLORKEY";
    int xv_val=0;

    // handle autopaint.. Normally we try to disable it so that bob-deint
    // doesn't actually bob up the top and bottom borders up and down...
    xv_draw_colorkey = true;
    if (xv_is_attrib_supported(disp, xv_port, attr_autopaint, &xv_val))
    {
        if (turnoffautopaint && xv_val)
        {
            xv_set_attrib(disp, xv_port, attr_autopaint, 0);
            if (!xv_get_attrib(disp, xv_port, attr_autopaint, xv_val) ||
                xv_val)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to disable autopaint");
                xv_draw_colorkey = false;
            }
        }
        else if (!turnoffautopaint && !xv_val)
        {
            xv_set_attrib(disp, xv_port, attr_autopaint, 1);
            if (!xv_get_attrib(disp, xv_port, attr_autopaint, xv_val) ||
                !xv_val)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to enable autopaint");
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
    int letterbox_color = XJ_letterbox_colour;
    static const char *attr_chroma = "XV_COLORKEY";
    if (!xv_is_attrib_supported(disp, xv_port, attr_chroma, &xv_colorkey))
    {
        // set to MythTV letterbox color as a sentinel
        xv_colorkey = letterbox_color;
    }
    else if (xv_colorkey == letterbox_color)
    {
        // if it is a valid attribute and set to the letterbox color, change it
        xv_set_attrib(disp, xv_port, attr_chroma, 1);

        if (xv_get_attrib(disp, xv_port, attr_chroma, xv_val) &&
            xv_val != letterbox_color)
        {
            xv_colorkey = xv_val;
        }
    }

    if (xv_colorkey == letterbox_color)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            "Chromakeying not possible with this XVideo port.");
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
    bool deint = VideoOutput::SetupDeinterlace(interlaced, overridefilter);
    window.SetNeedRepaint(true);
    return deint;
}

/**
 * \fn VideoOutput::ApproveDeintFilter(const QString&) const
 * Approves bobdeint filter for XVideo and otherwise defers to
 * VideoOutput::ApproveDeintFilter(const QString&).
 *
 * \return whether current video output supports a specific filter.
 */
bool VideoOutputXv::ApproveDeintFilter(const QString &filtername) const
{
    // TODO implement bobdeint for non-Xv
    VOSType vos = VideoOutputSubType();

    if (filtername == "bobdeint" && (vos >= XVideo))
        return true;

    return VideoOutput::ApproveDeintFilter(filtername);
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
    const QSize video_dim = window.GetVideoDim();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("CreateShmImages(%1): video_dim: %2x%3")
            .arg(num).arg(video_dim.width()).arg(video_dim.height()));

    MythXLocker lock(disp);
    Display *d = disp->GetDisplay();
    vector<unsigned char*> bufs;
    for (uint i = 0; i < num; i++)
    {
        XShmSegmentInfo *info = new XShmSegmentInfo;
        void *image = NULL;
        int size = 0;
        int desiredsize = 0;

        if (use_xv)
        {
            XvImage *img =
                XvShmCreateImage(d, xv_port, xv_chroma, 0,
                                 video_dim.width(), video_dim.height(), info);
            size = img->data_size + 64;
            image = img;
            desiredsize = video_dim.width() * video_dim.height() * 3 / 2;

            if (image && size < desiredsize)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "CreateXvShmImages(): "
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
                LOG(VB_GENERAL, LOG_ERR, LOC + "CreateXvShmImages(): "
                        "XvShmCreateImage() failed to create image "
                        "with the correct number of pixel planes.");
                XFree(image);
                image = NULL;
                delete info;
            }
        }
        else
        {
            int width  = window.GetDisplayVisibleRect().width()  & ~0x1;
            int height = window.GetDisplayVisibleRect().height() & ~0x1;
            XImage *img =
                XShmCreateImage(d, DefaultVisual(d, disp->GetScreen()),
                                disp->GetDepth(), ZPixmap, 0, info,
                                width, height);
            size = img->bytes_per_line * img->height + 64;
            image = img;
            desiredsize = (width * height * 3 / 2);
            if (image && size < desiredsize)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "CreateXvShmImages(): "
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

                XShmAttach(d, XJ_shm_infos[i]);
                disp->Sync(); // needed for FreeBSD?

                // Mark for delete immediately.
                // It won't actually be removed until after we detach it.
                shmctl(XJ_shm_infos[i]->shmid, IPC_RMID, 0);

                bufs.push_back((unsigned char*) XJ_shm_infos[i]->shmaddr);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        "CreateXvShmImages(): shmget() failed." + ENO);
                break;
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "CreateXvShmImages(): "
                    "XvShmCreateImage() failed to create image.");
            break;
        }
    }
    return bufs;
}

bool VideoOutputXv::CreateBuffers(VOSType subtype)
{
    bool ok = false;

    const QSize video_dim = window.GetVideoDim();
    const QRect display_visible_rect = window.GetDisplayVisibleRect();

    if (subtype == XVideo && xv_port >= 0)
    {
        vector<unsigned char*> bufs =
            CreateShmImages(vbuffers.Size(), true);

        ok = (bufs.size() >= vbuffers.Size()) &&
            vbuffers.CreateBuffers(FMT_YV12,
                                   video_dim.width(), video_dim.height(),
                                   bufs, XJ_yuv_infos);

        disp->Sync();
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
            MythXLocker lock(disp);
            Display *d = disp->GetDisplay();
            int bytes_per_line;
            int scrn = disp->GetScreen();
            Visual *visual = DefaultVisual(d, scrn);
            XJ_non_xv_image = XCreateImage(d, visual, disp->GetDepth(),
                                           ZPixmap, /*offset*/0, /*data*/0,
                                           display_visible_rect.width()  & ~0x1,
                                           display_visible_rect.height() & ~0x1,
                                           /*bitmap_pad*/8, 0);

            if (!XJ_non_xv_image)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "XCreateImage failed: " +
                    QString("XJ_disp(0x%1) visual(0x%2) \n")
                        .arg((uint64_t)d,0,16).arg((uint64_t)visual,0,16) +
                    QString("                        ") +
                    QString("depth(%1) ").arg(disp->GetDepth()) +
                    QString("WxH(%1""x%2) ")
                        .arg(display_visible_rect.width())
                        .arg(display_visible_rect.height()));
                return false;
            }
            bytes_per_line = XJ_non_xv_image->bytes_per_line;
            XJ_non_xv_image->data = (char*) malloc(
                bytes_per_line * display_visible_rect.height());
        }

        switch (XJ_non_xv_image->bits_per_pixel)
        {   // only allow these three output formats for non-xv videout
            case 16: non_xv_av_format = PIX_FMT_RGB565; break;
            case 24: non_xv_av_format = PIX_FMT_RGB24;  break;
            case 32: non_xv_av_format = PIX_FMT_RGB32; break;
            default: non_xv_av_format = PIX_FMT_NB;
        }
        if (PIX_FMT_NB == non_xv_av_format)
        {
            QString msg = QString(
                "Non XVideo modes only support displays with 16,\n\t\t\t"
                "24, or 32 bits per pixel. But you have a %1 bpp display.")
                .arg(disp->GetDepth()*8);

            LOG(VB_GENERAL, LOG_ERR, LOC + msg);
        }
        else
            ok = vbuffers.CreateBuffers(FMT_YV12,
                                        video_dim.width(), video_dim.height());
    }

    if (ok)
        CreatePauseFrame(subtype);

    return ok;
}

void VideoOutputXv::DeleteBuffers(VOSType subtype, bool delete_pause_frame)
{
    (void) subtype;
    DiscardFrames(true);

    if (chroma_osd)
    {
        delete chroma_osd;
        chroma_osd = NULL;
    }

    Display *d = disp->GetDisplay();

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
        MythXLocker lock(disp);
        XShmDetach(d, XJ_shm_infos[i]);
        XvImage *image = (XvImage*)
            xv_buffers[(unsigned char*) XJ_shm_infos[i]->shmaddr];
        if (image)
        {
            if ((XImage*)image == (XImage*)XJ_non_xv_image)
                XDestroyImage((XImage*)XJ_non_xv_image);
            else
                XFree(image);
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
}

void VideoOutputXv::EmbedInWidget(const QRect &rect)
{
    QMutexLocker locker(&global_lock);

    if (!window.IsEmbedding())
        VideoOutput::EmbedInWidget(rect);
    MoveResize();
}

void VideoOutputXv::StopEmbedding(void)
{
    if (!window.IsEmbedding())
        return;

    QMutexLocker locker(&global_lock);

    VideoOutput::StopEmbedding();
    MoveResize();
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
    if (!frame)
        return;

    vbuffers.DiscardFrame(frame);
}

void VideoOutputXv::ClearAfterSeek(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "ClearAfterSeek()");
    DiscardFrames(false);
}

void VideoOutputXv::DiscardFrames(bool next_frame_keyframe)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("DiscardFrames(%1)").arg(next_frame_keyframe));
    if (VideoOutputSubType() <= XVideo)
    {
        vbuffers.DiscardFrames(next_frame_keyframe);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("DiscardFrames() 3: %1 -- done()")
                .arg(vbuffers.GetStatus()));
        return;
    }
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

    global_lock.lock();
    framesPlayed = frame->frameNumber + 1;
    global_lock.unlock();

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

    framesPlayed = buffer->frameNumber + 1;
    int width = buffer->width;
    int height = buffer->height;

    // bad way to throttle frame display for non-Xv mode.
    // calculate fps we can do and skip enough frames so we don't exceed.
    if (non_xv_frames_shown == 0)
        non_xv_stop_time = time(NULL) + 4;

    const QRect display_visible_rect = window.GetDisplayVisibleRect();

    if ((!non_xv_fps) && (time(NULL) > non_xv_stop_time))
    {
        non_xv_fps = (int)(non_xv_frames_shown / 4);

        if (non_xv_fps < 25)
        {
            non_xv_show_frame = 120 / (non_xv_frames_shown + 1);
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("\n***\n"
                    "* Your system is not capable of displaying the\n"
                    "* full framerate at %1""x%2 resolution.  Frames\n"
                    "* will be skipped in order to keep the audio and\n"
                    "* video in sync.\n").arg(display_visible_rect.width())
                    .arg(display_visible_rect.height()));
        }
    }

    non_xv_frames_shown++;

    if ((non_xv_show_frame != 1) && (non_xv_frames_shown % non_xv_show_frame))
        return;

    if (!XJ_non_xv_image)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "XJ_non_xv_image == NULL");
        return;
    }

    int out_width  = display_visible_rect.width()  & ~0x1;
    int out_height = display_visible_rect.height() & ~0x1;
    unsigned char *sbuf = new unsigned char[out_width * out_height * 3 / 2];
    AVPicture image_in, image_out;
    static struct SwsContext  *scontext;

    avpicture_fill(&image_out, (uint8_t *)sbuf, PIX_FMT_YUV420P,
                   out_width, out_height);

    if ((out_width  == width) &&
        (out_height == height))
    {
        memcpy(sbuf, buffer->buf, width * height * 3 / 2);
    }
    else
    {
        avpicture_fill(&image_in, buffer->buf, PIX_FMT_YUV420P,
                       width, height);
        scontext = sws_getCachedContext(scontext, width, height,
                       PIX_FMT_YUV420P, out_width,
                       out_height, PIX_FMT_YUV420P,
                       SWS_FAST_BILINEAR, NULL, NULL, NULL);

        sws_scale(scontext, image_in.data, image_in.linesize, 0, height,
                  image_out.data, image_out.linesize);
    }

    avpicture_fill(&image_in, (uint8_t *)XJ_non_xv_image->data,
                   non_xv_av_format, out_width, out_height);


    // XXX TODO: join with the scaling after removing img_convert, img_resample
    myth_sws_img_convert(
        &image_in, non_xv_av_format, &image_out, PIX_FMT_YUV420P,
         out_width, out_height);

    {
        QMutexLocker locker(&global_lock);
        disp->Lock();
        if (XShm == video_output_subtype)
            XShmPutImage(disp->GetDisplay(), XJ_curwin, disp->GetGC(), XJ_non_xv_image,
                         0, 0, 0, 0, out_width, out_height, False);
        else
            XPutImage(disp->GetDisplay(), XJ_curwin, disp->GetGC(), XJ_non_xv_image,
                      0, 0, 0, 0, out_width, out_height);
        disp->Unlock();
    }

    delete [] sbuf;
}

// this is documented in videooutbase.cpp
void VideoOutputXv::PrepareFrame(VideoFrame *buffer, FrameScanType scan,
                                 OSD *osd)
{
    (void)osd;
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "IsErrored() in PrepareFrame()");
        return;
    }

    if (VideoOutputSubType() == XVideo)
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
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("####### Field %1 #######").arg(field));
        LOG(VB_GENERAL, LOG_DEBUG, QString("         src_y: %1").arg(src_y));
        LOG(VB_GENERAL, LOG_DEBUG, QString("        dest_y: %1").arg(dest_y));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString(" xv_src_y_incr: %1").arg(xv_src_y_incr));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("xv_dest_y_incr: %1").arg(xv_dest_y_incr));
        LOG(VB_GENERAL, LOG_DEBUG, QString("      disphoff: %1").arg(disphoff));
        LOG(VB_GENERAL, LOG_DEBUG, QString("          imgh: %1").arg(imgh));
        LOG(VB_GENERAL, LOG_DEBUG, QString("           mod: %1").arg(mod));
    }
    last_dest_y_field[field] = dest_y;
#endif
}

void VideoOutputXv::ShowXVideo(FrameScanType scan)
{
    VideoFrame *frame = GetLastShownFrame();

    XvImage *image = (XvImage*) xv_buffers[frame->buf];
    if (!image)
        return;

    const QRect video_rect         = window.GetVideoRect();
    const QRect display_video_rect = (vsz_enabled && chroma_osd) ?
                                      vsz_desired_display_rect :
                                      window.GetDisplayVideoRect();
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

    {
        QMutexLocker locker(&global_lock);
        int video_height = (3 != field) ?
            (video_rect.height()/2) : video_rect.height();
        disp->Lock();
        XvShmPutImage(disp->GetDisplay(), xv_port, XJ_curwin,
                      disp->GetGC(), image,
                      video_rect.left(), src_y,
                      video_rect.width(), video_height,
                      display_video_rect.left(), dest_y,
                      display_video_rect.width(),
                      display_video_rect.height(), False);
        disp->Unlock();
    }
}

// this is documented in videooutbase.cpp
void VideoOutputXv::Show(FrameScanType scan)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "IsErrored() is true in Show()");
        return;
    }

    if ((window.IsRepaintNeeded() || xv_need_bobdeint_repaint) &&
         VideoOutputSubType() >= XVideo)
    {
        DrawUnusedRects(/* don't do a sync*/false);
    }

    if (VideoOutputSubType() == XVideo)
        ShowXVideo(scan);

    disp->Sync();
}

void VideoOutputXv::DrawUnusedRects(bool sync)
{
    // boboff assumes the smallest interlaced resolution is 480 lines - 5%
    bool use_bob   = (m_deinterlacing && m_deintfiltername == "bobdeint");
    const QRect display_visible_rect = window.GetDisplayVisibleRect();
    const QRect display_video_rect   = window.GetDisplayVideoRect();
    int boboff_raw = (int)round(((double)display_video_rect.height()) /
                                456 - 0.00001);
    int boboff     = use_bob ? boboff_raw : 0;

    xv_need_bobdeint_repaint |= window.IsRepaintNeeded();

    Display *d = disp->GetDisplay();
    if (chroma_osd && chroma_osd->GetImage() && xv_need_bobdeint_repaint)
    {
        XLOCK(disp, XShmPutImage(d, XJ_curwin, disp->GetGC(),
                                 chroma_osd->GetImage(), 0, 0, 0, 0,
                                 display_visible_rect.width(),
                                 display_visible_rect.height(), False));
        if (sync)
            disp->Sync();

        window.SetNeedRepaint(false);
        xv_need_bobdeint_repaint = false;
        return;
    }

    if (xv_draw_colorkey && window.IsRepaintNeeded())
    {
        disp->SetForeground(xv_colorkey);
        disp->FillRectangle(XJ_curwin,
                 QRect(display_visible_rect.left(),
                       display_visible_rect.top() + boboff,
                       display_visible_rect.width(),
                       display_visible_rect.height() - 2 * boboff));
    }
    else if (xv_draw_colorkey && xv_need_bobdeint_repaint)
    {
        // if this is only for deinterlacing mode switching, draw
        // the border areas, presumably the main image is undamaged.
        disp->SetForeground(xv_colorkey);
        disp->FillRectangle(XJ_curwin,
                 QRect(display_visible_rect.left(),
                       display_visible_rect.top(),
                       display_visible_rect.width(),
                       boboff_raw));
        disp->FillRectangle(XJ_curwin,
                 QRect(display_visible_rect.left(),
                       display_visible_rect.height() - 2 * boboff_raw,
                       display_visible_rect.width(),
                       display_visible_rect.height()));
    }

    window.SetNeedRepaint(false);
    xv_need_bobdeint_repaint = false;

    // Set colour for masked areas
    disp->SetForeground(XJ_letterbox_colour);

    if (display_video_rect.left() > display_visible_rect.left())
    { // left
        disp->FillRectangle(XJ_curwin,
                 QRect(display_visible_rect.left(),
                       display_visible_rect.top(),
                       display_video_rect.left() - display_visible_rect.left(),
                       display_visible_rect.height()));
    }
    if (display_video_rect.left() + display_video_rect.width() <
        display_visible_rect.left() + display_visible_rect.width())
    { // right
        disp->FillRectangle(XJ_curwin,
                 QRect(display_video_rect.left() + display_video_rect.width(),
                       display_visible_rect.top(),
                       (display_visible_rect.left() +
                        display_visible_rect.width()) -
                       (display_video_rect.left() +
                        display_video_rect.width()),
                       display_visible_rect.height()));
    }
    if (display_video_rect.top() + boboff > display_visible_rect.top())
    { // top of screen
        disp->FillRectangle(XJ_curwin,
                 QRect(display_visible_rect.left(),
                       display_visible_rect.top(),
                       display_visible_rect.width(),
                       display_video_rect.top() + boboff -
                       display_visible_rect.top()));
    }
    if (display_video_rect.top() + display_video_rect.height() <
        display_visible_rect.top() + display_visible_rect.height())
    { // bottom of screen
        disp->FillRectangle(XJ_curwin,
                 QRect(display_visible_rect.left(),
                       display_video_rect.top() + display_video_rect.height(),
                       display_visible_rect.width(),
                      (display_visible_rect.top() +
                       display_visible_rect.height()) -
                      (display_video_rect.top() +
                       display_video_rect.height())));
    }

    if (sync)
        disp->Sync();
}

// documented in videooutbase.cpp
void VideoOutputXv::VideoAspectRatioChanged(float aspect)
{
    QMutexLocker locker(&global_lock);
    VideoOutput::VideoAspectRatioChanged(aspect);
}

void VideoOutputXv::UpdatePauseFrame(int64_t &disp_timecode)
{
    QMutexLocker locker(&global_lock);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "UpdatePauseFrame() " +
            vbuffers.GetStatus());

    if (VideoOutputSubType() <= XVideo)
    {
        // Try used frame first, then fall back to scratch frame.
        vbuffers.begin_lock(kVideoBuffer_used);

        VideoFrame *used_frame = NULL;
        if (vbuffers.Size(kVideoBuffer_used) > 0)
            used_frame = vbuffers.Head(kVideoBuffer_used);

        if (used_frame)
            CopyFrame(&av_pause_frame, used_frame);

        vbuffers.end_lock();

        if (!used_frame)
        {
            vbuffers.GetScratchFrame()->frameNumber = framesPlayed - 1;
            CopyFrame(&av_pause_frame, vbuffers.GetScratchFrame());
        }

        disp_timecode = av_pause_frame.disp_timecode;
    }
}

void VideoOutputXv::ProcessFrame(VideoFrame *frame, OSD *osd,
                                 FilterChain *filterList,
                                 const PIPMap &pipPlayers,
                                 FrameScanType scan)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "IsErrored() in ProcessFrame()");
        return;
    }

    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL);
    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        vector<const VideoFrame*> locks;
        locks.push_back(frame);
        locks.push_back(&av_pause_frame);
        CopyFrame(frame, &av_pause_frame);
        pauseframe = true;
    }

    bool safepauseframe = pauseframe && !IsBobDeint();
    if (!pauseframe || safepauseframe)
    {
        if (filterList)
            filterList->ProcessFrame(frame);

        if (deint_proc && m_deinterlaceBeforeOSD)
            m_deintFilter->ProcessFrame(frame, scan);
    }

    ShowPIPs(frame, pipPlayers);

    if (osd && !window.IsEmbedding())
    {
        if (!chroma_osd)
            DisplayOSD(frame, osd);
        else
        {
            QMutexLocker locker(&global_lock);
            window.SetNeedRepaint(
                chroma_osd->ProcessOSD(osd) || window.IsRepaintNeeded());
        }
    }

    if ((!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }
}

// this is documented in videooutbase.cpp
int VideoOutputXv::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    newValue = videoColourSpace.SetPictureAttribute(attribute, newValue);
    if (newValue >= 0)
        newValue = SetXVPictureAttribute(attribute, newValue);
    return newValue;
}

int VideoOutputXv::SetXVPictureAttribute(PictureAttribute attribute, int newValue)
{
    QString attrName = toXVString(attribute);
    QByteArray ascii_attr_name =  attrName.toLatin1();
    const char *cname = ascii_attr_name.constData();

    if (attrName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "\n\n\n attrName.isEmpty() \n\n\n");
        return -1;
    }

    int port_min = xv_attribute_min[attribute];
    int port_max = xv_attribute_max[attribute];
    int port_def = xv_attribute_def[attribute];
    int range    = port_max - port_min;

    int valAdj = (kPictureAttribute_Hue == attribute) ? xv_hue_base : 0;

    if (xv_set_defaults && range && (kPictureAttribute_Hue == attribute))
    {
        float tmp = (((float)(port_def - port_min) / (float)range) * 100.0);
        valAdj = (int)(tmp + 0.5);
    }

    int tmpval2 = (newValue + valAdj) % 100;
    int tmpval3 = (int) roundf(range * 0.01f * tmpval2);
    int value   = min(tmpval3 + port_min, port_max);

    xv_set_attrib(disp, xv_port, cname, value);

    return newValue;
}

void VideoOutputXv::InitPictureAttributes(void)
{
    PictureAttributeSupported supported = kPictureAttributeSupported_None;

    if (VideoOutputSubType() >= XVideo)
    {
        if (xv_set_defaults)
        {
            QByteArray ascii_name = "XV_SET_DEFAULTS";
            const char *name = ascii_name.constData();
            xv_set_attrib(disp, xv_port, name, 0);
        }

        int val, min_val, max_val;
        for (uint i = 0; i < kPictureAttribute_MAX; i++)
        {
            QString attrName = toXVString((PictureAttribute)i);
            QByteArray ascii_attr_name =  attrName.toLatin1();
            const char *cname = ascii_attr_name.constData();

            if (attrName.isEmpty())
                continue;

            if (xv_is_attrib_supported(disp, xv_port, cname,
                                       &val, &min_val, &max_val))
            {
                supported = (PictureAttributeSupported)
                    (supported | toMask((PictureAttribute)i));
                xv_attribute_min[(PictureAttribute)i] = min_val;
                xv_attribute_max[(PictureAttribute)i] = max_val;
                xv_attribute_def[(PictureAttribute)i] = val;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1: %2:%3:%4")
                    .arg(cname).arg(min_val).arg(val).arg(max_val));
                SetXVPictureAttribute((PictureAttribute)i,
                    videoColourSpace.GetPictureAttribute((PictureAttribute)i));
            }
        }

        videoColourSpace.SetSupportedAttributes(supported);
    }
}

QRect VideoOutputXv::GetPIPRect(PIPLocation  location,
                                MythPlayer  *pipplayer,
                                bool         do_pixel_adj) const
{
    (void)do_pixel_adj;

    if (!pipplayer || !pipplayer->UsingNullVideo())
        return VideoOutput::GetPIPRect(location, pipplayer);

    QRect position;
    const QSize video_disp_dim       = window.GetVideoDispDim();
    const QRect video_rect           = window.GetVideoRect();
    const QRect display_video_rect   = window.GetDisplayVideoRect();
    const QRect display_visible_rect = window.GetDisplayVisibleRect();
    float video_aspect               = window.GetVideoAspect();
    if (video_aspect < 0.01f)
        video_aspect = 1.3333f;

    const float pip_size             = (float)window.GetPIPSize();
    const float pipVideoAspect       = pipplayer->GetVideoAspect();

    // adjust for aspect override modes...
    int letterXadj = 0;
    int letterYadj = 0;
    float letterAdj = 1.0f;
    if (window.GetAspectOverride() != kAspect_Off)
    {
        letterXadj = max(-display_video_rect.left(), 0);
        float xadj = (float)video_rect.width() / display_visible_rect.width();
        letterXadj = (int)(letterXadj * xadj);
        float yadj = (float)video_rect.height() / display_visible_rect.height();
        letterYadj = max(-display_video_rect.top(), 0);
        letterYadj = (int)(letterYadj * yadj);
        letterAdj  = window.GetVideoAspect() /
            window.GetOverridenVideoAspect();
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

QStringList VideoOutputXv::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;

    QStringList list;

    MythXDisplay *disp = OpenMythXDisplay();

    if (!disp)
        return list;

    list = allowed_video_renderers(myth_codec_id, disp);

    delete disp;
    return list;
}

MythPainter* VideoOutputXv::GetOSDPainter(void)
{
    if (chroma_osd)
        return chroma_osd->GetPainter();

    return VideoOutput::GetOSDPainter();
}

static void SetFromEnv(bool &useXVideo, bool &useShm)
{
    // can be used to force non-Xv mode as well as non-Xv/non-Shm mode
    if (getenv("NO_XV"))
        useXVideo = false;
    if (getenv("NO_SHM"))
        useXVideo = useShm = false;
}

static void SetFromHW(MythXDisplay *d,    Window  curwin,
                      bool   &useXVideo,  bool    &useShm)
{
    (void)d;
    (void)curwin;

    if (!d)
        return;

    MythXLocker lock(d);

    // find out about XVideo support
    if (useXVideo)
    {
        uint p_ver, p_rel, p_req, p_event, p_err, ret;
        ret = XvQueryExtension(d->GetDisplay(), &p_ver, &p_rel,
                                    &p_req, &p_event, &p_err);
        if (Success != ret)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "XVideo output requested, but is not supported by display.");
            useXVideo = false;
        }
    }

    if (useShm)
    {
        const char *dispname = DisplayString(d->GetDisplay());
        if ((dispname) && (*dispname == ':'))
            useShm = (bool) XShmQueryExtension(d->GetDisplay());
    }
}

static QStringList allowed_video_renderers(
    MythCodecID myth_codec_id, MythXDisplay *display, Window curwin)
{
    if (!curwin)
        curwin = display->GetRoot();

    bool xv  = true;
    bool shm = true;

    myth2av_codecid(myth_codec_id);

    SetFromEnv(xv, shm);
    SetFromHW(display, curwin, xv, shm);

    QStringList list;
    if (codec_is_std(myth_codec_id))
    {
        if (xv)
            list += "xv-blit";
        if (shm)
            list += "xshm";
        list += "xlib";
    }

    return list;
}
