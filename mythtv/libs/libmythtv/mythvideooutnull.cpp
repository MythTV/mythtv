// MythTV
#include "mythlogging.h"
#include "mythvideooutnull.h"
#include "videodisplayprofile.h"

// Std
#include <map>
#include <iostream>

const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;

#define LOC QString("VidOutNull: ")

void MythVideoOutputNull::GetRenderOptions(RenderOptions& Options)
{
    Options.renderers->append("null");
    (*Options.safe_renderers)["dummy"].append("null");
    if (Options.decoders->contains("ffmpeg"))
        (*Options.safe_renderers)["ffmpeg"].append("null");
#ifdef USING_VTB
    if (Options.decoders->contains("vtb-dec"))
        (*Options.safe_renderers)["vtb-dec"].append("null");
#endif
#ifdef USING_VDPAU
    if (Options.decoders->contains("vdpau-dec"))
        (*Options.safe_renderers)["vdpau-dec"].append("null");
#endif
#ifdef USING_NVDEC
    if (Options.decoders->contains("nvdec-dec"))
        (*Options.safe_renderers)["nvdec-dec"].append("null");
#endif
#ifdef USING_VAAPI
    if (Options.decoders->contains("vaapi-dec"))
        (*Options.safe_renderers)["vaapi-dec"].append("null");
#endif
#ifdef USING_MEDIACODEC
    if (Options.decoders->contains("mediacodec-dec"))
        (*Options.safe_renderers)["mediacodec-dec"].append("null");
#endif
#ifdef USING_V4L2
    if (Options.decoders->contains("v4l2-dec"))
        (*Options.safe_renderers)["v4l2-dec"].append("null");
#endif
#ifdef USING_MMAL
    if (Options.decoders->contains("mmal-dec"))
        (*Options.safe_renderers)["mmal-dec"].append("null");
#endif
    Options.priorities->insert("null", 10);
}

MythVideoOutputNull::MythVideoOutputNull()
{
    memset(&m_avPauseFrame, 0, sizeof(m_avPauseFrame));
}

MythVideoOutputNull::~MythVideoOutputNull()
{
    QMutexLocker locker(&m_globalLock);

    if (m_avPauseFrame.buf)
    {
        delete [] m_avPauseFrame.buf;
        memset(&m_avPauseFrame, 0, sizeof(m_avPauseFrame));
    }

    m_videoBuffers.DeleteBuffers();
}

void MythVideoOutputNull::CreatePauseFrame(void)
{
    if (m_avPauseFrame.buf)
    {
        delete [] m_avPauseFrame.buf;
        m_avPauseFrame.buf = nullptr;
    }

    init(&m_avPauseFrame, FMT_YV12,
         new unsigned char[static_cast<unsigned long>(m_videoBuffers.GetScratchFrame()->size + 128)],
         m_videoBuffers.GetScratchFrame()->width,
         m_videoBuffers.GetScratchFrame()->height,
         m_videoBuffers.GetScratchFrame()->size);

    m_avPauseFrame.frameNumber = m_videoBuffers.GetScratchFrame()->frameNumber;
    m_avPauseFrame.frameCounter = m_videoBuffers.GetScratchFrame()->frameCounter;
    clear_vf(&m_avPauseFrame);
}

bool MythVideoOutputNull::InputChanged(const QSize& VideoDim,
                                       const QSize& VideoDispDim,
                                       float        Aspect,
                                       MythCodecID  CodecID,
                                       bool&        AspectOnly,
                                       int          ReferenceFrames,
                                       bool         ForceChange)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("InputChanged(WxH = %1x%2, aspect = %3)")
            .arg(VideoDispDim.width()).arg(VideoDispDim.height()).arg(static_cast<qreal>(Aspect)));

    if (!codec_is_std(CodecID))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("VideoOutputNull::InputChanged(): new video codec is not supported."));
        m_errorState = kError_Unknown;
        return false;
    }

    QMutexLocker locker(&m_globalLock);

    if (VideoDispDim == GetVideoDim())
    {
        m_videoBuffers.Clear();
        MoveResize();
        return true;
    }

    MythVideoOutput::InputChanged(VideoDim, VideoDispDim,
                                  Aspect, CodecID, AspectOnly,
                                  ReferenceFrames, ForceChange);
    m_videoBuffers.DeleteBuffers();

    MoveResize();

    const QSize video_dim = GetVideoDim();

    bool ok = m_videoBuffers.CreateBuffers(FMT_YV12, video_dim.width(), video_dim.height());
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "VideoOutputNull::InputChanged(): Failed to recreate buffers");
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

