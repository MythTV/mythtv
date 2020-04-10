#ifndef ICRINGBUFFER_H
#define ICRINGBUFFER_H

// MythTV
#include "ringbuffer.h"

class NetStream;

class ICRingBuffer : public MythMediaBuffer
{
  public:
    explicit ICRingBuffer(const QString &Url, MythMediaBuffer *Parent = nullptr);
    ~ICRingBuffer() override;

    bool      IsOpen            (void) const override;
    long long GetReadPosition   (void) const override;
    bool      OpenFile          (const QString &Url, uint Retry = static_cast<uint>(kDefaultOpenTimeout)) override;
    bool      IsStreamed        (void) override { return false;  }
    bool      IsSeekingAllowed  (void) override { return true; }
    bool      IsBookmarkAllowed (void) override { return false; }

    MythMediaBuffer* TakeBuffer (void);

  protected:
    int       SafeRead          (void *Buffer, uint Size) override;
    long long GetRealFileSizeInternal(void) const override;
    long long SeekInternal      (long long Position, int Whence) override;

  private:
    NetStream       *m_stream { nullptr };
    MythMediaBuffer *m_parent { nullptr };
};
#endif
