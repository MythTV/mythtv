// Copyright (c) 2005, Daniel Thor Kristjansson
// based on earlier work in MythTV's videout_xvmc.cpp

#include <unistd.h>

#include "mythconfig.h"

#include "mythcontext.h"
#include "videobuffers.h"
extern "C" {
#include "libavcodec/avcodec.h"
}
#include "fourcc.h"
#include "compat.h"
#include "mythverbose.h"

#ifdef USING_XVMC
#include "videoout_xv.h" // for xvmc stuff
#endif

#define DEBUG_FRAME_LOCKS 0

#define TRY_LOCK_SPINS                 100
#define TRY_LOCK_SPINS_BEFORE_WARNING   10
#define TRY_LOCK_SPIN_WAIT             100 /* usec */

int next_dbg_str = 0;

YUVInfo::YUVInfo(uint w, uint h, uint sz, const int *p, const int *o)
    : width(w), height(h), size(sz)
{
    if (p)
    {
        memcpy(pitches, p, 3 * sizeof(int));
    }
    else
    {
        pitches[0] = width;
        pitches[1] = pitches[2] = width >> 1;
    }

    if (o)
    {
        memcpy(offsets, o, 3 * sizeof(int));
    }
    else
    {
        offsets[0] = 0;
        offsets[1] = width * height;
        offsets[2] = offsets[1] + (offsets[1] >> 2);
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
 *  Frame locking is also available, the locks are reqursive
 *  QMutex locks. If more than one frame lock is needed the
 *  LockFrames should generally be called with all the needed
 *  locks in the list, and no locks currently held. This
 *  function will spin until all the locks can be held at
 *  once, avoiding deadlocks from mismatched locking order.
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

VideoBuffers::VideoBuffers()
    : numbuffers(0), needfreeframes(0), needprebufferframes(0),
      needprebufferframes_normal(0), needprebufferframes_small(0),
      keepprebufferframes(0), need_extra_for_pause(false), rpos(0), vpos(0),
      global_lock(QMutex::Recursive), use_frame_locks(true),
      frame_lock(QMutex::Recursive)
{
}

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
 * \param enable_frame_locking if true, the frames will be locked with a mutex,
 *                             this makes XvMC decoding safe, but adds some CPU
 *                             overhead. It is normally left off.
 */
void VideoBuffers::Init(uint numdecode, bool extra_for_pause,
                        uint need_free, uint needprebuffer_normal,
                        uint needprebuffer_small, uint keepprebuffer,
                        bool enable_frame_locking)
{
    QMutexLocker locker(&global_lock);

    Reset();

    uint numcreate = numdecode + ((extra_for_pause) ? 1 : 0);

    // make a big reservation, so that things that depend on
    // pointer to VideoFrames work even after a few push_backs
    buffers.reserve(max(numcreate, (uint)128));

    buffers.resize(numcreate);
    for (uint i = 0; i < numcreate; i++)
    {
        memset(at(i), 0, sizeof(VideoFrame));
        at(i)->codec            = FMT_NONE;
        at(i)->interlaced_frame = -1;
        at(i)->top_field_first  = +1;
        vbufferMap[at(i)]       = i;
    }

    numbuffers                  = numdecode;
    needfreeframes              = need_free;
    needprebufferframes         = needprebuffer_normal;
    needprebufferframes_normal  = needprebuffer_normal;
    needprebufferframes_small   = needprebuffer_small;
    keepprebufferframes         = keepprebuffer;
    need_extra_for_pause        = extra_for_pause;
    use_frame_locks             = enable_frame_locking;

    for (uint i = 0; i < numdecode; i++)
        enqueue(kVideoBuffer_avail, at(i));
}

/**
 * \fn VideoBuffers::Reset()
 *  Resets the class so that Init may be called again.
 */
void VideoBuffers::Reset()
{
    QMutexLocker locker(&global_lock);

    // Delete ffmpeg VideoFrames so we can create
    // a different number of buffers below
    frame_vector_t::iterator it = buffers.begin();
    for (;it != buffers.end(); it++)
    {
        if (it->qscale_table)
        {
            delete [] it->qscale_table;
            it->qscale_table = NULL;
        }
    }

    available.clear();
    used.clear();
    limbo.clear();
    finished.clear();
    decode.clear();
    pause.clear();
    displayed.clear();
    parents.clear();
    children.clear();
    vbufferMap.clear();
}

/**
 * \fn VideoBuffers::SetPrebuffering(bool normal)
 *  Sets prebuffering state to normal, or small.
 */
void VideoBuffers::SetPrebuffering(bool normal)
{
    QMutexLocker locker(&global_lock);
    needprebufferframes = (normal) ?
        needprebufferframes_normal : needprebufferframes_small;
}

VideoFrame *VideoBuffers::GetNextFreeFrameInternal(
    bool with_lock, bool allow_unsafe, BufferType enqueue_to)
{
    QMutexLocker locker(&global_lock);
    VideoFrame *frame = available.dequeue();

    // Try to get a frame not being used by the decoder
    for (uint i = 0; i <= available.size() && decode.contains(frame); i++)
    {
        if (!available.contains(frame))
            available.enqueue(frame);
        frame = available.dequeue();
    }

    while (frame && used.contains(frame))
    {
        VERBOSE(VB_PLAYBACK,
                QString("GetNextFreeFrame() served a busy frame %1. "
                        "Dropping. %2")
                .arg(DebugString(frame, true)).arg(GetStatus()));
        frame = available.dequeue();
    }

    // only way this should be triggered if we're in unsafe mode
    if (!frame && allow_unsafe)
    {
        VERBOSE(VB_PLAYBACK,
                QString("GetNextFreeFrame() is getting a busy frame %1. "
                        "      %2")
                .arg(DebugString(frame, true)).arg(GetStatus()));
        frame = used.dequeue();
    }

    if (frame)
    {
        safeEnqueue(enqueue_to, frame);

        bool success = true;
        if (with_lock)
            success = TryLockFrame(frame, "GetNextFreeFrame");

        if (!success)
        {
            safeEnqueue(kVideoBuffer_avail, frame);
            VERBOSE(VB_IMPORTANT,
                QString("GetNextFreeFrame() unable to lock frame %1. "
                        "Dropping %2. w/lock(%3) unsafe(%4)")
                    .arg(DebugString(frame)).arg(GetStatus())
                    .arg(with_lock).arg(allow_unsafe));
            DiscardFrame(frame);
            frame = NULL;
        }
    }

    return frame;
}

/**
 * \fn VideoBuffers::GetNextFreeFrame(bool,bool,BufferType)
 *  Gets a frame from available buffers list.
 *
 * \param with_lock    locks the frame, so that UnlockFrame() must be
 *                     called before anyone else can use it.
 * \param allow_unsafe allows busy buffers to be used if no available
 *                     buffers exist. Historic, should never be used.
 * \param enqueue_to   put new frame in some state other than limbo.
 */
VideoFrame *VideoBuffers::GetNextFreeFrame(bool with_lock,
                                           bool allow_unsafe,
                                           BufferType enqueue_to)
{
    for (uint tries = 1; true; tries++)
    {
        VideoFrame *frame = VideoBuffers::GetNextFreeFrameInternal(
            with_lock, allow_unsafe, enqueue_to);

        if (frame)
            return frame;

        if (tries >= TRY_LOCK_SPINS)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("GetNextFreeFrame() unable to "
                            "lock frame %1 times. Discarding Frames.")
                    .arg(TRY_LOCK_SPINS));
            DiscardFrames(true);
            continue;
        }

        if (tries && !(tries % TRY_LOCK_SPINS_BEFORE_WARNING))
        {
            VERBOSE(VB_PLAYBACK,
                    QString("GetNextFreeFrame() TryLock has "
                            "spun %1 times, this is a lot.").arg(tries));
        }
        usleep(TRY_LOCK_SPIN_WAIT);
    }

    return NULL;
}

