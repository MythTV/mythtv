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
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <sys/poll.h>

#include <map>
#include <iostream>
using namespace std;

#include "videodev_myth.h"
#include "videodev2_myth.h"

#include "videoout_ivtv.h"
extern "C" {
#include <inttypes.h>
#include "ivtv-ext-api.h"
}

#include "libmyth/mythcontext.h"

VideoOutputIvtv::VideoOutputIvtv(void)
{
    videofd = -1;
    fbfd = -1;
    pixels = NULL;
    lastcleared = false;
    videoDevice = "/dev/video16";
    skipplay = false;
    interruptdisplay = false;
}

VideoOutputIvtv::~VideoOutputIvtv()
{
    if (videofd)
    {
        ivtv_cfg_stop_decode sd;
        memset(&sd, 0, sizeof(sd));

        sd.hide_last = 1;
        ioctl(videofd, IVTV_IOC_S_STOP_DECODE, &sd);

        close(videofd);
    }
    videofd = -1;

    if (fbfd > 0)
    {
        struct ivtvfb_ioctl_state_info fbstate;
        memset(&fbstate, 0, sizeof(fbstate));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_GET_STATE");
            return;
        }

        fbstate.status = initglobalalpha;
        fbstate.alpha = storedglobalalpha;

        if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_SET_STATE");
            return;
        }

        ClearOSD();       
 
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

        ioctl(fbfd, IVTVFB_IOCTL_GET_ACTIVE_BUFFER, &osdcoords);

        struct ivtvfb_ioctl_dma_host_to_ivtv_args prep;
        memset(&prep, 0, sizeof(prep));

        prep.source = osdbuf_aligned;
        prep.dest_offset = 0;
        prep.count = osdcoords.max_offset;

        memset(osdbuf_aligned, 0x00, osdbufsize);

        ioctl(fbfd, IVTVFB_IOCTL_PREP_FRAME, &prep);

        usleep(20000);

        osdcoords.lines = XJ_height;
        osdcoords.offset = 0;
        osdcoords.pixel_stride = XJ_width * 2;

        ioctl(fbfd, IVTVFB_IOCTL_SET_ACTIVE_BUFFER, &osdcoords);
    }
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
                           unsigned int winid, int winx, int winy, int winw, 
                           int winh, unsigned int embedid)
{
    videoDevice = gContext->GetSetting("PVR350VideoDev");

    VideoOutput::Init(width, height, aspect, winid, winx, winy, winw, winh, 
                      embedid);

    osdbufsize = width * height * 4;

    MoveResize();

    Reopen();

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

        struct ivtvfb_ioctl_state_info fbstate;
        memset(&fbstate, 0, sizeof(fbstate));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_GET_STATE");
            return false;
        }

        initglobalalpha = fbstate.status;
        storedglobalalpha = fbstate.alpha;

        fbstate.status &= ~IVTVFB_STATUS_GLOBAL_ALPHA;
        fbstate.status |= IVTVFB_STATUS_LOCAL_ALPHA;
        fbstate.alpha = 0;

        if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_SET_STATE");
            return false;
        }

        struct ivtvfb_ioctl_get_frame_buffer igfb;
        memset(&igfb, 0, sizeof(igfb));

        ioctl(fbfd, IVTVFB_IOCTL_GET_FRAME_BUFFER, &igfb);

        stride = igfb.sizex * 4;

        osdbuffer = new char[osdbufsize + PAGE_SIZE];
        osdbuf_aligned = (char *)((int)osdbuffer + (PAGE_SIZE - 1));
        osdbuf_aligned = (char *)((int)osdbuf_aligned & PAGE_MASK);

        memset(osdbuf_aligned, 0x00, osdbufsize);

        ClearOSD();
    }

    cout << "Using the PVR-350 decoder/TV-out\n";
    return true;
}

void VideoOutputIvtv::Reopen(int skipframes, int newstartframe)
{
    if (videofd >= 0)
    {
        ivtv_cfg_stop_decode sd;
        memset(&sd, 0, sizeof(sd));

        ioctl(videofd, IVTV_IOC_S_STOP_DECODE, &sd);
        close(videofd);

        skipplay = true;
    }

    videofd = -1;

    if ((videofd = open(videoDevice.ascii(), O_WRONLY | O_LARGEFILE, 0555)) < 0)
    {
        perror("Cannot open ivtv video out device");
        return;
    }
    else
    {
        struct v4l2_control ctrl;
        memset(&ctrl, 0, sizeof(ctrl));

        ctrl.id = V4L2_CID_IVTV_DEC_PREBUFFER;
        ctrl.value = 0;

        ioctl(videofd, VIDIOC_S_CTRL, &ctrl);

        ctrl.id = V4L2_CID_IVTV_DEC_NUM_BUFFERS;
        ctrl.value = 0;

        ioctl(videofd, VIDIOC_S_CTRL, &ctrl);
        
        ivtv_cfg_start_decode startd;
        memset(&startd, 0, sizeof(startd));

        if (skipframes > 0)
        {
            startd.gop_offset = skipframes;
            startd.muted_audio_frames = 6;
        }

        ioctl(videofd, IVTV_IOC_S_START_DECODE, &startd);

        // change it back to hide last frame, in case we die unexpectedly.
        ivtv_cfg_stop_decode stopd;
        memset(&stopd, 0, sizeof(stopd));

        stopd.hide_last = 1;
        ioctl(videofd, IVTV_IOC_S_STOP_DECODE, &stopd);
    }

    startframenum = newstartframe;
    firstframe = true;
}

