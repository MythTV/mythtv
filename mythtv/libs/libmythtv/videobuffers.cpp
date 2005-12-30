// Copyright (c) 2005, Daniel Thor Kristjansson
// based on earlier work in MythTV's videout_xvmc.cpp

#include <unistd.h>
#include "mythcontext.h"
#include "videobuffers.h"

#ifdef USING_XVMC
#include "videoout_xv.h" // for xvmc stuff
#endif

#define DEBUG_FRAME_LOCKS 0

#define TRY_LOCK_SPINS                 100
#define TRY_LOCK_SPINS_BEFORE_WARNING   10
#define TRY_LOCK_SPIN_WAIT             100 /* usec */

int next_dbg_str = 0;

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
 *  but it can not yet be added to available because it is
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
 * \see VideoOutput
 */

VideoBuffers::VideoBuffers()
    : numbuffers(0), needfreeframes(0), needprebufferframes(0),
      needprebufferframes_normal(0), needprebufferframes_small(0),
      keepprebufferframes(0), need_extra_for_pause(false), rpos(0), vpos(0),
      global_lock(true), inheritence_lock(false), use_frame_locks(true),
      frame_lock(true)
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
 */
void VideoBuffers::Init(uint numdecode, bool extra_for_pause, 
                        uint need_free, uint needprebuffer_normal,
                        uint needprebuffer_small, uint keepprebuffer,
                        bool enable_frame_locking)
{
    global_lock.lock();

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
    
    global_lock.unlock();
}

/**
 * \fn VideoBuffers::Reset()
 *  Resets the class so that Init may be called again.
 */
void VideoBuffers::Reset()
{
    global_lock.lock();

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
    pause.clear();
    displayed.clear();
    parents.clear();
    children.clear();

    vbufferMap.clear();

    global_lock.unlock();
}

/**
 * \fn VideoBuffers::SetPrebuffering(bool normal)
 *  Sets prebuffering state to normal, or small.
 */
void VideoBuffers::SetPrebuffering(bool normal) 
{
    global_lock.lock();

    needprebufferframes = (normal) ?
        needprebufferframes_normal : needprebufferframes_small;

    global_lock.unlock();
};

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
    VideoFrame *frame = NULL;
    uint tries = 0;
    bool success = false;
    while (!success)
    {
        success = false;
        global_lock.lock();
        frame = available.dequeue();

        // Try to get a frame not being used by the decoder
        for (uint i = 0; i <= available.size() && decode.contains(frame); i++)
        {
            if (!available.contains(frame))
                available.enqueue(frame);
            frame = available.dequeue();
        }

        while (frame && used.contains(frame))
        {
            VERBOSE(VB_IMPORTANT,
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
            if (EnoughFreeFrames())
                available_wait.wakeAll();
        }
        if (frame)
        {
            safeEnqueue(enqueue_to, frame);

            success = true;
            if (with_lock)
                success = TryLockFrame(frame, "GetNextFreeFrame");
            if (!success)
                safeEnqueue(kVideoBuffer_avail, frame);
        }

        global_lock.unlock();

        if (!success)
        {
            if (frame)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("GetNextFreeFrame() unable to lock frame %1. "
                                "Dropping %2. w/lock(%3) unsafe(%4)")
                        .arg(DebugString(frame)).arg(GetStatus())
                        .arg(with_lock).arg(allow_unsafe));
                DiscardFrame(frame);
            }
            ++tries;
            if (tries<TRY_LOCK_SPINS)
            {
                if (tries && !(tries % TRY_LOCK_SPINS_BEFORE_WARNING))
                    VERBOSE(VB_PLAYBACK, "GetNextFreeFrame() TryLock has "
                            "spun "<<tries<<" times, this is a lot.");
                usleep(TRY_LOCK_SPIN_WAIT);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, 
                        QString("GetNextFreeFrame() unable to "
                        "lock frame %1 times. Discarding Frames.")
                        .arg(TRY_LOCK_SPINS));
                DiscardFrames();
            }
        }
    }

    return frame;
}