/**
 * \fn VideoBuffers::ReleaseFrame(VideoFrame*)
 *  Frame is ready to be for filtering or OSD application.
 *  Removes frame from limbo and adds it to used queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::ReleaseFrame(VideoFrame *frame)
{
    QMutexLocker locker(&global_lock);

    vpos = vbufferMap[frame];
    limbo.remove(frame);
    decode.enqueue(frame);
    used.enqueue(frame);
}

/**
 * \fn VideoBuffers::DeLimboFrame(VideoFrame*)
 *  If the frame is still in the limbo state it is added to the available queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::DeLimboFrame(VideoFrame *frame)
{
    QMutexLocker locker(&global_lock);
    if (limbo.contains(frame))
        limbo.remove(frame);

    // if decoder didn't release frame and the buffer is getting released by
    // the decoder assume that the frame is lost and return to available
    if (!decode.contains(frame))
        safeEnqueue(kVideoBuffer_avail, frame);
       
    // remove from decode queue since the decoder is finished
    while (decode.contains(frame))
        decode.remove(frame);
}

/**
 * \fn VideoBuffers::StartDisplayingFrame(void)
 *  Sets rpos to index of videoframe at head of used queue.
 */
void VideoBuffers::StartDisplayingFrame(void)
{
    QMutexLocker locker(&global_lock);
    rpos = vbufferMap[used.head()];
}

/**
 * \fn VideoBuffers::DoneDisplayingFrame(VideoFrame *frame)
 *  Removes frame from used queue and adds it to the available list.
 */
