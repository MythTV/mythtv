#include "avfringbuffer.h"
#include "mythcorecontext.h"

bool AVFRingBuffer::s_avfrProtocolInitialised = false;
URLProtocol AVFRingBuffer::s_avfrURL;

int AVFRingBuffer::AVF_Open(URLContext *h, const char *filename, int flags)
{
    (void)filename;
    (void)flags;

    h->priv_data = nullptr;
    return 0;
}

int AVFRingBuffer::AVF_Read(URLContext *h, uint8_t *buf, int buf_size)
{
    auto *avfr = (AVFRingBuffer *)h->priv_data;
    if (!avfr)
        return 0;

    int ret = avfr->GetRingBuffer()->Read(buf, buf_size);

    if (ret == 0)
        ret = AVERROR_EOF;
    return ret;
}

int AVFRingBuffer::AVF_Write(URLContext *h, const uint8_t *buf, int buf_size)
{
    auto *avfr = (AVFRingBuffer *)h->priv_data;
    if (!avfr)
        return 0;

    return avfr->GetRingBuffer()->Write(buf, buf_size);
}

int64_t AVFRingBuffer::AVF_Seek(URLContext *h, int64_t offset, int whence)
{
    auto *avfr = (AVFRingBuffer *)h->priv_data;
    if (!avfr)
        return 0;

    if (whence == AVSEEK_SIZE)
        return avfr->GetRingBuffer()->GetRealFileSize();

    if (whence == SEEK_END)
        return avfr->GetRingBuffer()->GetRealFileSize() + offset;

    return avfr->GetRingBuffer()->Seek(offset, whence);
}

int AVFRingBuffer::AVF_Close(URLContext *h)
{
    (void)h;
    return 0;
}

int AVFRingBuffer::AVF_Write_Packet(void *opaque, uint8_t *buf, int buf_size)
{
    if (!opaque)
        return 0;

    return ffurl_write((URLContext *)opaque, buf, buf_size);
}

int AVFRingBuffer::AVF_Read_Packet(void *opaque, uint8_t *buf, int buf_size)
{
    if (!opaque)
        return 0;

    return ffurl_read((URLContext *)opaque, buf, buf_size);
}

int64_t AVFRingBuffer::AVF_Seek_Packet(void *opaque, int64_t offset, int whence)
{
    if (!opaque)
        return 0;

    return ffurl_seek((URLContext *)opaque, offset, whence);
}

URLProtocol *AVFRingBuffer::GetRingBufferURLProtocol(void)
{
    QMutexLocker lock(avcodeclock);
    if (!s_avfrProtocolInitialised)
    {
        // just in case URLProtocol's members do not have default constructor
        memset((void *)&s_avfrURL, 0, sizeof(s_avfrURL));
        s_avfrURL.name              = "rbuffer";
        s_avfrURL.url_open          = AVF_Open;
        s_avfrURL.url_read          = AVF_Read;
        s_avfrURL.url_write         = AVF_Write;
        s_avfrURL.url_seek          = AVF_Seek;
        s_avfrURL.url_close         = AVF_Close;
        s_avfrURL.priv_data_size    = 0;
        s_avfrURL.flags             = URL_PROTOCOL_FLAG_NETWORK;
        s_avfrProtocolInitialised   = true;
    }
    return &s_avfrURL;
}

void AVFRingBuffer::SetInInit(bool state)
{
    m_initState = state;

    GetRingBuffer()->SetReadInternalMode(state);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
