#ifndef MYTHMEDIACODECCONTEXT_H
#define MYTHMEDIACODECCONTEXT_H

// MythTV
#include "mythcodeccontext.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
}

class MythMediaCodecContext : public MythCodecContext
{
  public:
    // MythCodecContext
    MythMediaCodecContext(DecoderBase *Parent, MythCodecID CodecID);
    void InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    int HwDecoderInit(AVCodecContext *Context) override;
    bool RetrieveFrame(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame) override;

    static MythCodecID GetBestSupportedCodec(AVCodecContext **Context,
                                             AVCodec       **Codec,
                                             const QString  &Decoder,
                                             AVStream       *Stream,
                                             uint            StreamType);
    static AVPixelFormat GetFormat          (AVCodecContext*, const AVPixelFormat *PixFmt);
    void   PostProcessFrame                 (AVCodecContext*, VideoFrame*) override;
    bool   IsDeinterlacing                  (bool &DoubleRate, bool = false) override;

  private:
    static int  InitialiseDecoder           (AVCodecContext *Context);
};

#endif // MYTHMEDIACODECCONTEXT_H
