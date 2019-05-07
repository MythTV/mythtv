#ifndef STREAMINGRINGBUFFER_H
#define STREAMINGRINGBUFFER_H

#include "ringbuffer.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/url.h"
}

class StreamingRingBuffer : public RingBuffer
{
  public:
    explicit StreamingRingBuffer(const QString &lfilename);
    virtual ~StreamingRingBuffer();

    // RingBuffer implementation
    bool IsOpen(void) const override; // RingBuffer
    long long GetReadPosition(void) const override; // RingBuffer
    bool OpenFile(const QString &lfilename,
                  uint retry_ms = kDefaultOpenTimeout) override; // RingBuffer
    bool IsStreamed(void) override      { return m_streamed;   } // RingBuffer
    bool IsSeekingAllowed(void) override { return m_allowSeeks; } // RingBuffer
    bool IsBookmarkAllowed(void) override { return false; } // RingBuffer

  protected:
    int safe_read(void *data, uint sz) override; // RingBuffer
    long long GetRealFileSizeInternal(void) const override; // RingBuffer
    long long SeekInternal(long long pos, int whence) override; // RingBuffer

  private:
    URLContext *m_context    {nullptr};
    bool        m_streamed   {true};
    bool        m_allowSeeks {false};
};

#endif // STREAMINGRINGBUFFER_H