void VideoOutputIvtv::EmbedInWidget(unsigned long wid, int x, int y, int w, 
                                    int h)
{
    if (embedding)
        return;

    struct ivtvfb_ioctl_state_info fbstate;
    memset(&fbstate, 0, sizeof(fbstate));

    if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        perror("IVTVFB_IOCTL_GET_STATE");

    fbstate.status = initglobalalpha;
    fbstate.alpha = 164;

    if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        perror("IVTVFB_IOCTL_SET_STATE");

    VideoOutput::EmbedInWidget(wid, x, y, w, h);
}

void VideoOutputIvtv::StopEmbedding(void)
{
    if (!embedding)
        return;

    struct ivtvfb_ioctl_state_info fbstate;
    memset(&fbstate, 0, sizeof(fbstate));

    if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        perror("IVTVFB_IOCTL_GET_STATE");

    fbstate.status &= ~IVTVFB_STATUS_GLOBAL_ALPHA;
    fbstate.status |= IVTVFB_STATUS_LOCAL_ALPHA;
    fbstate.alpha = 0;

    if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        perror("IVTVFB_IOCTL_SET_STATE");

    VideoOutput::StopEmbedding();
}

void VideoOutputIvtv::PrepareFrame(VideoFrame *buffer)
{
    (void)buffer;
}

void VideoOutputIvtv::Show()
{
}

void VideoOutputIvtv::DrawUnusedRects(void) 
{ 
}

void VideoOutputIvtv::UpdatePauseFrame(void) 
{ 
}

void VideoOutputIvtv::ProcessFrame(VideoFrame *frame, OSD *osd,
                                   FilterChain *filterList, 
                                   NuppelVideoPlayer *pipPlayer) 
{ 
    (void)filterList;
    (void)frame;
    (void)pipPlayer;

    if (fbfd > 0 && osd)
    {
        bool drawanyway = false;

        VideoFrame tmpframe;
        tmpframe.codec = FMT_ARGB32;
        tmpframe.buf = (unsigned char *)osdbuf_aligned;

        int ret = DisplayOSD(&tmpframe, osd, stride);

        if (ret < 0 && !lastcleared)
        {
            memset(tmpframe.buf, 0x0, XJ_height * stride);
            lastcleared = true;
            drawanyway = true;
        }

        if (ret >= 0)
            lastcleared = false;

        if (ret > 0 || drawanyway)
        {
            struct ivtvfb_ioctl_dma_host_to_ivtv_args prep;
            memset(&prep, 0, sizeof(prep));

            prep.source = osdbuf_aligned;
            prep.dest_offset = 0;
            prep.count = XJ_height * stride;

            ioctl(fbfd, IVTVFB_IOCTL_PREP_FRAME, &prep);
        }
    }
}

void VideoOutputIvtv::Play()
{
    if (!skipplay)
        ioctl(videofd, IVTV_IOC_PLAY, 0);
    skipplay = false;
}

void VideoOutputIvtv::Pause()
{
    ioctl(videofd, IVTV_IOC_PAUSE, 0);
}

void VideoOutputIvtv::InterruptDisplay(void)
{
    interruptdisplay = true;
}

int VideoOutputIvtv::WriteBuffer(unsigned char *buf, int count, int &frames)
{
    int n = 0;

    struct pollfd polls;
    polls.fd = videofd;
    polls.events = POLLOUT;
    polls.revents = 0;

    int ret = 0;
    int totalpassed = count;
    const int maxwrite = 32768;
    
    while (count > 0 && !interruptdisplay)
    {
        ret = poll(&polls, 1, 20);

        if (ret == 1 && polls.revents & POLLOUT)
        {
            n = write(videofd, buf, (count > maxwrite) ? maxwrite : count);
            if (n < 0)
            {
                perror("Writing to videodev");
                return n;
            }
            count -= n;
            buf += n;
        }

        if (interruptdisplay)
        {
            break;
        }
    }

    interruptdisplay = false;
       
    struct ivtv_ioctl_framesync frameinfo;
    memset(&frameinfo, 0, sizeof(frameinfo));
    if (ioctl(videofd, IVTV_IOC_GET_TIMING, &frameinfo) < 0)
    {
        perror("IVTV_IOC_FRAMESYNC");
        frames = 0;
        return (totalpassed - count);
    }

    // seems the decoder doesn't initialize this properly.
    if (frameinfo.frame >= 20000000)
        frameinfo.frame = 0;

    if (firstframe)
    {
        if (frameinfo.frame > 5)
            frameinfo.frame = 0;
        else
            firstframe = false;
    }

    interruptdisplay = false;
    
    frames = frameinfo.frame + startframenum;
    return totalpassed - count;
}

