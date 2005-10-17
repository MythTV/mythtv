#include <map>
#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "videoout_null.h"

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

VideoOutputNull::VideoOutputNull(void)
               : VideoOutput()
{
    VERBOSE(VB_PLAYBACK, "VideoOutputNull()");
    XJ_started = 0; 

    pauseFrame.buf = NULL;
}

VideoOutputNull::~VideoOutputNull()
{
    VERBOSE(VB_PLAYBACK, "~VideoOutputNull()");
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    Exit();
}

void VideoOutputNull::Zoom(int direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputNull::InputChanged(int width, int height, float aspect)
{
    VERBOSE(VB_PLAYBACK, "InputChanged(w "<<width<<", h"
            <<height<<", a"<<aspect<<")");
    VideoOutput::InputChanged(width, height, aspect);

    if (width == vbuffers.GetScratchFrame()->width &&
        height == vbuffers.GetScratchFrame()->height)
    {
        MoveResize();
        return;
    }

    XJ_width = width;
    XJ_height = height;

    vbuffers.DeleteBuffers();

    MoveResize();

    if (!vbuffers.CreateBuffers(XJ_width, XJ_height))
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputNull::InputChanged(): "
                "Failed to recreate buffers");
        errored = true;
    }

    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    pauseFrame.height = vbuffers.GetScratchFrame()->height;
    pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    pauseFrame.buf    = new unsigned char[pauseFrame.size];
    pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;
}

int VideoOutputNull::GetRefreshRate(void)
{
    return 0;
}

bool VideoOutputNull::Init(int width, int height, float aspect,
                           WId winid, int winx, int winy, int winw, 
                           int winh, WId embedid)
{
    if ((width <= 0) || (height <= 0))
        return false;

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames, 
                  kPrebufferFramesNormal, kPrebufferFramesSmall, 
                  kKeepPrebuffer);
    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    XJ_width = width;
    XJ_height = height;
    if (!vbuffers.CreateBuffers(width, height))
        return false;

    pauseFrame.height = vbuffers.GetScratchFrame()->height;
    pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    pauseFrame.buf    = new unsigned char[pauseFrame.size];
    pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    MoveResize();
    XJ_started = true;

    return true;
}

void VideoOutputNull::Exit(void)
{
    if (XJ_started) 
    {
        XJ_started = false;

        vbuffers.DeleteBuffers();
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
        buffer = vbuffers.GetScratchFrame();

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
    VideoFrame *pauseb = vbuffers.GetScratchFrame();
    VideoFrame *pauseu = vbuffers.head(kVideoBuffer_used);
    if (pauseu)
        memcpy(pauseFrame.buf, pauseu->buf, pauseu->size);
    else
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
