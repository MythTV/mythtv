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
#include "compat/cuda/dynlink_loader.h"
}

// Std
#include <vector>

class MythNVDECContext : public MythCodecContext
{
  public:
    explicit MythNVDECContext(MythCodecID CodecID);
    int  HwDecoderInit                   (AVCodecContext *Context) override;
    void SetDeinterlacing                (AVCodecContext *Context,
                                          VideoDisplayProfile *Profile, bool DoubleRate) override;
    void PostProcessFrame                (AVCodecContext *Context, VideoFrame *Frame) override;
    bool IsDeinterlacing                 (bool &DoubleRate, bool = false) override;
    static MythCodecID GetSupportedCodec (AVCodecContext *CodecContext,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Contextconst, const AVPixelFormat *PixFmt);
    static int  GetBuffer                (AVCodecContext *Context,
                                          AVFrame *Frame, int Flags);
    static int  InitialiseDecoder        (AVCodecContext *Context);
    static bool HaveNVDEC                (void);

  private:
    class MythNVDECCaps
    {
      public:
        MythNVDECCaps(cudaVideoCodec Codec, uint Depth, cudaVideoChromaFormat Format,
                      QSize Minimum, QSize Maximum, uint MacroBlocks);

        cudaVideoCodec m_codec;
        uint           m_depth;
        cudaVideoChromaFormat m_format;
        QSize          m_minimum;
        QSize          m_maximum;
        uint           m_macroBlocks;
    };

    static QMutex*     s_NVDECLock;
    static bool        s_NVDECAvailable;
    static std::vector<MythNVDECCaps> s_NVDECDecoderCaps;
    static void   NVDECCheck             (void);
    static void   NVDECCheckCallback     (void *Wait, void*, void*);

  private:
    MythDeintType m_deinterlacer         { DEINT_NONE  };
    bool          m_deinterlacer2x       { false       };
};

#endif // MYTHNVDECCONTEXT_H
