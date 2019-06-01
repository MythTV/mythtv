#ifndef MYTHVDPAUCONTEXT_H
#define MYTHVDPAUCONTEXT_H

// MythTV
#include "mythhwcontext.h"
#include "mythcodeccontext.h"

class MythVDPAUContext : public MythCodecContext
{
  public:
    explicit MythVDPAUContext(MythCodecID CodecID);
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
    MythCodecID m_codecID;
};

#endif // MYTHVDPAUCONTEXT_H
