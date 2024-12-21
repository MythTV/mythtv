#ifndef AVFRINGBUFFER_H
#define AVFRINGBUFFER_H

// MythTV
#include "libmythtv/io/mythmediabuffer.h"

// FFmpeg
extern "C" {
#include "libavformat/avio.h"
}

class MythAVFormatBuffer
{
  public:
    MythAVFormatBuffer(MythMediaBuffer *Buffer, bool write_flag, bool force_seek);
    ~MythAVFormatBuffer();

    void                SetInInit      (bool State);
    bool                IsInInit       (void) const;

    AVIOContext*        getAVIOContext() { return m_avioContext; }

  private:
    static int read_packet(void *opaque, uint8_t *buf, int buf_size);
    static int write_packet(void *opaque, const uint8_t *buf, int buf_size);
    static int64_t seek(void *opaque, int64_t offset, int whence);

    AVIOContext* alloc_context(bool write_flag, bool force_seek);

    MythMediaBuffer    *m_buffer       { nullptr };
    bool                m_initState    { true    };
    AVIOContext        *m_avioContext  { nullptr };
};
#endif
