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

// C++ headers
#include <map>
#include <iostream>
using namespace std;

#include "videodev_myth.h"
#include "videodev2_myth.h"

#include "videoout_ivtv.h"
extern "C" {
#include <inttypes.h>
#ifdef USING_IVTV_HEADER
#include <linux/ivtv.h>
#else
#include "ivtv_myth.h"
#endif
}

#include "libmyth/mythcontext.h"

#include "NuppelVideoPlayer.h"
#include "../libavcodec/avcodec.h"
#include "yuv2rgb.h"
#include "osd.h"
#include "osdsurface.h"

#define LOC QString("IVD: ")
#define LOC_ERR QString("IVD Error: ")

VideoOutputIvtv::VideoOutputIvtv(void) :
    videofd(-1),              fbfd(-1),
    fps(30000.0f/1001.0f),    videoDevice("/dev/video16"),
    driver_version(0),

    mapped_offset(0),         mapped_memlen(0),
    mapped_mem(NULL),         pixels(NULL),

    stride(0),

    lastcleared(false),
    osdbuffer(NULL),          osdbuf_aligned(NULL),
    osdbufsize(0),            osdbuf_revision(0xfffffff),

    last_speed(1.0f),
    internal_offset(0),       frame_at_speed_change(0),

    last_normal(true),        last_mask(0x2),

    alphaState(kAlpha_Solid)
{
}

VideoOutputIvtv::~VideoOutputIvtv()
{
    Close();

    if (fbfd >= 0)
    {
        ClearOSD();
        SetAlpha(kAlpha_Solid);

        close(fbfd);
    }

    if (osdbuffer)
        delete [] osdbuffer;
}

void VideoOutputIvtv::ClearOSD(void) 
{
    if (fbfd >= 0)
    {
        struct ivtv_osd_coords osdcoords;
        bzero(&osdcoords, sizeof(osdcoords));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_ACTIVE_BUFFER, &osdcoords) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to get active buffer for ClearOSD()" + ENO);
        }
        struct ivtvfb_ioctl_dma_host_to_ivtv_args prep;
        bzero(&prep, sizeof(prep));

        prep.source = osdbuf_aligned;
        prep.dest_offset = 0;
        prep.count = osdcoords.max_offset;

        bzero(osdbuf_aligned, osdbufsize);

        if (ioctl(fbfd, IVTVFB_IOCTL_PREP_FRAME, &prep) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to prepare frame" + ENO);
    }
}

void VideoOutputIvtv::SetAlpha(eAlphaState newAlphaState)
{
    if (alphaState == newAlphaState)
        return;

    alphaState = newAlphaState;

    struct ivtvfb_ioctl_state_info fbstate;
    bzero(&fbstate, sizeof(fbstate));
    if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to query alpha state" + ENO);

    if (alphaState == kAlpha_Local)
    {
        fbstate.status &= ~IVTVFB_STATUS_GLOBAL_ALPHA;
        fbstate.status |= IVTVFB_STATUS_LOCAL_ALPHA;
    }
    else
    {
        fbstate.status |= IVTVFB_STATUS_GLOBAL_ALPHA;
        fbstate.status &= ~IVTVFB_STATUS_LOCAL_ALPHA;
    }

    if (alphaState == kAlpha_Solid)
        fbstate.alpha = 255;
    else if (alphaState == kAlpha_Clear)
        fbstate.alpha = 0;
    else if (alphaState == kAlpha_Embedded)
        fbstate.alpha = gContext->GetNumSetting("PVR350EPGAlphaValue", 164);

    if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to set ivtv alpha values." + ENO);
}

void VideoOutputIvtv::InputChanged(int width, int height, float aspect)
{
    VERBOSE(VB_PLAYBACK, LOC + "InputChanged() -- begin");
    VideoOutput::InputChanged(width, height, aspect);
    MoveResize();
    VERBOSE(VB_PLAYBACK, LOC + "InputChanged() -- end");
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
    allowpreviewepg = false;

    videoDevice = gContext->GetSetting("PVR350VideoDev");

    VideoOutput::Init(width, height, aspect, winid, winx, winy, winw, winh, 
                      embedid);

    osdbufsize = width * height * 4;

    MoveResize();

    Open();

    if (videofd < 0)
        return false;

    if (fbfd < 0)
    {
        int fbno = 0;

        if (ioctl(videofd, IVTV_IOC_GET_FB, &fbno) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Framebuffer number query failed." + ENO +
                    "\n\t\t\tDid you load the ivtv-fb Linux kernel module?");
            return false;
        }

        if (fbno < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to determine framebuffer number." +
                    "\n\t\t\tDid you load the ivtv-fb Linux kernel module?");
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

        struct ivtvfb_ioctl_get_frame_buffer igfb;
        bzero(&igfb, sizeof(igfb));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_FRAME_BUFFER, &igfb) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Getting frame buffer" + ENO);

        stride = igfb.sizex * 4;

        long pagesize = sysconf(_SC_PAGE_SIZE);
        long pagemask = ~(pagesize-1);
        osdbuffer = new char[osdbufsize + pagesize];
        osdbuf_aligned = osdbuffer + (pagesize - 1);
        osdbuf_aligned = (char *)((unsigned long)osdbuf_aligned & pagemask);

        bzero(osdbuf_aligned, osdbufsize);

        ClearOSD();

        struct ivtv_osd_coords osdcoords;
        bzero(&osdcoords, sizeof(osdcoords));
        osdcoords.lines = XJ_height;
        osdcoords.offset = 0;
        osdcoords.pixel_stride = XJ_width * 2;

        if (ioctl(fbfd, IVTVFB_IOCTL_SET_ACTIVE_BUFFER, &osdcoords) < 0)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting active buffer" + ENO);
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to query decoder" + ENO);
    else
        driver_version = vcap.version;
    VERBOSE(VB_PLAYBACK, LOC + "Open() -- end");
}

