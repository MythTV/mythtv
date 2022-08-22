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

using VTBProfiles = QList<MythCodecContext::CodecProfile>;

class MythVTBContext : public MythCodecContext
{
  public:
    MythVTBContext(DecoderBase *Parent, MythCodecID CodecID);
   ~MythVTBContext() override;

    // Shared decode only and direct rendering
    void   InitVideoCodec                (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    bool   RetrieveFrame                 (AVCodecContext* Context,
                                          MythVideoFrame* Frame,
                                          AVFrame* AvFrame) override;
    int    HwDecoderInit                 (AVCodecContext *Context) override;
    static MythCodecID GetSupportedCodec (AVCodecContext **Context,
                                          const AVCodec **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);
    static bool HaveVTB                  (bool Reinit = false);
    static void GetDecoderList           (QStringList &Decoders);

  private:
    static const VTBProfiles& GetProfiles(void);
    static int  InitialiseDecoder        (AVCodecContext *Context);
    void        InitFramesContext        (AVCodecContext *Context);
    AVBufferRef*  m_framesContext { nullptr };
};

#endif
