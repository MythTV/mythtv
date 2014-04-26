#ifndef ICRINGBUFFER_H
#define ICRINGBUFFER_H

#include "ringbuffer.h"

class NetStream;

class ICRingBuffer : public RingBuffer
{
  public:
    static enum RingBufferType const kRingBufferType = kRingBuffer_MHEG;

    ICRingBuffer(const QString &url, RingBuffer *parent = 0);
    virtual ~ICRingBuffer();

    // RingBuffer implementation
    virtual bool IsOpen(void) const;
    virtual long long GetReadPosition(void) const;
    virtual bool OpenFile(const QString &url,
                          uint retry_ms = kDefaultOpenTimeout);
    virtual bool IsStreamed(void)       { return false;  }
    virtual bool IsSeekingAllowed(void) { return true; }
    virtual bool IsBookmarkAllowed(void) { return false; }

  protected:
    virtual int safe_read(void *data, uint sz);
    virtual long long GetRealFileSizeInternal(void) const;
    virtual long long SeekInternal(long long pos, int whence);

    // Operations
  public:
    // Take ownership of parent RingBuffer
    RingBuffer *Take();

  private:
    NetStream *m_stream;
    RingBuffer *m_parent; // parent RingBuffer
};

#endif // ICRINGBUFFER_H
