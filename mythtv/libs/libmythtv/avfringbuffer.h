#ifndef _AVFRINGBUFFER_H_
#define _AVFRINGBUFFER_H_

#include "ringbuffer.h"

extern "C" {
#include "libavformat/avformat.h"
}

extern URLProtocol AVF_RingBuffer_Protocol;

extern int AVF_Write_Packet(void *opaque, uint8_t *buf, int buf_size);
extern int AVF_Read_Packet(void *opaque, uint8_t *buf, int buf_size);
extern int64_t AVF_Seek_Packet(void *opaque, int64_t offset, int whence);

class AVFRingBuffer
{
  public:
    AVFRingBuffer(RingBuffer *rbuffer = NULL) { m_rbuffer = rbuffer; }

    void SetRingBuffer(RingBuffer *rbuffer) { m_rbuffer = rbuffer; }
    RingBuffer *GetRingBuffer(void)         { return m_rbuffer; }

  private:
    RingBuffer *m_rbuffer;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
