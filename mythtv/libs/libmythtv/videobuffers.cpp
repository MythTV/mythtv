// Copyright (c) 2005, Daniel Thor Kristjansson
// based on earlier work in MythTV's videout_xvmc.cpp

#include "videobuffers.h"
#include "mythcontext.h"

#ifdef USING_XVMC
extern "C" {
#include "../libavcodec/xvmc_render.h"
}
#endif


#define DEBUG_FRAME_LOCKS 0

#define TRY_LOCK_SPINS                 100
#define TRY_LOCK_SPINS_BEFORE_WARNING   10
#define TRY_LOCK_SPIN_WAIT             100 /* usec */

int next_dbg_str = 0;

/**
 * \class VideoBuffers
 *  This class creates tracks the state of the buffers used by 
 *  various Video Output classes.
 *  
 *  The states available for a buffer are available, used,
 *  limbo, process, and displayed.
 *  
 *  
 */

VideoBuffers::VideoBuffers()
    : numbuffers(0), needfreeframes(0), needprebufferframes(0),
      needprebufferframes_normal(0), needprebufferframes_small(0),
      keepprebufferframes(0), need_extra_for_pause(false), rpos(0), vpos(0),
      frame_lock(true), inheritence_lock(false), global_lock(true)
{
}

VideoBuffers::~VideoBuffers()
{
    DeleteBuffers();
}

/**
 * \fn VideoBuffers::Init(uint, bool, uint, uint, uint, uint)
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
 *                             and process
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
    global_lock.lock();

    uint numcreate = numdecode + ((extra_for_pause) ? 1 : 0);

    // make a big reservation, so that things that depend on
    // pointer to VideoFrames work even after a few push_backs
    buffers.reserve(max(numcreate, (uint)128));

    buffers.resize(numcreate);
    for (uint i = 0; i < numcreate; i++)
    {
        bzero(at(i), sizeof(VideoFrame));
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
    process.clear();
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
        while (frame && used.contains(frame))
        {
            VERBOSE(VB_GENERAL, "GetNextFreeFrame() served a busy frame. Dropping."
                    "    "<<GetStatus());
            frame = available.dequeue();
        }

        // only way this should be triggered if we're in unsafe mode
        if (!frame && allow_unsafe)
        {
            VERBOSE(VB_IMPORTANT, "GetNextFreeFrame() is getting a busy frame."
                    "        "<<GetStatus());
            frame = used.dequeue();
            if (EnoughFreeFrames())
                available_wait.wakeAll();
        }
        if (frame)
        {
            frame_queue_t *q = queue(enqueue_to);
            if (q)
                q->enqueue(frame);
            success = true;
            if (with_lock)
                success = TryLockFrame(frame, "GetNextFreeFrame");
            if (!success)
            {
                available.enqueue(frame);
                q->remove(frame);
            }
        }

        global_lock.unlock();

        if (!success)
        {
            if (frame)
            {
                VERBOSE(VB_IMPORTANT, "GetNextFreeFrame() unable to lock frame. "
                        <<DebugString(frame)<<" Dropping. "<<GetStatus());
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
 *  Removed frame from limbo and added it to used queue.
 * \param frame Frame to move to used.
 */
void VideoBuffers::ReleaseFrame(VideoFrame *frame)
{
    global_lock.lock();
    vpos = vbufferMap[frame];
    limbo.remove(frame);
    used.enqueue(frame);
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
    if (TryLockFrame(frame, "DiscardFrame"))
    {
        safeEnqueue(kVideoBuffer_avail, frame);
        if (EnoughFreeFrames())
            available_wait.wakeAll();

        UnlockFrame(frame, "DiscardFrame");
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Unable to Discard Frame "<<DebugString(frame));
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
    else if (type == kVideoBuffer_process)
        q = &process;
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
    else if (type == kVideoBuffer_process)
        q = &process;
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
        global_lock.lock();
        VideoFrame *frame = q->head();
        global_lock.unlock();
        return frame;
    }
    return NULL;
}

