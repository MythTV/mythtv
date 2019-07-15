#ifndef MYTHMMALCONTEXT_H
#define MYTHMMALCONTEXT_H

// MythTV
#include "mythcodeccontext.h"

class MythMMALContext : public MythCodecContext
{
  public:
    MythMMALContext(DecoderBase *Parent, MythCodecID Codec);
    static MythCodecID GetSupportedCodec(AVCodecContext *Context,
                                         AVCodec **Codec,
                                         const QString &Decoder,
                                         uint StreamType,
                                         AVPixelFormat &PixFmt);
    bool        RetrieveFrame           (AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame) override;
    static bool GetBuffer               (AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame, int);
};

#endif // MYTHMMALCONTEXT_H
