// Copyright (c) 2005, Daniel Thor Kristjansson
// based on earlier work in MythTV's videout_xvmc.cpp

// Std
#include <chrono>
#include <thread>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

#include "fourcc.h"
#include "mythcodecid.h"
#include "videobuffers.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
}

static constexpr uint16_t TRY_LOCK_SPINS                 { 2000 };
static constexpr uint16_t TRY_LOCK_SPINS_BEFORE_WARNING  { 9999 };
static constexpr std::chrono::milliseconds TRY_LOCK_SPIN_WAIT { 1ms };

int next_dbg_str = 0;

/*! \brief Store AVBufferRef's for later disposal
 *
 * \note Releasing hardware buffer references will at some point trigger the
 * deletion of AVBufferPool's, which in turn trigger the release of hardware
 * contexts and OpenGL interops - which may trigger a blocking callback. We cannot
 * hold the videobuffer lock while doing this as there are numerous playback paths
 * that can request the video buffer lock while the decoder thread is blocking.
 * So store the buffers and release once the master videobuffer lock is released.
*/
static inline void ReleaseDecoderResources(MythVideoFrame *Frame, std::vector<AVBufferRef *> &Discards)
{
    if (MythVideoFrame::HardwareFormat(Frame->m_type))
    {
        auto* ref = reinterpret_cast<AVBufferRef*>(Frame->m_priv[0]);
        if (ref != nullptr)
            Discards.push_back(ref);
        Frame->m_buffer = Frame->m_priv[0] = nullptr;

        if (MythVideoFrame::HardwareFramesFormat(Frame->m_type))
        {
            ref = reinterpret_cast<AVBufferRef*>(Frame->m_priv[1]);
            if (ref != nullptr)
                Discards.push_back(ref);
            Frame->m_priv[1] = nullptr;
        }
    }
}

static inline void DoDiscard(const std::vector<AVBufferRef *> &Discards)
{
    for (auto * it : Discards)
        av_buffer_unref(&it);
}

/**
 * \class VideoBuffers
 *  This class creates tracks the state of the buffers used by
 *  various VideoOutput derived classes.
 *
 *  The states available for a buffer are: available, limbo, used,
 *  process, and displayed.
 *
 *  The two most important states are available and used.
 *  Used is implemented as a FIFO, and is used to buffer
 *  frames ready for display. A decoder may decode frames
 *  out of order but must add them to the used queue in
 *  order. The available buffers are buffers that are ready
 *  to be used by the decoder.
 *
 *  Generally a buffer leaves the available state via
 *  GetNextFreeFrame(bool,bool,BufferType) and enters the
 *  limbo state. It then leaves the limbo state via
 *  ReleaseFrame(VideoFrame*) and enters the used state.
 *  Then it leaves the used state via DoneDisplayingFrame()
 *  and enters the available state.
 *
 *  At any point, DiscardFrame(VideoFrame*) can be called
 *  to remove the frame from the current state and add it
 *  to the available list.
 *
 *  However, there are two additional states available, these
 *  are currently used by VideoOutputXv for XvMC support.
 *  These are the process and displayed state. The process
 *  state simply indicates that the frame has been picked up
 *  in the VideoOutput::ProcessFrame() call, but
 *  VideoOutput::Show() has not yet used the frame. This
 *  is needed because a frame other than a used frame
 *  is being held by VideoOutput and we don't want to lose
 *  it if the stream is reset. The displayed state indicates
 *  that DoneDisplayingFrame() has been called for the frame,
 *  but it cannot yet be added to available because it is
 *  still being displayed. VideoOutputXv calls
 *  DiscardFrame(VideoFrame*) on the frames no longer
 *  being displayed at the end of the next
 *  DoneDisplayingFrame(), finally adding them to available.
 *
 *  The only method that returns with a lock held on the VideoBuffers
 *  object itself, preventing anyone else from using the VideoBuffers
 *  class, inluding to unlocking frames, is the begin_lock(BufferType).
 *  This method is to be used with extreme caution, in particular one
 *  should not attempt to acquire any locks before end_lock() is called.
 *
 *  There are also frame inheritence tracking functions, these are
 *  used by VideoOutputXv to avoid throwing away displayed frames too
 *  early. See videoout_xv.cpp for their use.
 *
 *  released = used + finished + displayed + pause
 *  total = available + limbo + released
 *  released_and_in_use_by_decoder = decode
 *
 *  available - frames not in use by decoder or display
 *  limbo     - frames in use by decoder but not released for display
 *  decode    - frames in use by decoder and released for display
 *  used      - frames released for display but not displayed or paused
 *  displayed - frames displayed but still used as a reference frame
 *  pause     - frames used for pause
 *  finished  - frames that are finished displaying but still in use by decoder
 *
 *  NOTE: All queues are mutually exclusive except "decode" which tracks frames
 *        that have been released but still in use by the decoder. If a frame
 *        has finished being processed/displayed but is still in use by the
 *        decoder (in the decode queue) then it is placed in the finished queue
 *        until the decoder is no longer using it (not in the decode queue).
 *
 * \see VideoOutput
 */

