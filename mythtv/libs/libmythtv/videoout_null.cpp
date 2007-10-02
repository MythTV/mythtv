#include <map>
#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "videoout_null.h"
#include "videodisplayprofile.h"

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

// this is documented in videooutbase.cpp
void VideoOutputNull::Zoom(ZoomDirection direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

bool VideoOutputNull::InputChanged(const QSize &input_size,
                                   float        aspect,
                                   MythCodecID  av_codec_id,
                                   void        *codec_private)
{
    VERBOSE(VB_PLAYBACK,
            QString("InputChanged(WxH = %1x%2, aspect = %3)")
            .arg(input_size.width())
            .arg(input_size.height()).arg(aspect));

    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private);

    if (input_size.width()  == vbuffers.GetScratchFrame()->width &&
        input_size.height() == vbuffers.GetScratchFrame()->height)
    {
        MoveResize();
        return true;
    }

    video_dim = input_size;

    vbuffers.DeleteBuffers();

    MoveResize();

    if (!vbuffers.CreateBuffers(video_dim.width(), video_dim.height()))
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputNull::InputChanged(): "
                "Failed to recreate buffers");
        errored = true;
    }

    db_vdisp_profile->SetVideoRenderer("null");

    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    pauseFrame.height = vbuffers.GetScratchFrame()->height;
    pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    pauseFrame.buf    = new unsigned char[pauseFrame.size];
    pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    return true;
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

    video_dim = QSize(width, height);

    if (!vbuffers.CreateBuffers(width, height))
        return false;

    db_vdisp_profile->SetVideoRenderer("null");

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
