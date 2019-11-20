#ifndef MYTHDRMPRIMECONTEXT_H
#define MYTHDRMPRIMECONTEXT_H

// MythTV
#include "mythcodeccontext.h"

class MythDRMPRIMEInterop;

class MythDRMPRIMEContext : public MythCodecContext
{
  public:
    MythDRMPRIMEContext(DecoderBase *Parent, MythCodecID CodecID);
   ~MythDRMPRIMEContext() override;

    static MythCodecID GetSupportedCodec (AVCodecContext **Context,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          AVStream       *Stream,
                                          uint            StreamType);
    virtual int  HwDecoderInit           (AVCodecContext *Context) override;
    virtual void InitVideoCodec          (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    virtual bool RetrieveFrame           (AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame) override;
    virtual bool DecoderWillResetOnFlush (void) override;
    static bool  HavePrimeDecoders       (AVCodecID Codec = AV_CODEC_ID_NONE);
    static enum  AVPixelFormat GetFormat (AVCodecContext*, const AVPixelFormat *PixFmt);
    bool         GetDRMBuffer            (AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame, int);

  protected:
    static MythCodecID GetPrimeCodec     (AVCodecContext **Context,
                                          AVCodec       **Codec,
                                          AVStream       *Stream,
                                          MythCodecID     Successs, // Xlib conflict
                                          MythCodecID     Failure,
                                          const QString  &CodecName,
                                          AVPixelFormat   Format);
    static QMutex        s_drmPrimeLock;
    static QStringList   s_drmPrimeDecoders;
    MythDRMPRIMEInterop *m_interop { nullptr };
};

#endif // MYTHDRMPRIMECONTEXT_H
