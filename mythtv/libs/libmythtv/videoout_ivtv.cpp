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

void VideoOutputIvtv::Reopen(int skipframes)
{
    if (videofd >= 0)
    {
        ivtv_cfg_stop_decode sd;
        memset(&sd, 0, sizeof(sd));

        ioctl(videofd, IVTV_IOC_S_STOP_DECODE, &sd);
        close(videofd);
    }

    videofd = -1;

    /* needs to be #defined*/
    if ((videofd = open(videoDevice.ascii(), O_WRONLY | O_LARGEFILE, 0555)) < 0)
        perror("Cannot open ivtv video out device");
    else
    {
        ivtv_cfg_start_decode sd;
        memset(&sd, 0, sizeof(sd));

        if (skipframes > 0)
            sd.gop_offset = skipframes;

        ioctl(videofd, IVTV_IOC_S_START_DECODE, &sd);
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
}

unsigned long VideoOutputIvtv::FrameSync()
{
    struct ivtv_ioctl_framesync frameinfo;
    if (ioctl(videofd, IVTV_IOC_FRAMESYNC, &frameinfo) < 0)
    {
        perror("IVTV_IOC_FRAMESYNC");
        return 0;
    }

    static const float MPEG_CLOCK_FREQ = 90000.0;
    unsigned long frame = (unsigned long)roundf(((float)frameinfo.pts /
                                                MPEG_CLOCK_FREQ) * fps);

    //return frameinfo.frame;
    return frame;
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
    int origcount = count, n = 0;

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

    return origcount;
}

