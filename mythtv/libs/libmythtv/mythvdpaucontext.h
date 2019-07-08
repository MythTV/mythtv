#ifndef MYTHVDPAUCONTEXT_H
#define MYTHVDPAUCONTEXT_H

// MythTV
#include "mythcodeccontext.h"

class MythVDPAUContext : public MythCodecContext
{
  public:
    MythVDPAUContext(DecoderBase *Parent, MythCodecID CodecID);

    bool   RetrieveFrame                 (AVCodecContext*, VideoFrame* Frame, AVFrame* AvFrame) override;
    static MythCodecID GetSupportedCodec (AVCodecContext *CodecContext,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);
    static enum AVPixelFormat GetFormat2 (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);

  private:
    static int  InitialiseContext        (AVCodecContext *Context);
};

#endif // MYTHVDPAUCONTEXT_H
