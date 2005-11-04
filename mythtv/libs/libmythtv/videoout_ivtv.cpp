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
#include <cerrno>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/param.h>

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

VideoOutputIvtv::VideoOutputIvtv(void)
{
    videofd = -1;
    fbfd = -1;
    pixels = NULL;
    lastcleared = false;
    videoDevice = "/dev/video16";
    driver_version = 0;
    last_speed = 1.0;
    frame_at_speed_change = 0;
    internal_offset = 0;
    last_normal = true;
    last_mask = 2;
    osdbuffer = NULL;
    alphaState = kAlpha_Solid;
}

VideoOutputIvtv::~VideoOutputIvtv()
{
    Close();

    if (fbfd > 0)
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
    if (fbfd > 0) 
    {
        struct ivtv_osd_coords osdcoords;
        memset(&osdcoords, 0, sizeof(osdcoords));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_ACTIVE_BUFFER, &osdcoords) < 0)
            perror("IVTVFB_IOCTL_GET_ACTIVE_BUFFER");
        struct ivtvfb_ioctl_dma_host_to_ivtv_args prep;
        memset(&prep, 0, sizeof(prep));

        prep.source = osdbuf_aligned;
        prep.dest_offset = 0;
        prep.count = osdcoords.max_offset;

        memset(osdbuf_aligned, 0x00, osdbufsize);

        if (ioctl(fbfd, IVTVFB_IOCTL_PREP_FRAME, &prep) < 0)
            perror("IVTVFB_IOCTL_PREP_FRAME");
    }
}

void VideoOutputIvtv::SetAlpha(eAlphaState newAlphaState)
{
    if (alphaState == newAlphaState)
        return;

    alphaState = newAlphaState;

    struct ivtvfb_ioctl_state_info fbstate;
    memset(&fbstate, 0, sizeof(fbstate));
    if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        perror("IVTVFB_IOCTL_GET_STATE");

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
        perror("IVTVFB_IOCTL_SET_STATE");
}

void VideoOutputIvtv::InputChanged(int width, int height, float aspect)
{
    VideoOutput::InputChanged(width, height, aspect);
    MoveResize();
}

int VideoOutputIvtv::GetRefreshRate(void)
{
    return 0;
}

bool VideoOutputIvtv::Init(int width, int height, float aspect, 
                           WId winid, int winx, int winy, int winw, 
                           int winh, WId embedid)
{
    allowpreviewepg = false;

    videoDevice = gContext->GetSetting("PVR350VideoDev");

    VideoOutput::Init(width, height, aspect, winid, winx, winy, winw, winh, 
                      embedid);

    osdbufsize = width * height * 4;

    MoveResize();

    Open();

    if (videofd <= 0)
        return false;

    if (fbfd == -1)
    {
        int fbno = 0;

        if (ioctl(videofd, IVTV_IOC_GET_FB, &fbno) < 0)
        {
            perror("IVTV_IOC_GET_FB");
            return false;
        }

        if (fbno < 0)
        {
            cerr << "invalid fb, are you using the ivtv-fb module?\n";
            return false;
        }

        QString fbdev = QString("/dev/fb%1").arg(fbno);
        fbfd = open(fbdev.ascii(), O_RDWR);
        if (fbfd < 0)
        {
            perror("ivtv framebuffer open");
            cerr << "Unable to open the ivtv framebuffer device '"
                 << fbdev.ascii() << "'\n";
            cerr << "OSD will be unavailable\n";
            return false;
        }

        struct ivtvfb_ioctl_get_frame_buffer igfb;
        memset(&igfb, 0, sizeof(igfb));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_FRAME_BUFFER, &igfb) < 0)
            perror("IVTVFB_IOCTL_GET_FRAME_BUFFER");

        stride = igfb.sizex * 4;

        long pagesize = sysconf(_SC_PAGE_SIZE);
        long pagemask = ~(pagesize-1);
        osdbuffer = new char[osdbufsize + pagesize];
        osdbuf_aligned = osdbuffer + (pagesize - 1);
        osdbuf_aligned = (char *)((unsigned long)osdbuf_aligned & pagemask);

        memset(osdbuf_aligned, 0x00, osdbufsize);

        ClearOSD();

        struct ivtv_osd_coords osdcoords;
        memset(&osdcoords, 0, sizeof(osdcoords));
        osdcoords.lines = XJ_height;
        osdcoords.offset = 0;
        osdcoords.pixel_stride = XJ_width * 2;

        if (ioctl(fbfd, IVTVFB_IOCTL_SET_ACTIVE_BUFFER, &osdcoords) < 0)
            perror("IVTVFB_IOCTL_SET_ACTIVE_BUFFER");
    }

    cout << "Using the PVR-350 decoder/TV-out\n";
    return true;
}

void VideoOutputIvtv::Close(void)
{
    if (videofd < 0)
        return;

    Stop(true);

    close(videofd);
    videofd = -1;
}

