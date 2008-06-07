// ANSI C headers
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cerrno>

// POSIX headers
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/param.h>

// Linux headers
#include <linux/fb.h>

// C++ headers
#include <map>
#include <iostream>
using namespace std;

#include "videodev_myth.h"
#include "videodev2_myth.h"

#include "videoout_ivtv.h"
extern "C" {
#include <inttypes.h>
#include "ivtv_myth.h"
}

#include "libmyth/mythcontext.h"

#include "NuppelVideoPlayer.h"
extern "C" {
#include "../libavcodec/avcodec.h"
}
#include "yuv2rgb.h"
#include "osd.h"
#include "osdsurface.h"
#include "videodisplayprofile.h"

#define LOC QString("IVD: ")
#define LOC_ERR QString("IVD Error: ")

/***************************************************************/
/* An extract from linux/video.h which is now required for the */
/* decoder. Included here since we need an up to date version. */

/* Decoder commands */
#define VIDEO_CMD_PLAY        (0)
#define VIDEO_CMD_STOP        (1)
#define VIDEO_CMD_FREEZE      (2)
#define VIDEO_CMD_CONTINUE    (3)

/* Flags for VIDEO_CMD_FREEZE */
#define VIDEO_CMD_FREEZE_TO_BLACK     	(1 << 0)

/* Flags for VIDEO_CMD_STOP */
#define VIDEO_CMD_STOP_TO_BLACK      	(1 << 0)
#define VIDEO_CMD_STOP_IMMEDIATELY     	(1 << 1)

#define VIDEO_GET_FRAME_COUNT _IOR('o', 58, __u64)
#define VIDEO_COMMAND _IOWR('o', 59, struct video_command)

/* The structure must be zeroed before use by the application
   This ensures it can be extended safely in the future. */
struct video_command {
    __u32 cmd;
    __u32 flags;
    union {
        struct {
            __u64 pts;
        } stop;

        struct {
            __u32 speed;
            __u32 format;
        } play;

        struct {
            __u32 data[16];
        } raw;
    };
};

class VideoOutputIvtvPriv
{
  public:
    VideoOutputIvtvPriv()
    {
        bzero(&ivtvfb_var,     sizeof(ivtvfb_var));
        bzero(&ivtvfb_var_old, sizeof(ivtvfb_var_old));
    }

    struct fb_var_screeninfo ivtvfb_var;
    struct fb_var_screeninfo ivtvfb_var_old;
};

/* End of extract from linux/video.h                           */
/***************************************************************/

/* Used by recent ivtv-fb. Replaces the older IVTVFB_IOCTL_PREP_FRAME */
/* Argument list is identical */
#define IVTVFB_IOC_DMA_FRAME  _IOW ('V', BASE_VIDIOC_PRIVATE+0, struct ivtvfb_ioctl_dma_host_to_ivtv_args)

/** \class  VideoOutputIvtv
 *  \brief  Implementation of video output for the Hauppage PVR 350 cards.
 */

VideoOutputIvtv::VideoOutputIvtv(void) :
    videofd(-1),              fbfd(-1),
    fps(30000.0f/1001.0f),    videoDevice("/dev/video16"),
    driver_version(0),
    has_v4l2_api(false),      has_pause_bug(false),

    mapped_offset(0),         mapped_memlen(0),
    mapped_mem(NULL),         pixels(NULL),

    stride(0),

    lastcleared(false),       pipon(false),
    osdon(false),
    osdbuffer(NULL),          osdbuf_aligned(NULL),
    osdbufsize(0),            osdbuf_revision(0xfffffff),

    last_speed(1.0f),
    internal_offset(0),       frame_at_speed_change(0),

    last_normal(true),        last_mask(0x2),

    alphaState(kAlpha_Solid), old_fb_ioctl(true),
    fb_dma_ioctl(IVTVFB_IOCTL_PREP_FRAME),
    color_key(false),         decoder_flush(true),
    paused(false)
{
    priv = new VideoOutputIvtvPriv();
}

VideoOutputIvtv::~VideoOutputIvtv()
{
    Close();

    if (osdbuffer)
    {
        delete [] osdbuffer;
        osdbuffer = NULL;
    }

    if (priv)
    {
        delete priv;
        priv = NULL;
    }
}