/**
 * \fn VideoBuffers::ReleaseFrame(VideoFrame*)
 *  Frame is ready to be for filtering or OSD application.
 *  Removes frame from limbo and adds it to used queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::ReleaseFrame(VideoFrame *frame)
{
    global_lock.lock();
    vpos = vbufferMap[frame];
    limbo.remove(frame);
    decode.enqueue(frame);
    used.enqueue(frame);
    global_lock.unlock();
}

/**
 * \fn VideoBuffers::DeLimboFrame(VideoFrame*)
 *  If the frame is still in the limbo state it is added to the available queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::DeLimboFrame(VideoFrame *frame)
{
    global_lock.lock();
    if (limbo.contains(frame))
    {
        limbo.remove(frame);
        available.enqueue(frame);
    }
    decode.remove(frame);
    global_lock.unlock();
}

/**
 * \fn VideoBuffers::StartDisplayingFrame(void)
 *  Sets rpos to index of videoframe at head of used queue.
 */
void VideoBuffers::StartDisplayingFrame(void)
{
    global_lock.lock();
    rpos = vbufferMap[used.head()];
    global_lock.unlock();
}

/**
 * \fn VideoBuffers::DoneDisplayingFrame(void)
 *  Removes frame from used queue and adds it to the available list.
 */
void VideoBuffers::DoneDisplayingFrame(void)
{
    global_lock.lock();

    VideoFrame *buf = used.dequeue();
    if (buf)
    {
        available.enqueue(buf);
        if (EnoughFreeFrames())
            available_wait.wakeAll();
    }

    global_lock.unlock();
}

/**
 * \fn VideoBuffers::DiscardFrame(VideoFrame*)
 *  Frame is ready to be reused by decoder.
 *  Add frame to available list, remove from any other list.
 */
void VideoBuffers::DiscardFrame(VideoFrame *frame)
{
    global_lock.lock();
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
    global_lock.unlock();
}

frame_queue_t *VideoBuffers::queue(BufferType type)
{
    frame_queue_t *q = NULL;

    global_lock.lock();
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
    global_lock.unlock();

    return q;
}

const frame_queue_t *VideoBuffers::queue(BufferType type) const
{
    const frame_queue_t *q = NULL;

    global_lock.lock();
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
    global_lock.unlock();

    return q;
}

VideoFrame *VideoBuffers::dequeue(BufferType type)
{
    frame_queue_t *q = queue(type);
    if (q)
    {
        global_lock.lock();
        VideoFrame *frame = q->dequeue();
        global_lock.unlock();
        return frame;
    }
    return NULL;
}

VideoFrame *VideoBuffers::head(BufferType type)
{
    frame_queue_t *q = queue(type);
    if (q)
    {
        QMutexLocker locker(&global_lock);
        if (q->size())
            return q->head();
    }
    return NULL;
}

VideoFrame *VideoBuffers::tail(BufferType type)
{
    frame_queue_t *q = queue(type);
    if (q)
    {
        QMutexLocker locker(&global_lock);
        if (q->size())
            return q->tail();
    }
    return NULL;
}

void VideoBuffers::enqueue(BufferType type, VideoFrame *frame)
{
    if (frame)
    {
        frame_queue_t *q = queue(type);
        if (q)
        {
            global_lock.lock();
            q->remove(frame);
            q->enqueue(frame);
            global_lock.unlock();
            if (q == &available && EnoughFreeFrames())
                available_wait.wakeAll();
        }
    }
}

void VideoBuffers::remove(BufferType type, VideoFrame *frame)
{
    if (frame)
    {
        global_lock.lock();
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
        global_lock.unlock();
    }
}

void VideoBuffers::requeue(BufferType dst, BufferType src, int num)
{
    global_lock.lock();
    num = (num <= 0) ? size(src) : num;
    for (uint i=0; i<(uint)num; i++)
    {
        VideoFrame *frame = dequeue(src);
        if (frame)
            enqueue(dst, frame);
    }
    global_lock.unlock();
}

void VideoBuffers::safeEnqueue(BufferType dst, VideoFrame* frame)
{
    if (frame)
    {
        global_lock.lock();
        remove(kVideoBuffer_all, frame);
        enqueue(dst, frame);
        global_lock.unlock();
    }
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
    global_lock.lock();
    frame_queue_t::iterator it;
    frame_queue_t *q = queue(type);
    if (q)
        it = q->end();
    else
        it = available.end();
    global_lock.unlock();
    return it;
}

uint VideoBuffers::size(BufferType type) const
{
    uint s = 0;
    const frame_queue_t *q = queue(type);
    if (q)
    {
        global_lock.lock();
        s = q->size();
        global_lock.unlock();
    }
    return s;
}

bool VideoBuffers::contains(BufferType type, VideoFrame *frame) const
{
    bool c = false;
    const frame_queue_t *q = queue(type);
    if (q)
    {
        global_lock.lock();
        c = q->contains(frame);
        global_lock.unlock();
    }
    return c;
}

