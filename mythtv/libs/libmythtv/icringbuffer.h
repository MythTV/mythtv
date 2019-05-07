#ifndef ICRINGBUFFER_H
#define ICRINGBUFFER_H

#include "ringbuffer.h"

class NetStream;

class ICRingBuffer : public RingBuffer
{
  public:
    static enum RingBufferType const kRingBufferType = kRingBuffer_MHEG;

    ICRingBuffer(const QString &url, RingBuffer *parent = nullptr);
    virtual ~ICRingBuffer();

    // RingBuffer implementation
    bool IsOpen(void) const override; // RingBuffer
    long long GetReadPosition(void) const override; // RingBuffer
    bool OpenFile(const QString &url,
                  uint retry_ms = kDefaultOpenTimeout) override; // RingBuffer
    bool IsStreamed(void) override      { return false;  } // RingBuffer
    bool IsSeekingAllowed(void) override { return true; } // RingBuffer
    bool IsBookmarkAllowed(void) override { return false; } // RingBuffer

  protected:
    int safe_read(void *data, uint sz) override; // RingBuffer
    long long GetRealFileSizeInternal(void) const override; // RingBuffer
    long long SeekInternal(long long pos, int whence) override; // RingBuffer

    // Operations
  public:
    // Take ownership of parent RingBuffer
    RingBuffer *Take();

  private:
    NetStream  *m_stream {nullptr};
    RingBuffer *m_parent {nullptr}; // parent RingBuffer
};

#endif // ICRINGBUFFER_H
