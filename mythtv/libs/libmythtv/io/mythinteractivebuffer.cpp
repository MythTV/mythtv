// Std
#include <cstdio>

// Qt
#include <QWriteLocker>

// Mythtv
#include "libmythbase/mythlogging.h"

#include "io/mythinteractivebuffer.h"
#include "mheg/netstream.h"

#define LOC QString("InteractiveBuf: ")

MythInteractiveBuffer::MythInteractiveBuffer(const QString &Url, MythMediaBuffer *Parent)
  : MythMediaBuffer(kMythBufferMHEG),
    m_parent(Parent)
{
    m_startReadAhead = true;
    MythInteractiveBuffer::OpenFile(Url);
}

MythInteractiveBuffer::~MythInteractiveBuffer()
{
    KillReadAheadThread();
    delete m_stream;
    delete m_parent;
}

bool MythInteractiveBuffer::IsOpen(void) const
{
    return m_stream ? m_stream->IsOpen() : false;
}

/** \fn MythInteractiveBuffer::OpenFile(const QString &, uint)
 *  \brief Opens a BBC NetStream for reading.
 *
 *  \param Url   Url of the stream to read.
 *  \return Returns true if the stream was opened.
 */
bool MythInteractiveBuffer::OpenFile(const QString &Url, std::chrono::milliseconds /*Retry*/)
{
    if (!NetStream::IsSupported(Url))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported URL '%1'").arg(Url));
        return false;
    }

    std::unique_ptr<NetStream> stream(new NetStream(Url, NetStream::kNeverCache));
    if (!stream || !stream->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open '%1'").arg(Url));
        return false;
    }

    if (!stream->WaitTillReady(30s))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Stream not ready '%1'").arg(Url));
        return false;
    }

    if (m_parent)
        m_parent->Pause();

    QWriteLocker locker(&m_rwLock);

    m_safeFilename = Url;
    m_filename = Url;

    delete m_stream;
    m_stream = stream.release();

    // The initial bitrate needs to be set with consideration for low bit rate
    // streams (e.g. radio @ 64Kbps) such that fill_min bytes are received
    // in a reasonable time period to enable decoders to peek the first few KB
    // to determine type & settings.
    m_rawBitrate = 128; // remotefile
    CalcReadAheadThresh();

    locker.unlock();
    Reset(true, false, true);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opened '%1'").arg(Url));
    return true;
}

long long MythInteractiveBuffer::GetReadPosition(void) const
{
    return m_stream ? m_stream->GetReadPosition() : 0;
}

long long MythInteractiveBuffer::SeekInternal(long long Position, int Whence)
{
    long long result = -1;
    if (!m_stream)
        return result;

    QWriteLocker locker(&m_posLock);

    // Optimize no-op seeks
    if (m_readAheadRunning && ((Whence == SEEK_SET && Position == m_readPos) ||
                               (Whence == SEEK_CUR && Position == 0)))
    {
        result = m_readPos;
        return result;
    }

    switch (Whence)
    {
        case SEEK_SET: break;
        case SEEK_CUR:
            Position += m_stream->GetReadPosition();
            break;
        case SEEK_END:
            Position += m_stream->GetSize();
            break;
        default:
            errno = EINVAL;
            m_generalWait.wakeAll();
            return result;
    }

    result = m_stream->Seek(Position);
    if (result >= 0)
    {
        m_readPos = result;
        m_ignoreReadPos = -1;
        if (m_readAheadRunning)
            ResetReadAhead(m_readPos);
        m_readAdjust = 0;
    }

    m_generalWait.wakeAll();
    return result;
}

int MythInteractiveBuffer::SafeRead(void *Buffer, uint Size)
{
    if (m_stream)
        return m_stream->safe_read(Buffer, Size, 1000);
    m_ateof = true;
    return 0;
}

long long MythInteractiveBuffer::GetRealFileSizeInternal(void) const
{
    return m_stream ? m_stream->GetSize() : -1;
}

MythMediaBuffer *MythInteractiveBuffer::TakeBuffer(void)
{
    MythMediaBuffer *parent = m_parent;
    if (parent && IsOpen())
        parent->Unpause();
    m_parent = nullptr;
    return parent;
}
