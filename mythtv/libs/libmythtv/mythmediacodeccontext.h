#ifndef MYTHMEDIACODECCONTEXT_H
#define MYTHMEDIACODECCONTEXT_H

// MythTV
#include "mythhwcontext.h"
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
    MythMediaCodecContext();
    int HwDecoderInit(AVCodecContext *Context) override;

    static MythCodecID GetBestSupportedCodec(AVCodecContext *AvCtx,
                                             AVCodec       **Codec,
                                             const QString  &Decoder,
                                             uint            StreamType,
                                             AVPixelFormat  &PixFmt);
    static AVPixelFormat GetFormat          (AVCodecContext *AvCtx,
                                             const AVPixelFormat *PixFmt);

  private:
    static int  InitialiseDecoder           (AVCodecContext *Context);
};

#endif // MYTHMEDIACODECCONTEXT_H
