#include "io/mythavformatbuffer.h"

#include <QtGlobal>
#include <QRecursiveMutex>

URLProtocol MythAVFormatBuffer::s_avfrURL;

MythAVFormatBuffer::MythAVFormatBuffer(MythMediaBuffer *Buffer)
  : m_buffer(Buffer)
{
}

void MythAVFormatBuffer::SetBuffer(MythMediaBuffer *Buffer)
{
    m_buffer = Buffer;
}

MythMediaBuffer* MythAVFormatBuffer::GetBuffer(void)
{
    return m_buffer;
}

int MythAVFormatBuffer::Open(URLContext *Context, const char* /*Filename*/, int /*Flags*/)
{
    Context->priv_data = nullptr;
    return 0;
}

int MythAVFormatBuffer::Read(URLContext *Context, uint8_t *Buffer, int Size)
{
    auto *avfr = reinterpret_cast<MythAVFormatBuffer*>(Context->priv_data);
    if (!avfr)
        return 0;

    int ret = avfr->GetBuffer()->Read(Buffer, Size);

    if (ret == 0)
        ret = AVERROR_EOF;
    return ret;
}

int MythAVFormatBuffer::Write(URLContext *Context, const uint8_t *Buffer, int Size)
{
    auto *avfr = reinterpret_cast<MythAVFormatBuffer*>(Context->priv_data);
    if (!avfr)
        return 0;

    return avfr->GetBuffer()->Write(Buffer, static_cast<uint>(Size));
}

int64_t MythAVFormatBuffer::Seek(URLContext *Context, int64_t Offset, int Whence)
{
    auto *avfr = reinterpret_cast<MythAVFormatBuffer*>(Context->priv_data);
    if (!avfr)
        return 0;

    if (Whence == AVSEEK_SIZE)
        return avfr->GetBuffer()->GetRealFileSize();

    if (Whence == SEEK_END)
        return avfr->GetBuffer()->GetRealFileSize() + Offset;

    return avfr->GetBuffer()->Seek(Offset, Whence);
}

int MythAVFormatBuffer::Close(URLContext* /*Context*/)
{
    return 0;
}

int MythAVFormatBuffer::WritePacket(void *Context, uint8_t *Buffer, int Size)
{
    if (!Context)
        return 0;
    return ffurl_write(reinterpret_cast<URLContext*>(Context), Buffer, Size);
}

int MythAVFormatBuffer::ReadPacket(void *Context, uint8_t *Buffer, int Size)
{
    if (!Context)
        return 0;
    return ffurl_read(reinterpret_cast<URLContext*>(Context), Buffer, Size);
}

int64_t MythAVFormatBuffer::SeekPacket(void *Context, int64_t Offset, int Whence)
{
    if (!Context)
        return 0;
    return ffurl_seek(reinterpret_cast<URLContext*>(Context), Offset, Whence);
}

URLProtocol *MythAVFormatBuffer::GetURLProtocol(void)
{
    static QRecursiveMutex s_avringbufferLock;
    static bool   s_avringbufferInitialised = false;

    QMutexLocker lock(&s_avringbufferLock);
    if (!s_avringbufferInitialised)
    {
        // just in case URLProtocol's members do not have default constructor
        memset(static_cast<void*>(&s_avfrURL), 0, sizeof(s_avfrURL));
        s_avfrURL.name            = "rbuffer";
        s_avfrURL.url_open        = Open;
        s_avfrURL.url_read        = Read;
        s_avfrURL.url_write       = Write;
        s_avfrURL.url_seek        = Seek;
        s_avfrURL.url_close       = Close;
        s_avfrURL.priv_data_size  = 0;
        s_avfrURL.flags           = URL_PROTOCOL_FLAG_NETWORK;
        s_avringbufferInitialised = true;
    }
    return &s_avfrURL;
}

void MythAVFormatBuffer::SetInInit(bool State)
{
    m_initState = State;
    GetBuffer()->SetReadInternalMode(State);
}

bool MythAVFormatBuffer::IsInInit(void) const
{
    return m_initState;
}
