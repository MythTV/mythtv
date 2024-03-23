#ifndef MYTHDRMPRIMECONTEXT_H
#define MYTHDRMPRIMECONTEXT_H

// MythTV
#ifdef USING_EGL
#include "opengl/mythdrmprimeinterop.h"
#else
#include "opengl/mythopenglinterop.h"
#endif
#include "decoders/mythcodeccontext.h"

class MythDRMPRIMEInterop;

class MythDRMPRIMEContext : public MythCodecContext
{
  public:
    MythDRMPRIMEContext(DecoderBase *Parent, MythCodecID CodecID);
   ~MythDRMPRIMEContext() override;

    static MythCodecID GetSupportedCodec (AVCodecContext **Context,
                                          const AVCodec **Codec,
                                          const QString  &Decoder,
                                          AVStream       *Stream,
                                          uint            StreamType);
    int  HwDecoderInit           (AVCodecContext *Context) override;
    void InitVideoCodec          (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    bool RetrieveFrame           (AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame) override;
    bool DecoderWillResetOnFlush (void) override;
    static bool  HavePrimeDecoders       (bool Reinit = false, AVCodecID Codec = AV_CODEC_ID_NONE);
    static enum  AVPixelFormat GetFormat (AVCodecContext *Context, const AVPixelFormat *PixFmt);
    bool         GetDRMBuffer            (AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame, int /*unused*/);

  protected:
    static MythCodecID GetPrimeCodec     (AVCodecContext **Context,
                                          const AVCodec **Codec,
                                          AVStream       *Stream,
                                          MythCodecID     Successs, // Xlib conflict
                                          MythCodecID     Failure,
                                          const QString  &CodecName,
                                          AVPixelFormat   Format);
    static QRecursiveMutex s_drmPrimeLock;
    static QStringList   s_drmPrimeDecoders;
    MythOpenGLInterop   *m_interop { nullptr };
};

#endif // MYTHDRMPRIMECONTEXT_H
