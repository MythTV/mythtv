// Copyright (c) 2005, Daniel Thor Kristjansson
// based on earlier work in MythTV's videout_xvmc.cpp

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

#include "mythconfig.h"

#include "mythcontext.h"
#include "videobuffers.h"
extern "C" {
#include "libavcodec/avcodec.h"
}
#include "fourcc.h"
#include "compat.h"
#include "mythlogging.h"

#define TRY_LOCK_SPINS                 2000
#define TRY_LOCK_SPINS_BEFORE_WARNING  9999
#define TRY_LOCK_SPIN_WAIT             1000 /* usec */

int next_dbg_str = 0;

YUVInfo::YUVInfo(uint w, uint h, uint sz, const int *p, const int *o,
                 int aligned)
    : m_width(w), m_height(h), m_size(sz)
{
    // make sure all our pitches are a multiple of "aligned" bytes
    // Needs to take into consideration that U and V channels are half
    // the width of Y channel
    uint width_aligned;

    if (!aligned)
    {
        width_aligned = m_width;
    }
    else
    {
        width_aligned = (m_width + aligned - 1) & ~(aligned - 1);
    }

    if (p)
    {
        memcpy(m_pitches, p, 3 * sizeof(int));
    }
    else
    {
        m_pitches[0] = width_aligned;
        m_pitches[1] = m_pitches[2] = (width_aligned+1) >> 1;
    }

    if (o)
    {
        memcpy(m_offsets, o, 3 * sizeof(int));
    }
    else
    {
        m_offsets[0] = 0;
        m_offsets[1] = width_aligned * m_height;
        m_offsets[2] = m_offsets[1] + ((width_aligned+1) >> 1) * ((m_height+1) >> 1);
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

VideoBuffers::~VideoBuffers()
{
    DeleteBuffers();
}

/**
 * \fn VideoBuffers::Init(uint, bool, uint, uint, uint, uint, bool)
 *  Creates buffers and sets various buffer management parameters.
 *
 *  This normally creates numdecode buffers, but it creates
 *  one more buffer if extra_for_pause is true. Only numdecode
 *  buffers are added to available and hence into the buffer
 *  management handled by VideoBuffers. The availability of
 *  any scratch frame must be managed by the video output
 *  class itself.
 *
 * \param numdecode            number of buffers to allocate for normal use
 * \param extra_for_pause      allocate an extra buffer, a scratch a frame for pause
 * \param need_free            maximum number of buffers needed in display
 *                             and pause
 * \param needprebuffer_normal number buffers you can put in used or limbo normally
 * \param needprebuffer_small  number of buffers you can put in used or limbo
 *                             after SetPrebuffering(false) has been called.
 * \param keepprebuffer        number of buffers in used or limbo that are considered
 *                             enough for decent playback.
 */
void VideoBuffers::Init(uint numdecode, bool extra_for_pause,
                        uint need_free, uint needprebuffer_normal,
                        uint needprebuffer_small, uint keepprebuffer)
{
    QMutexLocker locker(&m_globalLock);

    Reset();

    uint numcreate = numdecode + ((extra_for_pause) ? 1 : 0);

    // make a big reservation, so that things that depend on
    // pointer to VideoFrames work even after a few push_backs
    m_buffers.reserve(max(numcreate, (uint)128));

    m_buffers.resize(numcreate);
    for (uint i = 0; i < numcreate; i++)
    {
        memset(At(i), 0, sizeof(VideoFrame));
        At(i)->codec            = FMT_NONE;
        At(i)->interlaced_frame = -1;
        At(i)->top_field_first  = +1;
        m_vbufferMap[At(i)]       = i;
    }

    m_needFreeFrames            = need_free;
    m_needPrebufferFrames       = needprebuffer_normal;
    m_needPrebufferFramesNormal = needprebuffer_normal;
    m_needPrebufferFramesSmall  = needprebuffer_small;
    m_keepPrebufferFrames       = keepprebuffer;
    m_createdPauseFrame         = extra_for_pause;

    if (m_createdPauseFrame)
        Enqueue(kVideoBuffer_pause, At(numcreate - 1));

    for (uint i = 0; i < numdecode; i++)
        Enqueue(kVideoBuffer_avail, At(i));
}

/**
 * \fn VideoBuffers::Reset()
 *  Resets the class so that Init may be called again.
 */
void VideoBuffers::Reset()
{
    QMutexLocker locker(&m_globalLock);

    // Delete ffmpeg VideoFrames so we can create
    // a different number of buffers below
    frame_vector_t::iterator it = m_buffers.begin();
    for (;it != m_buffers.end(); ++it)
    {
        av_freep(&it->qscale_table);
    }

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
void VideoBuffers::SetPrebuffering(bool normal)
{
    QMutexLocker locker(&m_globalLock);
    m_needPrebufferFrames = (normal) ?
        m_needPrebufferFramesNormal : m_needPrebufferFramesSmall;
}

VideoFrame *VideoBuffers::GetNextFreeFrameInternal(BufferType enqueue_to)
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
        SafeEnqueue(enqueue_to, frame);

    return frame;
}

/**
 * \fn VideoBuffers::GetNextFreeFrame(bool,bool,BufferType)
 *  Gets a frame from available buffers list.
 *
 * \param enqueue_to   put new frame in some state other than limbo.
 */
VideoFrame *VideoBuffers::GetNextFreeFrame(BufferType enqueue_to)
{
    for (uint tries = 1; true; tries++)
    {
        VideoFrame *frame = VideoBuffers::GetNextFreeFrameInternal(enqueue_to);

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
void VideoBuffers::ReleaseFrame(VideoFrame *frame)
{
    QMutexLocker locker(&m_globalLock);

    m_vpos = m_vbufferMap[frame];
    m_limbo.remove(frame);
    //non directrendering frames are ffmpeg handled
    if (frame->directrendering != 0)
        m_decode.enqueue(frame);
    m_used.enqueue(frame);
}

/**
 * \fn VideoBuffers::DeLimboFrame(VideoFrame*)
 *  If the frame is still in the limbo state it is added to the available queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::DeLimboFrame(VideoFrame *frame)
{
    QMutexLocker locker(&m_globalLock);
    if (m_limbo.contains(frame))
        m_limbo.remove(frame);

    // if decoder didn't release frame and the buffer is getting released by
    // the decoder assume that the frame is lost and return to available
    if (!m_decode.contains(frame))
        SafeEnqueue(kVideoBuffer_avail, frame);

    // remove from decode queue since the decoder is finished
    while (m_decode.contains(frame))
        m_decode.remove(frame);
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
 * \fn VideoBuffers::DoneDisplayingFrame(VideoFrame *frame)
 *  Removes frame from used queue and adds it to the available list.
 */
void VideoBuffers::DoneDisplayingFrame(VideoFrame *frame)
{
    QMutexLocker locker(&m_globalLock);

    if(m_used.contains(frame))
        Remove(kVideoBuffer_used, frame);

    Enqueue(kVideoBuffer_finished, frame);

    // check if any finished frames are no longer used by decoder and return to available
    frame_queue_t ula(m_finished);
    frame_queue_t::iterator it = ula.begin();
    for (; it != ula.end(); ++it)
    {
        if (!m_decode.contains(*it))
        {
            Remove(kVideoBuffer_finished, *it);
            Enqueue(kVideoBuffer_avail, *it);
        }
    }
}

/**
 * \fn VideoBuffers::DiscardFrame(VideoFrame*)
 *  Frame is ready to be reused by decoder.
 *  Add frame to available list, remove from any other list.
 */
void VideoBuffers::DiscardFrame(VideoFrame *frame)
{
    QMutexLocker locker(&m_globalLock);
    SafeEnqueue(kVideoBuffer_avail, frame);
}

frame_queue_t *VideoBuffers::Queue(BufferType type)
{
    QMutexLocker locker(&m_globalLock);

    frame_queue_t *q = nullptr;

    if (type == kVideoBuffer_avail)
        q = &m_available;
    else if (type == kVideoBuffer_used)
        q = &m_used;
    else if (type == kVideoBuffer_displayed)
        q = &m_displayed;
    else if (type == kVideoBuffer_limbo)
        q = &m_limbo;
    else if (type == kVideoBuffer_pause)
        q = &m_pause;
    else if (type == kVideoBuffer_decode)
        q = &m_decode;
    else if (type == kVideoBuffer_finished)
        q = &m_finished;

    return q;
}

const frame_queue_t *VideoBuffers::Queue(BufferType type) const
{
    QMutexLocker locker(&m_globalLock);

    const frame_queue_t *q = nullptr;

    if (type == kVideoBuffer_avail)
        q = &m_available;
    else if (type == kVideoBuffer_used)
        q = &m_used;
    else if (type == kVideoBuffer_displayed)
        q = &m_displayed;
    else if (type == kVideoBuffer_limbo)
        q = &m_limbo;
    else if (type == kVideoBuffer_pause)
        q = &m_pause;
    else if (type == kVideoBuffer_decode)
        q = &m_decode;
    else if (type == kVideoBuffer_finished)
        q = &m_finished;

    return q;
}

VideoFrame *VideoBuffers::Dequeue(BufferType type)
{
    QMutexLocker locker(&m_globalLock);

    frame_queue_t *q = Queue(type);

    if (!q)
        return nullptr;

    return q->dequeue();
}

VideoFrame *VideoBuffers::Head(BufferType type)
{
    QMutexLocker locker(&m_globalLock);

    frame_queue_t *q = Queue(type);

    if (!q)
        return nullptr;

    if (!q->empty())
        return q->head();

    return nullptr;
}

VideoFrame *VideoBuffers::Tail(BufferType type)
{
    QMutexLocker locker(&m_globalLock);

    frame_queue_t *q = Queue(type);

    if (!q)
        return nullptr;

    if (!q->empty())
        return q->tail();

    return nullptr;
}

void VideoBuffers::Enqueue(BufferType type, VideoFrame *frame)
{
    if (!frame)
        return;

    frame_queue_t *q = Queue(type);
    if (!q)
        return;

    m_globalLock.lock();
    q->remove(frame);
    q->enqueue(frame);
    m_globalLock.unlock();
}

void VideoBuffers::Remove(BufferType type, VideoFrame *frame)
{
    if (!frame)
        return;

    QMutexLocker locker(&m_globalLock);

    if ((type & kVideoBuffer_avail) == kVideoBuffer_avail)
        m_available.remove(frame);
    if ((type & kVideoBuffer_used) == kVideoBuffer_used)
        m_used.remove(frame);
    if ((type & kVideoBuffer_displayed) == kVideoBuffer_displayed)
        m_displayed.remove(frame);
    if ((type & kVideoBuffer_limbo) == kVideoBuffer_limbo)
        m_limbo.remove(frame);
    if ((type & kVideoBuffer_pause) == kVideoBuffer_pause)
        m_pause.remove(frame);
    if ((type & kVideoBuffer_decode) == kVideoBuffer_decode)
        m_decode.remove(frame);
    if ((type & kVideoBuffer_finished) == kVideoBuffer_finished)
        m_finished.remove(frame);
}

void VideoBuffers::Requeue(BufferType dst, BufferType src, int num)
{
    QMutexLocker locker(&m_globalLock);

    num = (num <= 0) ? Size(src) : num;
    for (uint i=0; i<(uint)num; i++)
    {
        VideoFrame *frame = Dequeue(src);
        if (frame)
            Enqueue(dst, frame);
    }
}

void VideoBuffers::SafeEnqueue(BufferType dst, VideoFrame* frame)
{
    if (!frame)
        return;

    QMutexLocker locker(&m_globalLock);

    Remove(kVideoBuffer_all, frame);
    Enqueue(dst, frame);
}

frame_queue_t::iterator VideoBuffers::begin_lock(BufferType type)
{
    m_globalLock.lock();
    frame_queue_t *q = Queue(type);
    if (q)
        return q->begin();
    return m_available.begin();
}

frame_queue_t::iterator VideoBuffers::end(BufferType type)
{
    QMutexLocker locker(&m_globalLock);

    frame_queue_t::iterator it;
    frame_queue_t *q = Queue(type);
    if (q)
        it = q->end();
    else
        it = m_available.end();

    return it;
}

uint VideoBuffers::Size(BufferType type) const
{
    QMutexLocker locker(&m_globalLock);

    const frame_queue_t *q = Queue(type);
    if (q)
        return q->size();

    return 0;
}

bool VideoBuffers::Contains(BufferType type, VideoFrame *frame) const
{
    QMutexLocker locker(&m_globalLock);

    const frame_queue_t *q = Queue(type);
    if (q)
        return q->contains(frame);

    return false;
}

VideoFrame *VideoBuffers::GetScratchFrame(void)
{
    if (!m_createdPauseFrame || !Head(kVideoBuffer_pause))
    {
        LOG(VB_GENERAL, LOG_ERR, "GetScratchFrame() called, but not allocated");
    }

    QMutexLocker locker(&m_globalLock);
    return Head(kVideoBuffer_pause);
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

/**
 * \fn VideoBuffers::DiscardFrames(bool)
 *  Mark all used frames as ready to be reused, this is for seek.
 */
void VideoBuffers::DiscardFrames(bool next_frame_keyframe)
{
    QMutexLocker locker(&m_globalLock);
    LOG(VB_PLAYBACK, LOG_INFO, QString("VideoBuffers::DiscardFrames(%1): %2")
            .arg(next_frame_keyframe).arg(GetStatus()));

    if (!next_frame_keyframe)
    {
        frame_queue_t ula(m_used);
        frame_queue_t::iterator it = ula.begin();
        for (; it != ula.end(); ++it)
            DiscardFrame(*it);
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("VideoBuffers::DiscardFrames(%1): %2 -- done")
                .arg(next_frame_keyframe).arg(GetStatus()));
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
            .arg(next_frame_keyframe).arg(GetStatus()));
}

void VideoBuffers::ClearAfterSeek(void)
{
    {
        QMutexLocker locker(&m_globalLock);

        for (uint i = 0; i < Size(); i++)
            At(i)->timecode = 0;

        while (m_used.count() > 1)
        {
            VideoFrame *buffer = m_used.dequeue();
            m_available.enqueue(buffer);
        }

        if (m_used.count() > 0)
        {
            VideoFrame *buffer = m_used.dequeue();
            m_available.enqueue(buffer);
            m_vpos = m_vbufferMap[buffer];
            m_rpos = m_vpos;
        }
        else
        {
            m_vpos = m_rpos = 0;
        }
    }
}

bool VideoBuffers::CreateBuffers(VideoFrameType type, int width, int height)
{
    vector<unsigned char*> bufs;
    vector<YUVInfo>        yuvinfo;
    return CreateBuffers(type, width, height, bufs, yuvinfo);
}

bool VideoBuffers::CreateBuffers(VideoFrameType type, int width, int height,
                                 vector<unsigned char*> bufs,
                                 vector<YUVInfo>        yuvinfo)
{
    if ((FMT_YV12 != type) && (FMT_YUY2 != type))
        return false;

    bool ok = true;
    uint buf_size = buffersize(type, width, height);

    while (bufs.size() < Size())
    {
        unsigned char *data = (unsigned char*)av_malloc(buf_size + 64);
        if (!data)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to allocate memory for frame.");
            return false;
        }

        bufs.push_back(data);
        yuvinfo.emplace_back(width, height, buf_size, nullptr, nullptr);
        m_allocatedArrays.push_back(data);
    }

    for (uint i = 0; i < Size(); i++)
    {
        init(&m_buffers[i],
             type, bufs[i], yuvinfo[i].m_width, yuvinfo[i].m_height,
             max(buf_size, yuvinfo[i].m_size),
             (const int*) yuvinfo[i].m_pitches, (const int*) yuvinfo[i].m_offsets);

        ok &= (bufs[i] != nullptr);
    }

    Clear();

    return ok;
}

static unsigned char *ffmpeg_hack = (unsigned char*)
    "avlib should not use this private data";

bool VideoBuffers::CreateBuffer(int width, int height, uint num, void* data,
                                VideoFrameType fmt)
{
    if (num >= Size())
        return false;

    init(&m_buffers[num], fmt, (unsigned char*)data, width, height, 0);
    m_buffers[num].priv[0] = ffmpeg_hack;
    m_buffers[num].priv[1] = ffmpeg_hack;
    return true;
}

uint VideoBuffers::AddBuffer(int width, int height, void* data,
                             VideoFrameType fmt)
{
    QMutexLocker lock(&m_globalLock);

    uint num = Size();
    m_buffers.resize(num + 1);
    memset(&m_buffers[num], 0, sizeof(VideoFrame));
    m_buffers[num].interlaced_frame = -1;
    m_buffers[num].top_field_first  = 1;
    m_vbufferMap[At(num)] = num;
    if (!data)
    {
        int size = buffersize(fmt, width, height);
        data = av_malloc(size);
        m_allocatedArrays.push_back((unsigned char*)data);
    }
    init(&m_buffers[num], fmt, (unsigned char*)data, width, height, 0);
    m_buffers[num].priv[0] = ffmpeg_hack;
    m_buffers[num].priv[1] = ffmpeg_hack;
    Enqueue(kVideoBuffer_avail, At(num));

    return Size();
}

void VideoBuffers::DeleteBuffers()
{
    next_dbg_str = 0;
    for (uint i = 0; i < Size(); i++)
    {
        m_buffers[i].buf = nullptr;

        av_freep(&m_buffers[i].qscale_table);
    }

    for (size_t i = 0; i < m_allocatedArrays.size(); i++)
        av_free(m_allocatedArrays[i]);
    m_allocatedArrays.clear();
}

static unsigned long long to_bitmap(const frame_queue_t& list, int /*n*/);
QString VideoBuffers::GetStatus(int n) const
{
    if (n <= 0)
        n = Size();

    QString str("");
    if (m_globalLock.tryLock())
    {
        int m = Size();
        unsigned long long a = to_bitmap(m_available, m);
        unsigned long long u = to_bitmap(m_used, m);
        unsigned long long d = to_bitmap(m_displayed, m);
        unsigned long long l = to_bitmap(m_limbo, m);
        unsigned long long p = to_bitmap(m_pause, m);
        unsigned long long f = to_bitmap(m_finished, m);
        unsigned long long x = to_bitmap(m_decode, m);
        for (uint i=0; i<(uint)n; i++)
        {
            unsigned long long mask = 1ULL<<i;
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
        for (uint i=0; i<(uint)n; i++)
            str += " ";
    }
    return str;
}

void VideoBuffers::Clear(uint i)
{
    clear(At(i));
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

static int DebugNum(const VideoFrame *frame)
{
    map<const VideoFrame *, int>::iterator it = dbg_str.find(frame);
    if (it == dbg_str.end())
        return dbg_str[frame] = next_dbg_str++;

    return it->second;
}

const QString& DebugString(const VideoFrame *frame, bool short_str)
{
    if (short_str)
        return dbg_str_arr_short[DebugNum(frame) % DBG_STR_ARR_SIZE];
    return dbg_str_arr[DebugNum(frame) % DBG_STR_ARR_SIZE];
}

const QString& DebugString(uint str_num, bool short_str)
{
    return ((short_str) ? dbg_str_arr_short : dbg_str_arr)[str_num];
}

static unsigned long long to_bitmap(const frame_queue_t& list, int n)
{
    unsigned long long bitmap = 0;
    frame_queue_t::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        int shift = DebugNum(*it) % n;
        bitmap |= 1ULL<<shift;
    }
    return bitmap;
}

static QString bitmap_to_string(unsigned long long bitmap)
{
    QString str("");
    for (int i=0; i<8; i++)
        str += ((bitmap>>i)&1) ? DebugString(i, true) : " ";
    return str;
}

QString DebugString(const frame_queue_t& list)
{
    return bitmap_to_string(to_bitmap(list, DBG_STR_ARR_SIZE));
}

QString DebugString(const vector<const VideoFrame*>& list)
{
    // first create a bitmap..
    unsigned long long bitmap = 0;
    vector<const VideoFrame*>::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        int shift = DebugNum(*it) % DBG_STR_ARR_SIZE;
        bitmap |= 1ULL<<shift;
    }
    // then transform bitmap to string
    return bitmap_to_string(bitmap);
}
