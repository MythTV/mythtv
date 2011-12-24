#include "ffmpegvideodecoder.h"
#include "videoconsumer.h"
#include "frame.h"
#include "videosurface.h"

extern "C" {
#include "libavcodec/avcodec.h"
}


bool FFMpegVideoDecoder::Initialize(void)
{
    return true;
}

VideoSurface *FFMpegVideoDecoder::DecodeFrame(AVFrame *frame)
{
    return NULL;
}

void FFMpegVideoDecoder::DiscardFrame(VideoSurface *frame)
{
}

void FFMpegVideoDecoder::Shutdown(void)
{
}

static bool IS_DR1_PIX_FMT(const enum PixelFormat fmt)
{
    switch (fmt)
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUVJ420P:
            return true;
        default:
            return false;
    }
}


int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    VideoConsumer *nd = (VideoConsumer *)(c->opaque);
    (void)nd;

    if (!IS_DR1_PIX_FMT(c->pix_fmt))
    {
        return avcodec_default_get_buffer(c, pic);
    }

    VideoFrame *frame = NULL; //= nd->GetNextVideoFrame();

    if (!frame)
        return -1;

    for (int i = 0; i < 3; i++)
    {
        pic->data[i]     = frame->buf + frame->offsets[i];
        pic->linesize[i] = frame->pitches[i];
    }

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    pic->reordered_opaque = c->reordered_opaque;

    return 0;
}

void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;

    if (pic->type == FF_BUFFER_TYPE_INTERNAL)
    {
        avcodec_default_release_buffer(c, pic);
        return;
    }

    VideoConsumer *nd = (VideoConsumer *)(c->opaque);
    (void)nd;

    if(pic->type != FF_BUFFER_TYPE_USER);
        return;

    for (uint i = 0; i < 4; i++)
        pic->data[i] = NULL;
}



/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