void VideoOutputIvtv::ClearOSD(void) 
{
    if (fbfd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "ClearOSD() -- no framebuffer!");
        return;
    }

    VERBOSE(VB_PLAYBACK, LOC + "ClearOSD");

    struct ivtvfb_ioctl_dma_host_to_ivtv_args prep;
    bzero(&prep, sizeof(prep));

    bzero(osdbuf_aligned, osdbufsize);
    prep.source = osdbuf_aligned;
    prep.dest_offset = 0;

    if (old_fb_ioctl)
    {
        struct ivtv_osd_coords osdcoords;
        bzero(&osdcoords, sizeof(osdcoords));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_ACTIVE_BUFFER, &osdcoords) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to get active buffer for ClearOSD()" + ENO);
            return;
        }
        prep.count = osdcoords.max_offset;
    }
    else
    {
        prep.count = priv->ivtvfb_var.xres_virtual *
            priv->ivtvfb_var.yres * (priv->ivtvfb_var.bits_per_pixel / 8); 
    }

    if (!old_fb_ioctl)
        ioctl(fbfd, FBIOPAN_DISPLAY, &priv->ivtvfb_var);

    int ret = ioctl(fbfd, fb_dma_ioctl, &prep);
    if (ret < 0)
    {
        if (errno == EINVAL && fb_dma_ioctl != IVTVFB_IOC_DMA_FRAME)
        {
            fb_dma_ioctl = IVTVFB_IOC_DMA_FRAME;
            ret = ioctl(fbfd, fb_dma_ioctl, &prep);
        }
        if (ret < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to prepare frame" + ENO);
    }
}

void VideoOutputIvtv::SetColorKey(int state, int color)
{
    if (has_v4l2_api)
    {
        struct v4l2_format alpha_state;
        struct v4l2_framebuffer framebuffer_state;

        ioctl(videofd, VIDIOC_G_FBUF, &framebuffer_state);
        alpha_state.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
        ioctl(videofd, VIDIOC_G_FMT, &alpha_state);

        if (state)
        {
            framebuffer_state.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
            alpha_state.fmt.win.chromakey = color;
        }
        else
        {
            framebuffer_state.flags &= ~V4L2_FBUF_FLAG_CHROMAKEY;
        }

        ioctl(videofd, VIDIOC_S_FBUF, &framebuffer_state);
        ioctl(videofd, VIDIOC_S_FMT, &alpha_state);
    }
    else if (color_key)
    {
        struct ivtvfb_ioctl_colorkey ivtvfb_colorkey;
        // Setup color-key. This helps when X isn't running on the PVR350
        ivtvfb_colorkey.state = state;
        ivtvfb_colorkey.colorKey = color;
        ioctl(fbfd,IVTVFB_IOCTL_SET_COLORKEY, &ivtvfb_colorkey);
    }
}

