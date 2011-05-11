// -*- Mode: c++ -*-

#ifndef __VIDEOBUFFERS_H__
#define __VIDEOBUFFERS_H__

extern "C" {
#include "frame.h"
}
#include <vector>
#include <map>
using namespace std;

#include <QMutex>
#include <QString>
#include <QWaitCondition>

#ifdef USING_XVMC
#include <qwindowdefs.h>
#endif // USING_XVMC

#include "mythdeque.h"

#ifdef USING_X11
class MythXDisplay;
#endif

typedef MythDeque<VideoFrame*>                frame_queue_t;
typedef vector<VideoFrame>                    frame_vector_t;
typedef map<const VideoFrame*, frame_queue_t> frame_map_t;
typedef map<const void*, VideoFrame*>         surf_to_frame_map_t;
typedef map<const unsigned char*, void*>      buffer_map_t;
typedef map<const VideoFrame*, uint>          vbuffer_map_t;
typedef map<const VideoFrame*, QMutex*>       frame_lock_map_t;
typedef map<const VideoFrame*, VideoFrame*>   frame_to_frame_map_t;
typedef vector<unsigned char*>                uchar_vector_t;


const QString& DebugString(const VideoFrame *frame, bool short_str=false);
const QString& DebugString(uint str_num, bool short_str=false);
const QString DebugString(const frame_queue_t& list);
const QString DebugString(const vector<const VideoFrame*>& list);

enum BufferType
{
    kVideoBuffer_avail     = 0x00000001,
    kVideoBuffer_limbo     = 0x00000002,
    kVideoBuffer_used      = 0x00000004,
    kVideoBuffer_pause     = 0x00000008,
    kVideoBuffer_displayed = 0x00000010,
    kVideoBuffer_finished  = 0x00000020,
    kVideoBuffer_decode    = 0x00000040,
    kVideoBuffer_all       = 0x0000003F,
};

class YUVInfo
{
  public:
    YUVInfo(uint w, uint h, uint size, const int *p, const int *o);

  public:
    uint width;
    uint height;
    uint size;
    uint pitches[3];
    uint offsets[3];
};

class VideoBuffers
{
  public:
    VideoBuffers();
    virtual ~VideoBuffers();

    void Init(uint numdecode, bool extra_for_pause,
              uint need_free, uint needprebuffer_normal,
              uint needprebuffer_small, uint keepprebuffer,
              bool enable_frame_locking = false);

    bool CreateBuffers(VideoFrameType type, int width, int height,
                       vector<unsigned char*> bufs,
                       vector<YUVInfo>        yuvinfo);
    bool CreateBuffers(VideoFrameType type, int width, int height);
    void DeleteBuffers(void);

    void Reset(void);
    void DiscardFrames(bool next_frame_keyframe);
    void ClearAfterSeek(void);

    void SetPrebuffering(bool normal);

    VideoFrame *GetNextFreeFrame(bool with_lock, bool allow_unsafe,
                                 BufferType enqueue_to = kVideoBuffer_limbo);
    void ReleaseFrame(VideoFrame *frame);
    void DeLimboFrame(VideoFrame *frame);
    void StartDisplayingFrame(void);
    void DoneDisplayingFrame(VideoFrame *frame);
    void DiscardFrame(VideoFrame *frame);

    VideoFrame *at(uint i) { return &buffers[i]; }
    VideoFrame *dequeue(BufferType);
    VideoFrame *head(BufferType); // peek at next dequeue
    VideoFrame *tail(BufferType); // peek at last enqueue
    void requeue(BufferType dst, BufferType src, int num = 1);
    void enqueue(BufferType, VideoFrame*);
    void safeEnqueue(BufferType, VideoFrame* frame);
    void remove(BufferType, VideoFrame *); // multiple buffer types ok
    frame_queue_t::iterator begin_lock(BufferType); // this locks VideoBuffer
    frame_queue_t::iterator end(BufferType);
    void end_lock() { global_lock.unlock(); } // this unlocks VideoBuffer
    uint size(BufferType type) const;
    bool contains(BufferType type, VideoFrame*) const;

