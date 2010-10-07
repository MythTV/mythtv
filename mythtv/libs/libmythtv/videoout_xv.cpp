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
#include "osdxvmc.h"
#include "osdchromakey.h"

// MythTV X11 headers
#include "videoout_xv.h"
#include "mythxdisplay.h"
#include "util-xv.h"
#include "util-xvmc.h"

// MythTV General headers
#include "mythcorecontext.h"
#include "mythverbose.h"
#include "filtermanager.h"
#include "videodisplayprofile.h"
#define IGNORE_TV_PLAY_REC
#include "tv.h"
#include "fourcc.h"
#include "mythmainwindow.h"
#include "myth_imgconvert.h"
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

#include "libswscale/swscale.h"
}

#if ! HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

#ifndef XVMC_CHROMA_FORMAT_420
#define XVMC_CHROMA_FORMAT_420 0x00000001
#endif

static QStringList allowed_video_renderers(
    MythCodecID codec_id, MythXDisplay *display, Window curwin = 0);

static void SetFromEnv(bool &useXvVLD, bool &useXvIDCT, bool &useXvMC,
                       bool &useXV, bool &useShm);
static void SetFromHW(MythXDisplay *d, Window curwin,
                      bool &useXvMC, bool &useXV, bool &useShm);
static int calc_hue_base(const QString &adaptor_name);

const char *vr_str[] =
{
    "unknown", "xlib", "xshm", "xv-blit", "xvmc", "xvmc",
    "xvmc",
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
    if (opts.decoders->contains("libmpeg2"))
    {
        (*opts.safe_renderers)["libmpeg2"].append("xlib");
        (*opts.safe_renderers)["libmpeg2"].append("xshm");
        (*opts.safe_renderers)["libmpeg2"].append("xv-blit");
    }

    if (opts.decoders->contains("crystalhd"))
    {
        (*opts.safe_renderers)["crystalhd"].append("xlib");
        (*opts.safe_renderers)["crystalhd"].append("xshm");
        (*opts.safe_renderers)["crystalhd"].append("xv-blit");
    }

#ifdef USING_XVMC
    if (opts.decoders->contains("xvmc"))
    {
        (*opts.deints)["xvmc-blit"].append("bobdeint");
        (*opts.deints)["xvmc-blit"].append("onefield");
        (*opts.deints)["xvmc-blit"].append("none");
        (*opts.osds)["xvmc-blit"].append("chromakey");
        (*opts.osds)["xvmc-blit"].append("ia44blend");
        (*opts.safe_renderers)["dummy"].append("xvmc-blit");
        (*opts.safe_renderers)["xvmc"].append("xvmc-blit");
        (*opts.render_group)["x11"].append("xvmc-blit");
        opts.priorities->insert("xvmc-blit", 110);
    }
    if (opts.decoders->contains("xvmc-vld"))
        (*opts.safe_renderers)["xvmc-vld"].append("xvmc-blit");
#endif

}

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
VideoOutputXv::VideoOutputXv()
    : VideoOutput(),
      video_output_subtype(XVUnknown),
      global_lock(QMutex::Recursive),

      XJ_win(0), XJ_curwin(0), disp(NULL), XJ_letterbox_colour(0),
      XJ_started(false),

      XJ_non_xv_image(0), non_xv_frames_shown(0), non_xv_show_frame(1),
      non_xv_fps(0), non_xv_av_format(PIX_FMT_NB), non_xv_stop_time(0),

      xvmc_buf_attr(new XvMCBufferSettings()),
      xvmc_chroma(XVMC_CHROMA_FORMAT_420), xvmc_ctx(NULL),
      xvmc_osd_lock(),

      xv_port(-1),      xv_hue_base(0),
      xv_colorkey(0),   xv_draw_colorkey(false),
      xv_chroma(0),     xv_set_defaults(false),

      chroma_osd(NULL)
{
    VERBOSE(VB_PLAYBACK, LOC + "ctor");
    bzero(&av_pause_frame, sizeof(av_pause_frame));

    if (gCoreContext->GetNumSetting("UseVideoModes", 0))
        display_res = DisplayRes::GetDisplayRes(true);
}

VideoOutputXv::~VideoOutputXv()
{
    VERBOSE(VB_PLAYBACK, LOC + "dtor");

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

    if (xvmc_buf_attr)
        delete xvmc_buf_attr;
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
    /*
    QMutexLocker locker(&global_lock);

    window.SetDisplayVisibleRect(QRect(QPoint(0, 0), new_size));

    const QSize display_dim = QSize(
        (monitor_dim.width()  * new_size.width()) / monitor_sz.width(),
        (monitor_dim.height() * new_size.height())/ monitor_sz.height());
    window.SetDisplayDim(display_dim);

    window.SetDisplayAspect(
        ((float)display_dim.width()) / display_dim.height());

    MoveResize();
    */
}

