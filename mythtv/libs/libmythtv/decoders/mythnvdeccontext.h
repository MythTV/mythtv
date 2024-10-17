#ifndef MYTHNVDECCONTEXT_H
#define MYTHNVDECCONTEXT_H

// Qt
#include <QSize>

// MythTV
#include "mythframe.h"
#include "mythcodecid.h"
#include "mythcodeccontext.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"
#include <ffnvcodec/dynlink_cuda.h>
#include <ffnvcodec/dynlink_cuviddec.h>
}

// Std
#include <vector>

class MythNVDECContext : public MythCodecContext
{
  public:
    MythNVDECContext(DecoderBase *Parent, MythCodecID CodecID);
   ~MythNVDECContext() override;
    void InitVideoCodec                  (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    int  HwDecoderInit                   (AVCodecContext *Context) override;
    bool RetrieveFrame                   (AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame) override;
    void SetDeinterlacing                (AVCodecContext *Context,
                                          MythVideoProfile *Profile, bool DoubleRate) override;
    void PostProcessFrame                (AVCodecContext *Context, MythVideoFrame *Frame) override;
    bool IsDeinterlacing                 (bool &DoubleRate, bool StreamChange = false) override;
    bool DecoderWillResetOnFlush         (void) override;
    static MythCodecID GetSupportedCodec (AVCodecContext **CodecContext,
                                          const AVCodec **Codec,
                                          const QString  &Decoder,
                                          AVStream       *Stream,
                                          uint            StreamType);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context, const AVPixelFormat *PixFmt);
    static bool GetBuffer                (AVCodecContext *Context, MythVideoFrame *Frame,
                                          AVFrame *AvFrame, int Flags);
    static int  InitialiseDecoder        (AVCodecContext *Context);
    static bool HaveNVDEC                (bool Reinit = false);
    static void GetDecoderList           (QStringList &Decoders);

  private:
    class MythNVDECCaps
    {
      public:
        MythNVDECCaps(cudaVideoCodec Codec, uint Depth, cudaVideoChromaFormat Format,
                      QSize Minimum, QSize Maximum, uint MacroBlocks);
        bool Supports(cudaVideoCodec Codec, cudaVideoChromaFormat Format, uint Depth,
                      int Width, int Height) const;

        MythCodecContext::CodecProfile m_profile { MythCodecContext::NoProfile };
        VideoFrameType m_type           { FMT_NONE };
        cudaVideoCodec m_codec          { cudaVideoCodec_NumCodecs };
        uint           m_depth          { 0 };
        cudaVideoChromaFormat m_format  { cudaVideoChromaFormat_Monochrome };
        QSize          m_minimum;
        QSize          m_maximum;
        uint           m_macroBlocks    { 0 };
    };

  private:
    void          InitFramesContext(AVCodecContext *Context);
    AVBufferRef*  m_framesContext { nullptr };

    static const std::vector<MythNVDECCaps>& GetProfiles(void);
    MythDeintType m_deinterlacer         { DEINT_NONE  };
    bool          m_deinterlacer2x       { false       };
};

#endif // MYTHNVDECCONTEXT_H
