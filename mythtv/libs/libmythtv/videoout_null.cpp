#include <map>
#include <iostream>
using namespace std;

#include "mythverbose.h"
#include "videoout_null.h"
#include "videodisplayprofile.h"

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

void VideoOutputNull::GetRenderOptions(render_opts &opts,
                                       QStringList &cpudeints)
{
    opts.renderers->append("null");
    opts.deints->insert("null", cpudeints);
    (*opts.osds)["null"].append("softblend");
    (*opts.safe_renderers)["dummy"].append("null");
    (*opts.safe_renderers)["nuppel"].append("null");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("null");
    if (opts.decoders->contains("libmpeg2"))
        (*opts.safe_renderers)["libmpeg2"].append("null");
    opts.priorities->insert("null", 10);
}

VideoOutputNull::VideoOutputNull(void) :
    VideoOutput(), global_lock(QMutex::Recursive)
{
    VERBOSE(VB_PLAYBACK, "VideoOutputNull()");
    memset(&av_pause_frame, 0, sizeof(av_pause_frame));
}

VideoOutputNull::~VideoOutputNull()
{
    VERBOSE(VB_PLAYBACK, "~VideoOutputNull()");
    QMutexLocker locker(&global_lock);

    vbuffers.LockFrame(&av_pause_frame, "DeletePauseFrame");
    if (av_pause_frame.buf)
    {
        delete [] av_pause_frame.buf;
        memset(&av_pause_frame, 0, sizeof(av_pause_frame));
    }
    vbuffers.UnlockFrame(&av_pause_frame, "DeletePauseFrame");

    vbuffers.DeleteBuffers();
}

// this is documented in videooutbase.cpp
void VideoOutputNull::Zoom(ZoomDirection direction)
{
    QMutexLocker locker(&global_lock);
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputNull::CreatePauseFrame(void)
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
         vbuffers.GetScratchFrame()->bpp,
         vbuffers.GetScratchFrame()->size);

    av_pause_frame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    clear(&av_pause_frame, GUID_I420_PLANAR);

    vbuffers.UnlockFrame(&av_pause_frame, "CreatePauseFrame");
}

bool VideoOutputNull::InputChanged(const QSize &input_size,
                                   float        aspect,
                                   MythCodecID  av_codec_id,
                                   void        *codec_private,
                                   bool        &aspect_only)
{
    VERBOSE(VB_PLAYBACK,
            QString("InputChanged(WxH = %1x%2, aspect = %3)")
            .arg(input_size.width())
            .arg(input_size.height()).arg(aspect));

    if (!codec_is_std(av_codec_id))
    {
        VERBOSE(VB_IMPORTANT, QString("VideoOutputNull::InputChanged(): "
                                      "new video codec is not supported."));
        errorState = kError_Unknown;
        return false;
    }

    QMutexLocker locker(&global_lock);

    if (input_size == windows[0].GetVideoDispDim())
    {
        vbuffers.Clear(GUID_I420_PLANAR);
        MoveResize();
        return true;
    }

    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private,
                              aspect_only);
    vbuffers.DeleteBuffers();

    MoveResize();

    const QSize video_dim = windows[0].GetVideoDim();

    bool ok = vbuffers.CreateBuffers(video_dim.width(), video_dim.height());
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "VideoOutputNull::InputChanged(): "
                "Failed to recreate buffers");
        errorState = kError_Unknown;
    }
    else
    {
        CreatePauseFrame();
    }

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("null");

    return ok;
}

bool VideoOutputNull::Init(int width, int height, float aspect,
                           WId winid, int winx, int winy, int winw,
                           int winh, WId embedid)
{
    if ((width <= 0) || (height <= 0))
        return false;

    QMutexLocker locker(&global_lock);

    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);

    const QSize video_dim = windows[0].GetVideoDim();

    if (!vbuffers.CreateBuffers(video_dim.width(), video_dim.height()))
        return false;

    CreatePauseFrame();

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("null");

    MoveResize();

    return true;
}

void VideoOutputNull::EmbedInWidget(int x, int y, int w, int h)
{
    QMutexLocker locker(&global_lock);
    if (!windows[0].IsEmbedding())
        VideoOutput::EmbedInWidget(x, y, w, h);
}

void VideoOutputNull::StopEmbedding(void)
{
    QMutexLocker locker(&global_lock);
    if (windows[0].IsEmbedding())
        VideoOutput::StopEmbedding();
}

void VideoOutputNull::PrepareFrame(VideoFrame *buffer, FrameScanType t,
                                   OSD *osd)
{
    (void)osd;
    (void)t;

    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    vbuffers.LockFrame(buffer, "PrepareFrame");
    framesPlayed = buffer->frameNumber + 1;
    vbuffers.UnlockFrame(buffer, "PrepareFrame");
}

void VideoOutputNull::Show(FrameScanType )
{
}

void VideoOutputNull::DrawUnusedRects(bool)
{
}

void VideoOutputNull::UpdatePauseFrame(void)
{
    QMutexLocker locker(&global_lock);

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

void VideoOutputNull::ProcessFrame(VideoFrame *frame, OSD *osd,
                                   FilterChain *filterList,
                                   const PIPMap &pipPlayers,
                                   FrameScanType scan)
{
    (void)frame;
    (void)osd;
    (void)filterList;
    (void)pipPlayers;
    (void)scan;
}