    VideoFrame *GetScratchFrame(void);
    const VideoFrame *GetScratchFrame() const;
    VideoFrame *GetLastDecodedFrame(void) { return at(vpos); }
    VideoFrame *GetLastShownFrame(void) { return at(rpos); }
    void SetLastShownFrameToScratch() { rpos = size(); }

    uint ValidVideoFrames(void) const { return size(kVideoBuffer_used); }
    uint FreeVideoFrames(void) const { return size(kVideoBuffer_avail); }
    bool EnoughFreeFrames(void) const
        { return size(kVideoBuffer_avail) >= needfreeframes; }
    bool EnoughDecodedFrames(void) const
        { return size(kVideoBuffer_used) >= needprebufferframes; }
    bool EnoughPrebufferedFrames(void) const
        { return size(kVideoBuffer_used) >= keepprebufferframes; }

    const VideoFrame *at(uint i) const { return &buffers[i]; }
    const VideoFrame *GetLastDecodedFrame(void) const { return at(vpos); }
    const VideoFrame *GetLastShownFrame(void) const { return at(rpos); }
    uint size() const { return numbuffers; }
    uint allocSize() const { return buffers.size(); }

    void LockFrame(const VideoFrame *, const char* owner);
    void LockFrames(vector<const VideoFrame*>&, const char* owner);
    bool TryLockFrame(const VideoFrame *, const char* owner);
    void UnlockFrame(const VideoFrame *, const char* owner);
    void UnlockFrames(vector<const VideoFrame*>&, const char* owner);

    void AddInheritence(const VideoFrame *frame);
    void RemoveInheritence(const VideoFrame *frame);
    frame_queue_t Children(const VideoFrame *frame);
    bool HasChildren(const VideoFrame *frame);

    void Clear(uint i);
    void Clear(void);

#ifdef USING_XVMC
    VideoFrame* PastFrame(const VideoFrame *frame);
    VideoFrame* FutureFrame(const VideoFrame *frame);
    VideoFrame* GetOSDFrame(const VideoFrame *frame);
    void SetOSDFrame(VideoFrame *frame, VideoFrame *osd);
    VideoFrame* GetOSDParent(const VideoFrame *osd);
    bool CreateBuffers(int width, int height,
                       MythXDisplay *disp,
                       void* xvmc_ctx,
                       void* xvmc_surf_info,
                       vector<void*> surfs);
#endif

    bool CreateBuffer(int width, int height, uint num, void *data,
                      VideoFrameType fmt);

    QString GetStatus(int n=-1) const; // debugging method
  private:
    frame_queue_t         *queue(BufferType type);
    const frame_queue_t   *queue(BufferType type) const;
    VideoFrame            *GetNextFreeFrameInternal(
        bool with_lock, bool allow_unsafe, BufferType enqueue_to);

    frame_queue_t          available, used, limbo, pause, displayed, decode, finished;
    vbuffer_map_t          vbufferMap; // videobuffers to buffer's index
    frame_vector_t         buffers;
    uchar_vector_t         allocated_structs; // for DeleteBuffers
    uchar_vector_t         allocated_arrays;  // for DeleteBuffers
    frame_map_t            parents;    // prev & future frames
    frame_map_t            children;   // frames that depend on a parent frame

    uint                   numbuffers;
    uint                   needfreeframes;
    uint                   needprebufferframes;
    uint                   needprebufferframes_normal;
    uint                   needprebufferframes_small;;
    uint                   keepprebufferframes;
    bool                   need_extra_for_pause;

    uint                   rpos;
    uint                   vpos;

    mutable QMutex         global_lock;

    bool                   use_frame_locks;
    QMutex                 frame_lock;
    frame_lock_map_t       frame_locks;

#ifdef USING_XVMC
    surf_to_frame_map_t    xvmc_surf_to_frame;
    frame_to_frame_map_t   xvmc_osd_parent;
#endif
};

#endif // __VIDEOBUFFERS_H__