VideoFrame *VideoBuffers::tail(BufferType type)
{
    frame_queue_t *q = queue(type);
    if (q)
    {
        global_lock.lock();
        VideoFrame *frame = q->tail();
        global_lock.unlock();
        return frame;
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
            if (q == &available)
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
        if ((type & kVideoBuffer_process) == kVideoBuffer_process)
            process.remove(frame);
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
    global_lock.lock();
    remove(kVideoBuffer_all, frame);
    enqueue(dst, frame);
    global_lock.unlock();
}

frame_queue_t::iterator VideoBuffers::begin_lock(BufferType type)
{
    global_lock.lock();
    frame_queue_t *q = queue(type);
    if (q)
        return q->begin();
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
    for (uint i=0; i<size(); i++)
        RemoveInheritence(at(i));

    global_lock.lock();
    VERBOSE(VB_PLAYBACK, QString("DiscardFrames()       used %1 limbo %2")
            .arg(DebugString(used))
            .arg(DebugString(limbo)));

    while (!used.empty())
        DiscardFrame(used.dequeue());

    while (!limbo.empty())
        DiscardFrame(limbo.dequeue());

    const uint nbuf = available.count() +
        displayed.count() + process.count();
    if (nbuf != size())
    {
        for (uint i=0; i < size(); i++)
        {
            if (!available.contains(at(i)) &&
                !process.contains(at(i)) &&
                !displayed.contains(at(i)))
            {
                VERBOSE(VB_IMPORTANT, "ERROR: "<<DebugString(at(i), true)
                        <<" not in available, process, or displayed   "
                        <<GetStatus());
                DiscardFrame(at(i));
            }
        }
    }

    VERBOSE(VB_PLAYBACK, "DiscardFrames() -- done() "<<GetStatus());

    available_wait.wakeAll();

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

void VideoBuffers::LockFrame(const VideoFrame *frame, const QString &owner)
{
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

void VideoBuffers::LockFrames(vector<const VideoFrame*>& vec, const QString& owner)
{
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
            VERBOSE(VB_PLAYBACK, "no lock, frames: "<<DebugString(vec)
                    <<" "<<GetStatus()<<" "<<owner);
#endif
        }
    }
    while (!ok);
}

bool VideoBuffers::TryLockFrame(const VideoFrame *frame, const QString &owner)
{
    QMutex *mutex = NULL;
    (void)owner;

    if (!frame)
        return true;

    frame_lock.lock();
#if DEBUG_FRAME_LOCKS
    if (owner!="")
        VERBOSE(VB_PLAYBACK, "try lock frame:  "
                <<DebugString(frame)<<" "<<GetStatus()<<" "<<owner);
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

void VideoBuffers::UnlockFrame(const VideoFrame *frame, const QString &owner)
{
    (void)owner;

    if (!frame)
        return;

    frame_lock.lock();
#if DEBUG_FRAME_LOCKS
    if (owner!="")
        VERBOSE(VB_PLAYBACK, "unlocking frame: "
                <<DebugString(frame)<<" "<<GetStatus()<<" "<<owner);
#endif

    frame_lock_map_t::iterator it = frame_locks.find(frame);
    it->second->unlock();

    frame_lock.unlock();
}

void VideoBuffers::UnlockFrames(vector<const VideoFrame*>& vec, const QString& owner)
{
    (void)owner;
#if DEBUG_FRAME_LOCKS
    VERBOSE(VB_PLAYBACK, "unlocking frames:"<<DebugString(vec)<<" "
            <<GetStatus()<<" "<<owner);
#endif
    for (uint i=0; i<vec.size(); i++)
        UnlockFrame(vec[i], "");
}

void VideoBuffers::AddInheritence(const VideoFrame *frame)
{
#ifdef USING_XVMC
    inheritence_lock.lock();
    
    frame_map_t::iterator it = parents.find(frame);
    if (it == parents.end())
    {
        xvmc_render_state_t *render = (xvmc_render_state_t*) frame->buf;
        // find "parents"...
        frame_queue_t new_parents;
        VideoFrame *past = xvmc_surf_to_frame[render->p_past_surface];
        if (past==frame)
        {
            VERBOSE(VB_IMPORTANT, "AddInheritence("<<DebugString(frame)<<") past=frame");
            VERBOSE(VB_IMPORTANT, "render->p_surface      = "<<
                    ((void*)render->p_surface));
            VERBOSE(VB_IMPORTANT, "render->p_past_surface = "<<
                    ((void*)render->p_past_surface));
        }
        if (past && past!=frame)
        {
            if (used.contains(past))
                new_parents.push_back(past);
            else if (displayed.contains(past))
                new_parents.push_back(past);
            else if (limbo.contains(past))
                new_parents.push_back(past);
            else
            {
                VERBOSE(VB_IMPORTANT, "AddInheritence past   "<<DebugString(past)
                        <<" NOT in used or in done");
            }
        }
        VideoFrame *future = xvmc_surf_to_frame[render->p_future_surface];
        if (future==frame)
        {
            VERBOSE(VB_IMPORTANT, "AddInheritence("<<DebugString(frame)<<") future=frame");
            VERBOSE(VB_IMPORTANT, "render->p_surface        = "<<
                    ((void*)render->p_surface));
            VERBOSE(VB_IMPORTANT, "render->p_future_surface = "<<
                    ((void*)render->p_future_surface));
        }
        if (future && future!=frame)
        {
            if (used.contains(future))
                new_parents.push_back(future);
            else if (limbo.contains(future))
                new_parents.push_back(future);
            else
            {
                VERBOSE(VB_IMPORTANT, "AddInheritence future "
                        <<DebugString(future)<<" NOT in used or in limbo");
                if (displayed.contains(future))
                    VERBOSE(VB_IMPORTANT, "AddInheritence future "
                            <<DebugString(future)<<" is in done");
                if (limbo.contains(future))
                    VERBOSE(VB_IMPORTANT, "AddInheritence future "
                            <<DebugString(future)<<" is in limbo");
            }
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
        VERBOSE(VB_IMPORTANT, "RemoveInheritenc:"<<DebugString(frame)
                <<" parents.size() = "<<parents.size());
        frame_queue_t::iterator pit = new_parents.begin();
        for (int i=0; pit != new_parents.end() && i<8; (++pit), (++i))
            VERBOSE(VB_IMPORTANT, "Parent #"<<i<<": "<<DebugString(*pit));
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

VideoFrame* VideoBuffers::PastFrame(VideoFrame *frame)
{
    LockFrame(frame, "PastFrame");
    xvmc_render_state_t* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = xvmc_surf_to_frame[r->p_past_surface];
    UnlockFrame(frame, "PastFrame");
    return f;
}

VideoFrame* VideoBuffers::FutureFrame(VideoFrame *frame)
{
    LockFrame(frame, "FutureFrame");
    xvmc_render_state_t* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = xvmc_surf_to_frame[r->p_future_surface];
    UnlockFrame(frame, "FutureFrame");
    return f;
}

VideoFrame* VideoBuffers::GetOSDFrame(VideoFrame *frame)
{
    LockFrame(frame, "GetOSDFrame");
    xvmc_render_state_t* r = GetRender(frame);
    VideoFrame* f = NULL;
    if (r)
        f = (VideoFrame *)(r->p_osd_target_surface_render);
    UnlockFrame(frame, "GetOSDFrame");
    return f;
}

void VideoBuffers::SetOSDFrame(VideoFrame *frame, VideoFrame *osd)
{
    LockFrame(frame, "SetOSDFrame");
    xvmc_render_state_t* r = GetRender(frame);
    if (r)
        r->p_osd_target_surface_render = osd;
    UnlockFrame(frame, "SetOSDFrame");
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
    uint buf_size = (width * height * 3) / 2;
    while (bufs.size() < allocSize())
    {
        bufs.push_back(new unsigned char[buf_size + 64]);
        if (bufs.back())
            allocated_arrays.push_back(bufs.back());
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

typedef struct
{
    XvMCSurface         surface;
    XvMCBlockArray      blocks;
    XvMCMacroBlockArray macro_blocks;
    uint num_data_blocks;
    uint num_mv_blocks;
} xvmc_vo_surf_t;

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

        if (surf->num_data_blocks)
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
    xvmc_surf_to_frame.clear();
}

static unsigned long long to_bitmap(const frame_queue_t& list);
QString VideoBuffers::GetStatus(int n)
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
        unsigned long long p = to_bitmap(process);
        for (uint i=0; i<(uint)n; i++)
        {
            unsigned long long mask = 1<<i;
            if (a & mask)
                str += "A";
            else if (u & mask)
                str += "u";
            else if (d & mask)
                str += "d";
            else if (l & mask)
                str += "L";
            else if (p & mask)
                str += "P";
            else
                str += " ";
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

QString dbg_str_arr[40] =
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
QString dbg_str_arr_short[40] =
{
"A","B","C","D",
"E","F","G","H", // 8
"a","b","c","d",
"e","f","g","h", // 16
"0","1","2","3",
"4","5","6","7", // 24
"I","J","K","L",
"M","N","O","P", // 32
"i","j","k","l",
"m","n","o","p", // 40
};

map<const VideoFrame *, int> dbg_str;

int DebugNum(const VideoFrame *frame)
{
    map<const VideoFrame *, int>::iterator it = dbg_str.find(frame);
    if (it == dbg_str.end())
    {
        dbg_str[frame] = next_dbg_str;
        next_dbg_str = (next_dbg_str+1) % 40;
    }
    return dbg_str[frame];
}

const QString& DebugString(const VideoFrame *frame, bool short_str)
{
    return ((short_str) ? dbg_str_arr_short : dbg_str_arr)[DebugNum(frame)];
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