bool MythVideoOutputNull::Init(const QSize& VideoDim, const QSize& VideoDispDim,
                               float Aspect, const QRect& DisplayVisibleRect, MythCodecID CodecID)
{
    if (VideoDispDim.isEmpty())
        return false;

    if (!codec_is_std(CodecID))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Cannot create VideoOutputNull for codec %1")
            .arg(toString(CodecID)));
        return false;
    }

    QMutexLocker locker(&m_globalLock);

    MythVideoOutput::Init(VideoDim, VideoDispDim, Aspect, DisplayVisibleRect, CodecID);
    m_videoBuffers.Init(VideoBuffers::GetNumBuffers(FMT_YV12), true, kNeedFreeFrames,
                        kPrebufferFramesNormal, kPrebufferFramesSmall);

    const QSize videodim = GetVideoDim();

    if (!m_videoBuffers.CreateBuffers(FMT_YV12, videodim.width(), videodim.height()))
        return false;

    CreatePauseFrame();

    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetVideoRenderer("null");

    MoveResize();

    return true;
}

void MythVideoOutputNull::EmbedInWidget(const QRect& EmbedRect)
{
    // TODO is this locking required (and associated override of EmbedInWidget)
    QMutexLocker locker(&m_globalLock);
    MythVideoOutput::EmbedInWidget(EmbedRect);
}

void MythVideoOutputNull::StopEmbedding(void)
{
    // TODO is this locking required (and associated override of StopEmbedding)
    QMutexLocker locker(&m_globalLock);
    MythVideoOutput::StopEmbedding();
}

void MythVideoOutputNull::SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force /*=DEINT_NONE*/)
{
    if (DEINT_NONE != Force)
    {
        MythVideoOutput::SetDeinterlacing(Enable, DoubleRate, Force);
        return;
    }

    m_deinterlacing   = false;
    m_deinterlacing2X = false;
    m_forcedDeinterlacer = DEINT_NONE;
    m_videoBuffers.SetDeinterlacing(DEINT_NONE, DEINT_NONE, m_videoCodecID);
}

void MythVideoOutputNull::RenderFrame(VideoFrame* Frame, FrameScanType /*Scan*/, OSD* /*Osd*/)
{
    if (!Frame)
        Frame = m_videoBuffers.GetScratchFrame();
    if (Frame)
        m_framesPlayed = Frame->frameNumber + 1;
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "No frame in PrepareFrame!");
}

void MythVideoOutputNull::EndFrame()
{
}

void MythVideoOutputNull::UpdatePauseFrame(int64_t& DisplayTimecode, FrameScanType /*Scan*/)
{
    QMutexLocker locker(&m_globalLock);

    // Try used frame first, then fall back to scratch frame.
    m_videoBuffers.BeginLock(kVideoBuffer_used);
    VideoFrame *used = nullptr;
    if (m_videoBuffers.Size(kVideoBuffer_used) > 0)
        used = m_videoBuffers.Head(kVideoBuffer_used);

    if (used)
        CopyFrame(&m_avPauseFrame, used);
    m_videoBuffers.EndLock();

    if (!used)
    {
        m_videoBuffers.GetScratchFrame()->frameNumber = m_framesPlayed - 1;
        CopyFrame(&m_avPauseFrame, m_videoBuffers.GetScratchFrame());
    }

    DisplayTimecode = m_avPauseFrame.disp_timecode;
}

void MythVideoOutputNull::PrepareFrame(VideoFrame* Frame, FrameScanType Scan)
{
    if (Frame && !Frame->dummy)
        m_deinterlacer.Filter(Frame, Scan, nullptr);
}
