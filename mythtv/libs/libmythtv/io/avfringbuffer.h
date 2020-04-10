#ifndef AVFRINGBUFFER_H
#define AVFRINGBUFFER_H

// MythTV
#include "io/ringbuffer.h"

// FFmpeg
extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/url.h"
}

extern URLProtocol AVF_RingBuffer_Protocol;

class AVFRingBuffer
{
  public:
    explicit AVFRingBuffer(RingBuffer *Buffer = nullptr);
    void                SetRingBuffer  (RingBuffer *Buffer);
    RingBuffer*         GetRingBuffer  (void);
    static URLProtocol* GetURLProtocol (void);
    static int          WritePacket    (void* Context, uint8_t *Buffer, int Size);
    static int          ReadPacket     (void* Context, uint8_t *Buffer, int Size);
    static int64_t      SeekPacket     (void* Context, int64_t Offset, int Whence);
    static int          Open           (URLContext* Context, const char *Filename, int Flags);
    static int          Read           (URLContext* Context, uint8_t *Buffer, int Size);
    static int          Write          (URLContext* Context, const uint8_t *Buffer, int Size);
    static int64_t      Seek           (URLContext* Context, int64_t Offset, int Whence);
    static int          Close          (URLContext*);
    void                SetInInit      (bool State);
    bool                IsInInit       (void);

  private:
    RingBuffer         *m_ringBuffer   { nullptr };
    bool                m_initState    { true    };
    static URLProtocol  s_avfrURL;
};
#endif