// documented in videooutbase.cpp
bool VideoOutputXv::InputChanged(const QSize &input_size,
                                 float        aspect,
                                 MythCodecID  av_codec_id,
                                 void        *codec_private,
                                 bool        &aspect_only)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3) '%4'->'%5'")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    QMutexLocker locker(&global_lock);

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = input_size     != window.GetActualVideoDim();
    bool asp_changed = aspect         != window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            aspect_only = true;
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
    InitOSD();

    MoveResize();

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "InputChanged(): "
                "Failed to recreate buffers");
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

    static const uint kFeatureNone      = 0x0000;
    static const uint kFeatureXvMC      = 0x0001;
    static const uint kFeatureVLD       = 0x0002;
    static const uint kFeatureIDCT      = 0x0004;
    static const uint kFeatureChromakey = 0x0008;
    static const uint kFeaturePicCtrl   = 0x0010;
};

/**
 * Internal function used to release an XVideo port.
 */
void VideoOutputXv::UngrabXvPort(MythXDisplay *disp, int port)
{
    if (!disp)
        return;

    VERBOSE(VB_PLAYBACK, LOC + QString("Closing XVideo port %1").arg(port));
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
    XLOCK(disp, ret = XvQueryAdaptors(disp->GetDisplay(), root,
                                     &p_num_adaptors, &ai));
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

                disp->Lock();
                ret = XvGrabPort(disp->GetDisplay(), p, CurrentTime);
                if (Success == ret)
                {
                    VERBOSE(VB_PLAYBACK, LOC + "Grabbed xv port "<<p);
                    port = p;
                    xvsetdefaults = add_open_xv_port(disp, p);
                }
                disp->Unlock();
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
                    disp->Lock();
                    ret = XvGrabPort(disp->GetDisplay(), p, CurrentTime);
                    if (Success == ret)
                    {
                        VERBOSE(VB_PLAYBACK,  LOC + "Grabbed xv port "<<p);
                        port = p;
                        xvsetdefaults = add_open_xv_port(disp, p);
                    }
                    disp->Unlock();
                }
            }
        }
        if (port != -1)
        {
            VERBOSE(VB_PLAYBACK, LOC + req[j].description.arg(port));
            VERBOSE(VB_PLAYBACK, LOC +
                        QString("XV_SET_DEFAULTS is %1supported on this port")
                        .arg(xvsetdefaults ? "" : "not "));

            bool xv_vsync = xv_is_attrib_supported(disp, port,
                                                   "XV_SYNC_TO_VBLANK");
            VERBOSE(VB_PLAYBACK, LOC + QString("XV_SYNC_TO_VBLANK %1supported")
                    .arg(xv_vsync ? "" : "not "));
            if (xv_vsync)
            {
                VERBOSE(VB_PLAYBACK, LOC + QString("XVideo Sync to VBlank %1set")
                    .arg(xv_set_attrib(disp, port, "XV_SYNC_TO_VBLANK", 1) ?
                         "" : "NOT "));
            }

            break;
        }
    }
    if (port == -1)
        VERBOSE(VB_PLAYBACK, LOC + "No suitable XVideo port found");

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
 * XvMC does not use this pause frame facility so this only creates
 * a pause buffer for the other output methods.
 *
 * \sideeffect sets av_pause_frame.
 */