VideoFrame *VideoBuffers::GetScratchFrame(void)
{
    if (!need_extra_for_pause)
    {
        VERBOSE(VB_IMPORTANT,
                "GetScratchFrame() called, but not allocated");
    }
    return at(allocSize()-1);
}

const VideoFrame *VideoBuffers::GetScratchFrame(void) const
{
    if (!need_extra_for_pause)
    {
        VERBOSE(VB_IMPORTANT,
                "GetScratchFrame() called, but not allocated");
    }
    return at(allocSize()-1);
}

/**
 * \fn VideoBuffers::DiscardFrames(void)
 *  Mark all used frames as ready to be reused, this is for seek.
 */
void VideoBuffers::DiscardFrames(void)
{
    global_lock.lock();
    VERBOSE(VB_PLAYBACK, QString("VideoBuffers::DiscardFrames(): %1")
            .arg(GetStatus()));

    // Remove inheritence of all frames not in displayed or pause
    frame_queue_t ula(used);
    ula.insert(ula.end(), limbo.begin(), limbo.end());
    ula.insert(ula.end(), available.begin(), available.end());
    frame_queue_t::iterator it;
    for (it = ula.begin(); it != ula.end(); ++it)
        RemoveInheritence(*it);

    // Discard frames
    frame_queue_t discards(used);
    discards.insert(discards.end(), limbo.begin(), limbo.end());
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
                        QString("VideoBuffers::DiscardFrames(): ERROR, %1 not "
                                "in available, pause, or displayed %2")
                        .arg(DebugString(at(i), true))
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

    global_lock.unlock();
}

void VideoBuffers::ClearAfterSeek(void)
{
    global_lock.lock();

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

    if (EnoughFreeFrames())
        available_wait.wakeAll();

    global_lock.unlock();
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
        mutex = frame_locks[frame] = new QMutex(true);
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
        mutex = frame_locks[frame] = new QMutex(true);
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
    inheritence_lock.lock();
    
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

    inheritence_lock.unlock();
#endif // USING_XVMC
}

void VideoBuffers::RemoveInheritence(const VideoFrame *frame)
{
    inheritence_lock.lock();

    frame_map_t::iterator it = parents.find(frame);
    if (it == parents.end())
    {
        inheritence_lock.unlock();
        return;
    }

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

    inheritence_lock.unlock();
}

frame_queue_t VideoBuffers::Children(const VideoFrame *frame)
{
    frame_queue_t c;
    frame_map_t::iterator it = children.find(frame);
    if (it != children.end())
        c = it->second;
    return c;
}

bool VideoBuffers::HasChildren(const VideoFrame *frame)
{
    frame_map_t::iterator it = children.find(frame);
    if (it != children.end())
        return !(it->second.empty());
    return false;
}

#ifdef USING_XVMC

inline xvmc_render_state_t *GetRender(VideoFrame *frame)
{
    if (frame)
        return (xvmc_render_state_t*) frame->buf;
    else
        return NULL;
}

inline const xvmc_render_state_t *GetRender(const VideoFrame *frame)
{
    if (frame)
        return (const xvmc_render_state_t*) frame->buf;
    else
        return NULL;
}

VideoFrame* VideoBuffers::PastFrame(const VideoFrame *frame)
{
    LockFrame(frame, "PastFrame");
    const xvmc_render_state_t* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = xvmc_surf_to_frame[r->p_past_surface];
    UnlockFrame(frame, "PastFrame");
    return f;
}

VideoFrame* VideoBuffers::FutureFrame(const VideoFrame *frame)
{
    LockFrame(frame, "FutureFrame");
    const xvmc_render_state_t* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = xvmc_surf_to_frame[r->p_future_surface];
    UnlockFrame(frame, "FutureFrame");
    return f;
}

VideoFrame* VideoBuffers::GetOSDFrame(const VideoFrame *frame)
{
    LockFrame(frame, "GetOSDFrame");
    const xvmc_render_state_t* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = (VideoFrame*) (r->p_osd_target_surface_render);

    QString dbg_str("GetOSDFrame ");
#if DEBUG_FRAME_LOCKS
    if (f)
        dbg_str.append(DebugString(f, true));
#endif
    UnlockFrame(frame, dbg_str);

    return f;
}