void VideoOutputIvtv::SetAlpha(eAlphaState newAlphaState)
{
    if (alphaState == newAlphaState)
        return;

#if 0
    if (newAlphaState == kAlpha_Local)
        VERBOSE(VB_PLAYBACK, LOC + "SetAlpha(Local)");
    if (newAlphaState == kAlpha_Clear)
        VERBOSE(VB_PLAYBACK, LOC + "SetAlpha(Clear)");
    if (newAlphaState == kAlpha_Solid)
        VERBOSE(VB_PLAYBACK, LOC + "SetAlpha(Solid)");
    if (newAlphaState == kAlpha_Embedded)
        VERBOSE(VB_PLAYBACK, LOC + "SetAlpha(Embedded)");
#endif

    struct ivtvfb_ioctl_state_info fbstate;
    bzero(&fbstate, sizeof(fbstate));

    if (has_v4l2_api)
    {
        struct v4l2_format alpha_state;
        struct v4l2_framebuffer framebuffer_state;

        ioctl(videofd, VIDIOC_G_FBUF, &framebuffer_state);
        alpha_state.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
        ioctl(videofd, VIDIOC_G_FMT, &alpha_state);

        if (newAlphaState == kAlpha_Local)
        {
            framebuffer_state.flags &= ~V4L2_FBUF_FLAG_GLOBAL_ALPHA;
            framebuffer_state.flags |= V4L2_FBUF_FLAG_LOCAL_ALPHA;
        }
        else
        {
            framebuffer_state.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
            framebuffer_state.flags &= ~V4L2_FBUF_FLAG_LOCAL_ALPHA;
        }

        if (newAlphaState == kAlpha_Solid)
            alpha_state.fmt.win.global_alpha = 255;
        else if (newAlphaState == kAlpha_Clear)
            alpha_state.fmt.win.global_alpha = 0;
        else if (newAlphaState == kAlpha_Embedded)
            alpha_state.fmt.win.global_alpha =
                gContext->GetNumSetting("PVR350EPGAlphaValue", 164);

        if (ioctl(videofd, VIDIOC_S_FBUF, &framebuffer_state) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to set ivtv alpha mode" + ENO);
        if (ioctl(videofd, VIDIOC_S_FMT, &alpha_state) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to set ivtv alpha values." + ENO);
    }
    else
    {
        if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    LOC_ERR + "Failed to query alpha state" + ENO);
        }

        if (newAlphaState == kAlpha_Local)
        {
            fbstate.status &= ~IVTVFB_STATUS_GLOBAL_ALPHA;
            fbstate.status |= IVTVFB_STATUS_LOCAL_ALPHA;
        }
        else
        {
            fbstate.status |= IVTVFB_STATUS_GLOBAL_ALPHA;
            fbstate.status &= ~IVTVFB_STATUS_LOCAL_ALPHA;
        }

        if (newAlphaState == kAlpha_Solid)
            fbstate.alpha = 255;
        else if (newAlphaState == kAlpha_Clear)
            fbstate.alpha = 0;
        else if (newAlphaState == kAlpha_Embedded)
            fbstate.alpha =
                gContext->GetNumSetting("PVR350EPGAlphaValue", 164);

        if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to set ivtv alpha state." + ENO);
        }
    }

    // If using the new ioctl we need to check the fb mode
    if (!old_fb_ioctl)
    {
        struct fb_var_screeninfo *tmpfb_var = NULL;

        if (newAlphaState == kAlpha_Embedded)
        {
            // If EPG switched on, select old fb mode
            tmpfb_var = &priv->ivtvfb_var_old;
        }
        else if (newAlphaState != kAlpha_Embedded &&
                 alphaState == kAlpha_Embedded)
        {
            // If EPG switched off, select new fb mode
            tmpfb_var = &priv->ivtvfb_var;
        }

        // Change fb mode if required
        if (tmpfb_var)
        {
            if ((priv->ivtvfb_var_old.bits_per_pixel != 32) &&
                 (priv->ivtvfb_var_old.bits_per_pixel != 8))
            {
                // Hide osd during mode change
                ioctl(fbfd, FBIOBLANK, VESA_VSYNC_SUSPEND);
                tmpfb_var->activate = FB_ACTIVATE_NOW;
                if (ioctl(fbfd, FBIOPUT_VSCREENINFO, tmpfb_var) < 0)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "Failed to switch framebuffer "
                            "settings for EPG" + ENO);
                }

                // Restore osd
                ioctl(fbfd, FBIOBLANK, VESA_NO_BLANKING);
            }

            // Reset display pan
            priv->ivtvfb_var.xoffset = 0;
            priv->ivtvfb_var.yoffset = 0;
            ioctl(fbfd, FBIOPAN_DISPLAY, &priv->ivtvfb_var);
        }
    }

    alphaState = newAlphaState;
}

bool VideoOutputIvtv::InputChanged(const QSize &input_size,
                                   float        aspect,
                                   MythCodecID  av_codec_id,
                                   void        *codec_private)
{
    VERBOSE(VB_PLAYBACK, LOC + "InputChanged() -- begin");
    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private);
    MoveResize();
    VERBOSE(VB_PLAYBACK, LOC + "InputChanged() -- end");
    return true;
}

int VideoOutputIvtv::GetRefreshRate(void)
{
    return 0;
}

int VideoOutputIvtv::ValidVideoFrames(void) const
{
    return 131; // approximation for when output buffer is full...
}

