#ifndef _FFMPEGVIDEODECODER_H
#define _FFMPEGVIDEODECODER_H

#include "videodecoder.h"
#include "openclinterface.h"
#include "videosurface.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

class FFMpegVideoDecoder : public VideoDecoder
{
  public:
    FFMpegVideoDecoder(OpenCLDevice *dev) : VideoDecoder(dev, true) {};
    ~FFMpegVideoDecoder() { Shutdown(); };
    const char *Name(void) { return "FFMpeg"; };
    bool Initialize(void);
    VideoSurface *DecodeFrame(AVFrame *frame);
    void DiscardFrame(VideoSurface *frame);
    void Shutdown(void);
    void SetCodec(AVCodec *codec) { m_codecId = codec->id; };
};

int  get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
