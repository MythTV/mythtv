#ifndef STREAMINGRINGBUFFER_H
#define STREAMINGRINGBUFFER_H

#include "ringbuffer.h"

extern "C" {
#include "libavformat/avformat.h"
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
    virtual long long Seek(long long pos, int whence, bool has_lock);
    virtual long long GetRealFileSize(void);
    virtual bool IsStreamed(void) { return true; }

  protected:
    virtual int safe_read(void *data, uint sz);

  private:
    URLContext *m_context;
};

#endif // STREAMINGRINGBUFFER_H