bool VideoOutputIvtv::Init(int width, int height, float aspect, 
                           WId winid, int winx, int winy, int winw, 
                           int winh, WId embedid)
{
    VERBOSE(VB_PLAYBACK, LOC + "Init() -- begin");

    db_vdisp_profile->SetVideoRenderer("ivtv");

    allowpreviewepg = true;

    videoDevice = gContext->GetSetting("PVR350VideoDev");

    VideoOutput::Init(width, height, aspect, winid, winx, winy, winw, winh, 
                      embedid);

    osdbufsize = video_dim.width() * video_dim.height() * 4;

    MoveResize();

    Open();

    if (videofd < 0)
        return false;

    if (fbfd < 0)
    {
        int fbno = 0;

        if (has_v4l2_api)
        {
            struct v4l2_framebuffer fbuf;

            ioctl(videofd, VIDIOC_G_FBUF, &fbuf);
            for (fbno = 0; fbno < 10; fbno++)
            {
                struct fb_fix_screeninfo si;
                char buf[10];

                sprintf(buf, "/dev/fb%d", fbno);
                fbfd = open(buf, O_RDWR);
                if (fbfd < 0) 
                    continue;
                ioctl(fbfd, FBIOGET_FSCREENINFO, &si);
                if (si.smem_start == (unsigned long)fbuf.base)
                    break;
                close(fbfd);
                fbfd = -1;
            }
            if (fbfd < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to locate framebuffer" +
                        "\n\t\t\tDid you load the ivtv-fb "
                        "Linux kernel module?");
                return false;
            }
        }
        else
        {
            if (ioctl(videofd, IVTV_IOC_GET_FB, &fbno) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Framebuffer number query failed." + ENO +
                        "\n\t\t\tDid you load the ivtv-fb "
                        "Linux kernel module?");
                return false;
            }

            if (fbno < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to determine framebuffer number." +
                        "\n\t\t\tDid you load the ivtv-fb "
                        "Linux kernel module?");
                return false;
            }

            QString fbdev = QString("/dev/fb%1").arg(fbno);
            fbfd = open(fbdev.ascii(), O_RDWR);

            if (fbfd < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open framebuffer " +
                    QString("'%1'").arg(fbdev) + ENO +
                    "\n\t\t\tThis is needed for the OSD.");
                return false;
            }
        }

        struct ivtvfb_ioctl_get_frame_buffer igfb;
        bzero(&igfb, sizeof(igfb));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_FRAME_BUFFER, &igfb) < 0)
        {
            if (errno == EINVAL)
            {
                struct fb_fix_screeninfo ivtvfb_fix;
                if (ioctl(fbfd, FBIOGET_FSCREENINFO, &ivtvfb_fix) < 0)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "Getting frame buffer" + ENO);
                }
                else
                {
                    old_fb_ioctl = false;
                    ioctl(fbfd, FBIOGET_VSCREENINFO, &priv->ivtvfb_var_old);
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Getting frame buffer" + ENO);
            }
        }

        long pagesize = sysconf(_SC_PAGE_SIZE);
        long pagemask = ~(pagesize-1);
        osdbuffer = new char[osdbufsize + pagesize];
        osdbuf_aligned = osdbuffer + (pagesize - 1);
        osdbuf_aligned = (char*) ((unsigned long)osdbuf_aligned & pagemask);

        bzero(osdbuf_aligned, osdbufsize);

        if (old_fb_ioctl)
        {
            struct ivtv_osd_coords osdcoords;
            stride = igfb.sizex * 4;
            bzero(&osdcoords, sizeof(osdcoords));
            osdcoords.lines = video_dim.height();
            osdcoords.offset = 0;
            osdcoords.pixel_stride = video_dim.width() * 2;
            if (ioctl(fbfd, IVTVFB_IOCTL_SET_ACTIVE_BUFFER, &osdcoords) < 0)
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting active buffer" + ENO);
        }
        else
        {
            bzero (&priv->ivtvfb_var, sizeof(priv->ivtvfb_var));

            // Switch dimensions to match the framebuffer
            video_dim = QSize(priv->ivtvfb_var_old.xres,
                              priv->ivtvfb_var_old.yres);

            memcpy(&priv->ivtvfb_var, &priv->ivtvfb_var_old,
                   sizeof priv->ivtvfb_var);

            // The OSD only supports 32bpp, so only change mode if needed
            if (priv->ivtvfb_var_old.bits_per_pixel != 32)
            {
                priv->ivtvfb_var.xres_virtual = priv->ivtvfb_var.xres;
                priv->ivtvfb_var.yres_virtual = priv->ivtvfb_var.yres;
                priv->ivtvfb_var.xoffset = 0;
                priv->ivtvfb_var.yoffset = 0;
                priv->ivtvfb_var.bits_per_pixel = 32;
                priv->ivtvfb_var.nonstd = 0;
                priv->ivtvfb_var.activate = FB_ACTIVATE_NOW;

                if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &priv->ivtvfb_var) < 0)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "Setting frame buffer" + ENO);
                }
            }
            else
            {
                priv->ivtvfb_var.xoffset = 0;
                priv->ivtvfb_var.yoffset = 0;
                ioctl(fbfd, FBIOPAN_DISPLAY, &priv->ivtvfb_var);
            }

            stride = priv->ivtvfb_var.xres_virtual * 4;
        }

        // Setup color-key. This helps when X isn't running on the PVR350
        SetColorKey (1, 0x00010001);

        ClearOSD();

        SetAlpha(kAlpha_Clear);
    }

    VERBOSE(VB_GENERAL, "Using the PVR-350 decoder/TV-out");

    VERBOSE(VB_PLAYBACK, LOC + "Init() -- end");
    return true;
}

