#include "icringbuffer.h"

#include <stdio.h> // SEEK_SET

#include <QScopedPointer>
#include <QWriteLocker>

#include "netstream.h"
#include "mythlogging.h"


#define LOC QString("ICRingBuf ")


ICRingBuffer::ICRingBuffer(const QString &url, RingBuffer *parent)
  : RingBuffer(kRingBufferType), m_stream(NULL), m_parent(parent)
{
    startreadahead = true;
    OpenFile(url);
}

ICRingBuffer::~ICRingBuffer()
{
    KillReadAheadThread();

    delete m_stream;
    m_stream = NULL;

    delete m_parent;
    m_parent = NULL;
}

bool ICRingBuffer::IsOpen(void) const
{
    return m_stream ? m_stream->IsOpen() : false;
}

bool ICRingBuffer::OpenFile(const QString &url, uint retry_ms)
{
    if (!NetStream::IsSupported(url))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported URL %1").arg(url) );
        return false;
    }

    QScopedPointer<NetStream> stream(new NetStream(url, NetStream::kNeverCache));
    if (!stream || !stream->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open %1").arg(url) );
        return false;
    }

    if (!stream->WaitTillReady(30000))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Stream not ready%1").arg(url) );
        return false;
    }

    if (m_parent)
        m_parent->Pause();

    QWriteLocker locker(&rwlock);

    safefilename = url;
    filename = url;

    delete m_stream;
    m_stream = stream.take();

    // The initial bitrate needs to be set with consideration for low bit rate
    // streams (e.g. radio @ 64Kbps) such that fill_min bytes are received
    // in a reasonable time period to enable decoders to peek the first few KB
    // to determine type & settings.
    rawbitrate = 128; // remotefile
    CalcReadAheadThresh();

    locker.unlock();
    Reset(true, false, true);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opened %1").arg(url));
    return true;
}

long long ICRingBuffer::GetReadPosition(void) const
{
    return m_stream ? m_stream->GetReadPosition() : 0;
}

long long ICRingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    if (!m_stream)
        return -1;

    // lockForWrite takes priority over lockForRead, so this will
    // take priority over the lockForRead in the read ahead thread.
    if (!has_lock)
        rwlock.lockForWrite();

    poslock.lockForWrite();

    long long ret;

    // Optimize no-op seeks
    if (readaheadrunning &&
        ((whence == SEEK_SET && pos == readpos) ||
         (whence == SEEK_CUR && pos == 0)))
    {
        ret = readpos;

        poslock.unlock();
        if (!has_lock)
            rwlock.unlock();

        return ret;
    }

    switch (whence)
    {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            pos += m_stream->GetReadPosition();
            break;
        case SEEK_END:
            pos += m_stream->GetSize();
            break;
        default:
            errno = EINVAL;
            ret = -1;
            goto err;
    }

    ret = m_stream->Seek(pos);
    if (ret >= 0)
    {
        readpos = ret;

        ignorereadpos = -1;

        if (readaheadrunning)
            ResetReadAhead(readpos);

        readAdjust = 0;
    }

err:
    poslock.unlock();

    generalWait.wakeAll();

    if (!has_lock)
        rwlock.unlock();

    return ret;
}

int ICRingBuffer::safe_read(void *data, uint sz)
{
    return m_stream ? m_stream->safe_read(data, sz, 1000) : (ateof = true, 0);
}

long long ICRingBuffer::GetRealFileSize(void) const
{
    return m_stream ? m_stream->GetSize() : -1;
}

// Take ownership of parent RingBuffer
RingBuffer *ICRingBuffer::Take()
{
    RingBuffer *parent = m_parent;
    if (parent && IsOpen())
        parent->Unpause();
    m_parent = 0;
    return parent;
}

// End of file
