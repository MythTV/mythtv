#include <map>
#include <iostream>
using namespace std;

#include "videoout_null.h"

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

VideoOutputNull::VideoOutputNull(void)
               : VideoOutput()
{
    XJ_started = 0; 

    pauseFrame.buf = NULL;
}

VideoOutputNull::~VideoOutputNull()
{
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    Exit();
}

void VideoOutputNull::AspectChanged(float aspect)
{
    VideoOutput::AspectChanged(aspect);
    MoveResize();
}

void VideoOutputNull::Zoom(int direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputNull::InputChanged(int width, int height, float aspect)
{
    VideoOutput::InputChanged(width, height, aspect);

    DeleteNullBuffers();
    CreateNullBuffers();

    MoveResize();

    scratchFrame = &(vbuffers[kNumBuffers]);

    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width = scratchFrame->width;
    pauseFrame.bpp = scratchFrame->bpp;
    pauseFrame.size = scratchFrame->size;
    pauseFrame.buf = new unsigned char[pauseFrame.size];
    pauseFrame.frameNumber = scratchFrame->frameNumber;
}

int VideoOutputNull::GetRefreshRate(void)
{
    return 0;
}

bool VideoOutputNull::Init(int width, int height, float aspect,
                           WId winid, int winx, int winy, int winw, 
                           int winh, WId embedid)
{
    VideoOutput::InitBuffers(kNumBuffers, true, kNeedFreeFrames, 
                             kPrebufferFramesNormal, kPrebufferFramesSmall, 
                             kKeepPrebuffer);
    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    if (!CreateNullBuffers())
        return false;

    scratchFrame = &(vbuffers[kNumBuffers]);

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width = scratchFrame->width;
    pauseFrame.bpp = scratchFrame->bpp;
    pauseFrame.size = scratchFrame->size;
    pauseFrame.buf = new unsigned char[pauseFrame.size];
    pauseFrame.frameNumber = scratchFrame->frameNumber;

    MoveResize();
    XJ_started = true;

    return true;
}

bool VideoOutputNull::CreateNullBuffers(void)
{
    for (int i = 0; i < numbuffers + 1; i++)
    {
        vbuffers[i].height = XJ_height;
        vbuffers[i].width = XJ_width;
        vbuffers[i].bpp = 12;
        vbuffers[i].size = XJ_height * XJ_width * 3 / 2;
        vbuffers[i].codec = FMT_YV12;
        vbuffers[i].buf = new unsigned char[vbuffers[i].size + 64];
        memset(vbuffers[i].buf, 0, XJ_height * XJ_width);
        memset(vbuffers[i].buf + XJ_height * XJ_width, 127, 
               XJ_height * XJ_width / 2);
    }

    return true;
}

void VideoOutputNull::Exit(void)
{
    if (XJ_started) 
    {
        XJ_started = false;

        DeleteNullBuffers();
    }
}

void VideoOutputNull::DeleteNullBuffers()
{
    for (int i = 0; i < numbuffers + 1; i++)
    {
        delete [] vbuffers[i].buf;
        vbuffers[i].buf = NULL;
    }
}

void VideoOutputNull::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    if (embedding)
        return;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);
}
 
void VideoOutputNull::StopEmbedding(void)
{
    if (!embedding)
        return;

    VideoOutput::StopEmbedding();
}

void VideoOutputNull::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    (void)t;

    if (!buffer)
        buffer = scratchFrame;

    framesPlayed = buffer->frameNumber + 1;
}

void VideoOutputNull::Show(FrameScanType )
{
}

void VideoOutputNull::DrawUnusedRects(bool)
{
}

void VideoOutputNull::UpdatePauseFrame(void)
{
    VideoFrame *pauseb = scratchFrame;
    if (usedVideoBuffers.count() > 0)
        pauseb = usedVideoBuffers.head();
    memcpy(pauseFrame.buf, pauseb->buf, pauseb->size);
}

void VideoOutputNull::ProcessFrame(VideoFrame *frame, OSD *osd,
                                   FilterChain *filterList,
                                   NuppelVideoPlayer *pipPlayer)
{
    (void)frame;
    (void)osd;
    (void)filterList;
    (void)pipPlayer;
}