/** \fn VideoOutputIvtv::Close(void)
 *  \brief Closes decoder device
 */
void VideoOutputIvtv::Close(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Close() -- begin");

    if (fbfd >= 0)
    {
        ClearOSD();
        SetAlpha(kAlpha_Solid);
        SetColorKey (0,0);

        if (!old_fb_ioctl)
        {
            priv->ivtvfb_var_old.activate = FB_ACTIVATE_NOW;

            if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &priv->ivtvfb_var_old) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to restore framebuffer settings" + ENO);
            }
        }

        close (fbfd);
        fbfd = -1;
    }

    if (videofd >= 0)
    {
        Stop(true /* hide */);

        close(videofd);
        videofd = -1;
    }
    VERBOSE(VB_PLAYBACK, LOC + "Close() -- end");
}

/** \fn VideoOutputIvtv::Open(void)
 *  \brief Opens decoder device
 */
void VideoOutputIvtv::Open(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Open() -- begin");
    if (videofd >= 0)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Open() -- end");
        return;
    }

    videofd = open(videoDevice.ascii(), O_WRONLY | O_NONBLOCK, 0555);
    if (videofd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open decoder device " +
                QString("'%1'").arg(videoDevice) + ENO);
        VERBOSE(VB_PLAYBACK, LOC + "Open() -- end");
        return;
    }

    struct v4l2_capability vcap;
    bzero(&vcap, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to query decoder" + ENO);
    }
    else
    {
        driver_version = vcap.version;
        if (driver_version >= 0x000306)
            color_key = true;
        if (driver_version >= 0x000A00)
            decoder_flush = false;
        has_v4l2_api  = (driver_version >= 0x010000);
        has_pause_bug = (driver_version == 0x010000);

        VERBOSE(VB_GENERAL, LOC + QString("ivtv version %1.%2.%3")
                .arg(driver_version >> 16)
                .arg((driver_version >> 8) & 0xFF)
                .arg(driver_version & 0xFF));
    }
    VERBOSE(VB_PLAYBACK, LOC + "Open() -- end");
}

void VideoOutputIvtv::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    (void)buffer;
    (void)t;
}

void VideoOutputIvtv::Show(FrameScanType)
{
}

void VideoOutputIvtv::DrawUnusedRects(bool) 
{ 
}

void VideoOutputIvtv::UpdatePauseFrame(void) 
{ 
}

void VideoOutputIvtv::ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer)
{
    if (!pipplayer)
        return;

    int pipw, piph;

    VideoFrame *pipimage = pipplayer->GetCurrentFrame(pipw, piph);

    if (!pipimage || !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    int xoff;
    int yoff;

    unsigned char *pipbuf = pipimage->buf;

    if (pipw != pip_desired_display_size.width() ||
        piph != pip_desired_display_size.height())
    {
        DoPipResize(pipw, piph);

        if (pip_tmp_buf && pip_scaling_context)
        {
            AVPicture img_in, img_out;

            avpicture_fill(
                &img_out, (uint8_t*) pip_tmp_buf, PIX_FMT_YUV420P,
                pip_display_size.width(), pip_display_size.height());

            avpicture_fill(&img_in, (uint8_t*) pipimage->buf, PIX_FMT_YUV420P,
                           pipw, piph);

            img_resample(pip_scaling_context, &img_out, &img_in);

            pipw = pip_display_size.width();
            piph = pip_display_size.height();

            pipbuf = pip_tmp_buf;
        }
    }

    switch (db_pip_location)
    {
        case kPIP_END:
        case kPIPTopLeft:
                xoff = 50;
                yoff = 40;
                break;
        case kPIPBottomLeft:
                xoff = 50;
                yoff = frame->height - piph - 40;
                break;
        case kPIPTopRight:
                xoff = frame->width - pipw - 50;
                yoff = 40;
                break;
        case kPIPBottomRight:
                xoff = frame->width - pipw - 50;
                yoff = frame->height - piph - 40;
                break;
    }

    unsigned char *outputbuf = new unsigned char[pipw * piph * 4];
    yuv2rgb_fun convert = yuv2rgb_init_mmx(32, MODE_RGB);

    convert(outputbuf, pipbuf, pipbuf + (pipw * piph), 
            pipbuf + (pipw * piph * 5 / 4), pipw, piph,
            pipw * 4, pipw, pipw / 2, 1);

    pipplayer->ReleaseCurrentFrame(pipimage);

    if (frame->width < 0)
        frame->width = video_dim.width();

    for (int i = 0; i < piph; i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff * 4,
               outputbuf + i * pipw * 4, pipw * 4);
    }

    delete [] outputbuf;
}