uint VideoBuffers::GetNumBuffers(int PixelFormat, int MaxReferenceFrames, bool Decoder /*=false*/)
{
    uint refs = static_cast<uint>(MaxReferenceFrames);
    switch (PixelFormat)
    {
        case FMT_DXVA2: return 30;
        // It is currrently unclear whether VTB just happy with a smaller buffer size
        // or needs reference frames plus headroom - use the latter for now.
        case FMT_VTB:   return refs + 8;
        // Max 16 ref frames, 12 headroom and allocate 2 extra in the VAAPI frames
        // context for additional references held by the VPP deinterlacer (i.e.
        // prevent buffer starvation in the decoder)
        // This covers the 'worst case' samples.
        case FMT_VAAPI: return Decoder ? (refs + 14) : (refs + 12);
        case FMT_VDPAU: return refs + 12;
        // Copyback of hardware frames. These decoders are buffering internally
        // already - so no need for a large presentation buffer
        case FMT_NONE:  return 8; // NOLINT(bugprone-branch-clone)
        // As for copyback, these decoders buffer internally
        case FMT_NVDEC: return 8;
        case FMT_MEDIACODEC: return 8;
        case FMT_MMAL: return 8;
        // the default number of output buffers in FFmpeg v4l2_m2m.h is 6
        case FMT_DRMPRIME: return 6;
        // Standard software decode
        case FMT_YV12:  return refs + 14;
        default: break;
    }
    return 30;
}

/*! \brief Creates buffers and sets various buffer management parameters.
 *
 * \param NumDecode           number of buffers to allocate for normal use
 * \param NeedFree            maximum number of buffers needed in display
 *                            and pause
 * \param NeedPrebufferNormal number buffers you can put in used or limbo normally
 * \param NeedPrebufferSmall  number of buffers you can put in used or limbo
 *                            after SetPrebuffering(false) has been called.
 */
void VideoBuffers::Init(uint NumDecode, uint NeedFree,
                        uint NeedPrebufferNormal, uint NeedPrebufferSmall)
{
    QMutexLocker locker(&m_globalLock);

    Reset();

    // make a big reservation, so that things that depend on
    // pointer to VideoFrames work even after a few push_backs
    m_buffers.reserve(std::max(NumDecode, 128U));
    m_buffers.resize(NumDecode);
    for (uint i = 0; i < NumDecode; i++)
        m_vbufferMap[At(i)] = i;

    m_needFreeFrames            = NeedFree;
    m_needPrebufferFrames       = NeedPrebufferNormal;
    m_needPrebufferFramesNormal = NeedPrebufferNormal;
    m_needPrebufferFramesSmall  = NeedPrebufferSmall;

    for (uint i = 0; i < NumDecode; i++)
        Enqueue(kVideoBuffer_avail, At(i));
    SetDeinterlacing(DEINT_NONE, DEINT_NONE, kCodec_NONE);
}

void VideoBuffers::SetDeinterlacing(MythDeintType Single, MythDeintType Double,
                                    MythCodecID CodecID)
{
    QMutexLocker locker(&m_globalLock);
    for (auto & buffer : m_buffers)
        SetDeinterlacingFlags(buffer, Single, Double, CodecID);
}

/*! \brief Set the appropriate flags for single and double rate deinterlacing
 * \note Double rate support is determined by the VideoOutput class and must be set appropriately
 * \note Driver deinterlacers are only available for hardware frames with the exception of
 * NVDEC and VTB which can use shaders.
 * \note Shader and CPU deinterlacers are disabled for hardware frames (except for shaders with NVDEC and VTB)
 * \todo Handling of decoder deinterlacing with NVDEC
*/
void VideoBuffers::SetDeinterlacingFlags(MythVideoFrame &Frame, MythDeintType Single,
                                         MythDeintType Double, MythCodecID CodecID)
{
    static const MythDeintType kDriver   = DEINT_ALL & ~(DEINT_CPU | DEINT_SHADER);
    static const MythDeintType kShader   = DEINT_ALL & ~(DEINT_CPU | DEINT_DRIVER);
    static const MythDeintType kSoftware = DEINT_ALL & ~(DEINT_SHADER | DEINT_DRIVER);
    Frame.m_deinterlaceSingle  = Single;
    Frame.m_deinterlaceDouble  = Double;

    if (codec_is_copyback(CodecID))
    {
        if (codec_is_vaapi_dec(CodecID) || codec_is_nvdec_dec(CodecID))
            Frame.m_deinterlaceAllowed = kSoftware | kShader | kDriver;
        else // VideoToolBox, MediaCodec and VDPAU copyback
            Frame.m_deinterlaceAllowed = kSoftware | kShader;
    }
    else if (FMT_DRMPRIME == Frame.m_type)
    {   // NOLINT(bugprone-branch-clone)
        Frame.m_deinterlaceAllowed = kShader; // No driver deint - if RGBA frames are returned, shaders will be disabled
    }
    else if (FMT_MMAL == Frame.m_type)
    {
        Frame.m_deinterlaceAllowed = kShader; // No driver deint yet (TODO) and YUV frames returned
    }
    else if (FMT_VTB == Frame.m_type)
    {
        Frame.m_deinterlaceAllowed = kShader; // No driver deint and YUV frames returned
    }
    else if (FMT_NVDEC == Frame.m_type)
    {
        Frame.m_deinterlaceAllowed = kShader | kDriver; // YUV frames and decoder deint
    }
    else if (FMT_VDPAU == Frame.m_type)
    {   // NOLINT(bugprone-branch-clone)
        Frame.m_deinterlaceAllowed = kDriver; // No YUV frames for shaders
    }
    else if (FMT_VAAPI == Frame.m_type)
    {
        Frame.m_deinterlaceAllowed = kDriver; // DRM will allow shader if no VPP
    }
    else
    {
        Frame.m_deinterlaceAllowed = kSoftware | kShader;
    }
}

