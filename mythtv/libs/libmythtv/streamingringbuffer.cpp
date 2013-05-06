#include <QUrl>
#include "mythcorecontext.h"
#include "mythlogging.h"

#include "streamingringbuffer.h"

#define LOC QString("StreamRingBuf(%1): ").arg(filename)

StreamingRingBuffer::StreamingRingBuffer(const QString &lfilename)
  : RingBuffer(kRingBuffer_HTTP), m_context(NULL), m_streamed(true),
    m_allowSeeks(false)
{
    startreadahead = false;
    OpenFile(lfilename);
}

StreamingRingBuffer::~StreamingRingBuffer()
{
    KillReadAheadThread();

    rwlock.lockForWrite();
    if (m_context)
        ffurl_close(m_context);
    rwlock.unlock();
}

bool StreamingRingBuffer::IsOpen(void) const
{
    bool result;
    rwlock.lockForRead();
    result = (bool)m_context;
    rwlock.unlock();
    return result;
}

long long StreamingRingBuffer::GetReadPosition(void) const
{
    return 0;
}

bool StreamingRingBuffer::OpenFile(const QString &lfilename, uint retry_ms)
{
    avcodeclock->lock();
    av_register_all();
    avcodeclock->unlock();
    RingBuffer::AVFormatInitNetwork();

    rwlock.lockForWrite();

    safefilename = lfilename;
    filename = lfilename;

    // TODO check whether local area file

    QUrl url = filename;
    if (url.path().endsWith(QLatin1String("m3u8"), Qt::CaseInsensitive))
    {
        url.setScheme("hls+http");
    }

    int res = ffurl_open(&m_context, url.toString().toLatin1(), AVIO_FLAG_READ,
                         NULL, NULL);
    if (res >=0 && m_context &&
        !m_context->is_streamed && ffurl_seek(m_context, 0, SEEK_SET) >= 0)
    {
        m_streamed   = false;
        m_allowSeeks = true;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Trying %1 (allow seeks: %2")
        .arg(filename).arg(m_allowSeeks));

    rwlock.unlock();

    if (res < 0 || !m_context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open stream (error %1)") .arg(res));
        return false;
    }

    return true;
}

long long StreamingRingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    if (!m_context)
        return 0;

    poslock.lockForWrite();
    int seek = ffurl_seek(m_context, pos, whence);
    poslock.unlock();

    if (seek < 0)
    {
        ateof = true;
        return 0;
    }
    return pos;
}

int StreamingRingBuffer::safe_read(void *data, uint sz)
{
    uint len = 0;

    if (m_context)
    {
        while (len < sz)
        {
            int ret = ffurl_read(m_context, (unsigned char*)data + len, sz - len);
            if (ret < 0)
            {
                if (ret == AVERROR_EOF)
                {
                    ateof = true;
                }
                errno = ret;
                break;
            }
            len += ret;
        }
    }
    return len;
}

long long StreamingRingBuffer::GetRealFileSize(void) const
{
    long long result = -1;
    rwlock.lockForRead();
    if (m_context)
        result = ffurl_size(m_context);
    rwlock.unlock();
    return result;
}
