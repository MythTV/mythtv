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
    StreamingRingBuffer(const QString &lfilename);
    virtual ~StreamingRingBuffer();

    virtual bool IsOpen(void) const;
    virtual long long GetReadPosition(void) const;
    virtual bool OpenFile(const QString &lfilename,
                          uint retry_ms = kDefaultOpenTimeout);
    virtual bool IsStreamed(void)       { return m_streamed;   }
    virtual bool IsSeekingAllowed(void) { return m_allowSeeks; }
    virtual bool IsBookmarkAllowed(void) { return false; }

  protected:
    virtual int safe_read(void *data, uint sz);
    virtual long long GetRealFileSizeInternal(void) const;
    virtual long long SeekInternal(long long pos, int whence);

  private:
    URLContext *m_context;
    bool        m_streamed;
    bool        m_allowSeeks;
};

#endif // STREAMINGRINGBUFFER_H
