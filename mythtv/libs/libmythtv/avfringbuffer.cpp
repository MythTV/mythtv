#include "avfringbuffer.h"

int AVF_Open(URLContext *h, const char *filename, int flags)
{
    (void)filename;
    (void)flags;

    h->priv_data = NULL;
    return 0;
}

int AVF_Read(URLContext *h, uint8_t *buf, int buf_size)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

    if (!avfr)
        return 0;

    return avfr->GetRingBuffer()->Read(buf, buf_size);
}

int AVF_Write(URLContext *h, const uint8_t *buf, int buf_size)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

    if (!avfr)
        return 0;

    return avfr->GetRingBuffer()->Write(buf, buf_size);
}

int64_t AVF_Seek(URLContext *h, int64_t offset, int whence)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

    if (!avfr)
        return 0;

    if (whence == AVSEEK_SIZE)
        return avfr->GetRingBuffer()->GetRealFileSize();

    if (whence == SEEK_END)
        return avfr->GetRingBuffer()->GetRealFileSize() + offset;

    return avfr->GetRingBuffer()->Seek(offset, whence);
}

int AVF_Close(URLContext *h)
{
    (void)h;
    return 0;
}

URLProtocol AVF_RingBuffer_Protocol = {
    "rbuffer",
    AVF_Open,
    AVF_Read,
    AVF_Write,
    AVF_Seek,
    AVF_Close,
    NULL
};

int AVF_Write_Packet(void *opaque, uint8_t *buf, int buf_size)
{
    if (!opaque)
        return 0;

    return url_write((URLContext *)opaque, buf, buf_size);
}

int AVF_Read_Packet(void *opaque, uint8_t *buf, int buf_size)
{
    if (!opaque)
        return 0;

    return url_read((URLContext *)opaque, buf, buf_size);
}

int64_t AVF_Seek_Packet(void *opaque, int64_t offset, int whence)
{
    if (!opaque)
        return 0;

    return url_seek((URLContext *)opaque, offset, whence);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
