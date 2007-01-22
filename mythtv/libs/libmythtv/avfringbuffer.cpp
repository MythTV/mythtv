#include "avfringbuffer.h"

int AVF_Open(URLContext *h, const char *filename, int flags)
{
    (void)h;
    (void)filename;
    (void)flags;
    return 0;
}   
    
int AVF_Read(URLContext *h, uint8_t *buf, int buf_size)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

    return avfr->GetRingBuffer()->Read(buf, buf_size);
}   
    
int AVF_Write(URLContext *h, uint8_t *buf, int buf_size)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;
  
    return avfr->GetRingBuffer()->Write(buf, buf_size);
}

offset_t AVF_Seek(URLContext *h, offset_t offset, int whence)
{
    AVFRingBuffer *avfr = (AVFRingBuffer *)h->priv_data;

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
    URLContext *h = (URLContext *)opaque;
    return url_write(h, buf, buf_size);
}

int AVF_Read_Packet(void *opaque, uint8_t *buf, int buf_size)
{
    URLContext *h = (URLContext *)opaque;
    return url_read(h, buf, buf_size);
}

offset_t AVF_Seek_Packet(void *opaque, int64_t offset, int whence)
{
    URLContext *h = (URLContext *)opaque;
    return url_seek(h, offset, whence);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
