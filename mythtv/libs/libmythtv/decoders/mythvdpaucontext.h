#ifndef MYTHVDPAUCONTEXT_H
#define MYTHVDPAUCONTEXT_H

// MythTV
#include "mythcodeccontext.h"

class MythVDPAUContext : public MythCodecContext
{
  public:
    MythVDPAUContext(DecoderBase *Parent, MythCodecID CodecID);

    void   InitVideoCodec                (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    bool   RetrieveFrame                 (AVCodecContext *Context, MythVideoFrame* Frame, AVFrame* AvFrame) override;
    bool   DecoderWillResetOnFlush       (void) override;
    bool   DecoderWillResetOnAspect      (void) override;
    bool   DecoderNeedsReset             (AVCodecContext *Context) override;

    static MythCodecID GetSupportedCodec (AVCodecContext **CodecContext,
                                          const AVCodec **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);
    static enum AVPixelFormat GetFormat2 (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);

  private:
    static int  InitialiseContext        (AVCodecContext *Context);
};

#endif // MYTHVDPAUCONTEXT_H
