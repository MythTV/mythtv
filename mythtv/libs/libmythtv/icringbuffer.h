#ifndef ICRINGBUFFER_H
#define ICRINGBUFFER_H

// MythTV
#include "ringbuffer.h"

class NetStream;

class ICRingBuffer : public RingBuffer
{
  public:
    explicit ICRingBuffer(const QString &Url, RingBuffer *Parent = nullptr);
    ~ICRingBuffer() override;

    bool      IsOpen            (void) const override;
    long long GetReadPosition   (void) const override;
    bool      OpenFile          (const QString &Url, uint Retry = static_cast<uint>(kDefaultOpenTimeout)) override;
    bool      IsStreamed        (void) override { return false;  }
    bool      IsSeekingAllowed  (void) override { return true; }
    bool      IsBookmarkAllowed (void) override { return false; }

    RingBuffer* TakeRingBuffer(void);

  protected:
    int       SafeRead          (void *Buffer, uint Size) override;
    long long GetRealFileSizeInternal(void) const override;
    long long SeekInternal      (long long Position, int Whence) override;

  private:
    NetStream  *m_stream { nullptr };
    RingBuffer *m_parent { nullptr };
};
#endif
