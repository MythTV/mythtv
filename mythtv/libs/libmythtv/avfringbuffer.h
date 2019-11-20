#ifndef _AVFRINGBUFFER_H_
#define _AVFRINGBUFFER_H_

#include "ringbuffer.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/url.h"
}

extern URLProtocol AVF_RingBuffer_Protocol;

class AVFRingBuffer
{
public:
    explicit AVFRingBuffer(RingBuffer *rbuffer = nullptr)
        : m_rbuffer(rbuffer) { }
    void                SetRingBuffer(RingBuffer *rbuffer)
    {
        m_rbuffer = rbuffer;
    }
    RingBuffer         *GetRingBuffer(void)
    {
        return m_rbuffer;
    }
    static URLProtocol *GetRingBufferURLProtocol(void);
    static int          AVF_Write_Packet(void *opaque, uint8_t *buf, int buf_size);
    static int          AVF_Read_Packet(void *opaque, uint8_t *buf, int buf_size);
    static int64_t      AVF_Seek_Packet(void *opaque, int64_t offset, int whence);
    static int          AVF_Open(URLContext *h, const char *filename, int flags);
    static int          AVF_Read(URLContext *h, uint8_t *buf, int buf_size);
    static int          AVF_Write(URLContext *h, const uint8_t *buf, int buf_size);
    static int64_t      AVF_Seek(URLContext *h, int64_t offset, int whence);
    static int          AVF_Close(URLContext *h);
    void                SetInInit(bool state);
    bool                IsInInit(void) { return m_initState; }

private:
    RingBuffer         *m_rbuffer {nullptr};
    bool                m_initState {true};
    static bool         s_avfrProtocolInitialised;
    static URLProtocol  s_avfrURL;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
