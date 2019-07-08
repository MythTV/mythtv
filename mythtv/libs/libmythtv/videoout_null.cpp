#include <map>
#include <iostream>
using namespace std;

#include "mythlogging.h"
#include "videoout_null.h"
#include "videodisplayprofile.h"

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
#ifdef USING_VTB
    if (opts.decoders->contains("vtb-dec"))
        (*opts.safe_renderers)["vtb-dec"].append("null");
#endif
#ifdef USING_VDPAU
    if (opts.decoders->contains("vdpau-dec"))
        (*opts.safe_renderers)["vdpau-dec"].append("null");
#endif
#ifdef USING_NVDEC
    if (opts.decoders->contains("nvdec-dec"))
        (*opts.safe_renderers)["nvdec-dec"].append("null");
#endif
#ifdef USING_VAAPI
    if (opts.decoders->contains("vaapi-dec"))
        (*opts.safe_renderers)["vaapi-dec"].append("null");
#endif
#ifdef USING_MEDIACODEC
    if (opts.decoders->contains("mediacodec-dec"))
        (*opts.safe_renderers)["mediacodec-dec"].append("null");
#endif
    opts.priorities->insert("null", 10);
}

VideoOutputNull::VideoOutputNull() :
    global_lock(QMutex::Recursive)
{
    LOG(VB_PLAYBACK, LOG_INFO, "VideoOutputNull()");
    memset(&av_pause_frame, 0, sizeof(av_pause_frame));
}

VideoOutputNull::~VideoOutputNull()
{
    LOG(VB_PLAYBACK, LOG_INFO, "~VideoOutputNull()");
    QMutexLocker locker(&global_lock);

    if (av_pause_frame.buf)
    {
        delete [] av_pause_frame.buf;
        memset(&av_pause_frame, 0, sizeof(av_pause_frame));
    }

    m_videoBuffers.DeleteBuffers();
}

void VideoOutputNull::CreatePauseFrame(void)
{
    if (av_pause_frame.buf)
    {
        delete [] av_pause_frame.buf;
        av_pause_frame.buf = nullptr;
    }

    init(&av_pause_frame, FMT_YV12,
         new unsigned char[m_videoBuffers.GetScratchFrame()->size + 128],
         m_videoBuffers.GetScratchFrame()->width,
         m_videoBuffers.GetScratchFrame()->height,
         m_videoBuffers.GetScratchFrame()->size);

    av_pause_frame.frameNumber = m_videoBuffers.GetScratchFrame()->frameNumber;
    av_pause_frame.frameCounter = m_videoBuffers.GetScratchFrame()->frameCounter;
    clear(&av_pause_frame);
}

bool VideoOutputNull::InputChanged(const QSize &video_dim_buf,
                                   const QSize &video_dim_disp,
                                   float        aspect,
                                   MythCodecID  av_codec_id,
                                   bool        &aspect_only,
                                   MythMultiLocker* Locks,
                                   int          ReferenceFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("InputChanged(WxH = %1x%2, aspect = %3)")
            .arg(video_dim_disp.width())
            .arg(video_dim_disp.height()).arg(static_cast<qreal>(aspect)));

    if (!codec_is_std(av_codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("VideoOutputNull::InputChanged(): "
                                         "new video codec is not supported."));
        m_errorState = kError_Unknown;
        return false;
    }

    QMutexLocker locker(&global_lock);

    if (video_dim_disp == m_window.GetVideoDim())
    {
        m_videoBuffers.Clear();
        MoveResize();
        return true;
    }

    VideoOutput::InputChanged(video_dim_buf, video_dim_disp,
                              aspect, av_codec_id, aspect_only, Locks,
                              ReferenceFrames);
    m_videoBuffers.DeleteBuffers();

    MoveResize();

    const QSize video_dim = m_window.GetVideoDim();

    bool ok = m_videoBuffers.CreateBuffers(FMT_YV12, video_dim.width(), video_dim.height());
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, "VideoOutputNull::InputChanged(): "
                                 "Failed to recreate buffers");
        m_errorState = kError_Unknown;
    }
    else
    {
        CreatePauseFrame();
    }

    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetVideoRenderer("null");

    return ok;
}

bool VideoOutputNull::Init(const QSize &video_dim_buf,
                           const QSize &video_dim_disp,
                           float aspect, WId winid,
                           const QRect &win_rect, MythCodecID codec_id)
{
    if ((video_dim_disp.width() <= 0) || (video_dim_disp.height() <= 0))
        return false;

    if (!codec_is_std(codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Cannot create VideoOutputNull for codec %1")
            .arg(toString(codec_id)));
        return false;
    }

    QMutexLocker locker(&global_lock);

    VideoOutput::Init(video_dim_buf, video_dim_disp,
                      aspect, winid, win_rect, codec_id);

    m_videoBuffers.Init(VideoBuffers::GetNumBuffers(FMT_YV12), true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);

    // XXX should this be GetActualVideoDim() ?
    const QSize video_dim = m_window.GetVideoDim();

    if (!m_videoBuffers.CreateBuffers(FMT_YV12, video_dim.width(), video_dim.height()))
        return false;

    CreatePauseFrame();

    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetVideoRenderer("null");

    MoveResize();

    return true;
}

void VideoOutputNull::EmbedInWidget(const QRect &rect)
{
    QMutexLocker locker(&global_lock);
    if (!m_window.IsEmbedding())
        VideoOutput::EmbedInWidget(rect);
}

void VideoOutputNull::StopEmbedding(void)
{
    QMutexLocker locker(&global_lock);
    if (m_window.IsEmbedding())
        VideoOutput::StopEmbedding();
}

void VideoOutputNull::SetDeinterlacing(bool, bool)
{
    m_videoBuffers.SetDeinterlacing(DEINT_NONE, DEINT_NONE, m_videoCodecID);
}

void VideoOutputNull::PrepareFrame(VideoFrame *buffer, FrameScanType t,
                                   OSD *osd)
{
    (void)osd;
    (void)t;

    if (!buffer)
        buffer = m_videoBuffers.GetScratchFrame();

    m_framesPlayed = buffer->frameNumber + 1;
}

void VideoOutputNull::Show(FrameScanType  /*scan*/)
{
}

void VideoOutputNull::UpdatePauseFrame(int64_t &disp_timecode)
{
    QMutexLocker locker(&global_lock);

    // Try used frame first, then fall back to scratch frame.
    m_videoBuffers.BeginLock(kVideoBuffer_used);
    VideoFrame *used_frame = nullptr;
    if (m_videoBuffers.Size(kVideoBuffer_used) > 0)
        used_frame = m_videoBuffers.Head(kVideoBuffer_used);

    if (used_frame)
        CopyFrame(&av_pause_frame, used_frame);
    m_videoBuffers.EndLock();

    if (!used_frame)
    {
        m_videoBuffers.GetScratchFrame()->frameNumber = m_framesPlayed - 1;
        CopyFrame(&av_pause_frame, m_videoBuffers.GetScratchFrame());
    }

    disp_timecode = av_pause_frame.disp_timecode;
}

void VideoOutputNull::ProcessFrame(VideoFrame *Frame, OSD*, const PIPMap &,
                                   FrameScanType Scan)
{
    if (Frame && !Frame->dummy)
        m_deinterlacer.Filter(Frame, Scan);
}
