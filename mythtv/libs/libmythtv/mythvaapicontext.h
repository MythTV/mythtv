#ifndef MYTHVAAPICONTEXT_H
#define MYTHVAAPICONTEXT_H

// MythTV
#include "mythtvexp.h" // for mythfrontend/globalsettings.cpp
#include "mythcodecid.h"

// VAAPI
#include "va/va.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
}

class MTV_PUBLIC MythVAAPIContext
{
  public:
    static MythCodecID GetSupportedCodec (AVCodecContext *CodecContext,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (struct AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);
    static bool HaveVAAPI                (bool ReCheck = false);

  private:
    static int  InitialiseContext        (AVCodecContext *Context);
    static VAProfile VAAPIProfileForCodec(const AVCodecContext *Codec);
};

#endif // MYTHVAAPICONTEXT_H
