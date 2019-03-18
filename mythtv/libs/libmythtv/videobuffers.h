#ifndef __VIDEOBUFFERS_H__
#define __VIDEOBUFFERS_H__

// Qt
#include <QMutex>
#include <QString>

// MythTV
#include "mythtvexp.h"
#include "mythframe.h"
#include "mythdeque.h"

// Std
#include <vector>
#include <map>
using namespace std;

typedef MythDeque<VideoFrame*>       frame_queue_t;
typedef vector<VideoFrame>           frame_vector_t;
typedef map<const VideoFrame*, uint> vbuffer_map_t;
typedef vector<unsigned char*>       uchar_vector_t;

const QString& DebugString(const VideoFrame *Frame, bool Short = false);
const QString& DebugString(uint  FrameNum, bool Short = false);

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
    YUVInfo(uint Width, uint Height, uint Size, const int *Pitches,
            const int *Offsets, int Alignment = 64);

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

    static uint GetNumBuffers(int PixelFormat);
    void Init(uint NumDecode, bool ExtraForPause,
              uint NeedFree, uint NeedprebufferNormal,
              uint NeedPrebufferSmall, uint KeepPrebuffer);

    bool CreateBuffers(VideoFrameType Type, int Width, int Height,
                       vector<unsigned char*> Buffers,
                       vector<YUVInfo>        YUVInfos);
    bool CreateBuffers(VideoFrameType Type, int Width, int Height);
    void DeleteBuffers(void);

    void Reset(void);
    void DiscardFrames(bool NextFrameIsKeyFrame);
    void ClearAfterSeek(void);

    void SetPrebuffering(bool Normal);

    VideoFrame *GetNextFreeFrame(BufferType EnqueueTo = kVideoBuffer_limbo);
    void ReleaseFrame(VideoFrame *Frame);
    void DeLimboFrame(VideoFrame *Frame);
    void StartDisplayingFrame(void);
    void DoneDisplayingFrame(VideoFrame *Frame);
    void DiscardFrame(VideoFrame *Frame);

    VideoFrame *At(uint FrameNum);
    VideoFrame *Dequeue(BufferType Type);
    VideoFrame *Head(BufferType Type);
    VideoFrame *Tail(BufferType Type);
    void Requeue(BufferType Dest, BufferType Source, int Count = 1);
    void Enqueue(BufferType Type, VideoFrame* Frame);
    void SafeEnqueue(BufferType Type, VideoFrame* Frame);
    void Remove(BufferType Type, VideoFrame *Frame);
    frame_queue_t::iterator BeginLock(BufferType Type);
    frame_queue_t::iterator End(BufferType Type);
    void EndLock();
    uint Size(BufferType Type) const;
    bool Contains(BufferType Type, VideoFrame* Frame) const;

    VideoFrame *GetScratchFrame(void);
    VideoFrame *GetLastDecodedFrame(void);
    VideoFrame *GetLastShownFrame(void);
    void SetLastShownFrameToScratch(void);

    uint ValidVideoFrames(void) const;
    uint FreeVideoFrames(void) const;
    bool EnoughFreeFrames(void) const;
    bool EnoughDecodedFrames(void) const;
    bool EnoughPrebufferedFrames(void) const;

    const VideoFrame *At(uint i) const;
    const VideoFrame *GetLastDecodedFrame(void) const;
    const VideoFrame *GetLastShownFrame(void) const;
    uint  Size(void) const;
    void Clear(uint FrameNum);
    void Clear(void);
    bool CreateBuffer(int Width, int Height, uint Number, void *Data, VideoFrameType Format);
    uint AddBuffer(int Width, int Geight, void* Data, VideoFrameType Format);

    QString GetStatus(uint Num = 0) const;

  private:
    frame_queue_t       *Queue(BufferType Type);
    const frame_queue_t *Queue(BufferType Type) const;
    VideoFrame          *GetNextFreeFrameInternal(BufferType EnqueueTo);
    void                 ReleaseDecoderResources(VideoFrame *Frame);

    frame_queue_t        m_available;
    frame_queue_t        m_used;
    frame_queue_t        m_limbo;
    frame_queue_t        m_pause;
    frame_queue_t        m_displayed;
    frame_queue_t        m_decode;
    frame_queue_t        m_finished;
    vbuffer_map_t        m_vbufferMap;
    frame_vector_t       m_buffers;
    uchar_vector_t       m_allocatedArrays;

    uint                 m_needFreeFrames            { 0 };
    uint                 m_needPrebufferFrames       { 0 };
    uint                 m_needPrebufferFramesNormal { 0 };
    uint                 m_needPrebufferFramesSmall  { 0 };
    uint                 m_keepPrebufferFrames       { 0 };
    bool                 m_createdPauseFrame         { false };
    uint                 m_rpos                      { 0 };
    uint                 m_vpos                      { 0 };
    mutable QMutex       m_globalLock                { QMutex::Recursive };
};

#endif // __VIDEOBUFFERS_H__
