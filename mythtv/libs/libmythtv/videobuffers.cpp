// Copyright (c) 2005, Daniel Thor Kristjansson
// based on earlier work in MythTV's videout_xvmc.cpp

// MythTV
#include "mythconfig.h"
#include "mythcontext.h"
#include "fourcc.h"
#include "compat.h"
#include "mythlogging.h"
#include "videobuffers.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
}

// Std
#include <chrono>
#include <thread>

#define TRY_LOCK_SPINS                 2000
#define TRY_LOCK_SPINS_BEFORE_WARNING  9999
#define TRY_LOCK_SPIN_WAIT             1000 /* usec */

int next_dbg_str = 0;

YUVInfo::YUVInfo(uint Width, uint Height, uint Size, const int *Pitches,
                 const int *Offsets, int Alignment)
  : m_width(Width),
    m_height(Height),
    m_size(Size)
{
    // make sure all our pitches are a multiple of "aligned" bytes
    // Needs to take into consideration that U and V channels are half
    // the width of Y channel
    uint alignedwidth;

    if (!Alignment)
        alignedwidth = m_width;
    else
        alignedwidth = (m_width + Alignment - 1) & ~(Alignment - 1);

    if (Pitches)
    {
        memcpy(m_pitches, Pitches, 3 * sizeof(int));
    }
    else
    {
        m_pitches[0] = alignedwidth;
        m_pitches[1] = m_pitches[2] = (alignedwidth+1) >> 1;
    }

    if (Offsets)
    {
        memcpy(m_offsets, Offsets, 3 * sizeof(int));
    }
    else
    {
        m_offsets[0] = 0;
        m_offsets[1] = alignedwidth * m_height;
        m_offsets[2] = m_offsets[1] + ((alignedwidth+1) >> 1) * ((m_height+1) >> 1);
    }
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

uint VideoBuffers::GetNumBuffers(int PixelFormat)
{
    switch (PixelFormat)
    {
        case FMT_VDPAU: return 28;
        case FMT_NVDEC: return 8;
        case FMT_VTB:   return 24;
        case FMT_VAAPI: return 24;
        case FMT_DXVA2: return 30;
        case FMT_YV12:  return 31;
        case FMT_MEDIACODEC: return 8;
        default: break;
    }
    return 30;
}

VideoBuffers::~VideoBuffers()
{
    DeleteBuffers();
}

/*! \brief Creates buffers and sets various buffer management parameters.
 *
 *  This normally creates numdecode buffers, but it creates
 *  one more buffer if extra_for_pause is true. Only numdecode
 *  buffers are added to available and hence into the buffer
 *  management handled by VideoBuffers. The availability of
 *  any scratch frame must be managed by the video output
 *  class itself.
 *
 * \param NumDecode           number of buffers to allocate for normal use
 * \param ExtraForPause       allocate an extra buffer, a scratch a frame for pause
 * \param NeedFree            maximum number of buffers needed in display
 *                            and pause
 * \param NeedPrebufferNormal number buffers you can put in used or limbo normally
 * \param NeedPrebufferSmall  number of buffers you can put in used or limbo
 *                            after SetPrebuffering(false) has been called.
 * \param KeepPrebuffer       number of buffers in used or limbo that are considered
 *                            enough for decent playback.
 */
void VideoBuffers::Init(uint NumDecode, bool ExtraForPause,
                        uint NeedFree,  uint NeedPrebufferNormal,
                        uint NeedPrebufferSmall, uint KeepPrebuffer)
{
    QMutexLocker locker(&m_globalLock);

    Reset();

    uint numcreate = NumDecode + ((ExtraForPause) ? 1 : 0);

    // make a big reservation, so that things that depend on
    // pointer to VideoFrames work even after a few push_backs
    m_buffers.reserve(max(numcreate, (uint)128));

    m_buffers.resize(numcreate);
    for (uint i = 0; i < numcreate; i++)
    {
        memset(At(i), 0, sizeof(VideoFrame));
        At(i)->codec            = FMT_NONE;
        At(i)->interlaced_frame = -1;
        At(i)->top_field_first  = 1;
        m_vbufferMap[At(i)]     = i;
    }

    m_needFreeFrames            = NeedFree;
    m_needPrebufferFrames       = NeedPrebufferNormal;
    m_needPrebufferFramesNormal = NeedPrebufferNormal;
    m_needPrebufferFramesSmall  = NeedPrebufferSmall;
    m_keepPrebufferFrames       = KeepPrebuffer;
    m_createdPauseFrame         = ExtraForPause;

    if (m_createdPauseFrame)
        Enqueue(kVideoBuffer_pause, At(numcreate - 1));

    for (uint i = 0; i < NumDecode; i++)
        Enqueue(kVideoBuffer_avail, At(i));
    SetDeinterlacing(DEINT_NONE, DEINT_NONE);
}

void VideoBuffers::SetDeinterlacing(MythDeintType Single, MythDeintType Double)
{
    QMutexLocker locker(&m_globalLock);
    frame_vector_t::iterator it = m_buffers.begin();
    for ( ; it != m_buffers.end(); ++it)
        SetDeinterlacingFlags(*it, Single, Double);
}

/*! \brief Set the appropriate flags for single and double rate deinterlacing
 * \note Double rate support is determined by the VideoOutput class and must be set appropriately
 * \note Driver deinterlacers are only available for hardware frames with the exception of
 * NVDEC and VTB which can use shaders.
 * \note Shader and CPU deinterlacers are disabled for hardware frames (except for shaders with NVDEC and VTB)
 * \note There is no support for CPU deinterlacing of NV12 frames (and 10bit eqivalents) so we
 * fallback to shaders
 * \todo Handling of decoder deinterlacing with NVDEC
*/
void VideoBuffers::SetDeinterlacingFlags(VideoFrame &Frame, MythDeintType Single, MythDeintType Double)
{
    static const MythDeintType hardware = DEINT_ALL & ~(DEINT_CPU | DEINT_SHADER);
    static const MythDeintType software = DEINT_ALL & ~DEINT_DRIVER;
    static const MythDeintType shaders  = software & ~DEINT_CPU;
    Frame.deinterlace_single = Single;
    Frame.deinterlace_double = Double;
    Frame.deinterlace_allowed = format_is_hw(Frame.codec) ? (format_is_hwyuv(Frame.codec) ? software : hardware) :
                               (format_is_nv12(Frame.codec) ? shaders : software);
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

void VideoBuffers::ReleaseDecoderResources(VideoFrame *Frame)
{
#if defined(USING_MEDIACODEC) || defined(USING_VTB) || defined(USING_VAAPI) || defined(USING_VDPAU) || defined(USING_NVDEC)
    if ((Frame->codec == FMT_VTB)   || (Frame->codec == FMT_MEDIACODEC) ||
        (Frame->codec == FMT_VAAPI) || (Frame->codec == FMT_VDPAU) ||
        (Frame->codec == FMT_NVDEC))
    {
        AVBufferRef* ref = reinterpret_cast<AVBufferRef*>(Frame->priv[0]);
        if (ref != nullptr)
            av_buffer_unref(&ref);
        Frame->buf = Frame->priv[0] = nullptr;
#if defined(USING_VAAPI) || defined(USING_VDPAU) || defined(USING_NVDEC)
        ref = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
        if (ref != nullptr)
            av_buffer_unref(&ref);
        Frame->priv[1] = nullptr;
#endif
    }
#endif
    (void)Frame;
}

VideoFrame *VideoBuffers::GetNextFreeFrameInternal(BufferType EnqueueTo)
{
    QMutexLocker locker(&m_globalLock);
    VideoFrame *frame = nullptr;

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
                .arg(DebugString(frame, true)).arg(GetStatus()));
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
VideoFrame *VideoBuffers::GetNextFreeFrame(BufferType EnqueueTo)
{
    for (uint tries = 1; true; tries++)
    {
        VideoFrame *frame = VideoBuffers::GetNextFreeFrameInternal(EnqueueTo);
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
        std::this_thread::sleep_for(std::chrono::microseconds(TRY_LOCK_SPIN_WAIT));
    }

    return nullptr;
}

/**
 * \fn VideoBuffers::ReleaseFrame(VideoFrame*)
 *  Frame is ready to be for filtering or OSD application.
 *  Removes frame from limbo and adds it to used queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::ReleaseFrame(VideoFrame *Frame)
{
    QMutexLocker locker(&m_globalLock);

    m_vpos = m_vbufferMap[Frame];
    m_limbo.remove(Frame);
    //non directrendering frames are ffmpeg handled
    if (Frame->directrendering != 0)
        m_decode.enqueue(Frame);
    m_used.enqueue(Frame);
}

/**
 * \fn VideoBuffers::DeLimboFrame(VideoFrame*)
 *  If the frame is still in the limbo state it is added to the available queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::DeLimboFrame(VideoFrame *Frame)
{
    QMutexLocker locker(&m_globalLock);
    if (m_limbo.contains(Frame))
        m_limbo.remove(Frame);

    // if decoder didn't release frame and the buffer is getting released by
    // the decoder assume that the frame is lost and return to available
    if (!m_decode.contains(Frame))
    {
        ReleaseDecoderResources(Frame);
        SafeEnqueue(kVideoBuffer_avail, Frame);
    }

    // remove from decode queue since the decoder is finished
    while (m_decode.contains(Frame))
        m_decode.remove(Frame);
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
void VideoBuffers::DoneDisplayingFrame(VideoFrame *Frame)
{
    QMutexLocker locker(&m_globalLock);

    if(m_used.contains(Frame))
        Remove(kVideoBuffer_used, Frame);

    Enqueue(kVideoBuffer_finished, Frame);

    // check if any finished frames are no longer used by decoder and return to available
    frame_queue_t ula(m_finished);
    frame_queue_t::iterator it = ula.begin();
    for (; it != ula.end(); ++it)
    {
        if (!m_decode.contains(*it))
        {
            Remove(kVideoBuffer_finished, *it);
            ReleaseDecoderResources(*it);
            Enqueue(kVideoBuffer_avail, *it);
        }
    }
}

/**
 * \fn VideoBuffers::DiscardFrame(VideoFrame*)
 *  Frame is ready to be reused by decoder.
 *  Add frame to available list, remove from any other list.
 */
void VideoBuffers::DiscardFrame(VideoFrame *Frame)
{
    QMutexLocker locker(&m_globalLock);
    ReleaseDecoderResources(Frame);
    SafeEnqueue(kVideoBuffer_avail, Frame);
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

VideoFrame* VideoBuffers::At(uint FrameNum)
{
    return &m_buffers[FrameNum];
}

const VideoFrame* VideoBuffers::At(uint FrameNum) const
{
    return &m_buffers[FrameNum];
}

VideoFrame *VideoBuffers::Dequeue(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return nullptr;
    return queue->dequeue();
}

VideoFrame *VideoBuffers::Head(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return nullptr;
    if (!queue->empty())
        return queue->head();
    return nullptr;
}

VideoFrame *VideoBuffers::Tail(BufferType Type)
{
    QMutexLocker locker(&m_globalLock);
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return nullptr;
    if (!queue->empty())
        return queue->tail();
    return nullptr;
}

void VideoBuffers::Enqueue(BufferType Type, VideoFrame *Frame)
{
    if (!Frame)
        return;
    frame_queue_t *queue = Queue(Type);
    if (!queue)
        return;
    m_globalLock.lock();
    queue->remove(Frame);
    queue->enqueue(Frame);
    m_globalLock.unlock();
}

void VideoBuffers::Remove(BufferType Type, VideoFrame *Frame)
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

void VideoBuffers::Requeue(BufferType Dest, BufferType Source, int Count)
{
    QMutexLocker locker(&m_globalLock);
    Count = (Count <= 0) ? Size(Source) : Count;
    for (uint i=0; i<(uint)Count; i++)
    {
        VideoFrame *frame = Dequeue(Source);
        if (frame)
            Enqueue(Dest, frame);
    }
}

void VideoBuffers::SafeEnqueue(BufferType Type, VideoFrame* Frame)
{
    if (!Frame)
        return;
    QMutexLocker locker(&m_globalLock);
    Remove(kVideoBuffer_all, Frame);
    Enqueue(Type, Frame);
}

frame_queue_t::iterator VideoBuffers::BeginLock(BufferType Type)
{
    m_globalLock.lock();
    frame_queue_t *queue = Queue(Type);
    if (queue)
        return queue->begin();
    else
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

bool VideoBuffers::Contains(BufferType Type, VideoFrame *Frame) const
{
    QMutexLocker locker(&m_globalLock);
    const frame_queue_t *queue = Queue(Type);
    if (queue)
        return queue->contains(Frame);
    return false;
}

VideoFrame *VideoBuffers::GetScratchFrame(void)
{
    if (!m_createdPauseFrame || !Head(kVideoBuffer_pause))
    {
        LOG(VB_GENERAL, LOG_ERR, "GetScratchFrame() called, but not allocated");
        return nullptr;
    }

    QMutexLocker locker(&m_globalLock);
    return Head(kVideoBuffer_pause);
}

VideoFrame* VideoBuffers::GetLastDecodedFrame(void)
{
    return At(m_vpos);
}

VideoFrame* VideoBuffers::GetLastShownFrame(void)
{
    return At(m_rpos);
}

void VideoBuffers::SetLastShownFrameToScratch(void)
{
    if (!m_createdPauseFrame || !Head(kVideoBuffer_pause))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "SetLastShownFrameToScratch() called but no pause frame");
        return;
    }

    VideoFrame *pause = Head(kVideoBuffer_pause);
    m_rpos = m_vbufferMap[pause];
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

bool VideoBuffers::EnoughPrebufferedFrames(void) const
{
    return Size(kVideoBuffer_used) >= m_keepPrebufferFrames;
}

const VideoFrame* VideoBuffers::GetLastDecodedFrame(void) const
{
    return At(m_vpos);
}

const VideoFrame* VideoBuffers::GetLastShownFrame(void) const
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
    QMutexLocker locker(&m_globalLock);
    LOG(VB_PLAYBACK, LOG_INFO, QString("VideoBuffers::DiscardFrames(%1): %2")
            .arg(NextFrameIsKeyFrame).arg(GetStatus()));

    if (!NextFrameIsKeyFrame)
    {
        frame_queue_t ula(m_used);
        frame_queue_t::iterator it = ula.begin();
        for (; it != ula.end(); ++it)
            DiscardFrame(*it);
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("VideoBuffers::DiscardFrames(%1): %2 -- done")
                .arg(NextFrameIsKeyFrame).arg(GetStatus()));
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
        DiscardFrame(*it);

    // Verify that things are kosher
    if (m_available.count() + m_pause.count() + m_displayed.count() != Size())
    {
        for (uint i=0; i < Size(); i++)
        {
            if (!m_available.contains(At(i)) &&
                !m_pause.contains(At(i)) &&
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
                DiscardFrame(At(i));
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
    {
        QMutexLocker locker(&m_globalLock);

        for (uint i = 0; i < Size(); i++)
            At(i)->timecode = 0;

        for (uint i = 0; (i < Size()) && (m_used.count() > 1); i++)
        {
            VideoFrame *buffer = At(i);
            if (m_used.contains(buffer) && !m_decode.contains(buffer))
            {
                m_used.remove(buffer);
                m_available.enqueue(buffer);
                ReleaseDecoderResources(buffer);
            }
        }

        if (m_used.count() > 0)
        {
            for (uint i = 0; i < Size(); i++)
            {
                VideoFrame *buffer = At(i);
                if (m_used.contains(buffer) && !m_decode.contains(buffer))
                {
                    m_used.remove(buffer);
                    m_available.enqueue(buffer);
                    ReleaseDecoderResources(buffer);
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
}

bool VideoBuffers::CreateBuffers(VideoFrameType Type, QSize Size, bool ExtraForPause,
                                 uint NeedFree, uint NeedprebufferNormal,
                                 uint NeedPrebufferSmall, uint KeepPrebuffer)
{
    Init(GetNumBuffers(Type), ExtraForPause, NeedFree, NeedprebufferNormal,
         NeedPrebufferSmall, KeepPrebuffer);
    return CreateBuffers(Type, Size.width(), Size.height());
}

bool VideoBuffers::CreateBuffers(VideoFrameType Type, int Width, int Height)
{
    if (format_is_hw(Type))
    {
        bool success = true;
        for (uint i = 0; i < Size(); i++)
            success &= CreateBuffer(Width, Height, i, nullptr, Type);
        return success;
    }

    vector<YUVInfo> yuvinfo;
    return CreateBuffers(Type, Width, Height, yuvinfo);
}

bool VideoBuffers::CreateBuffers(VideoFrameType Type, int Width, int Height, vector<YUVInfo> YUVInfos)
{
    if ((FMT_YV12 != Type) && (FMT_YUY2 != Type))
        return false;

    uint bufsize = buffersize(Type, Width, Height);
    while (YUVInfos.size() < Size())
        YUVInfos.emplace_back(Width, Height, bufsize, nullptr, nullptr);

    bool ok = true;
    for (uint i = 0; i < Size(); i++)
    {
        uint size = std::max(bufsize, YUVInfos[i].m_size);
        unsigned char *data = static_cast<unsigned char*>(av_malloc(size + 64));
        if (!data)
            LOG(VB_GENERAL, LOG_CRIT, "Failed to allocate video buffer memory");
        init(&m_buffers[i], Type, data, YUVInfos[i].m_width, YUVInfos[i].m_height, size,
             (const int*) YUVInfos[i].m_pitches, (const int*) YUVInfos[i].m_offsets);
        ok &= (m_buffers[i].buf != nullptr);
    }

    Clear();

    return ok;
}

bool VideoBuffers::CreateBuffer(int Width, int Height, uint Number,
                                void* Data, VideoFrameType Format)
{
    if (Number >= Size())
        return false;
    init(&m_buffers[Number], Format, (unsigned char*)Data, Width, Height, 0);
    return true;
}

void VideoBuffers::DeleteBuffers(void)
{
    next_dbg_str = 0;
    for (uint i = 0; i < Size(); i++)
        av_freep(&(m_buffers[i].buf));
}

bool VideoBuffers::ReinitBuffer(VideoFrame *Frame, VideoFrameType Type)
{
    if (!Frame)
        return false;
    if (format_is_hw(Type) || format_is_hw(Frame->codec))
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot re-initialise a hardware buffer");
        return false;
    }

    // Find the frame
    BeginLock(kVideoBuffer_all);
    for (uint i = 0; i < Size(); ++i)
    {
        if (At(i) == Frame)
        {
            VideoFrameType old = Frame->codec;
            int size = static_cast<int>(buffersize(Type, Frame->width, Frame->height));
            unsigned char *buf = Frame->buf;
            if ((Frame->size != size) || !buf)
            {
                // Free existing buffer
                av_freep(&buf);
                Frame->buf = nullptr;

                // Initialise new
                buf = static_cast<unsigned char*>(av_malloc(static_cast<size_t>(size + 64)));
                if (!buf)
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to reallocate frame buffer");
                    EndLock();
                    return false;
                }
            }

            MythDeintType singler = Frame->deinterlace_single;
            MythDeintType doubler = Frame->deinterlace_double;
            init(Frame, Type, buf, Frame->width, Frame->height, size);
            // retain deinterlacer settings and update restrictions based on new frame type
            SetDeinterlacingFlags(*Frame, singler, doubler);
            clear(Frame);
            LOG(VB_PLAYBACK, LOG_INFO, QString("Reallocated frame %1->%2")
                .arg(format_description(old)).arg(format_description(Type)));
            EndLock();
            return true;
        }
    }
    EndLock();
    LOG(VB_GENERAL, LOG_ERR, "Failed to find frame to reallocate");
    return false;
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
            unsigned long long mask = 1Ull << i;
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

void VideoBuffers::Clear(uint FrameNum)
{
    clear(At(FrameNum));
}

void VideoBuffers::Clear(void)
{
    for (uint i = 0; i < Size(); i++)
        Clear(i);
}

/*******************************
 ** Debugging functions below **
 *******************************/

#define DBG_STR_ARR_SIZE 40
QString dbg_str_arr[DBG_STR_ARR_SIZE] =
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
QString dbg_str_arr_short[DBG_STR_ARR_SIZE] =
{
    "A","B","C","D","E","F","G","H", // 8
    "a","b","c","d","e","f","g","h", // 16
    "0","1","2","3","4","5","6","7", // 24
    "I","J","K","L","M","N","O","P", // 32
    "i","j","k","l","m","n","o","p", // 40
};

map<const VideoFrame *, int> dbg_str;

static int DebugNum(const VideoFrame *Frame)
{
    map<const VideoFrame *, int>::iterator it = dbg_str.find(Frame);
    if (it == dbg_str.end())
        return dbg_str[Frame] = next_dbg_str++;
    return it->second;
}

const QString& DebugString(const VideoFrame *Frame, bool Short)
{
    if (Short)
        return dbg_str_arr_short[DebugNum(Frame) % DBG_STR_ARR_SIZE];
    else
        return dbg_str_arr[DebugNum(Frame) % DBG_STR_ARR_SIZE];
}

const QString& DebugString(uint FrameNum, bool Short)
{
    return ((Short) ? dbg_str_arr_short : dbg_str_arr)[FrameNum];
}

static unsigned long long to_bitmap(const frame_queue_t& Queue, int Num)
{
    unsigned long long bitmap = 0;
    frame_queue_t::const_iterator it = Queue.begin();
    for (; it != Queue.end(); ++it)
    {
        int shift = DebugNum(*it) % Num;
        bitmap |= 1ULL << shift;
    }
    return bitmap;
}
