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
#include <linux/fb.h>

#include <map>
#include <iostream>
using namespace std;

#include "videoout_ivtv.h"
extern "C" {
#include <inttypes.h>
#include "ivtv-ext-api.h"
}

VideoOutputIvtv::VideoOutputIvtv(void)
{
    videofd = -1;
    fbfd = -1;
    mapped_mem = NULL;
    pixels = NULL;
    lastcleared = false;
    videoDevice = "/dev/video16";
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

    if (fbfd)
    {
        struct ivtvfb_ioctl_state_info fbstate;
        memset(&fbstate, 0, sizeof(fbstate));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_GET_STATE");
            return;
        }

        initglobalalpha = fbstate.status & IVTVFB_STATUS_GLOBAL_ALPHA;
        storedglobalalpha = fbstate.alpha;

        if (initglobalalpha)
            fbstate.status |= IVTVFB_STATUS_GLOBAL_ALPHA;
        else
            fbstate.status &= ~IVTVFB_STATUS_GLOBAL_ALPHA;

        fbstate.alpha = storedglobalalpha;

        if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_SET_STATE");
            return;
        }

        if (mapped_mem)
        {
            memset(pixels, 0x00, 720 * 480 * 4);
            munmap(mapped_mem, mapped_memlen);
        }

        close(fbfd);
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
    VideoOutput::Init(width, height, aspect, winid, winx, winy, winw, winh, 
                      embedid);
    MoveResize();

    Reopen();

    if (videofd >= 0)
        return false;

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
    }

    videofd = -1;

    if ((videofd = open(videoDevice.ascii(), O_WRONLY | O_LARGEFILE, 0555)) < 0)
    {
        perror("Cannot open ivtv video out device");
        return;
    }
    else
    {
        ivtv_cfg_start_decode startd;
        memset(&startd, 0, sizeof(startd));

        if (skipframes > 0)
            startd.gop_offset = skipframes;

        ioctl(videofd, IVTV_IOC_S_START_DECODE, &startd);

        // change it back to hide last frame, in case we die unexpectedly.
        ivtv_cfg_stop_decode stopd;
        memset(&stopd, 0, sizeof(stopd));

        stopd.hide_last = 1;
        ioctl(videofd, IVTV_IOC_S_STOP_DECODE, &stopd);
    }

    startframenum = newstartframe;

    if (fbfd == -1)
    {
        int fbno = 0;
        
        if (ioctl(videofd, IVTV_IOC_GET_FB, &fbno) < 0)
        {
            perror("IVTV_IOC_GET_FB");
            return;
        }

        if (fbno < 0)
        {
            cerr << "invalid fb, are you using the ivtv-fb module?\n";
            return;
        }

        QString fbdev = QString("/dev/fb%1").arg(fbno);
        fbfd = open(fbdev.ascii(), O_RDWR);
        if (fbfd < 0)
        {
            cerr << "Unable to open the ivtv framebuffer\n";
            return;
        }

        struct ivtvfb_ioctl_state_info fbstate;
        memset(&fbstate, 0, sizeof(fbstate));

        if (ioctl(fbfd, IVTVFB_IOCTL_GET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_GET_STATE");
            return;
        }

        initglobalalpha = fbstate.status & IVTVFB_STATUS_GLOBAL_ALPHA;
        storedglobalalpha = fbstate.alpha;

        fbstate.status &= ~IVTVFB_STATUS_GLOBAL_ALPHA;
        fbstate.status |= IVTVFB_STATUS_LOCAL_ALPHA;
        fbstate.alpha = 0;

        if (ioctl(fbfd, IVTVFB_IOCTL_SET_STATE, &fbstate) < 0)
        {
            perror("IVTVFB_IOCTL_SET_STATE");
            return;
        }

        struct ivtvfb_ioctl_get_frame_buffer igfb;
        memset(&igfb, 0, sizeof(igfb));

        ioctl(fbfd, IVTVFB_IOCTL_GET_FRAME_BUFFER, &igfb);

        struct ivtv_osd_coords osdcoords;
        memset(&osdcoords, 0, sizeof(osdcoords));

        ioctl(fbfd, IVTVFB_IOCTL_GET_ACTIVE_BUFFER, &osdcoords);

        mapped_offset = osdcoords.offset;
        mapped_memlen = osdcoords.max_offset;

        mapped_mem = (char *)mmap(0, mapped_memlen, PROT_READ|PROT_WRITE,
                                  MAP_SHARED, fbfd, mapped_offset);
        if (mapped_mem == (char *)-1)
        {
            perror("Unable to mmap ivtv-fb buffer");
            mapped_mem = NULL;
            return;
        }

        pixels = mapped_mem;
        width = igfb.sizex;
        height = igfb.sizey;
        stride = igfb.sizex * 4;

        memset(pixels, 0x00, 720 * 480 * 4);
    }
}

void VideoOutputIvtv::EmbedInWidget(unsigned long wid, int x, int y, int w, 
                                    int h)
{
    if (embedding)
        return;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);
}

void VideoOutputIvtv::StopEmbedding(void)
{
    if (!embedding)
        return;

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
                                   vector<VideoFilter *> &filterList, 
                                   NuppelVideoPlayer *pipPlayer) 
{ 
    (void)filterList;
    (void)frame;
    (void)pipPlayer;

    if (mapped_mem && osd)
    {
        VideoFrame tmpframe;
        tmpframe.codec = FMT_ARGB32;
        tmpframe.buf = (unsigned char *)pixels;

        int ret = DisplayOSD(&tmpframe, osd, stride);

        if (ret < 0 && !lastcleared)
        {
            memset(pixels, 0x0, XJ_height * stride);
            lastcleared = true;
        }

        if (ret >= 0)
            lastcleared = false;
    }
}

void VideoOutputIvtv::Play()
{
    ioctl(videofd, IVTV_IOC_PLAY, 0);
}

void VideoOutputIvtv::Pause()
{
    ioctl(videofd, IVTV_IOC_PAUSE, 0);
}

int VideoOutputIvtv::WriteBuffer(unsigned char *buf, int count)
{
    int n = 0;

    while (count > 0)
    {
        n = write(videofd, buf, count);
        if (n < 0)
        {
            perror("Writing to videodev");
            return n;
        }
        count -= n;
        buf += n;
    }

    struct ivtv_ioctl_framesync frameinfo;
    memset(&frameinfo, 0, sizeof(frameinfo));
    if (ioctl(videofd, IVTV_IOC_GET_TIMING, &frameinfo) < 0)
    {
        perror("IVTV_IOC_FRAMESYNC");
        return 0;
    }

    // seems the decoder doesn't initialize this properly.
    if (frameinfo.frame >= 20000000)
        frameinfo.frame = 0;

    return frameinfo.frame + startframenum;
}