void VideoBuffers::SetOSDFrame(VideoFrame *frame, VideoFrame *osd)
{
    if (frame==osd)
    {
        VERBOSE(VB_IMPORTANT, QString("SetOSDFrame() -- frame==osd %1")
                .arg(GetStatus()));
        return;
    }

    LockFrame(frame, "SetOSDFrame");
    xvmc_render_state_t* r = GetRender(frame);
    if (r)
    {
        global_lock.lock();

        VideoFrame* old_osd = (VideoFrame*) r->p_osd_target_surface_render;
        if (old_osd)
            xvmc_osd_parent[old_osd] = NULL;

        r->p_osd_target_surface_render = osd;

        if (osd)
            xvmc_osd_parent[osd] = frame;

        global_lock.unlock();
    }
    UnlockFrame(frame, "SetOSDFrame");
}

VideoFrame* VideoBuffers::GetOSDParent(const VideoFrame *osd)
{
    global_lock.lock();
    VideoFrame *parent = xvmc_osd_parent[osd];
    global_lock.unlock();
    return parent;
}

#endif

bool VideoBuffers::CreateBuffers(int width, int height)
{
    vector<unsigned char*> bufs;
    return CreateBuffers(width, height, bufs);
}

bool VideoBuffers::CreateBuffers(int width, int height, vector<unsigned char*> bufs)
{
    bool ok = true;
    uint bpp = 12 / 4; /* bits per pixel div common factor */
    uint bpb =  8 / 4; /* bits per byte div common factor */
    uint buf_size = (width * height * bpp + 4/* to round up */) / bpb;
    while (bufs.size() < allocSize())
    {
        unsigned char *data = new unsigned char[buf_size + 64];

        // init buffers (y plane to 0, u/v planes to 127),
        // to prevent green screens..
        bzero(data, width * height);
        memset(data + width * height, 127, width * height / 2);

        bufs.push_back(data);
        if (bufs.back())
        {
            VERBOSE(VB_PLAYBACK, "Created data @"
                    <<((void*)data)<<"->"<<((void*)(data+buf_size)));
            allocated_arrays.push_back(bufs.back());
        }
        else
            ok = false;
    }
    for (uint i = 0; i < allocSize(); i++)
    {
        buffers[i].width = width;
        buffers[i].height = height;
        buffers[i].bpp = 12;
        buffers[i].size = buf_size;
        buffers[i].codec = FMT_YV12;
        buffers[i].qscale_table = NULL;
        buffers[i].qstride = 0;
        buffers[i].buf = bufs[i];
        ok &= (bufs[i] != NULL);
    }
    return ok;
}

#ifdef USING_XVMC
bool VideoBuffers::CreateBuffers(int width, int height,
                                 Display *disp, 
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
                "Woo Hoo! We have more XvMC Surfaces than we need");
        Reset();
        bool extra_for_pause = allocSize() > numbuffers;
        bool normal = (needprebufferframes_normal == needprebufferframes);
        uint increment = surfs.size() - allocSize();
        
        Init(numbuffers, extra_for_pause, 
             needfreeframes, needprebufferframes_normal+increment-1,
             needprebufferframes_small, keepprebufferframes+1);
        SetPrebuffering(normal);
    }

    for (uint i = 0; i < allocSize(); i++)
    {
        xvmc_vo_surf_t *surf    = (xvmc_vo_surf_t*) surfs[i];
        xvmc_render_state_t *render = new xvmc_render_state_t;
        allocated_structs.push_back((unsigned char*)render);
        memset(render, 0, sizeof(xvmc_render_state_t));
        buffers[i].buf          = (unsigned char*) render;

        // constants
        render->magic           = MP_XVMC_RENDER_MAGIC;
        render->state           = 0;
        buffers[i].bpp          = -1;
        buffers[i].codec        = FMT_XVMC_IDCT_MPEG2;
        buffers[i].size         = sizeof(XvMCSurface);

        // from videoout_xv
        render->disp            = disp;
        render->ctx             = &xvmc_ctx;

        // from width, height, and xvmv block and surface arrays
        render->p_surface       = &surf->surface;
        buffers[i].height       = height;
        buffers[i].width        = width;

        render->total_number_of_data_blocks = surf->blocks.num_blocks;
        render->total_number_of_mv_blocks   = surf->macro_blocks.num_blocks;
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
        render->mc_type         = xvmc_surf_info.mc_type;
        render->idct = (xvmc_surf_info.mc_type & XVMC_IDCT) == XVMC_IDCT;
        render->chroma_format   = xvmc_surf_info.chroma_format;
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
        delete [] allocated_arrays[i];
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

int DebugNum(const VideoFrame *frame)
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
