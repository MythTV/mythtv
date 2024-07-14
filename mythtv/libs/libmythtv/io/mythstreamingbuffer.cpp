#include "io/mythstreamingbuffer.h"

// Qt
#include <QUrl>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

// FFmpeg
extern "C" {
#include "libavformat/avio.h"
}

#define LOC QString("StreamRingBuf(%1): ").arg(m_filename)

MythStreamingBuffer::MythStreamingBuffer(const QString &Filename)
  : MythMediaBuffer(kMythBufferHTTP)
{
    m_startReadAhead = false;
    MythStreamingBuffer::OpenFile(Filename);
}

MythStreamingBuffer::~MythStreamingBuffer()
{
    KillReadAheadThread();

    m_rwLock.lockForWrite();
    if (m_context)
        avio_closep(&m_context);
    m_rwLock.unlock();
}

bool MythStreamingBuffer::IsOpen(void) const
{
    m_rwLock.lockForRead();
    bool result = m_context;
    m_rwLock.unlock();
    return result;
}

long long MythStreamingBuffer::GetReadPosition(void) const
{
    return 0;
}

/** \fn StreamingRingBuffer::OpenFile(const QString &, uint)
 *  \brief Opens a network stream for reading.
 *
 *  \param lfilename   Url of the stream to read.
 *  \param retry_ms    Ignored. This value is part of the API
 *                     inherited from the parent class.
 *  \return Returns true if the stream was opened.
 */
bool MythStreamingBuffer::OpenFile(const QString &Filename, std::chrono::milliseconds /*Retry*/)
{
    MythMediaBuffer::AVFormatInitNetwork();

    m_rwLock.lockForWrite();

    m_safeFilename = Filename;
    m_filename = Filename;

    // TODO check whether local area file

    QUrl url = m_filename;
    if (url.path().endsWith(QLatin1String("m3u8"), Qt::CaseInsensitive))
        url.setScheme("hls+http");

    int res = avio_open(&m_context, url.toString().toLatin1(), AVIO_FLAG_READ);
    if (res < 0 || !m_context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open stream (%1) (error %2)")
            .arg(m_filename, QString::number(res)));
        m_lastError = QObject::tr("Failed to open stream (%1)").arg(res);
        m_rwLock.unlock();
        return false;
    }

    m_streamed   = (m_context->seekable & AVIO_SEEKABLE_NORMAL) == 0;
    m_allowSeeks = !m_streamed && avio_seek(m_context, 0, SEEK_SET) >= 0;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opened %1 (allow seeks: %2")
        .arg(m_filename, QVariant(m_allowSeeks).toString()));

    m_rwLock.unlock();
    return true;
}

long long MythStreamingBuffer::SeekInternal(long long Position, int Whence)
{
    if (!m_context)
        return 0;

    m_posLock.lockForWrite();
    int64_t seek = avio_seek(m_context, Position, Whence);
    m_posLock.unlock();

    if (seek < 0)
    {
        m_ateof = true;
        return 0;
    }
    return seek;
}

int MythStreamingBuffer::SafeRead(void *Buffer, uint Size)
{
    int len = 0;

    if (m_context)
    {
        while (len < static_cast<int>(Size))
        {
            int ret = avio_read(m_context, static_cast<unsigned char*>(Buffer) + len,
                                 static_cast<int>(Size) - len);
            if (ret < 0)
            {
                if (ret == AVERROR_EOF)
                    m_ateof = true;
                errno = ret;
                break;
            }
            if (ret == 0) // nothing read, exit early
                break;    // should EOF be set ??
            len += ret;
        }
    }
    return len;
}

long long MythStreamingBuffer::GetRealFileSizeInternal(void) const
{
    long long result = -1;
    m_rwLock.lockForRead();
    if (m_context)
        result = avio_size(m_context);
    m_rwLock.unlock();
    return result;
}
