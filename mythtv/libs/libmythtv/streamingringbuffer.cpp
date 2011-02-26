#include "mythverbose.h"

#include "streamingringbuffer.h"

#define LOC QString("StreamRingBuf(%1): ").arg(filename)

StreamingRingBuffer::StreamingRingBuffer(const QString &lfilename)
  : m_context(NULL)
{
    OpenFile(lfilename);
}

StreamingRingBuffer::~StreamingRingBuffer()
{
    if (m_context)
        url_close(m_context);
}

bool StreamingRingBuffer::IsOpen(void) const
{
    return m_context;
}

long long StreamingRingBuffer::GetReadPosition(void) const
{
    return 0;
}

bool StreamingRingBuffer::OpenFile(const QString &lfilename, uint retry_ms)
{
    av_register_all();

    filename = lfilename;

    int res = url_open(&m_context, filename.toAscii(), URL_RDONLY);

    if (res < 0 || !m_context)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("Failed to open stream (error %1)")
            .arg(res));
        return false;
    }

    return true;
}

long long StreamingRingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    return 0;
}

int StreamingRingBuffer::safe_read(void *data, uint sz)
{
    if (m_context)
        return url_read(m_context, (unsigned char*)data, sz);
    return 0;
}

long long StreamingRingBuffer::GetRealFileSize(void)
{
    if (m_context)
        return url_filesize(m_context);
    return -1;
}
