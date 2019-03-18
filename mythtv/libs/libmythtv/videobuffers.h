// -*- Mode: c++ -*-

#ifndef __VIDEOBUFFERS_H__
#define __VIDEOBUFFERS_H__

#include <vector>
#include <map>
using namespace std;

#include <QMutex>
#include <QString>
#include <QWaitCondition>

#include "mythtvexp.h"
#include "mythframe.h"
#include "mythdeque.h"

#ifdef USING_X11
class MythXDisplay;
#endif

typedef MythDeque<VideoFrame*>                frame_queue_t;
typedef vector<VideoFrame>                    frame_vector_t;
typedef map<const unsigned char*, void*>      buffer_map_t;
typedef map<const VideoFrame*, uint>          vbuffer_map_t;
typedef map<const VideoFrame*, QMutex*>       frame_lock_map_t;
typedef vector<unsigned char*>                uchar_vector_t;


const QString& DebugString(const VideoFrame *frame, bool short_str=false);
const QString& DebugString(uint str_num, bool short_str=false);
QString DebugString(const frame_queue_t& list);
QString DebugString(const vector<const VideoFrame*>& list);

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
    YUVInfo(uint w, uint h, uint sz, const int *p, const int *o,
            int aligned = 64);

  public:
    uint m_width;
    uint m_height;
    uint m_size;
    uint m_pitches[3];
    uint m_offsets[3];
};

class MTV_PUBLIC VideoBuffers
{
  public:
    VideoBuffers() = default;
    virtual ~VideoBuffers();

    void Init(uint numdecode, bool extra_for_pause,
              uint need_free, uint needprebuffer_normal,
              uint needprebuffer_small, uint keepprebuffer);

    bool CreateBuffers(VideoFrameType type, int width, int height,
                       vector<unsigned char*> bufs,
                       vector<YUVInfo>        yuvinfo);
    bool CreateBuffers(VideoFrameType type, int width, int height);
    void DeleteBuffers(void);

    void Reset(void);
    void DiscardFrames(bool next_frame_keyframe);
    void ClearAfterSeek(void);

    void SetPrebuffering(bool normal);

    VideoFrame *GetNextFreeFrame(BufferType enqueue_to = kVideoBuffer_limbo);
    void ReleaseFrame(VideoFrame *frame);
    void DeLimboFrame(VideoFrame *frame);
    void StartDisplayingFrame(void);
    void DoneDisplayingFrame(VideoFrame *frame);
    void DiscardFrame(VideoFrame *frame);

    VideoFrame *At(uint i) { return &m_buffers[i]; }
    VideoFrame *Dequeue(BufferType);
    VideoFrame *Head(BufferType); // peek at next dequeue
    VideoFrame *Tail(BufferType); // peek at last enqueue
    void Requeue(BufferType dst, BufferType src, int num = 1);
    void Enqueue(BufferType, VideoFrame*);
    void SafeEnqueue(BufferType, VideoFrame* frame);
    void Remove(BufferType, VideoFrame *); // multiple buffer types ok
    frame_queue_t::iterator begin_lock(BufferType); // this locks VideoBuffer
    frame_queue_t::iterator end(BufferType);
    void end_lock() { m_globalLock.unlock(); } // this unlocks VideoBuffer
    uint Size(BufferType type) const;
    bool Contains(BufferType type, VideoFrame*) const;

    VideoFrame *GetScratchFrame(void);
    VideoFrame *GetLastDecodedFrame(void) { return At(m_vpos); }
    VideoFrame *GetLastShownFrame(void) { return At(m_rpos); }
    void SetLastShownFrameToScratch(void);

    uint ValidVideoFrames(void) const { return Size(kVideoBuffer_used); }
    uint FreeVideoFrames(void) const { return Size(kVideoBuffer_avail); }
    bool EnoughFreeFrames(void) const
        { return Size(kVideoBuffer_avail) >= m_needFreeFrames; }
    bool EnoughDecodedFrames(void) const
        { return Size(kVideoBuffer_used) >= m_needPrebufferFrames; }
    bool EnoughPrebufferedFrames(void) const
        { return Size(kVideoBuffer_used) >= m_keepPrebufferFrames; }

    const VideoFrame *At(uint i) const { return &m_buffers[i]; }
    const VideoFrame *GetLastDecodedFrame(void) const { return At(m_vpos); }
    const VideoFrame *GetLastShownFrame(void) const { return At(m_rpos); }
    uint  Size() const { return m_buffers.size(); }

    void Clear(uint i);
    void Clear(void);

    bool CreateBuffer(int width, int height, uint num, void *data,
                      VideoFrameType fmt);
    uint AddBuffer(int width, int height, void* data,
                   VideoFrameType fmt);

    QString GetStatus(int n=-1) const; // debugging method
  private:
    frame_queue_t       *Queue(BufferType type);
    const frame_queue_t *Queue(BufferType type) const;
    VideoFrame          *GetNextFreeFrameInternal(BufferType enqueue_to);

    frame_queue_t        m_available;
    frame_queue_t        m_used;
    frame_queue_t        m_limbo;
    frame_queue_t        m_pause;
    frame_queue_t        m_displayed;
    frame_queue_t        m_decode;
    frame_queue_t        m_finished;
    vbuffer_map_t        m_vbufferMap; // videobuffers to buffer's index
    frame_vector_t       m_buffers;
    uchar_vector_t       m_allocatedArrays;  // for DeleteBuffers

    uint                 m_needFreeFrames            {0};
    uint                 m_needPrebufferFrames       {0};
    uint                 m_needPrebufferFramesNormal {0};
    uint                 m_needPrebufferFramesSmall  {0};
    uint                 m_keepPrebufferFrames       {0};
    bool                 m_createdPauseFrame         {false};
    uint                 m_rpos                      {0};
    uint                 m_vpos                      {0};

    mutable QMutex       m_globalLock                {QMutex::Recursive};
};

#endif // __VIDEOBUFFERS_H__
