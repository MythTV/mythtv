#ifndef AVFRINGBUFFER_H
#define AVFRINGBUFFER_H

// MythTV
#include "libmythtv/io/mythmediabuffer.h"

// FFmpeg
extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/url.h"
}

class MythAVFormatBuffer
{
  public:
    explicit MythAVFormatBuffer(MythMediaBuffer *Buffer = nullptr);
    void                SetBuffer      (MythMediaBuffer *Buffer);
    MythMediaBuffer*    GetBuffer      (void);
    static URLProtocol* GetURLProtocol (void);
    static int          WritePacket    (void* Context, uint8_t *Buffer, int Size);
    static int          ReadPacket     (void* Context, uint8_t *Buffer, int Size);
    static int64_t      SeekPacket     (void* Context, int64_t Offset, int Whence);
    static int          Open           (URLContext* Context, const char *Filename, int Flags);
    static int          Read           (URLContext* Context, uint8_t *Buffer, int Size);
    static int          Write          (URLContext* Context, const uint8_t *Buffer, int Size);
    static int64_t      Seek           (URLContext* Context, int64_t Offset, int Whence);
    static int          Close          (URLContext* /*Context*/);
    void                SetInInit      (bool State);
    bool                IsInInit       (void) const;

  private:
    MythMediaBuffer    *m_buffer       { nullptr };
    bool                m_initState    { true    };
    static URLProtocol  s_avfrURL;
};
#endif
