#ifndef VIDEOBUFFERS_H
#define VIDEOBUFFERS_H

// Std
#include <vector>
#include <map>

// Qt
#include <QSize>
#include <QRecursiveMutex>
#include <QString>

// MythTV
#include "libmythbase/mythdeque.h"
#include "libmythtv/mythtvexp.h"
#include "libmythtv/mythframe.h"
#include "libmythtv/mythcodecid.h"

using frame_queue_t  = MythDeque<MythVideoFrame*> ;
using frame_vector_t = std::vector<MythVideoFrame>;
using vbuffer_map_t  = std::map<const MythVideoFrame*, uint>;

const QString& DebugString(const MythVideoFrame *Frame, bool Short = false);
const QString& DebugString(uint  FrameNum, bool Short = false);

enum BufferType : std::uint8_t
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

class MTV_PUBLIC VideoBuffers
{
  public:
    VideoBuffers() = default;
   ~VideoBuffers() = default;

    static uint GetNumBuffers(int PixelFormat, int MaxReferenceFrames = 16, bool Decoder = false);
    void Init(uint NumDecode,
              uint NeedFree, uint NeedprebufferNormal,
              uint NeedPrebufferSmall);
    bool CreateBuffers(VideoFrameType Type, const VideoFrameTypes* RenderFormats, QSize Size,
                       uint NeedFree, uint NeedprebufferNormal,
                       uint NeedPrebufferSmall, int MaxReferenceFrames = 16);
    bool CreateBuffers(VideoFrameType Type, int Width, int Height, const VideoFrameTypes* RenderFormats);
    static bool ReinitBuffer(MythVideoFrame *Frame, VideoFrameType Type, MythCodecID CodecID, int Width, int Height);
    void SetDeinterlacing(MythDeintType Single, MythDeintType Double, MythCodecID CodecID);

    void Reset(void);
    void DiscardFrames(bool NextFrameIsKeyFrame);
    void ClearAfterSeek(void);

    void SetPrebuffering(bool Normal);
    MythVideoFrame *GetNextFreeFrame(BufferType EnqueueTo = kVideoBuffer_limbo);
    void ReleaseFrame(MythVideoFrame *Frame);
    void DeLimboFrame(MythVideoFrame *Frame);
    void StartDisplayingFrame(void);
    void DoneDisplayingFrame(MythVideoFrame *Frame);
    void DiscardFrame(MythVideoFrame *Frame);
    void DiscardPauseFrames(void);
    bool DiscardAndRecreate(MythCodecID CodecID, QSize VideoDim, int References);

    MythVideoFrame *At(uint FrameNum);
    MythVideoFrame *Dequeue(BufferType Type);
    MythVideoFrame *Head(BufferType Type);
    MythVideoFrame *Tail(BufferType Type);
    void Enqueue(BufferType Type, MythVideoFrame* Frame);
    void SafeEnqueue(BufferType Type, MythVideoFrame* Frame);
    void Remove(BufferType Type, MythVideoFrame *Frame);
    frame_queue_t::iterator BeginLock(BufferType Type);
    frame_queue_t::iterator End(BufferType Type);
    void EndLock();
    uint Size(BufferType Type) const;
    bool Contains(BufferType Type, MythVideoFrame* Frame) const;
    MythVideoFrame *GetLastDecodedFrame(void);
    MythVideoFrame *GetLastShownFrame(void);

    uint ValidVideoFrames(void) const;
    uint FreeVideoFrames(void) const;
    bool EnoughFreeFrames(void) const;
    bool EnoughDecodedFrames(void) const;

    const MythVideoFrame *At(uint FrameNum) const;
    const MythVideoFrame *GetLastDecodedFrame(void) const;
    const MythVideoFrame *GetLastShownFrame(void) const;
    uint  Size(void) const;

    QString GetStatus(uint Num = 0) const;

  private:
    frame_queue_t       *Queue(BufferType Type);
    const frame_queue_t *Queue(BufferType Type) const;
    MythVideoFrame      *GetNextFreeFrameInternal(BufferType EnqueueTo);
    static void          SetDeinterlacingFlags(MythVideoFrame &Frame, MythDeintType Single,
                                               MythDeintType Double, MythCodecID CodecID);

    frame_queue_t        m_available;
    frame_queue_t        m_used;
    frame_queue_t        m_limbo;
    frame_queue_t        m_pause;
    frame_queue_t        m_displayed;
    frame_queue_t        m_decode;
    frame_queue_t        m_finished;
    vbuffer_map_t        m_vbufferMap;
    frame_vector_t       m_buffers;
    const VideoFrameTypes* m_renderFormats { nullptr };

    uint                 m_needFreeFrames            { 0 };
    uint                 m_needPrebufferFrames       { 0 };
    uint                 m_needPrebufferFramesNormal { 0 };
    uint                 m_needPrebufferFramesSmall  { 0 };
    uint                 m_rpos                      { 0 };
    uint                 m_vpos                      { 0 };
    mutable QRecursiveMutex m_globalLock;
};

#endif // VIDEOBUFFERS_H