void VideoBuffers::DoneDisplayingFrame(VideoFrame *frame)
{
    QMutexLocker locker(&global_lock);

    if(used.contains(frame))
        remove(kVideoBuffer_used, frame);

    enqueue(kVideoBuffer_finished, frame);

    // check if any finished frames are no longer used by decoder and return to available
    frame_queue_t ula(finished);
    frame_queue_t::iterator it = ula.begin();
    for (; it != ula.end(); ++it)
    {
        if (!decode.contains(*it))
        {
            remove(kVideoBuffer_finished, *it);
            enqueue(kVideoBuffer_avail, *it);
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
    QMutexLocker locker(&global_lock);

    bool ok = TryLockFrame(frame, "DiscardFrame A");
    for (uint i=0; i<5 && !ok; i++)
    {
        global_lock.unlock();
        usleep(50);
        global_lock.lock();
        ok = TryLockFrame(frame, "DiscardFrame B");
    }

    if (ok)
    {
        safeEnqueue(kVideoBuffer_avail, frame);
        UnlockFrame(frame, "DiscardFrame");
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("VideoBuffers::DiscardFrame(): "
                                      "Unable to obtain lock on %1, %2")
                .arg(DebugString(frame, true)).arg(GetStatus()));
    }
}

frame_queue_t *VideoBuffers::queue(BufferType type)
{
    QMutexLocker locker(&global_lock);

    frame_queue_t *q = NULL;

    if (type == kVideoBuffer_avail)
        q = &available;
    else if (type == kVideoBuffer_used)
        q = &used;
    else if (type == kVideoBuffer_displayed)
        q = &displayed;
    else if (type == kVideoBuffer_limbo)
        q = &limbo;
    else if (type == kVideoBuffer_pause)
        q = &pause;
    else if (type == kVideoBuffer_decode)
        q = &decode;
    else if (type == kVideoBuffer_finished)
        q = &finished;

    return q;
}

const frame_queue_t *VideoBuffers::queue(BufferType type) const
{
    QMutexLocker locker(&global_lock);

    const frame_queue_t *q = NULL;

    if (type == kVideoBuffer_avail)
        q = &available;
    else if (type == kVideoBuffer_used)
        q = &used;
    else if (type == kVideoBuffer_displayed)
        q = &displayed;
    else if (type == kVideoBuffer_limbo)
        q = &limbo;
    else if (type == kVideoBuffer_pause)
        q = &pause;
    else if (type == kVideoBuffer_decode)
        q = &decode;
    else if (type == kVideoBuffer_finished)
        q = &finished;

    return q;
}

VideoFrame *VideoBuffers::dequeue(BufferType type)
{
    QMutexLocker locker(&global_lock);

    frame_queue_t *q = queue(type);

    if (!q)
        return NULL;

    return q->dequeue();
}

VideoFrame *VideoBuffers::head(BufferType type)
{
    QMutexLocker locker(&global_lock);

    frame_queue_t *q = queue(type);

    if (!q)
        return NULL;

    if (q->size())
        return q->head();

    return NULL;
}

VideoFrame *VideoBuffers::tail(BufferType type)
{
    QMutexLocker locker(&global_lock);

    frame_queue_t *q = queue(type);

    if (!q)
        return NULL;

    if (q->size())
        return q->tail();

    return NULL;
}

void VideoBuffers::enqueue(BufferType type, VideoFrame *frame)
{
    if (!frame)
        return;

    frame_queue_t *q = queue(type);
    if (!q)
        return;

    global_lock.lock();
    q->remove(frame);
    q->enqueue(frame);
    global_lock.unlock();

    return;
}

void VideoBuffers::remove(BufferType type, VideoFrame *frame)
{
    if (!frame)
        return;

    QMutexLocker locker(&global_lock);

    if ((type & kVideoBuffer_avail) == kVideoBuffer_avail)
        available.remove(frame);
    if ((type & kVideoBuffer_used) == kVideoBuffer_used)
        used.remove(frame);
    if ((type & kVideoBuffer_displayed) == kVideoBuffer_displayed)
        displayed.remove(frame);
    if ((type & kVideoBuffer_limbo) == kVideoBuffer_limbo)
        limbo.remove(frame);
    if ((type & kVideoBuffer_pause) == kVideoBuffer_pause)
        pause.remove(frame);
    if ((type & kVideoBuffer_decode) == kVideoBuffer_decode)
        decode.remove(frame);
    if ((type & kVideoBuffer_finished) == kVideoBuffer_finished)
        finished.remove(frame);
}

void VideoBuffers::requeue(BufferType dst, BufferType src, int num)
{
    QMutexLocker locker(&global_lock);

    num = (num <= 0) ? size(src) : num;
    for (uint i=0; i<(uint)num; i++)
    {
        VideoFrame *frame = dequeue(src);
        if (frame)
            enqueue(dst, frame);
    }
}

void VideoBuffers::safeEnqueue(BufferType dst, VideoFrame* frame)
{
    if (!frame)
        return;

    QMutexLocker locker(&global_lock);

    remove(kVideoBuffer_all, frame);
    enqueue(dst, frame);
}

frame_queue_t::iterator VideoBuffers::begin_lock(BufferType type)
{
    global_lock.lock();
    frame_queue_t *q = queue(type);
    if (q)
        return q->begin();
    else
        return available.begin();
}

frame_queue_t::iterator VideoBuffers::end(BufferType type)
{
    QMutexLocker locker(&global_lock);

    frame_queue_t::iterator it;
    frame_queue_t *q = queue(type);
    if (q)
        it = q->end();
    else
        it = available.end();

    return it;
}

uint VideoBuffers::size(BufferType type) const
{
    QMutexLocker locker(&global_lock);

    const frame_queue_t *q = queue(type);
    if (q)
        return q->size();

    return 0;
}

bool VideoBuffers::contains(BufferType type, VideoFrame *frame) const
{
    QMutexLocker locker(&global_lock);

    const frame_queue_t *q = queue(type);
    if (q)
        return q->contains(frame);

    return false;
}

VideoFrame *VideoBuffers::GetScratchFrame(void)
{
    if (!need_extra_for_pause)
    {
        VERBOSE(VB_IMPORTANT,
                "GetScratchFrame() called, but not allocated");
    }

    QMutexLocker locker(&global_lock);
    return at(allocSize()-1);
}

const VideoFrame *VideoBuffers::GetScratchFrame(void) const
{
    if (!need_extra_for_pause)
    {
        VERBOSE(VB_IMPORTANT,
                "GetScratchFrame() called, but not allocated");
    }

    QMutexLocker locker(&global_lock);
    return at(allocSize()-1);
}

/**
 * \fn VideoBuffers::DiscardFrames(bool)
 *  Mark all used frames as ready to be reused, this is for seek.
 */
void VideoBuffers::DiscardFrames(bool next_frame_keyframe)
{
    QMutexLocker locker(&global_lock);
    VERBOSE(VB_PLAYBACK, QString("VideoBuffers::DiscardFrames(%1): %2")
            .arg(next_frame_keyframe).arg(GetStatus()));

    if (!next_frame_keyframe)
    {
        for (bool change = true; change;)
        {
            change = false;
            frame_queue_t ula(used);
            frame_queue_t::iterator it = ula.begin();
            for (; it != ula.end(); ++it)
            {
                if (!HasChildren(*it))
                {
                    RemoveInheritence(*it);
                    DiscardFrame(*it);
                    change = true;
                }
            }
        }
        VERBOSE(VB_PLAYBACK,
                QString("VideoBuffers::DiscardFrames(%1): %2 -- done")
                .arg(next_frame_keyframe).arg(GetStatus()));
        return;
    }

    // Remove inheritence of all frames not in displayed or pause
    frame_queue_t ula(used);
    ula.insert(ula.end(), limbo.begin(), limbo.end());
    ula.insert(ula.end(), available.begin(), available.end());
    ula.insert(ula.end(), finished.begin(), finished.end());
    frame_queue_t::iterator it;
    for (it = ula.begin(); it != ula.end(); ++it)
        RemoveInheritence(*it);

    // Discard frames
    frame_queue_t discards(used);
    discards.insert(discards.end(), limbo.begin(), limbo.end());
    discards.insert(discards.end(), finished.begin(), finished.end());
    for (it = discards.begin(); it != discards.end(); ++it)
        DiscardFrame(*it);

    // Verify that things are kosher
    if (available.count() + pause.count() + displayed.count() != size())
    {
        for (uint i=0; i < size(); i++)
        {
            if (!available.contains(at(i)) &&
                !pause.contains(at(i)) &&
                !displayed.contains(at(i)))
            {
                VERBOSE(VB_IMPORTANT,
                        QString("VideoBuffers::DiscardFrames(): ERROR, %1 (%2) not "
                                "in available, pause, or displayed %3")
                        .arg(DebugString(at(i), true)).arg((long long)at(i))
                        .arg(GetStatus()));
                DiscardFrame(at(i));
            }
        }
    }

    // Make sure frames used by decoder are last...
    // This is for libmpeg2 which still uses the frames after a reset.
    for (it = decode.begin(); it != decode.end(); ++it)
        remove(kVideoBuffer_all, *it);
    for (it = decode.begin(); it != decode.end(); ++it)
        available.enqueue(*it);
    decode.clear();

    VERBOSE(VB_PLAYBACK, QString("VideoBuffers::DiscardFrames(): %1 -- done()")
            .arg(GetStatus()));

    VERBOSE(VB_PLAYBACK,
            QString("VideoBuffers::DiscardFrames(%1): %2 -- done")
            .arg(next_frame_keyframe).arg(GetStatus()));
}

void VideoBuffers::ClearAfterSeek(void)
{
    {
        QMutexLocker locker(&global_lock);

        for (uint i = 0; i < size(); i++)
            at(i)->timecode = 0;

        while (used.count() > 1)
        {
            VideoFrame *buffer = used.dequeue();
            available.enqueue(buffer);
        }

        if (used.count() > 0)
        {
            VideoFrame *buffer = used.dequeue();
            available.enqueue(buffer);
            vpos = vbufferMap[buffer];
            rpos = vpos;
        }
        else
        {
            vpos = rpos = 0;
        }
    }
}

void VideoBuffers::LockFrame(const VideoFrame *frame, const char* owner)
{
    if (!use_frame_locks)
        return;

    QMutex *mutex = NULL;
    (void)owner;

    if (!frame)
        return;

    frame_lock.lock();
#if DEBUG_FRAME_LOCKS
    if (owner!="")
        VERBOSE(VB_PLAYBACK, QString("locking frame:   %1 %2 %3")
                .arg(DebugString(frame)).arg(GetStatus()).arg(owner));
#endif

    frame_lock_map_t::iterator it = frame_locks.find(frame);
    if (it == frame_locks.end())
        mutex = frame_locks[frame] = new QMutex(QMutex::Recursive);
    else
        mutex = it->second;

    frame_lock.unlock();

    mutex->lock();
}

void VideoBuffers::LockFrames(vector<const VideoFrame*>& vec,
                              const char* owner)
{
    if (!use_frame_locks)
        return;

    (void)owner;
    bool ok;
    vector<bool> oks;
    oks.resize(vec.size());

#if DEBUG_FRAME_LOCKS
    VERBOSE(VB_PLAYBACK, QString("lock frames:     %1 %2 %3")
            .arg(DebugString(vec)).arg(GetStatus()).arg(owner));
#endif
    do
    {
        ok = true;
        for (uint i=0; i<vec.size(); i++)
            ok &= oks[i] = TryLockFrame(vec[i], "");
        if (!ok)
        {
            for (uint i=0; i<vec.size(); i++)
                if (oks[i])
                    UnlockFrame(vec[i], "");
            usleep(50);
#if DEBUG_FRAME_LOCKS
            VERBOSE(VB_PLAYBACK, QString("no lock, frames: %1 %2 %3")
                    .arg(DebugString(vec)).arg(GetStatus()).arg(owner));
#endif
        }
    }
    while (!ok);
}

bool VideoBuffers::TryLockFrame(const VideoFrame *frame, const char* owner)
{
    if (!use_frame_locks)
        return true;

    QMutex *mutex = NULL;
    (void)owner;

    if (!frame)
        return true;

    frame_lock.lock();
#if DEBUG_FRAME_LOCKS
    if (owner!="")
        VERBOSE(VB_PLAYBACK, QString("try lock frame:  %1 %2 %3")
                .arg(DebugString(frame)).arg(GetStatus()).arg(owner));
#endif

    frame_lock_map_t::iterator it = frame_locks.find(frame);
    if (it == frame_locks.end())
        mutex = frame_locks[frame] = new QMutex(QMutex::Recursive);
    else
        mutex = it->second;

    bool ok = mutex->tryLock();

#if DEBUG_FRAME_LOCKS
    if (owner!="")
    {
        QString str = (ok) ? "got lock:        " : "try lock failed: ";
        VERBOSE(VB_PLAYBACK, str.append(DebugString(frame)));
    }
#endif
    frame_lock.unlock();

    return ok;
}

void VideoBuffers::UnlockFrame(const VideoFrame *frame, const char* owner)
{
    if (!use_frame_locks)
        return;

    (void)owner;

    if (!frame)
        return;

    frame_lock.lock();
#if DEBUG_FRAME_LOCKS
    if (owner!="")
        VERBOSE(VB_PLAYBACK, QString("unlocking frame: %1 %2 %3")
                .arg(DebugString(frame)).arg(GetStatus()).arg(owner));
#endif

    frame_lock_map_t::iterator it = frame_locks.find(frame);
    it->second->unlock();

    frame_lock.unlock();
}

void VideoBuffers::UnlockFrames(vector<const VideoFrame*>& vec,
                                const char* owner)
{
    if (!use_frame_locks)
        return;

    (void)owner;
#if DEBUG_FRAME_LOCKS
    VERBOSE(VB_PLAYBACK, QString("unlocking frames:%1 %2 %3")
            .arg(DebugString(vec)).arg(GetStatus()).arg(owner));
#endif
    for (uint i=0; i<vec.size(); i++)
        UnlockFrame(vec[i], "");
}

void VideoBuffers::AddInheritence(const VideoFrame *frame)
{
    (void)frame;
#ifdef USING_XVMC
    QMutexLocker locker(&global_lock);

    frame_map_t::iterator it = parents.find(frame);
    if (it == parents.end())
    {
        // find "parents"...
        frame_queue_t new_parents;
        VideoFrame *past = PastFrame(frame);
        if (past == frame)
            VERBOSE(VB_IMPORTANT, QString("AddInheritence(%1) Error, past=frame")
                    .arg(DebugString(frame)));
        else if (past)
        {
            bool in_correct_buffer =
                used.contains(past) || displayed.contains(past) ||
                limbo.contains(past) || pause.contains(past);
            if (in_correct_buffer)
                new_parents.push_back(past);
            else
                VERBOSE(VB_IMPORTANT, QString("AddInheritence past %1 NOT "
                                              "in used or in done. %2")
                        .arg(DebugString(past)).arg(GetStatus()));
        }

        VideoFrame *future = FutureFrame(frame);
        if (future == frame)
            VERBOSE(VB_IMPORTANT, QString("AddInheritence(%1) Error, future=frame")
                    .arg(DebugString(frame)));
        else if (future)
        {
            if (used.contains(future) || limbo.contains(future))
                new_parents.push_back(future);
            else
                VERBOSE(VB_IMPORTANT, QString("AddInheritence future %1 NOT "
                                              "in used or in limbo. %2")
                        .arg(DebugString(future)).arg(GetStatus()));
        }

        parents[frame] = new_parents;
        // add self as "child"
        frame_queue_t::iterator it = new_parents.begin();
        for (; it != new_parents.end(); ++it)
            children[*it].push_back((VideoFrame*)frame);
    }
#endif // USING_XVMC
}

void VideoBuffers::RemoveInheritence(const VideoFrame *frame)
{
    QMutexLocker locker(&global_lock);

    frame_map_t::iterator it = parents.find(frame);
    if (it == parents.end())
        return;

    frame_queue_t new_parents;
    frame_queue_t &p = it->second;
    for (frame_queue_t::iterator pit = p.begin(); pit != p.end(); ++pit)
    {
        frame_map_t::iterator cit = children.find(*pit);
        if (cit != children.end())
        {
            frame_queue_t::iterator fit = cit->second.find((VideoFrame*)frame);
            if (fit != cit->second.end())
                cit->second.erase(fit);
            else
                new_parents.push_back(*pit);
        }
    }

    if (new_parents.empty())
        parents.erase(it);
    else
    {
        parents[frame] = new_parents;
        VERBOSE(VB_IMPORTANT, QString("RemoveInheritenc:%1 parents.size() = ")
                .arg(DebugString(frame)).arg(parents.size()));
        frame_queue_t::iterator pit = new_parents.begin();
        for (int i=0; pit != new_parents.end() && i<8; (++pit), (++i))
            VERBOSE(VB_IMPORTANT, QString("Parent #%1: %2")
                    .arg(i).arg(DebugString(*pit)));
    }
}

frame_queue_t VideoBuffers::Children(const VideoFrame *frame)
{
    QMutexLocker locker(&global_lock);

    frame_queue_t c;
    frame_map_t::iterator it = children.find(frame);
    if (it != children.end())
        c = it->second;
    return c;
}

bool VideoBuffers::HasChildren(const VideoFrame *frame)
{
    QMutexLocker locker(&global_lock);

    frame_map_t::iterator it = children.find(frame);
    if (it != children.end())
        return !(it->second.empty());
    return false;
}

#ifdef USING_XVMC

inline struct xvmc_pix_fmt *GetRender(VideoFrame *frame)
{
    if (frame)
        return (struct xvmc_pix_fmt*) frame->buf;
    else
        return NULL;
}

inline const struct xvmc_pix_fmt *GetRender(const VideoFrame *frame)
{
    if (frame)
        return (const struct xvmc_pix_fmt*) frame->buf;
    else
        return NULL;
}

VideoFrame* VideoBuffers::PastFrame(const VideoFrame *frame)
{
    LockFrame(frame, "PastFrame");
    const struct xvmc_pix_fmt* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = xvmc_surf_to_frame[r->p_past_surface];
    UnlockFrame(frame, "PastFrame");
    return f;
}

VideoFrame* VideoBuffers::FutureFrame(const VideoFrame *frame)
{
    LockFrame(frame, "FutureFrame");
    const struct xvmc_pix_fmt* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = xvmc_surf_to_frame[r->p_future_surface];
    UnlockFrame(frame, "FutureFrame");
    return f;
}

VideoFrame* VideoBuffers::GetOSDFrame(const VideoFrame *frame)
{
    LockFrame(frame, "GetOSDFrame");
    const struct xvmc_pix_fmt* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = (VideoFrame*) (r->p_osd_target_surface_render);

    QString dbg_str("GetOSDFrame ");
#if DEBUG_FRAME_LOCKS
    if (f)
        dbg_str.append(DebugString(f, true));
#endif
    UnlockFrame(frame, dbg_str.toLocal8Bit().constData());

    return f;
}

void VideoBuffers::SetOSDFrame(VideoFrame *frame, VideoFrame *osd)
{
    if (frame == osd)
    {
        VERBOSE(VB_IMPORTANT, QString("SetOSDFrame() -- frame==osd %1")
                .arg(GetStatus()));
        return;
    }

    LockFrame(frame, "SetOSDFrame");
    struct xvmc_pix_fmt* r = GetRender(frame);
    if (r)
    {
        QMutexLocker locker(&global_lock);

        VideoFrame* old_osd = (VideoFrame*) r->p_osd_target_surface_render;
        if (old_osd)
            xvmc_osd_parent[old_osd] = NULL;

        r->p_osd_target_surface_render = osd;

        if (osd)
            xvmc_osd_parent[osd] = frame;
    }

    UnlockFrame(frame, "SetOSDFrame");
}

VideoFrame* VideoBuffers::GetOSDParent(const VideoFrame *osd)
{
    QMutexLocker locker(&global_lock);
    return xvmc_osd_parent[osd];
}

#endif

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
    int  type_bpp = bitsperpixel(type);
    uint bpp = type_bpp / 4; /* bits per pixel div common factor */
    uint bpb =  8 / 4; /* bits per byte div common factor */

    // If the buffer sizes are not a multple of 16, adjust.
    // old versions of MythTV allowed people to set invalid
    // dimensions for MPEG-4 capture, no need to segfault..
    uint adj_w = (width  + 15) & ~0xF;
    uint adj_h = (height + 15) & ~0xF;
    uint buf_size = (adj_w * adj_h * bpp + 4/* to round up */) / bpb;

    while (bufs.size() < allocSize())
    {
        unsigned char *data = (unsigned char*)av_malloc(buf_size + 64);
        if (!data)
        {
            VERBOSE(VB_IMPORTANT, "Failed to allocate memory for frame.");
            return false;
        }

        bufs.push_back(data);
        yuvinfo.push_back(YUVInfo(width, height, buf_size, NULL, NULL));

        if (bufs.back())
        {
            VERBOSE(VB_PLAYBACK+VB_EXTRA, "Created data @"
                    <<((void*)data)<<"->"<<((void*)(data+buf_size)));
            allocated_arrays.push_back(bufs.back());
        }
        else
            ok = false;
    }

    for (uint i = 0; i < allocSize(); i++)
    {
        init(&buffers[i],
             type, bufs[i], yuvinfo[i].width, yuvinfo[i].height,
             max(buf_size, yuvinfo[i].size),
             (const int*) yuvinfo[i].pitches, (const int*) yuvinfo[i].offsets);

        ok &= (bufs[i] != NULL);
    }

    Clear();

    return ok;
}

