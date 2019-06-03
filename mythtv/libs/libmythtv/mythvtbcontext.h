#ifndef MYTHVTBCONTEXT_H
#define MYTHVTBCONTEXT_H

// MythTV
#include "mythcodecid.h"
#include "mythcodeccontext.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
}

class MythVTBContext : public MythCodecContext
{
  public:
    MythVTBContext(MythCodecID CodecID);

    // Shared decode only and direct rendering
    int         HwDecoderInit            (AVCodecContext *Context) override;
    static MythCodecID GetSupportedCodec (AVCodecContext *CodecContext,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);

  private:
    static bool CheckDecoderSupport      (uint StreamType, AVCodec **Codec);
    static int  InitialiseDecoder        (AVCodecContext *Context);
};

#endif