/**
 * \fn VideoBuffers::Reset()
 *  Resets the class so that Init may be called again.
 */
void VideoBuffers::Reset()
{
    QMutexLocker locker(&m_globalLock);
    m_available.clear();
    m_used.clear();
    m_limbo.clear();
    m_finished.clear();
    m_decode.clear();
    m_pause.clear();
    m_displayed.clear();
    m_vbufferMap.clear();
}

/**
 * \fn VideoBuffers::SetPrebuffering(bool normal)
 *  Sets prebuffering state to normal, or small.
 */
void VideoBuffers::SetPrebuffering(bool Normal)
{
    QMutexLocker locker(&m_globalLock);
    m_needPrebufferFrames = (Normal) ? m_needPrebufferFramesNormal : m_needPrebufferFramesSmall;
}

MythVideoFrame *VideoBuffers::GetNextFreeFrameInternal(BufferType EnqueueTo)
{
    QMutexLocker locker(&m_globalLock);
    MythVideoFrame *frame = nullptr;

    // Try to get a frame not being used by the decoder
    for (size_t i = 0; i < m_available.size(); i++)
    {
        frame = m_available.dequeue();
        if (m_decode.contains(frame))
            m_available.enqueue(frame);
        else
            break;
    }

    while (frame && m_used.contains(frame))
    {
        LOG(VB_PLAYBACK, LOG_NOTICE,
            QString("GetNextFreeFrame() served a busy frame %1. Dropping. %2")
                .arg(DebugString(frame, true), GetStatus()));
        frame = m_available.dequeue();
    }

    if (frame)
        SafeEnqueue(EnqueueTo, frame);
    return frame;
}

/*! \brief Gets a frame from available buffers list.
 *
 * \param EnqueueTo Put new frame in some state other than limbo.
 */
MythVideoFrame *VideoBuffers::GetNextFreeFrame(BufferType EnqueueTo)
{
    for (uint tries = 1; true; tries++)
    {
        MythVideoFrame *frame = VideoBuffers::GetNextFreeFrameInternal(EnqueueTo);
        if (frame)
            return frame;

        if (tries >= TRY_LOCK_SPINS)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("GetNextFreeFrame: "
            "available:%1 used:%2 limbo:%3 pause:%4 displayed:%5 decode:%6 finished:%7")
            .arg(m_available.size()).arg(m_used.size()).arg(m_limbo.size())
            .arg(m_pause.size()).arg(m_displayed.size()).arg(m_decode.size())
            .arg(m_finished.size()));
            LOG(VB_GENERAL, LOG_ERR,
                QString("GetNextFreeFrame() unable to "
                        "lock frame %1 times. Discarding Frames.")
                    .arg(TRY_LOCK_SPINS));
            DiscardFrames(true);
            continue;
        }

        if (tries && !(tries % TRY_LOCK_SPINS_BEFORE_WARNING))
        {
            LOG(VB_PLAYBACK, LOG_NOTICE,
                QString("GetNextFreeFrame() TryLock has "
                        "spun %1 times, this is a lot.").arg(tries));
        }
        std::this_thread::sleep_for(TRY_LOCK_SPIN_WAIT);
    }

    return nullptr;
}

/**
 * \fn VideoBuffers::ReleaseFrame(VideoFrame*)
 *  Frame is ready to be for filtering or OSD application.
 *  Removes frame from limbo and adds it to used queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::ReleaseFrame(MythVideoFrame *Frame)
{
    QMutexLocker locker(&m_globalLock);

    m_vpos = m_vbufferMap[Frame];
    m_limbo.remove(Frame);
    //non directrendering frames are ffmpeg handled
    if (Frame->m_directRendering)
        m_decode.enqueue(Frame);
    m_used.enqueue(Frame);
}

/**
 * \fn VideoBuffers::DeLimboFrame(VideoFrame*)
 *  If the frame is still in the limbo state it is added to the available queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::DeLimboFrame(MythVideoFrame *Frame)
{
    std::vector<AVBufferRef*> discards;

    m_globalLock.lock();

    if (m_limbo.contains(Frame))
        m_limbo.remove(Frame);

    // if decoder didn't release frame and the buffer is getting released by
    // the decoder assume that the frame is lost and return to available
    if (!m_decode.contains(Frame))
    {
        ReleaseDecoderResources(Frame, discards);
        SafeEnqueue(kVideoBuffer_avail, Frame);
    }

    // remove from decode queue since the decoder is finished
    while (m_decode.contains(Frame))
        m_decode.remove(Frame);

    m_globalLock.unlock();

    DoDiscard(discards);
}

/**
 * \fn VideoBuffers::StartDisplayingFrame(void)
 *  Sets rpos to index of videoframe at head of used queue.
 */