static unsigned char *ffmpeg_hack = (unsigned char*)
    "avlib should not use this private data";

bool VideoBuffers::CreateBuffer(int width, int height, uint num, void* data,
                                VideoFrameType fmt)
{
    if (num >= allocSize())
        return false;

    init(&buffers[num], fmt, (unsigned char*)data, width, height, 0);
    buffers[num].priv[0] = ffmpeg_hack;
    buffers[num].priv[1] = ffmpeg_hack;
    return true;
}

#ifdef USING_XVMC
bool VideoBuffers::CreateBuffers(int width, int height,
                                 MythXDisplay *disp,
                                 void *p_xvmc_ctx,
                                 void *p_xvmc_surf_info,
                                 vector<void*> surfs)
{
    XvMCContext &xvmc_ctx           = *((XvMCContext*) p_xvmc_ctx);
    XvMCSurfaceInfo &xvmc_surf_info = *((XvMCSurfaceInfo*) p_xvmc_surf_info);

    static unsigned char *ffmpeg_vld_hack = (unsigned char*)
        "avlib should not use this private data in XvMC-VLD mode.";

    if (surfs.size()>allocSize())
    {
        VERBOSE(VB_PLAYBACK,
                QString("Allocated %1 XvMC surfaces, minimum was %2 surfaces")
                .arg(surfs.size()).arg(allocSize()));

        Reset();
        uint increment = surfs.size() - allocSize();

        Init(surfs.size(),
             false /* create an extra frame for pause? */,
             needfreeframes,
             needprebufferframes_normal + increment - 1,
             needprebufferframes_small,
             keepprebufferframes + 1);
    }

    for (uint i = 0; i < allocSize(); i++)
    {
        xvmc_vo_surf_t *surf    = (xvmc_vo_surf_t*) surfs[i];
        struct xvmc_pix_fmt *render = new struct xvmc_pix_fmt;
        allocated_structs.push_back((unsigned char*)render);
        memset(render, 0, sizeof(struct xvmc_pix_fmt));

        // constants
        render->xvmc_id         = AV_XVMC_ID;
        render->state           = 0;

        // from videoout_xv
        render->disp            = disp->GetDisplay();
        render->ctx             = &xvmc_ctx;

        // from xvmv block and surface arrays
        render->p_surface       = &surf->surface;

        render->allocated_data_blocks = surf->blocks.num_blocks;
        render->allocated_mv_blocks   = surf->macro_blocks.num_blocks;

        init(&buffers[i],
             FMT_XVMC_IDCT_MPEG2, (unsigned char*) render,
             width, height, sizeof(XvMCSurface));

        buffers[i].priv[0]      = ffmpeg_vld_hack;
        buffers[i].priv[1]      = ffmpeg_vld_hack;

        if (surf->blocks.blocks)
        {
            render->data_blocks = surf->blocks.blocks;
            buffers[i].priv[0]  = (unsigned char*) &(surf->blocks);

            render->mv_blocks   = surf->macro_blocks.macro_blocks;
            buffers[i].priv[1]  = (unsigned char*) &(surf->macro_blocks);
        }

        // from surface info
        render->idct = (xvmc_surf_info.mc_type & XVMC_IDCT) == XVMC_IDCT;
        render->unsigned_intra  = (xvmc_surf_info.flags &
                                  XVMC_INTRA_UNSIGNED) == XVMC_INTRA_UNSIGNED;

        xvmc_surf_to_frame[render->p_surface] = &buffers[i];
    }
    return true;
}
#endif