void VideoOutputIvtv::ProcessFrame(VideoFrame *frame, OSD *osd,
                                   FilterChain *filterList, 
                                   NuppelVideoPlayer *pipPlayer) 
{ 
    (void)filterList;
    (void)frame;

    if (fbfd < 0)
        return;

    if (!osd && !pipon)
        return;

    if (embedding && alphaState != kAlpha_Embedded)
        SetAlpha(kAlpha_Embedded);
    else if (!embedding && alphaState == kAlpha_Embedded && lastcleared)
        SetAlpha(kAlpha_Clear);

    if (embedding)
        return;

    VideoFrame tmpframe;
    init(&tmpframe, FMT_ARGB32, (unsigned char*) osdbuf_aligned,
         stride, video_dim.height(), 32, stride * video_dim.height());

    OSDSurface *surface = NULL;
    if (osd)
        surface = osd->Display();

    // Clear osdbuf if OSD has changed, or PiP has been toggled
    bool clear = (pipPlayer!=0) ^ pipon;
    int new_revision = osdbuf_revision;
    if (surface)
    {
        new_revision = surface->GetRevision();
        clear |= surface->GetRevision() != osdbuf_revision;
    }

    bool drawanyway = false;
    if (clear)
    {
        bzero(tmpframe.buf, video_dim.height() * stride);
        drawanyway = true;
    }

    if (pipPlayer)
    {
        ShowPip(&tmpframe, pipPlayer);
        osdbuf_revision = 0xfffffff; // make sure OSD is redrawn
        lastcleared = false;
        drawanyway  = true;
    }

    int ret = 0;
    ret = DisplayOSD(&tmpframe, osd, stride, osdbuf_revision);
    osdbuf_revision = new_revision;

    // Handle errors, such as no surface, by clearing OSD surface.
    // If there is a PiP, we need to actually clear the buffer, otherwise
    // we can get away with setting the alpha to kAlpha_Clear.
    if (ret < 0 && osdon)
    {
        if (!clear || pipon)
        {
            bzero(tmpframe.buf, video_dim.height() * stride);
            // redraw PiP...
            if (pipPlayer)
                ShowPip(&tmpframe, pipPlayer);
        }
        drawanyway  |= !lastcleared || pipon;
        lastcleared &= !pipon;
    }

    // Set these so we know if/how to clear if need be, the next time around.
    osdon = (ret >= 0);
    pipon = (bool) pipPlayer;

    // If there is an OSD, make sure we draw OSD surface
    lastcleared &= !osdon;

    // If nothing on OSD surface, just set the alpha to zero
    if (!osdon && !pipon)
    {
        if (lastcleared)
            return;

        lastcleared = true;
        SetAlpha(kAlpha_Clear);

        if (color_key)
#ifdef WORDS_BIGENDIAN
            wmemset((wchar_t*) osdbuf_aligned, 0x01000100, osdbufsize/4);
#else
            wmemset((wchar_t*) osdbuf_aligned, 0x00010001, osdbufsize/4);
#endif 
    }

    // If there has been no OSD change and no draw has been forced we're done
    if (ret <= 0 && !drawanyway)
        return;

    // The OSD surface needs to be updated...
    struct ivtvfb_ioctl_dma_host_to_ivtv_args prep;
    bzero(&prep, sizeof(prep));
    prep.source = osdbuf_aligned;
    prep.count  = video_dim.height() * stride;

     // This shouldn't be here. OSD should be rendered correctly to start with
#ifdef WORDS_BIGENDIAN
    int b_index, i_index;
    unsigned int *osd_int = (unsigned int*) osdbuf_aligned;
    if (!lastcleared)
    {
        for (b_index = 0, i_index = 0; b_index < prep.count;
             b_index += 4, i_index ++)
        {
            if (osd_int[i_index])
            {
                osd_int[i_index] =
                    ((unsigned char)osdbuf_aligned[b_index+0]) |
                    ((unsigned char)osdbuf_aligned[b_index+1] << 8) |
                    ((unsigned char)osdbuf_aligned[b_index+2] << 16) |
                    ((unsigned char)osdbuf_aligned[b_index+3] << 24);
            }
        }
    }
#endif

    if (!old_fb_ioctl)
        ioctl(fbfd, FBIOPAN_DISPLAY, &priv->ivtvfb_var);

    if (ioctl(fbfd, fb_dma_ioctl, &prep) < 0)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to process frame" + ENO);

    if (!lastcleared)
        SetAlpha(kAlpha_Local);
}