void VideoOutputXv::CreatePauseFrame(VOSType subtype)
{
    // All methods but XvMC use a pause frame, create it if needed
    if ((subtype <= XVideo))
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
             vbuffers.GetScratchFrame()->size);

        av_pause_frame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

        clear(&av_pause_frame);

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
bool VideoOutputXv::InitVideoBuffers(bool use_xv, bool use_shm)
{
    bool done = false;

    // If use_xvmc try to create XvMC buffers
#ifdef USING_XVMC
    if (!done && codec_is_xvmc(video_codec_id))
    {
        // Create ffmpeg VideoFrames
        bool vld, idct, mc, dummy;
        myth2av_codecid(video_codec_id, vld, idct, mc, dummy);

        vbuffers.Init(xvmc_buf_attr->GetNumSurf(),
                      false /* create an extra frame for pause? */,
                      xvmc_buf_attr->GetFrameReserve(),
                      xvmc_buf_attr->GetPreBufferGoal(),
                      xvmc_buf_attr->GetPreBufferGoal(),
                      xvmc_buf_attr->GetNeededBeforeDisplay(),
                      true /*use_frame_locking*/);


        done = InitXvMC();

        if (!done)
            vbuffers.Reset();
    }
#endif // USING_XVMC

    if (!done && !codec_is_std(video_codec_id))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to initialize buffers for codec %1")
                .arg(toString(video_codec_id)));
        return false;
    }

    // Create ffmpeg VideoFrames
    if (!done)
        vbuffers.Init(31, true, 1, 12, 4, 2, false);

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
    {
        QString tmp = vr_str[VideoOutputSubType()];
        QString rend = db_vdisp_profile->GetVideoRenderer();
        if ((tmp == "xvmc") && (rend.left(4) == "xvmc"))
            tmp = rend;
        db_vdisp_profile->SetVideoRenderer(tmp);
    }

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
bool VideoOutputXv::InitXvMC()
{
#ifdef USING_XVMC
    MythXLocker lock(disp);
    disp->StartLog();
    QString adaptor_name = QString::null;
    const QSize video_dim = window.GetVideoDim();
    xv_port = GrabSuitableXvPort(disp, disp->GetRoot(), video_codec_id,
                                 video_dim.width(), video_dim.height(),
                                 xv_set_defaults,
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

    // create XvMC buffers
    bool ok = CreateXvMCBuffers();
    ok &= disp->StopLog();

    if (!ok)
    {
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
        window.SetAllowPreviewEPG(true);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create XvMC Buffers.");

        xvmc_osd_lock.lock();
        for (uint i=0; i<xvmc_osd_available.size(); i++)
            delete xvmc_osd_available[i];
        xvmc_osd_available.clear();
        xvmc_osd_lock.unlock();
        UngrabXvPort(disp, xv_port);
        xv_port = -1;
    }

    return ok;
#else // USING_XVMC
    return false;
#endif // USING_XVMC
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
    MythXLocker lock(disp);
    disp->StartLog();
    QString adaptor_name = QString::null;
    const QSize video_dim = window.GetVideoDim();
    xv_port = GrabSuitableXvPort(disp, disp->GetRoot(), kCodec_MPEG2,
                                 video_dim.width(), video_dim.height(),
                                 xv_set_defaults, 0, NULL, &adaptor_name);
    if (xv_port == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not find suitable XVideo surface.");
        return false;
    }

    VERBOSE(VB_IMPORTANT, LOC + QString("XVideo Adaptor Name: '%1'")
            .arg(adaptor_name));

    xv_hue_base = calc_hue_base(adaptor_name);

    bool foundimageformat = false;
    int ids[] = { GUID_YV12_PLANAR, GUID_I420_PLANAR, GUID_IYUV_PLANAR, };
    int format_cnt = 0;
    XvImageFormatValues *formats;
    formats = XvListImageFormats(disp->GetDisplay(), xv_port, &format_cnt);

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
        XFree(formats);

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
        UngrabXvPort(disp, xv_port);
        xv_port = -1;
    }

    bool ok = xv_port >= 0;
    if (ok)
        ok = CreateBuffers(XVideo);

    if (!disp->StopLog())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create XVideo Buffers.");
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

    VERBOSE(VB_IMPORTANT, LOC +
            "Falling back to X shared memory video output."
            "\n\t\t\t      *** May be slow ***");

    bool ok = CreateBuffers(XShm);

    if (!disp->StopLog())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate X shared memory.");
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

    VERBOSE(VB_IMPORTANT, LOC +
            "Falling back to X11 video output over a network socket."
            "\n\t\t\t      *** May be very slow ***");

    bool ok = CreateBuffers(Xlib);

    if (!disp->StopLog())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create X buffers.");
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
MythCodecID VideoOutputXv::GetBestSupportedCodec(
    uint width,       uint height,
    uint osd_width,   uint osd_height,
    uint stream_type, int xvmc_chroma,
    bool test_surface, bool force_xv)
{
    (void)width, (void)height, (void)osd_width, (void)osd_height;
    (void)stream_type, (void)xvmc_chroma, (void)test_surface;

    MythCodecID ret = (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

    if (force_xv || !codec_is_std_mpeg(ret))
        return ret;

#ifdef USING_XVMC
    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    QString dec = vdp.GetDecoder();
    if ((dec == "libmpeg2") || (dec == "ffmpeg"))
        return ret;

    // Disable features based on environment and DB values.
    bool use_xvmc_vld = false, use_xvmc_idct = false, use_xvmc = false;
    bool use_xv = true, use_shm = true;
    if (dec == "xvmc")
        use_xvmc_idct = use_xvmc = true;
    else if (dec == "xvmc-vld")
        use_xvmc_vld = use_xvmc = true;

    SetFromEnv(use_xvmc_vld, use_xvmc_idct, use_xvmc, use_xv, use_shm);

    // Disable features based on hardware capabilities.
    MythXDisplay *disp = OpenMythXDisplay();
    if (disp)
    {
        MythXLocker lock(disp);
        SetFromHW(disp, disp->GetRoot(), use_xvmc, use_xv, use_shm);
    }

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
    if (disp && test_surface && !codec_is_std(ret))
    {
        XvMCSurfaceInfo info;

        ok = false;
        bool dummy;
        int port = GrabSuitableXvPort(disp, disp->GetRoot(), ret, width, height,
                                      dummy, xvmc_chroma, &info);
        if (port >= 0)
        {
            XvMCContext *ctx =
                CreateXvMCContext(disp, port, info.surface_type_id,
                                  width, height);
            ok = NULL != ctx;
            DeleteXvMCContext(disp, ctx);
            UngrabXvPort(disp, port);
        }
    }

    ok |= cnt_open_xv_port() > 0; // also ok if we already opened port..

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

    if (disp)
        delete disp;
#endif // USING_XVMC

    return ret;
}

bool VideoOutputXv::InitOSD(void)
{
    QString osdrenderer = db_vdisp_profile->GetOSDRenderer();

    if (osdrenderer == "chromakey")
    {
        if ((xv_colorkey == (int)XJ_letterbox_colour) ||
            (video_output_subtype < XVideo))
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Disabling ChromaKeyOSD as"
                    " colorkeying will not work."));
        }
        else if (!((32 == disp->GetDepth()) || (24 == disp->GetDepth())))
        {
            VERBOSE(VB_IMPORTANT, LOC + QString(
                        "Number of bits per pixel is %1, \n\t\t\t"
                        "but we only support ARGB 32 bbp for ChromaKeyOSD.")
                    .arg(disp->GetDepth()));
        }
        else
        {
            chroma_osd = new ChromaKeyOSD(this);
            xvmc_buf_attr->SetOSDNum(0); // disable XvMC blending OSD
        }
        return chroma_osd;
    }

    // Other OSD's don't require initialization here...
    return true;
}