void VideoBuffers::StartDisplayingFrame(void)
{
    QMutexLocker locker(&m_globalLock);
    m_rpos = m_vbufferMap[m_used.head()];
}

/**
 * \fn VideoBuffers::DoneDisplayingFrame(VideoFrame *Frame)
 *  Removes frame from used queue and adds it to the available list.
 */
void VideoBuffers::DoneDisplayingFrame(MythVideoFrame *Frame)
{
    std::vector<AVBufferRef*> discards;

    m_globalLock.lock();

    if(m_used.contains(Frame))
        Remove(kVideoBuffer_used, Frame);

    Enqueue(kVideoBuffer_finished, Frame);

    // check if any finished frames are no longer used by decoder and return to available
    frame_queue_t ula(m_finished);
    for (auto & it : ula)
    {
        if (!m_decode.contains(it))
        {
            Remove(kVideoBuffer_finished, it);
            ReleaseDecoderResources(it, discards);
            Enqueue(kVideoBuffer_avail, it);
        }
    }

    m_globalLock.unlock();

    DoDiscard(discards);
}

/**
 * \fn VideoBuffers::DiscardFrame(VideoFrame*)
 *  Frame is ready to be reused by decoder.
 *  Add frame to available list, remove from any other list.
 */
void VideoBuffers::DiscardFrame(MythVideoFrame *Frame)
{
    std::vector<AVBufferRef*> discards;
    m_globalLock.lock();
    ReleaseDecoderResources(Frame, discards);
    SafeEnqueue(kVideoBuffer_avail, Frame);
    m_globalLock.unlock();
    DoDiscard(discards);
}

void VideoBuffers::DiscardPauseFrames(void)
{
    std::vector<AVBufferRef*> discards;

    m_globalLock.lock();
    while (Size(kVideoBuffer_pause))
    {
        MythVideoFrame* frame = Tail(kVideoBuffer_pause);
        ReleaseDecoderResources(frame, discards);
        SafeEnqueue(kVideoBuffer_avail, frame);
    }
    m_globalLock.unlock();

    DoDiscard(discards);
}