/** \fn VideoOutputIvtv::Start(int,int)
 *  \brief Start decoding
 *  \param skip Sets GOP offset
 *  \param mute If true mutes audio
 */
void VideoOutputIvtv::Start(int skip, int mute)
{
    VERBOSE(VB_PLAYBACK, LOC + "Start("<<skip<<" skipped, "
            <<mute<<" muted) -- begin");
    struct ivtv_cfg_start_decode start;
    bzero(&start, sizeof(start));
    start.gop_offset = skip;
    start.muted_audio_frames = mute;

    if (has_v4l2_api)
    {
        struct video_command cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = VIDEO_CMD_PLAY;
        cmd.play.speed = 1000;
        ioctl(videofd, VIDEO_COMMAND, &cmd);
        paused = false;
    }
    else
    {
        while (ioctl(videofd, IVTV_IOC_START_DECODE, &start) < 0)
        {
            if (errno != EBUSY)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to start decoder" + ENO);
                break;
            }
        }
    }
    VERBOSE(VB_PLAYBACK, LOC + "Start("<<skip<<" skipped, "
            <<mute<<" muted) -- end");
}

/** \fn VideoOutputIvtv::Stop(bool)
 *  \brief Stops decoding
 *  \param hide If true we hide the last video decoded frame.
 */
void VideoOutputIvtv::Stop(bool hide)
{
    VERBOSE(VB_PLAYBACK, LOC + "Stop("<<hide<<") -- begin");
    struct ivtv_cfg_stop_decode stop;
    bzero(&stop, sizeof(stop));
    stop.hide_last = hide;

    if (has_v4l2_api)
    {
        struct video_command cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = VIDEO_CMD_STOP;
        cmd.flags = VIDEO_CMD_STOP_IMMEDIATELY;
        if (hide)
            cmd.flags |= VIDEO_CMD_STOP_TO_BLACK;
        if (ioctl(videofd, VIDEO_COMMAND, &cmd) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to stop decoder" + ENO);
    }
    else
    {
        while (ioctl(videofd, IVTV_IOC_STOP_DECODE, &stop) < 0)
        {
            if (errno != EBUSY)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to stop decoder" + ENO);
                break;
            }
        }
    }

    frame_at_speed_change = 0;
    internal_offset       = 0;
    VERBOSE(VB_PLAYBACK, LOC + "Stop("<<hide<<") -- end");
}

/** \fn VideoOutputIvtv::Pause(void)
 *  \brief Pauses decoding
 */
void VideoOutputIvtv::Pause(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Pause() -- begin");
    if (has_v4l2_api)
    {
        struct video_command cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = VIDEO_CMD_FREEZE;
        ioctl(videofd, VIDEO_COMMAND, &cmd);
        paused = true;
    }
    else
    {
        while (ioctl(videofd, IVTV_IOC_PAUSE, 0) < 0)
        {
            if (errno != EBUSY)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to pause decoder" + ENO);
                break;
            }
        }
    }
    VERBOSE(VB_PLAYBACK, LOC + "Pause() -- end");
}

/** \fn VideoOutputIvtv::Poll(int)
 *  \brief Waits for decoder to be ready for more data
 *  \param delay milliseconds to wait before timing out.
 *  \return value returned by POSIX poll() function
 */
int VideoOutputIvtv::Poll(int delay)
{
    //VERBOSE(VB_PLAYBACK, LOC + "Poll("<<delay<<") -- begin");
    struct pollfd polls;
    polls.fd = videofd;
    polls.events = POLLOUT;
    polls.revents = 0;

    int res = poll(&polls, 1, delay);

    if (res < 0)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Polling" + ENO);

    //VERBOSE(VB_PLAYBACK, LOC + "Poll("<<delay<<") -- end");
    return res;
}

/** \fn VideoOutputIvtv::WriteBuffer(unsigned char*,int)
 *  \brief Writes data to the decoder device
 *  \param buf buffer to write to the decoder
 *  \param len number of bytes to write
 *  \return actual number of bytes written
 */
uint VideoOutputIvtv::WriteBuffer(unsigned char *buf, int len)
{
    int count = write(videofd, buf, len);

    if (count < 0)
    {
        if (errno != EAGAIN)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Writing to decoder" + ENO);
            return count;
        }
        count = 0;
    }

    return count;
}

/** \fn VideoOutputIvtv::GetFirmwareFramesPlayed(void)
 *  \brief Returns number of frames decoded as reported by decoder.
 */