#define XV_INIT_FATAL_ERROR_TEST(test,msg) \
do { \
    if (test) \
    { \
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg << " Exiting playback."); \
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
    WId winid, int winx, int winy, int winw, int winh,
    MythCodecID codec_id, WId embedid)
{
    window.SetNeedRepaint(true);

    XV_INIT_FATAL_ERROR_TEST(winid <= 0, "Invalid Window ID.");

    disp = OpenMythXDisplay();
    XV_INIT_FATAL_ERROR_TEST(!disp, "Failed to open display.");

    // HACK -- begin
    //usleep(50 * 1000);
    // HACK -- end

    // Initialize X stuff
    MythXLocker lock(disp);

    XJ_curwin     = winid;
    XJ_win        = winid;

    VERBOSE(VB_PLAYBACK, LOC + "Creating gc");
    XV_INIT_FATAL_ERROR_TEST(!disp->CreateGC(XJ_win), "Failed to create GC.");

    VERBOSE(VB_PLAYBACK, LOC + "XJ_screen_num: '"<<disp->GetScreen()<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_curwin:     '"<<XJ_curwin<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_win:        '"<<XJ_win<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_root:       '"<<disp->GetRoot()<<"'");
    VERBOSE(VB_PLAYBACK, LOC + "XJ_gc:         '"<<disp->GetGC()<<"'");

    // The letterbox color..
    XJ_letterbox_colour = disp->GetBlack();
    Colormap cmap = XDefaultColormap(disp->GetDisplay(), disp->GetScreen());
    XColor colour, colour_exact;
    QString name = toXString(db_letterbox_colour);
    QByteArray ascii_name =  name.toAscii();
    const char *cname = ascii_name.constData();
    if (XAllocNamedColor(disp->GetDisplay(), cmap, cname, &colour, &colour_exact))
        XJ_letterbox_colour = colour.pixel;

    XJ_started = true;

    // Basic setup
    VideoOutput::Init(width, height, aspect,
                      winid, winx, winy, winw, winh,
                      codec_id, embedid);

    // Set resolution/measurements (check XRandR, Xinerama, config settings)
    InitDisplayMeasurements(width, height, true);

    // Set embedding window id
    if (embedid > 0)
        XJ_curwin = XJ_win = embedid;

    if (!InitSetupBuffers())
        return false;

    InitColorKey(true);
    InitOSD();

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
                VERBOSE(VB_IMPORTANT, "Failed to disable autopaint");
                xv_draw_colorkey = false;
            }
        }
        else if (!turnoffautopaint && !xv_val)
        {
            xv_set_attrib(disp, xv_port, attr_autopaint, 1);
            if (!xv_get_attrib(disp, xv_port, attr_autopaint, xv_val) ||
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
        VERBOSE(VB_PLAYBACK, LOC +
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

    if (filtername == "bobdeint" && (vos >= XVideo))
        return true;

    return VideoOutput::ApproveDeintFilter(filtername);
}

XvMCContext* VideoOutputXv::CreateXvMCContext(
    MythXDisplay* disp, int port, int surf_type, int width, int height)
{
    (void)disp; (void)port; (void)surf_type; (void)width; (void)height;
#ifdef USING_XVMC
    int ret = Success;
    XvMCContext *ctx = new XvMCContext;
    XLOCK(disp, ret = XvMCCreateContext(disp->GetDisplay(), port, surf_type,
                                        width, height, XVMC_DIRECT, ctx));
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

void VideoOutputXv::DeleteXvMCContext(MythXDisplay* disp, XvMCContext*& ctx)
{
    (void)disp; (void)ctx;
#ifdef USING_XVMC
    if (ctx)
    {
        XLOCK(disp, XvMCDestroyContext(disp->GetDisplay(), ctx));
        delete ctx;
        ctx = NULL;
    }
#endif // !USING_XVMC
}

bool VideoOutputXv::CreateXvMCBuffers(void)
{
#ifdef USING_XVMC
    const QSize video_dim = window.GetVideoDim();
    xvmc_ctx = CreateXvMCContext(disp, xv_port,
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
                                     disp, xvmc_ctx,
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
            new XvMCOSD(disp, xv_port, xvmc_surf_info.surface_type_id,
                        xvmc_surf_info.flags);
        xvmc_osd->CreateBuffer(*xvmc_ctx,
                               video_dim.width(), video_dim.height());
        xvmc_osd_available.push_back(xvmc_osd);
    }
    xvmc_osd_lock.unlock();

    disp->Sync();
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
    const QSize video_dim = window.GetVideoDim();
    uint num_mv_blocks   = (((video_dim.width()  + 15) / 16) *
                            ((video_dim.height() + 15) / 16));
    uint num_data_blocks = num_mv_blocks * blocks_per_macroblock;

    // need the equivalent of 5 extra surfaces for VLD decoding -- Tegue
    if (surface_has_vld)
        num += 5;

    // create needed XvMC stuff
    bool ok = true;
    MythXLocker lock(disp);
    Display *d = disp->GetDisplay();

    for (uint i = 0; i < num; i++)
    {
        xvmc_vo_surf_t *surf = new xvmc_vo_surf_t;
        bzero(surf, sizeof(xvmc_vo_surf_t));

        int ret = XvMCCreateSurface(d, xvmc_ctx, &(surf->surface));
        ok &= (Success == ret);

        if (ok && !surface_has_vld)
        {
            ret = XvMCCreateBlocks(d, xvmc_ctx, num_data_blocks,
                                   &(surf->blocks));
            if (Success != ret)
            {
                XvMCDestroySurface(d, &(surf->surface));
                ok = false;
            }
        }

        if (ok && !surface_has_vld)
        {
            ret = XvMCCreateMacroBlocks(d, xvmc_ctx, num_mv_blocks,
                                        &(surf->macro_blocks));
            if (Success != ret)
            {
                XvMCDestroyBlocks(d, &(surf->blocks));
                XvMCDestroySurface(d, &(surf->surface));
                ok = false;
            }
        }

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

        for (uint i = 0; i < 5; i++)
        {
            xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*)surfaces.back();
            surfaces.pop_back();
            XvMCDestroySurface(d, &(surf->surface));
            delete surf;
        }
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
    const QSize video_dim = window.GetVideoDim();
    VERBOSE(VB_PLAYBACK, LOC +
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

    const QSize video_dim = window.GetVideoDim();
    const QRect display_visible_rect = window.GetDisplayVisibleRect();

    if (subtype > XVideo && xv_port >= 0)
        ok = CreateXvMCBuffers();
    else if (subtype == XVideo && xv_port >= 0)
    {
        vector<unsigned char*> bufs =
            CreateShmImages(vbuffers.allocSize(), true);

        ok = (bufs.size() >= vbuffers.allocSize()) &&
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
                                           display_visible_rect.width(),
                                           display_visible_rect.height(),
                                           /*bitmap_pad*/8, 0);

            if (!XJ_non_xv_image)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "XCreateImage failed: "
                        <<"XJ_disp("<<d<<") visual("<<visual<<") "<<endl
                        <<"                        "
                        <<"depth("<<disp->GetDepth()<<") "
                        <<"WxH("<<display_visible_rect.width()
                        <<"x"<<display_visible_rect.height()<<") ");
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

            VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
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
        xvmc_buf_attr->SetOSDNum(1);
    }

    Display *d = disp->GetDisplay();
#ifdef USING_XVMC
    // XvMC buffers
    for (uint i=0; i<xvmc_surfs.size(); i++)
    {
        xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*) xvmc_surfs[i];
        XLOCK(disp, XvMCHideSurface(d, &(surf->surface)));
    }
    DiscardFrames(true);
    for (uint i=0; i<xvmc_surfs.size(); i++)
    {
        disp->Lock();
        xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*) xvmc_surfs[i];
        XvMCDestroySurface(d, &(surf->surface));
        XvMCDestroyMacroBlocks(d, &(surf->macro_blocks));
        XvMCDestroyBlocks(d, &(surf->blocks));
        disp->Unlock();
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
#endif // USING_XVMC

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

#ifdef USING_XVMC
    DeleteXvMCContext(disp, xvmc_ctx);
#endif // USING_XVMC
}

void VideoOutputXv::EmbedInWidget(int x, int y, int w, int h)
{
    QMutexLocker locker(&global_lock);

    if (!window.IsEmbedding())
        VideoOutput::EmbedInWidget(x, y, w, h);
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
    if (VideoOutputSubType() >= XVideoMC)
    {
        for (uint i=0; i<xvmc_surfs.size(); i++)
        {
            xvmc_vo_surf_t *surf = (xvmc_vo_surf_t*) xvmc_surfs[i];
            XLOCK(disp, XvMCHideSurface(disp->GetDisplay(), &(surf->surface)));
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
 * \fn VideoOutputXv::DoneDisplayingFrame(VideoFrame *frame)
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
void VideoOutputXv::DoneDisplayingFrame(VideoFrame *frame)
{
    if (VideoOutputSubType() <= XVideo)
    {
        VideoOutput::DoneDisplayingFrame(frame);
        return;
    }

#ifdef USING_XVMC
    if (vbuffers.contains(kVideoBuffer_used, frame))
    {
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
    struct xvmc_pix_fmt *render = NULL, *osdrender = NULL;
    VideoFrame *osdframe = NULL;

    if (frame)
    {
        global_lock.lock();
        framesPlayed = frame->frameNumber + 1;
        global_lock.unlock();

        vbuffers.LockFrame(frame, "PrepareFrameXvMC");
        SyncSurface(frame);
        render = GetRender(frame);
        render->state |= AV_XVMC_STATE_DISPLAY_PENDING;
        if (xvmc_buf_attr->GetOSDNum())
            osdframe = vbuffers.GetOSDFrame(frame);
        vbuffers.UnlockFrame(frame, "PrepareFrameXvMC");
    }

    if (osdframe)
    {
        vbuffers.LockFrame(osdframe, "PrepareFrameXvMC -- osd");
        SyncSurface(osdframe);
        osdrender = GetRender(osdframe);
        osdrender->state |= AV_XVMC_STATE_DISPLAY_PENDING;
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

    const QRect display_visible_rect = window.GetDisplayVisibleRect();

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

    int out_width  = display_visible_rect.width()  & ~0x1;
    int out_height = display_visible_rect.height() & ~0x1;
    unsigned char *sbuf = new unsigned char[out_width * out_height * 3 / 2];
    AVPicture image_in, image_out;
    static struct SwsContext  *scontext;

    avpicture_fill(&image_out, (uint8_t *)sbuf, PIX_FMT_YUV420P,
                   out_width, out_height);

    vbuffers.LockFrame(buffer, "PrepareFrameMem");
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
    vbuffers.UnlockFrame(buffer, "PrepareFrameMem");

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
void VideoOutputXv::PrepareFrame(VideoFrame *buffer, FrameScanType scan, OSD *osd)
{
    (void)osd;
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
    const QRect video_rect         = window.GetVideoRect();
    const QRect display_video_rect = (vsz_enabled && chroma_osd) ?
                                      vsz_desired_display_rect :
                                      window.GetDisplayVideoRect();
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
    if (XVideoVLD == VideoOutputSubType())
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
    struct xvmc_pix_fmt *showingsurface = (osdframe) ?
        GetRender(osdframe) : GetRender(frame);
    XvMCSurface *surf = showingsurface->p_surface;

    // actually display the frame
    disp->Lock();
    XvMCPutSurface(disp->GetDisplay(), surf, XJ_curwin,
                   video_rect.left(), src_y,
                   video_rect.width(), video_rect.height(),
                   display_video_rect.left(), dest_y,
                   display_video_rect.width(),
                   display_video_rect.height(), field);
    XFlush(disp->GetDisplay()); // send XvMCPutSurface call to X11 server
    disp->Unlock();

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

    vbuffers.UnlockFrame(frame, "ShowXVideo");
    {
        QMutexLocker locker(&global_lock);
        vbuffers.LockFrame(frame, "ShowXVideo");
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

    if ((window.IsRepaintNeeded() || xv_need_bobdeint_repaint) &&
         VideoOutputSubType() >= XVideo)
    {
        DrawUnusedRects(/* don't do a sync*/false);
    }

    if (VideoOutputSubType() > XVideo)
        ShowXvMC(scan);
    else if (VideoOutputSubType() == XVideo)
        ShowXVideo(scan);

    disp->Sync();
}

void VideoOutputXv::ShowPIP(VideoFrame  *frame,
                            MythPlayer  *pipplayer,
                            PIPLocation  loc)
{
    if (VideoOutputSubType() >= XVideoMC &&
        VideoOutputSubType() <= XVideoVLD)
    {
        return;
    }

    VideoOutput::ShowPIP(frame, pipplayer, loc);
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
    struct xvmc_pix_fmt *render = GetRender(frame);
    // disable questionable ffmpeg surface munging
    if (render->p_past_surface == render->p_surface)
        render->p_past_surface = NULL;
    vbuffers.AddInheritence(frame);

    Status status;
    if (XVideoVLD == VideoOutputSubType())
    {
        vbuffers.LockFrame(frame, "DrawSlice -- VLD");
        XLOCK(disp, status = XvMCPutSlice2(disp->GetDisplay(), xvmc_ctx,
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
        XLOCK(disp, status =
            XvMCRenderSurface(disp->GetDisplay(), xvmc_ctx,
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

void VideoOutputXv::UpdatePauseFrame(void)
{
    QMutexLocker locker(&global_lock);

    VERBOSE(VB_PLAYBACK, LOC + "UpdatePauseFrame() " + vbuffers.GetStatus());

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
        if (!window.IsEmbedding() && osd)
        {
            bool needrepaint = chroma_osd->ProcessOSD(osd);
            if (!window.IsRepaintNeeded() && needrepaint)
                window.SetNeedRepaint(true);
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
    if (!window.IsEmbedding() && osd)
        xvmc_osd = GetAvailableOSD();

    if (xvmc_osd && xvmc_osd->IsValid())
    {
        VideoFrame *osdframe = NULL;
        bool show = DisplayOSD(xvmc_osd->OSDFrame(), osd);

        if (show && xvmc_osd->NeedFrame())
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
                    {
                        XLOCK(disp, XvMCHideSurface(disp->GetDisplay(),
                                            GetRender(*it)->p_surface));
                    }
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
        if (show && !xvmc_osd->NeedFrame())
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
                                    const PIPMap &pipPlayers,
                                    FrameScanType scan)
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

    vbuffers.UnlockFrame(frame, "ProcessFrameMem");
}

// this is documented in videooutbase.cpp
void VideoOutputXv::ProcessFrame(VideoFrame *frame, OSD *osd,
                                 FilterChain *filterList,
                                 const PIPMap &pipPlayers,
                                 FrameScanType scan)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsErrored() in ProcessFrame()");
        return;
    }

    if (VideoOutputSubType() <= XVideo)
        ProcessFrameMem(frame, osd, filterList, pipPlayers, scan);
    else
        ProcessFrameXvMC(frame, osd);
}

// this is documented in videooutbase.cpp
int VideoOutputXv::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    if (!supported_attributes)
        return -1;

    QString attrName = toXVString(attribute);
    QByteArray ascii_attr_name =  attrName.toAscii();
    const char *cname = ascii_attr_name.constData();

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

#ifdef USING_XVMC
    // Needed for VIA XvMC to commit change immediately.
    if (video_output_subtype > XVideo)
    {
        Atom xv_atom;
        XLOCK(disp, xv_atom = XInternAtom(disp->GetDisplay(), cname, False));
        if (xv_atom != None)
            XLOCK(disp, XvMCSetAttribute(disp->GetDisplay(), xvmc_ctx,
                                         xv_atom, value));
    }
#endif

    SetPictureAttributeDBValue(attribute, newValue);
    return newValue;
}

void VideoOutputXv::InitPictureAttributes(void)
{
    supported_attributes = kPictureAttributeSupported_None;

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
            QByteArray ascii_attr_name =  attrName.toAscii();
            const char *cname = ascii_attr_name.constData();

            if (attrName.isEmpty())
                continue;

            if (xv_is_attrib_supported(disp, xv_port, cname,
                                       &val, &min_val, &max_val))
            {
                supported_attributes = (PictureAttributeSupported)
                    (supported_attributes | toMask((PictureAttribute)i));
                xv_attribute_min[(PictureAttribute)i] = min_val;
                xv_attribute_max[(PictureAttribute)i] = max_val;
                xv_attribute_def[(PictureAttribute)i] = val;
                VERBOSE(VB_PLAYBACK, LOC + QString("%1: %2:%3:%4")
                    .arg(cname).arg(min_val).arg(val).arg(max_val));
            }
        }

        if (xv_set_defaults)
            restore_port_attributes(xv_port, false);
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
    struct xvmc_pix_fmt *render = GetRender(frame);
    if (render)
    {
        Display *dsp        = render->disp;
        XvMCSurface *surf   = render->p_surface;
        int res = 0, status = 0;
        if (dsp && surf)
        {
            MythXDisplay *xd = GetMythXDisplay(dsp);
            if (xd)
                XLOCK(xd, res = XvMCGetSurfaceStatus(dsp, surf, &status));
        }
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
    struct xvmc_pix_fmt *render = GetRender(frame);
    if (render)
    {
        Display *dsp        = render->disp;
        XvMCSurface *surf   = render->p_surface;
        int res = 0, status = 0;
        if (dsp && surf)
        {
            MythXDisplay *xd = GetMythXDisplay(dsp);
            if (xd)
                XLOCK(xd, res = XvMCGetSurfaceStatus(dsp, surf, &status));
        }
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
    struct xvmc_pix_fmt *render = GetRender(frame);
    if (render)
    {
        Display *dsp      = render->disp;
        XvMCSurface *surf = render->p_surface;
        if (past_future == -1)
            surf = render->p_past_surface;
        else if (past_future == +1)
            surf = render->p_future_surface;

        if (dsp && surf)
        {
            int status = 0, res = Success;
            MythXDisplay *xd = GetMythXDisplay(dsp);
            if (xd)
            {
                XLOCK(xd, res = XvMCGetSurfaceStatus(dsp, surf, &status));
                if (res != Success)
                    VERBOSE(VB_PLAYBACK, LOC_ERR + "SyncSurface(): " +
                            QString("XvMCGetSurfaceStatus %1").arg(res));
                if (status & XVMC_RENDERING)
                {
                    XLOCK(xd, XvMCFlushSurface(dsp, surf));
                    while (IsRendering(frame))
                        usleep(50);
                }
            }
        }
    }
#endif // USING_XVMC
}

void VideoOutputXv::FlushSurface(VideoFrame* frame)
{
    (void)frame;
#ifdef USING_XVMC
    struct xvmc_pix_fmt *render = GetRender(frame);
    if (render)
    {
        Display *dsp      = render->disp;
        XvMCSurface *surf = render->p_surface;
        if (dsp && IsRendering(frame))
        {
            MythXDisplay *xd = GetMythXDisplay(dsp);
            if (xd)
                XLOCK(xd, XvMCFlushSurface(dsp, surf));
        }
    }
#endif // USING_XVMC
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

static void SetFromHW(MythXDisplay *d,    Window  curwin,
                      bool    &useXvMC,   bool   &useXVideo,
                      bool    &useShm)
{
    (void)d;
    (void)curwin;

    if (!d)
        return;

    MythXLocker lock(d);
    // find out about XvMC support
    if (useXvMC)
    {
#ifdef USING_XVMC
        int mc_event, mc_err, ret;
        ret = XvMCQueryExtension(d->GetDisplay(), &mc_event, &mc_err);
        if (True != ret)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "XvMC output requested, "
                    "but is not supported by display.");
            useXvMC = false;
        }

        int mc_ver, mc_rel;
        ret = XvMCQueryVersion(d->GetDisplay(), &mc_ver, &mc_rel);
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
        ret = XvQueryExtension(d->GetDisplay(), &p_ver, &p_rel,
                                    &p_req, &p_event, &p_err);
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

    bool vld, idct, mc, xv, shm,  dummy;

    myth2av_codecid(myth_codec_id, vld, idct, mc, dummy);

    xv = shm = !vld && !idct;

    SetFromEnv(vld, idct, mc, xv, shm);
    SetFromHW(display, curwin, mc, xv, shm);
    idct &= mc;

    QStringList list;
    if (codec_is_std(myth_codec_id))
    {
        if (xv)
            list += "xv-blit";
        if (shm)
            list += "xshm";
        list += "xlib";
    }
    else
    {
        if (vld || idct || mc)
            list += "xvmc-blit";
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