void VideoBuffers::DeleteBuffers()
{
    next_dbg_str = 0;
    for (uint i = 0; i < allocSize(); i++)
    {
        buffers[i].buf = NULL;

        if (buffers[i].qscale_table)
        {
            delete [] buffers[i].qscale_table;
            buffers[i].qscale_table = NULL;
        }
    }

    for (uint i = 0; i < allocated_structs.size(); i++)
        delete allocated_structs[i];
    allocated_structs.clear();

    for (uint i = 0; i < allocated_arrays.size(); i++)
        av_free(allocated_arrays[i]);
    allocated_arrays.clear();
#ifdef USING_XVMC
    xvmc_surf_to_frame.clear();
#endif
}

static unsigned long long to_bitmap(const frame_queue_t& list);
QString VideoBuffers::GetStatus(int n) const
{
    if (0 > n)
        n = numbuffers;

    QString str("");
    if (global_lock.tryLock())
    {
        unsigned long long a = to_bitmap(available);
        unsigned long long u = to_bitmap(used);
        unsigned long long d = to_bitmap(displayed);
        unsigned long long l = to_bitmap(limbo);
        unsigned long long p = to_bitmap(pause);
        unsigned long long f = to_bitmap(finished);
        unsigned long long x = to_bitmap(decode);
        for (uint i=0; i<(uint)n; i++)
        {
            unsigned long long mask = 1<<i;
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
        global_lock.unlock();
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
    clear(at(i));
}

void VideoBuffers::Clear(void)
{
    for (uint i = 0; i < allocSize(); i++)
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
    {
        dbg_str[frame] = next_dbg_str;
        next_dbg_str = (next_dbg_str+1) % DBG_STR_ARR_SIZE;
    }
    return dbg_str[frame];
}

const QString& DebugString(const VideoFrame *frame, bool short_str)
{
    if (short_str)
        return dbg_str_arr_short[DebugNum(frame)];
    else
        return dbg_str_arr[DebugNum(frame)];
}

const QString& DebugString(uint i, bool short_str)
{
    return ((short_str) ? dbg_str_arr_short : dbg_str_arr)[i];
}

static unsigned long long to_bitmap(const frame_queue_t& list)
{
    unsigned long long bitmap = 0;
    frame_queue_t::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        int shift = DebugNum(*it);
        bitmap |= 1<<shift;
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

const QString DebugString(const frame_queue_t& list)
{
    return bitmap_to_string(to_bitmap(list));
}

const QString DebugString(const vector<const VideoFrame*>& list)
{
    // first create a bitmap..
    unsigned long long bitmap = 0;
    vector<const VideoFrame*>::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        int shift = DebugNum(*it);
        bitmap |= 1<<shift;
    }
    // then transform bitmap to string
    return bitmap_to_string(bitmap);
}