long long VideoOutputIvtv::GetFirmwareFramesPlayed(void)
{
    if (has_v4l2_api)
    {
        long long frame;
        if (ioctl(videofd, VIDEO_GET_FRAME_COUNT, &frame) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Fetching frames played from decoder" + ENO);
        return frame;
    }
    else
    {
        struct ivtv_ioctl_framesync frameinfo;
        bzero(&frameinfo, sizeof(frameinfo));

        if (ioctl(videofd, IVTV_IOC_GET_TIMING, &frameinfo) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Fetching frames played from decoder" + ENO);
        }

        return frameinfo.frame;
    }
}

/** \fn VideoOutputIvtv::GetFramesPlayed(void)
 *  \brief Returns number of frames played since last reset.
 *
 *   This adjust the value returned by GetFirmwareFramesPlayed(void)
 *   to report the number of frames played since playback started
 *   irrespective of current and past playback speeds.
 */
long long VideoOutputIvtv::GetFramesPlayed(void)
{
    long long frame = GetFirmwareFramesPlayed();
    float f = internal_offset + (frame - frame_at_speed_change) * last_speed;
    return (long long)round(f);
}

/** \fn VideoOutputIvtv::Play(float speed, bool normal, int mask)
 *  \brief Initializes decoder parameters
 */
bool VideoOutputIvtv::Play(float speed, bool normal, int mask)
{
    VERBOSE(VB_PLAYBACK, LOC + "Play("<<speed<<", "<<normal<<", "<<mask<<")");
    internal_offset = GetFramesPlayed();
    frame_at_speed_change = GetFirmwareFramesPlayed();

    speed = (speed >= 2.0f)  ? 2.0f : speed;
    speed = (speed <= 0.05f) ? 1.0f : speed;

    if (has_v4l2_api)
    {
        struct video_command cmd;
        // Some ivtv versions can't resume playback properly from pause.
        if (has_pause_bug && paused && speed == 1.0f)
        {
            // Work around the bug by stepping up to playback speed
            memset(&cmd, 0, sizeof(cmd));
            cmd.cmd = VIDEO_CMD_PLAY;
            cmd.play.speed = 500;
            ioctl(videofd, VIDEO_COMMAND, &cmd);
        }
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = VIDEO_CMD_PLAY;
        cmd.play.speed = (__u32)(1000.0f * speed);
        ioctl(videofd, VIDEO_COMMAND, &cmd);
        paused = false;
    }
    else
    {
        struct ivtv_speed play;
        bzero(&play, sizeof(play));
        play.scale = (speed >= 2.0f) ? (int)roundf(speed) : 1;
        play.scale = (speed <= 0.5f) ? (int)roundf(1.0f / speed) : play.scale;
        play.speed = (speed > 1.0f);
        play.smooth = 0;
        play.direction = 0;
        play.fr_mask = mask;
        play.b_per_gop = 0;
        play.aud_mute = !normal;
        play.fr_field = 0;
        play.mute = 0;

        while (ioctl(videofd, IVTV_IOC_S_SPEED, &play) < 0)
        {
            if (errno != EBUSY)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Setting decoder's playback speed" + ENO);
                break;
            }
            usleep(1000);
        }
    }

    last_speed = speed;
    last_normal = normal;
    last_mask = mask;

    return true;
}

/** \fn VideoOutputIvtv::Flush(void)
 *  \brief Flushes out data already sent to decoder.
 */
void VideoOutputIvtv::Flush(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Flush()");
    if (decoder_flush)
    {
        int arg = 0;
        if (ioctl(videofd, IVTV_IOC_DEC_FLUSH, &arg) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Flushing decoder" + ENO);
    }
}

/** \fn VideoOutputIvtv::Step(void)
 *  \brief Step through video one frame at a time.
 */
void VideoOutputIvtv::Step(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Step()");

    if (has_v4l2_api)
    {
        struct video_command cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.cmd = VIDEO_CMD_PLAY;
        cmd.play.speed = 1;
        ioctl(videofd, VIDEO_COMMAND, &cmd);
    }
    else
    {
        int arg = 0; // STEP_FRAME;

        while (ioctl(videofd, IVTV_IOC_DEC_STEP, &arg) < 0)
        {
            if (errno != EBUSY)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting Step" + ENO);
                break;
            }
            usleep(1000);
        }
    }
}

QStringList VideoOutputIvtv::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    QStringList list;

    if (((kCodec_MPEG1 == myth_codec_id) ||
         (kCodec_MPEG2 == myth_codec_id)) &&
        (video_dim.width() <= 720) &&
        (video_dim.height() <= 576))
    {
        list += "ivtv";
    }

    return list;
}