void VideoOutputIvtv::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    (void)buffer;
    (void)t;
}

void VideoOutputIvtv::Show(FrameScanType )
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

    if (pipw != desired_pipw || piph != desired_piph)
    {
        DoPipResize(pipw, piph);

        if (piptmpbuf && pipscontext)
        {
            AVPicture img_in, img_out;

            avpicture_fill(&img_out, (uint8_t *)piptmpbuf, PIX_FMT_YUV420P,
                           pipw_out, piph_out);
            avpicture_fill(&img_in, (uint8_t *)pipimage->buf, PIX_FMT_YUV420P,
                           pipw, piph);

            img_resample(pipscontext, &img_out, &img_in);

            pipw = pipw_out;
            piph = piph_out;

            pipbuf = piptmpbuf;
        }
    }

    switch (PIPLocation)
    {
        default:
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
        frame->width = XJ_width;

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

    if (fbfd >= 0 && osd)
    {
        bool drawanyway = false;

        if (embedding && alphaState != kAlpha_Embedded)
            SetAlpha(kAlpha_Embedded);
        else if (!embedding && alphaState == kAlpha_Embedded && lastcleared)
            SetAlpha(kAlpha_Clear);

        VideoFrame tmpframe;
        tmpframe.codec = FMT_ARGB32;
        tmpframe.buf = (unsigned char *)osdbuf_aligned;
        tmpframe.width = stride;
        tmpframe.height = XJ_height;

        // Clear osdbuf if OSD has changed.
        OSDSurface *surface = osd->Display();
        int new_revision = osdbuf_revision;
        if (surface && (surface->GetRevision() != osdbuf_revision))
        {
            bzero(tmpframe.buf, XJ_height * stride);
            new_revision = surface->GetRevision();
        }

        int ret = DisplayOSD(&tmpframe, osd, stride, osdbuf_revision);
        osdbuf_revision = new_revision;

        if (ret < 0 && !lastcleared)
        {
            lastcleared = true;
            drawanyway = true;
        }

        if (pipPlayer)
        {
            ShowPip(&tmpframe, pipPlayer);
            drawanyway = true;
            lastcleared = false;
        }

        if (ret >= 0)
            lastcleared = false;

        if (lastcleared && drawanyway)
        {
            if (!embedding)
                SetAlpha(kAlpha_Clear);
            lastcleared = true;
        } 
        else if (ret > 0 || drawanyway)
        {
            if (embedding)
                return;

            struct ivtvfb_ioctl_dma_host_to_ivtv_args prep;
            bzero(&prep, sizeof(prep));

            prep.source = osdbuf_aligned;
            prep.dest_offset = 0;
            prep.count = XJ_height * stride;

            if (ioctl(fbfd, IVTVFB_IOCTL_PREP_FRAME, &prep) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to process frame" + ENO);
            }
            if (alphaState != kAlpha_Local)
                SetAlpha(kAlpha_Local);
        }
    }
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

    while (ioctl(videofd, IVTV_IOC_START_DECODE, &start) < 0)
    {
        if (errno != EBUSY)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to start decoder" + ENO);
            break;
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

    while (ioctl(videofd, IVTV_IOC_STOP_DECODE, &stop) < 0)
    {
        if (errno != EBUSY)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to stop decoder" + ENO);
            break;
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
    while (ioctl(videofd, IVTV_IOC_PAUSE, 0) < 0)
    {
        if (errno != EBUSY)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to pause decoder" + ENO);
            break;
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
    struct ivtv_ioctl_framesync frameinfo;
    bzero(&frameinfo, sizeof(frameinfo));

    if (ioctl(videofd, IVTV_IOC_GET_TIMING, &frameinfo) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Fetching frames played from decoder" + ENO);
    }
    //cerr<<"<"<<frameinfo.frame<<">";
    return frameinfo.frame;
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

    internal_offset = GetFramesPlayed();
    frame_at_speed_change = GetFirmwareFramesPlayed();

    while (ioctl(videofd, IVTV_IOC_S_SPEED, &play) < 0)
    {
        if (errno != EBUSY)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Setting decoder's playback speed" + ENO);
            break;
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
    int arg = 0;

    if (ioctl(videofd, IVTV_IOC_DEC_FLUSH, &arg) < 0)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Flushing decoder" + ENO);
}

/** \fn VideoOutputIvtv::Step(void)
 *  \brief Step through video one frame at a time.
 */
void VideoOutputIvtv::Step(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Step()");
    enum {
        STEP_FRAME     = 0,
        STEP_TOP_FIELD = 1,
        STEP_BOT_FIELD = 2,
    };

    int arg = STEP_FRAME;

    while (ioctl(videofd, IVTV_IOC_DEC_STEP, &arg) < 0)
    {
        if (errno != EBUSY)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Setting Step" + ENO);
            break;
        }
    }
}