/*! \brief Discard all buffers and recreate
 *
 * This is used to 'atomically' recreate VideoBuffers that may use hardware buffer
 * references. It is only used by MythVideoOutputOpenGL (other MythVideoOutput
 * classes currently do not support hardware frames).
*/
bool VideoBuffers::DiscardAndRecreate(MythCodecID CodecID, QSize VideoDim, int References)
{
    bool result = false;
    std::vector<AVBufferRef*> refs;

    m_globalLock.lock();
    LOG(VB_PLAYBACK, LOG_INFO, QString("DiscardAndRecreate: %1").arg(GetStatus()));

    // Remove pause frames (cutdown version of DiscardPauseFrames)
    while (Size(kVideoBuffer_pause))
    {
        MythVideoFrame* frame = Tail(kVideoBuffer_pause);
        ReleaseDecoderResources(frame, refs);
        SafeEnqueue(kVideoBuffer_avail, frame);
    }

    // See DiscardFrames
    frame_queue_t ula(m_used);
    ula.insert(ula.end(), m_limbo.begin(), m_limbo.end());
    ula.insert(ula.end(), m_available.begin(), m_available.end());
    ula.insert(ula.end(), m_finished.begin(), m_finished.end());

    frame_queue_t discards(m_used);
    discards.insert(discards.end(), m_limbo.begin(), m_limbo.end());
    discards.insert(discards.end(), m_finished.begin(), m_finished.end());
    for (auto & discard : discards)
    {
        ReleaseDecoderResources(discard, refs);
        SafeEnqueue(kVideoBuffer_avail, discard);
    }

    if (m_available.count() + m_pause.count() + m_displayed.count() != Size())
    {
        for (uint i = 0; i < Size(); i++)
        {
            if (!m_available.contains(At(i)) && !m_pause.contains(At(i)) &&
                !m_displayed.contains(At(i)))
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("VideoBuffers::DiscardFrames(): %1 (%2) not "
                            "in available, pause, or displayed %3")
                        .arg(DebugString(At(i), true)).arg(reinterpret_cast<long long>(At(i)))
                        .arg(GetStatus()));
                ReleaseDecoderResources(At(i), refs);
                SafeEnqueue(kVideoBuffer_avail, At(i));
            }
        }
    }

    for (auto & it : m_decode)
        Remove(kVideoBuffer_all, it);
    for (auto & it : m_decode)
        m_available.enqueue(it);
    m_decode.clear();

    Reset();

    // Recreate - see MythVideoOutputOpenGL::CreateBuffers
    if (codec_is_copyback(CodecID))
    {
        Init(VideoBuffers::GetNumBuffers(FMT_NONE), 1, 4, 2);
        result = CreateBuffers(FMT_YV12, VideoDim.width(), VideoDim.height(), m_renderFormats);
    }
    else if (codec_is_mediacodec(CodecID))
    {
        result = CreateBuffers(FMT_MEDIACODEC, m_renderFormats, VideoDim, 1, 2, 2);
    }
    else if (codec_is_vaapi(CodecID))
    {
        result = CreateBuffers(FMT_VAAPI, m_renderFormats, VideoDim, 2, 1, 4, References);
    }
    else if (codec_is_vtb(CodecID))
    {
        result = CreateBuffers(FMT_VTB, m_renderFormats, VideoDim, 1, 4, 2);
    }
    else if (codec_is_vdpau(CodecID))
    {
        result = CreateBuffers(FMT_VDPAU, m_renderFormats, VideoDim, 2, 1, 4, References);
    }
    else if (codec_is_nvdec(CodecID))
    {
        result = CreateBuffers(FMT_NVDEC, m_renderFormats, VideoDim, 2, 1, 4);
    }
    else if (codec_is_mmal(CodecID))
    {
        result = CreateBuffers(FMT_MMAL, m_renderFormats, VideoDim, 2, 1, 4);
    }
    else if (codec_is_v4l2(CodecID) || codec_is_drmprime(CodecID))
    {
        result = CreateBuffers(FMT_DRMPRIME, m_renderFormats, VideoDim, 2, 1, 4);
    }
    else
    {
        result = CreateBuffers(FMT_YV12, m_renderFormats, VideoDim, 1, 8, 4, References);
    }

    LOG(VB_PLAYBACK, LOG_INFO, QString("DiscardAndRecreate: %1").arg(GetStatus()));
    m_globalLock.unlock();

    // and finally release references now that the lock is released
    DoDiscard(refs);
    return result;
}

frame_queue_t *VideoBuffers::Queue(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = nullptr;
    if (Type == kVideoBuffer_avail)
        queue = &m_available;
    else if (Type == kVideoBuffer_used)
        queue = &m_used;
    else if (Type == kVideoBuffer_displayed)
        queue = &m_displayed;
    else if (Type == kVideoBuffer_limbo)
        queue = &m_limbo;
    else if (Type == kVideoBuffer_pause)
        queue = &m_pause;
    else if (Type == kVideoBuffer_decode)
        queue = &m_decode;
    else if (Type == kVideoBuffer_finished)
        queue = &m_finished;
    return queue;
}

const frame_queue_t *VideoBuffers::Queue(BufferType Type) const
{
    QMutexLocker locker(&m_globalLock);
    const frame_queue_t *queue = nullptr;
    if (Type == kVideoBuffer_avail)
        queue = &m_available;
    else if (Type == kVideoBuffer_used)
        queue = &m_used;
    else if (Type == kVideoBuffer_displayed)
        queue = &m_displayed;
    else if (Type == kVideoBuffer_limbo)
        queue = &m_limbo;
    else if (Type == kVideoBuffer_pause)
        queue = &m_pause;
    else if (Type == kVideoBuffer_decode)
        queue = &m_decode;
    else if (Type == kVideoBuffer_finished)
        queue = &m_finished;
    return queue;
}

MythVideoFrame* VideoBuffers::At(uint FrameNum)
{
    return &m_buffers[FrameNum];
}

const MythVideoFrame* VideoBuffers::At(uint FrameNum) const
{
    return &m_buffers[FrameNum];
}

MythVideoFrame *VideoBuffers::Dequeue(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return nullptr;
    return queue->dequeue();
}

MythVideoFrame *VideoBuffers::Head(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return nullptr;
    if (!queue->empty())
        return queue->head();
    return nullptr;
}

MythVideoFrame *VideoBuffers::Tail(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return nullptr;
    if (!queue->empty())
        return queue->tail();
    return nullptr;
}

void VideoBuffers::Enqueue(BufferType Type, MythVideoFrame *Frame)
{
    if (!Frame)
        return;
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return;
    m_globalLock.lock();
    queue->remove(Frame);
    queue->enqueue(Frame);
    if (Type == kVideoBuffer_pause)
        Frame->m_pauseFrame = true;
    m_globalLock.unlock();
}