void VideoOutputIvtv::Open(void)
{
    if (videofd >= 0)
        return;

    if ((videofd = open(videoDevice.ascii(), 
        O_WRONLY | O_NONBLOCK, 0555)) < 0)
    {
        perror("Cannot open ivtv video out device");
        return;
    }

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
        perror("VIDIOC_QUERYCAP");
    else
        driver_version = vcap.version;
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

    if (fbfd > 0 && osd)
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

        int ret = DisplayOSD(&tmpframe, osd, stride);

        if (ret < 0 && !lastcleared)
        {
            memset(tmpframe.buf, 0x0, XJ_height * stride);
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
            memset(&prep, 0, sizeof(prep));

            prep.source = osdbuf_aligned;
            prep.dest_offset = 0;
            prep.count = XJ_height * stride;

            if (ioctl(fbfd, IVTVFB_IOCTL_PREP_FRAME, &prep) < 0)
                perror("IVTVFB_IOCTL_PREP_FRAME");
            if (alphaState != kAlpha_Local)
                SetAlpha(kAlpha_Local);
        }
    }
}

void VideoOutputIvtv::Start(int skip, int mute)
{
    struct ivtv_cfg_start_decode start;
    memset(&start, 0, sizeof start);
    start.gop_offset = skip;
    start.muted_audio_frames = mute;

    while (ioctl(videofd, IVTV_IOC_START_DECODE, &start) < 0)
    {
        if (errno != EBUSY)
        {
            perror("IVTV_IOC_START_DECODE");
            break;
        }
    }
}

void VideoOutputIvtv::Stop(bool hide)
{
    struct ivtv_cfg_stop_decode stop;
    memset(&stop, 0, sizeof stop);
    stop.hide_last = hide;

    while (ioctl(videofd, IVTV_IOC_STOP_DECODE, &stop) < 0)
    {
        if (errno != EBUSY)
        {
            perror("IVTV_IOC_STOP_DECODE");
            break;
        }
    }

    frame_at_speed_change = 0;
    internal_offset       = 0;
}

void VideoOutputIvtv::Pause(void)
{
    while (ioctl(videofd, IVTV_IOC_PAUSE, 0) < 0)
    {
        if (errno != EBUSY)
        {
            perror("IVTV_IOC_PAUSE");
            break;
        }
    }
}

int VideoOutputIvtv::Poll(int delay)
{
    struct pollfd polls;
    polls.fd = videofd;
    polls.events = POLLOUT;
    polls.revents = 0;

    int res = poll(&polls, 1, delay);

    if (res < 0)
        perror("Polling on videodev");

    return res;
}

int VideoOutputIvtv::WriteBuffer(unsigned char *buf, int len)
{
    int count;

    //cerr << "ivtv writing video... ";
    count = write(videofd, buf, len);
    if (count < 0)
    {
        if (errno != EAGAIN)
        {
            perror("Writing to videodev");
            return count;
        }
        count = 0;
    }
    //cerr << "wrote " << count << " of " << len << endl;

    return count;
}

int VideoOutputIvtv::GetFirmwareFramesPlayed(void)
{
    struct ivtv_ioctl_framesync frameinfo;
    memset(&frameinfo, 0, sizeof frameinfo);

    if (ioctl(videofd, IVTV_IOC_GET_TIMING, &frameinfo) < 0)
        perror("IVTV_IOC_GET_TIMING");

    return frameinfo.frame;
}

int VideoOutputIvtv::GetFramesPlayed(void)
{
    int frame = GetFirmwareFramesPlayed();
    float f = internal_offset + (frame - frame_at_speed_change) * last_speed;
    return (int)round(f);
}

bool VideoOutputIvtv::Play(float speed, bool normal, int mask)
{
    struct ivtv_speed play;
    memset(&play, 0, sizeof play);
    if (speed >= 2.0)
        play.scale = (int)roundf(speed);
    else if (speed <= 0.5)
        play.scale = (int)roundf(1 / speed);
    else
        play.scale = 1;
    play.smooth = 0;
    play.speed = (speed > 1.0);
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
            perror("IVTV_IOC_S_SPEED");
            break;
        }
    }

    last_speed = speed;
    last_normal = normal;
    last_mask = mask;

    return true;
}

void VideoOutputIvtv::Flush(void)
{
    int arg = 0;

    if (ioctl(videofd, IVTV_IOC_DEC_FLUSH, &arg) < 0)
        perror("IVTV_IOC_DEC_FLUSH");
}

void VideoOutputIvtv::Step(void)
{
    int arg = 0;

    while (ioctl(videofd, IVTV_IOC_DEC_STEP, &arg) < 0)
    {
        if (errno != EBUSY)
        {
            perror("IVTV_IOC_DEC_STEP");
            break;
        }
    }
}
