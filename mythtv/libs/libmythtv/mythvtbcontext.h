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
    MythVTBContext(DecoderBase *Parent, MythCodecID CodecID);

    // Shared decode only and direct rendering
    void   InitVideoCodec                (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    bool   RetrieveFrame                 (AVCodecContext* Context,
                                          VideoFrame* Frame,
                                          AVFrame* AvFrame) override;
    int    HwDecoderInit                 (AVCodecContext *Context) override;
    static MythCodecID GetSupportedCodec (AVCodecContext **Context,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);

  private:
    static bool CheckDecoderSupport      (AVCodecContext **Context, uint StreamType);
    static int  InitialiseDecoder        (AVCodecContext *Context);
};

#endif