void VideoBuffers::Remove(BufferType Type, MythVideoFrame *Frame)
{
    if (!Frame)
        return;

    QMutexLocker locker(&m_globalLock);
    if ((Type & kVideoBuffer_avail) == kVideoBuffer_avail)
        m_available.remove(Frame);
    if ((Type & kVideoBuffer_used) == kVideoBuffer_used)
        m_used.remove(Frame);
    if ((Type & kVideoBuffer_displayed) == kVideoBuffer_displayed)
        m_displayed.remove(Frame);
    if ((Type & kVideoBuffer_limbo) == kVideoBuffer_limbo)
        m_limbo.remove(Frame);
    if ((Type & kVideoBuffer_pause) == kVideoBuffer_pause)
        m_pause.remove(Frame);
    if ((Type & kVideoBuffer_decode) == kVideoBuffer_decode)
        m_decode.remove(Frame);
    if ((Type & kVideoBuffer_finished) == kVideoBuffer_finished)
        m_finished.remove(Frame);
}

void VideoBuffers::SafeEnqueue(BufferType Type, MythVideoFrame* Frame)
{
    if (!Frame)
        return;
    QMutexLocker locker(&m_globalLock);
    Remove(kVideoBuffer_all, Frame);
    Enqueue(Type, Frame);
}

/*! \brief Lock the video buffers
 *
 * \note Use with caution. Holding this lock while releasing hardware buffers
 * will almost certainly lead to deadlocks.
*/
frame_queue_t::iterator VideoBuffers::BeginLock(BufferType Type)
{
    m_globalLock.lock();
    frame_queue_t *queue = Queue(Type);
    if (queue)
        return queue->begin();
    return m_available.begin();
}

void VideoBuffers::EndLock(void)
{
    m_globalLock.unlock();
}

frame_queue_t::iterator VideoBuffers::End(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = Queue(Type);
    return (queue ? queue->end() : m_available.end());
}

uint VideoBuffers::Size(BufferType Type) const
{
    QMutexLocker locker(&m_globalLock);
    const frame_queue_t *queue = Queue(Type);
    if (queue)
        return queue->size();
    return 0;
}

bool VideoBuffers::Contains(BufferType Type, MythVideoFrame *Frame) const
{
    QMutexLocker locker(&m_globalLock);
    const frame_queue_t *queue = Queue(Type);
    if (queue)
        return queue->contains(Frame);
    return false;
}

MythVideoFrame* VideoBuffers::GetLastDecodedFrame(void)
{
    return At(m_vpos);
}

MythVideoFrame* VideoBuffers::GetLastShownFrame(void)
{
    return At(m_rpos);
}

uint VideoBuffers::ValidVideoFrames(void) const
{
    return Size(kVideoBuffer_used);
}

uint VideoBuffers::FreeVideoFrames(void) const
{
    return Size(kVideoBuffer_avail);
}

bool VideoBuffers::EnoughFreeFrames(void) const
{
    return Size(kVideoBuffer_avail) >= m_needFreeFrames;
}

bool VideoBuffers::EnoughDecodedFrames(void) const
{
    return Size(kVideoBuffer_used) >= m_needPrebufferFrames;
}

const MythVideoFrame* VideoBuffers::GetLastDecodedFrame(void) const
{
    return At(m_vpos);
}

const MythVideoFrame* VideoBuffers::GetLastShownFrame(void) const
{
    return At(m_rpos);
}

uint VideoBuffers::Size(void) const
{
    return m_buffers.size();
}

/**
 * \fn VideoBuffers::DiscardFrames(bool)
 *  Mark all used frames as ready to be reused, this is for seek.
 */
void VideoBuffers::DiscardFrames(bool NextFrameIsKeyFrame)
{
    std::vector<AVBufferRef*> refs;
    m_globalLock.lock();
    LOG(VB_PLAYBACK, LOG_INFO, QString("VideoBuffers::DiscardFrames(%1): %2")
            .arg(NextFrameIsKeyFrame).arg(GetStatus()));

    if (!NextFrameIsKeyFrame)
    {
        frame_queue_t ula(m_used);
        for (auto & it : ula)
        {
            ReleaseDecoderResources(it, refs);
            SafeEnqueue(kVideoBuffer_avail, it);
        }
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("VideoBuffers::DiscardFrames(%1): %2 -- done")
                .arg(NextFrameIsKeyFrame).arg(GetStatus()));
        m_globalLock.unlock();
        DoDiscard(refs);
        return;
    }

    // Remove inheritence of all frames not in displayed or pause
    frame_queue_t ula(m_used);
    ula.insert(ula.end(), m_limbo.begin(), m_limbo.end());
    ula.insert(ula.end(), m_available.begin(), m_available.end());
    ula.insert(ula.end(), m_finished.begin(), m_finished.end());
    frame_queue_t::iterator it;

    // Discard frames
    frame_queue_t discards(m_used);
    discards.insert(discards.end(), m_limbo.begin(), m_limbo.end());
    discards.insert(discards.end(), m_finished.begin(), m_finished.end());
    for (it = discards.begin(); it != discards.end(); ++it)
    {
        ReleaseDecoderResources(*it, refs);
        SafeEnqueue(kVideoBuffer_avail, *it);
    }

    // Verify that things are kosher
    if (m_available.count() + m_pause.count() + m_displayed.count() != Size())
    {
        for (uint i = 0; i < Size(); i++)
        {
            if (!m_available.contains(At(i)) && !m_pause.contains(At(i)) &&
                !m_displayed.contains(At(i)))
            {
                // This message is DEBUG because it does occur
                // after Reset is called.
                // That happens when failing over from OpenGL
                // to another method, if QT painter is selected.
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("VideoBuffers::DiscardFrames(): %1 (%2) not "
                            "in available, pause, or displayed %3")
                        .arg(DebugString(At(i), true)).arg((long long)At(i))
                        .arg(GetStatus()));
                ReleaseDecoderResources(At(i), refs);
                SafeEnqueue(kVideoBuffer_avail, At(i));
            }
        }
    }

    // Make sure frames used by decoder are last...
    // This is for libmpeg2 which still uses the frames after a reset.
    for (it = m_decode.begin(); it != m_decode.end(); ++it)
        Remove(kVideoBuffer_all, *it);
    for (it = m_decode.begin(); it != m_decode.end(); ++it)
        m_available.enqueue(*it);
    m_decode.clear();

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("VideoBuffers::DiscardFrames(%1): %2 -- done")
            .arg(NextFrameIsKeyFrame).arg(GetStatus()));

    m_globalLock.unlock();
    DoDiscard(refs);
}

