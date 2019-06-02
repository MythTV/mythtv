#ifndef MYTHVAAPICONTEXT_H
#define MYTHVAAPICONTEXT_H

// MythTV
#include "decoders/mythcodeccontext.h"

// VAAPI
#include "va/va.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
}

class MTV_PUBLIC MythVAAPIContext : public MythCodecContext
{
  public:
    explicit MythVAAPIContext(MythCodecID CodecID);
    int    HwDecoderInit                 (AVCodecContext *Context) override;
    static MythCodecID GetSupportedCodec (AVCodecContext *Context,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          int             StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const AVPixelFormat *PixFmt);
    static enum AVPixelFormat GetFormat2 (AVCodecContext *Context,
                                          const AVPixelFormat *PixFmt);
    static bool HaveVAAPI                (bool ReCheck = false);

  private:
    static int  InitialiseContext        (AVCodecContext *Context);
    static VAProfile VAAPIProfileForCodec(const AVCodecContext *Codec);
    MythCodecID m_codecID;
};

#endif // MYTHVAAPICONTEXT_H
