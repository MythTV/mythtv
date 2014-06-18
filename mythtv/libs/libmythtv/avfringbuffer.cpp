#include "avfringbuffer.h"
#include "mythcorecontext.h"

bool AVFRingBuffer::m_avrprotocol_initialised = false;
URLProtocol AVFRingBuffer::m_avfrURL;

int AVFRingBuffer::AVF_Open(URLContext *h, const char *filename, int flags)
{
    (void)filename;
    (void)flags;

    h->priv_data = NULL;
    return 0;
}

int AVFRingBuffer::AVF_Read(URLContext *h, uint8_t *buf, int buf_size)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

    if (!avfr)
        return AVERROR(EBADF);

    int ret = avfr->GetRingBuffer()->Read(buf, buf_size);

    return (ret < 0) ? AVERROR(errno) : ret;
}

int AVFRingBuffer::AVF_Write(URLContext *h, const uint8_t *buf, int buf_size)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

    if (!avfr)
        return AVERROR(EBADF);

    int ret = avfr->GetRingBuffer()->Write(buf, buf_size);

    return (ret < 0) ? AVERROR(errno) : ret;
}

int64_t AVFRingBuffer::AVF_Seek(URLContext *h, int64_t offset, int whence)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

    if (!avfr)
        return AVERROR(EBADF);

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
    if (!m_avrprotocol_initialised)
    {
        // just in case URLProtocol's members do not have default constructor
        memset((void *)&m_avfrURL, 0, sizeof(m_avfrURL));
        m_avfrURL.name              = "rbuffer";
        m_avfrURL.url_open          = AVF_Open;
        m_avfrURL.url_read          = AVF_Read;
        m_avfrURL.url_write         = AVF_Write;
        m_avfrURL.url_seek          = AVF_Seek;
        m_avfrURL.url_close         = AVF_Close;
        m_avfrURL.priv_data_size    = 0;
        m_avfrURL.flags             = URL_PROTOCOL_FLAG_NETWORK;
        m_avrprotocol_initialised   = true;
    }
    return &m_avfrURL;
}

void AVFRingBuffer::SetInInit(bool state)
{
    m_initState = state;

    GetRingBuffer()->SetReadInternalMode(state);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