/*! \brief Clear used frames after seeking.
 *
 * \note The use of this function after seeking is a little
 * dubious as the decoder is often not paused. In extreme cases
 * every video buffer has already been requested by the decoder
 * by the time this function is called. Should probably be
 * repurposed as ClearBeforeSeek...
*/
void VideoBuffers::ClearAfterSeek(void)
{
    std::vector<AVBufferRef*> discards;
    {
        QMutexLocker locker(&m_globalLock);

        for (uint i = 0; i < Size(); i++)
            At(i)->m_timecode = 0ms;

        for (uint i = 0; (i < Size()) && (m_used.count() > 1); i++)
        {
            MythVideoFrame *buffer = At(i);
            if (m_used.contains(buffer) && !m_decode.contains(buffer))
            {
                m_used.remove(buffer);
                m_available.enqueue(buffer);
                ReleaseDecoderResources(buffer, discards);
            }
        }

        if (m_used.count() > 0)
        {
            for (uint i = 0; i < Size(); i++)
            {
                MythVideoFrame *buffer = At(i);
                if (m_used.contains(buffer) && !m_decode.contains(buffer))
                {
                    m_used.remove(buffer);
                    m_available.enqueue(buffer);
                    ReleaseDecoderResources(buffer, discards);
                    m_vpos = m_vbufferMap[buffer];
                    m_rpos = m_vpos;
                    break;
                }
            }
        }
        else
        {
            m_vpos = m_rpos = 0;
        }
    }

    DoDiscard(discards);
}

bool VideoBuffers::CreateBuffers(VideoFrameType Type, const VideoFrameTypes* RenderFormats, QSize Size,
                                 uint NeedFree, uint NeedprebufferNormal,
                                 uint NeedPrebufferSmall, int MaxReferenceFrames)
{
    m_renderFormats = RenderFormats;
    Init(GetNumBuffers(Type, MaxReferenceFrames), NeedFree, NeedprebufferNormal, NeedPrebufferSmall);
    return CreateBuffers(Type, Size.width(), Size.height(), m_renderFormats);
}

bool VideoBuffers::CreateBuffers(VideoFrameType Type, int Width, int Height, const VideoFrameTypes* RenderFormats)
{
    bool success = true;
    m_renderFormats = RenderFormats;

    // Hardware buffers with no allocated memory
    if (MythVideoFrame::HardwareFormat(Type))
    {
        for (uint i = 0; i < Size(); i++)
            m_buffers[i].Init(Type, Width, Height, m_renderFormats);
        LOG(VB_PLAYBACK, LOG_INFO, QString("Created %1 empty %2 (%3x%4) video buffers")
           .arg(Size()).arg(MythVideoFrame::FormatDescription(Type)).arg(Width).arg(Height));
        return true;
    }

    // Software buffers
    for (uint i = 0; i < Size(); i++)
    {
        m_buffers[i].Init(Type, Width, Height, m_renderFormats);
        m_buffers[i].ClearBufferToBlank();
        success &= m_buffers[i].m_dummy || (m_buffers[i].m_buffer != nullptr);
    }

    // workaround null buffers for audio only (remove when these buffers aren't used)
    if (!success && (Width < 1 || Height < 1))
        success = true;

    LOG(VB_PLAYBACK, LOG_INFO, QString("Created %1 %2 (%3x%4) video buffers")
       .arg(Size()).arg(MythVideoFrame::FormatDescription(Type)).arg(Width).arg(Height));
    return success;
}

