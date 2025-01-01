#include "io/mythavformatbuffer.h"

// FFmpeg
extern "C" {
#include "libavformat/avio.h"
#include "libavutil/error.h"
}

#include <QtGlobal>
#include <QRecursiveMutex>

#include "libmythbase/mythlogging.h"

MythAVFormatBuffer::MythAVFormatBuffer(MythMediaBuffer *Buffer, bool write_flag, bool force_seek)
  : m_buffer(Buffer), m_avioContext(alloc_context(write_flag, force_seek))
{
    if (m_avioContext == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, "MythAVFormatBuffer failed to allocate an AVIOContext.");
    }
}

MythAVFormatBuffer::~MythAVFormatBuffer()
{
    if (m_avioContext != nullptr)
    {
        avio_flush(m_avioContext);
        av_freep(reinterpret_cast<void*>(&m_avioContext->buffer));
        if (m_avioContext->write_flag)
        {
            LOG(VB_LIBAV, LOG_DEBUG, QString("AVIOContext (%1): %2 bytes written")
                .arg(pointerToQString(m_avioContext), QString::number(m_avioContext->bytes_written)));
        }
        else
        {
            LOG(VB_LIBAV, LOG_DEBUG, QString("AVIOContext (%1): %2 bytes read")
                .arg(pointerToQString(m_avioContext), QString::number(m_avioContext->bytes_read)));
        }
        avio_context_free(&m_avioContext);
    }
}

AVIOContext* MythAVFormatBuffer::alloc_context(bool write_flag, bool force_seek)
{
    AVIOContext* context = nullptr;
    int buf_size = m_buffer->BestBufferSize();
    auto *avio_buffer = static_cast<unsigned char *>(av_malloc(buf_size));
    if (avio_buffer == nullptr)
    {
        LOG(VB_LIBAV, LOG_ERR, "av_malloc() failed to create a buffer for avio.");
        return nullptr;
    }
    context = avio_alloc_context(avio_buffer, buf_size, write_flag, this,
                                 read_packet, write_packet, seek);
    if (context == nullptr)
    {
        return context;
    }
    if (m_buffer->IsStreamed() && !force_seek)
    {
        context->seekable &= ~AVIO_SEEKABLE_NORMAL;
    }
    return context;
}

int MythAVFormatBuffer::read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    auto *p = reinterpret_cast<MythAVFormatBuffer*>(opaque);
    if (!p)
        return AVERROR(EINVAL);

    int ret = p->m_buffer->Read(buf, buf_size);

    if (ret == 0)
        ret = AVERROR_EOF;
    return ret;
}

int MythAVFormatBuffer::write_packet(void *opaque, const uint8_t *buf, int buf_size)
{
    auto *p = reinterpret_cast<MythAVFormatBuffer*>(opaque);
    if (!p)
        return AVERROR(EINVAL);

    return p->m_buffer->Write(buf, static_cast<uint>(buf_size));
}

int64_t MythAVFormatBuffer::seek(void *opaque, int64_t offset, int whence)
{
    auto *p = reinterpret_cast<MythAVFormatBuffer*>(opaque);
    if (!p)
        return AVERROR(EINVAL);

    if (whence == AVSEEK_SIZE)
        return p->m_buffer->GetRealFileSize();

    if (whence == SEEK_END)
        return p->m_buffer->Seek(p->m_buffer->GetRealFileSize() + offset, SEEK_SET);

    return p->m_buffer->Seek(offset, whence);
}

void MythAVFormatBuffer::SetInInit(bool State)
{
    m_initState = State;
    m_buffer->SetReadInternalMode(State);
}

bool MythAVFormatBuffer::IsInInit(void) const
{
    return m_initState;
}