bool VideoBuffers::ReinitBuffer(MythVideoFrame *Frame, VideoFrameType Type, MythCodecID CodecID,
                                int Width, int Height)
{
    if (!Frame)
        return false;

    if (MythVideoFrame::HardwareFormat(Type) || MythVideoFrame::HardwareFormat(Frame->m_type))
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot re-initialise a hardware buffer");
        return false;
    }

    VideoFrameType old = Frame->m_type;
    const auto * formats = Frame->m_renderFormats;
    LOG(VB_PLAYBACK, LOG_INFO, QString("Reallocating frame %1 %2x%3->%4 %5x%6")
        .arg(MythVideoFrame::FormatDescription(old)).arg(Frame->m_width).arg(Frame->m_height)
        .arg(MythVideoFrame::FormatDescription(Type)).arg(Width).arg(Height));

    MythDeintType singler = Frame->m_deinterlaceSingle;
    MythDeintType doubler = Frame->m_deinterlaceDouble;
    Frame->Init(Type, Width, Height, formats);
    Frame->ClearBufferToBlank();

    // retain deinterlacer settings and update restrictions based on new frame type
    SetDeinterlacingFlags(*Frame, singler, doubler, CodecID);
    return true;
}

static unsigned long long to_bitmap(const frame_queue_t& Queue, int Num);

QString VideoBuffers::GetStatus(uint Num) const
{
    if (Num == 0)
        Num = Size();

    QString str("");
    if (m_globalLock.tryLock())
    {
        int count = Size();
        unsigned long long a = to_bitmap(m_available, count);
        unsigned long long u = to_bitmap(m_used, count);
        unsigned long long d = to_bitmap(m_displayed, count);
        unsigned long long l = to_bitmap(m_limbo, count);
        unsigned long long p = to_bitmap(m_pause, count);
        unsigned long long f = to_bitmap(m_finished, count);
        unsigned long long x = to_bitmap(m_decode, count);
        for (uint i = 0; i < Num; i++)
        {
            unsigned long long mask = 1ULL << i;
            QString tmp("");
            if (a & mask)
                tmp += (x & mask) ? "a" : "A";
            if (u & mask)
                tmp += (x & mask) ? "u" : "U";
            if (d & mask)
                tmp += (x & mask) ? "d" : "D";
            if (l & mask)
                tmp += (x & mask) ? "l" : "L";
            if (p & mask)
                tmp += (x & mask) ? "p" : "P";
            if (f & mask)
                tmp += (x & mask) ? "f" : "F";
            if (0 == tmp.length())
                str += " ";
            else if (1 == tmp.length())
                str += tmp;
            else
                str += "(" + tmp + ")";
        }
        m_globalLock.unlock();
    }
    else
    {
        for (uint i = 0; i < Num; i++)
            str += " ";
    }
    return str;
}

/*******************************
 ** Debugging functions below **
 *******************************/

static constexpr size_t DBG_STR_ARR_SIZE { 40 };
const std::array<const QString,DBG_STR_ARR_SIZE> dbg_str_arr
{
    "A       ",    " B      ",    "  C     ",    "   D    ",
    "    E   ",    "     F  ",    "      G ",    "       H", // 8
    "a       ",    " b      ",    "  c     ",    "   d    ",
    "    e   ",    "     f  ",    "      g ",    "       h", // 16
    "0       ",    " 1      ",    "  2     ",    "   3    ",
    "    4   ",    "     5  ",    "      6 ",    "       7", // 24
    "I       ",    " J      ",    "  K     ",    "   L    ",
    "    M   ",    "     N  ",    "      O ",    "       P", // 32
    "i       ",    " j      ",    "  k     ",    "   l    ",
    "    m   ",    "     n  ",    "      o ",    "       p", // 40
};
const std::array<const QString,DBG_STR_ARR_SIZE> dbg_str_arr_short
{
    "A","B","C","D","E","F","G","H", // 8
    "a","b","c","d","e","f","g","h", // 16
    "0","1","2","3","4","5","6","7", // 24
    "I","J","K","L","M","N","O","P", // 32
    "i","j","k","l","m","n","o","p", // 40
};

std::map<const MythVideoFrame *, int> dbg_str;

static int DebugNum(const MythVideoFrame *Frame)
{
    auto it = dbg_str.find(Frame);
    if (it == dbg_str.end())
        return dbg_str[Frame] = next_dbg_str++;
    return it->second;
}

const QString& DebugString(const MythVideoFrame *Frame, bool Short)
{
    if (Short)
        return dbg_str_arr_short[DebugNum(Frame) % DBG_STR_ARR_SIZE];
    return dbg_str_arr[DebugNum(Frame) % DBG_STR_ARR_SIZE];
}

const QString& DebugString(uint FrameNum, bool Short)
{
    return ((Short) ? dbg_str_arr_short : dbg_str_arr)[FrameNum];
}

static unsigned long long to_bitmap(const frame_queue_t& Queue, int Num)
{
    unsigned long long bitmap = 0;
    for (auto *it : Queue)
    {
        int shift = DebugNum(it) % Num;
        bitmap |= 1ULL << shift;
    }
    return bitmap;
}
