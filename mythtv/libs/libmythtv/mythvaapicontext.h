#ifndef MYTHVAAPICONTEXT_H
#define MYTHVAAPICONTEXT_H

// MythTV
#include "decoders/mythcodeccontext.h"

// VAAPI
#include "va/va.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersrc.h"
}

class MTV_PUBLIC MythVAAPIContext : public MythCodecContext
{
  public:
    MythVAAPIContext                     (DecoderBase *Parent, MythCodecID CodecID);
   ~MythVAAPIContext() override;

    void   InitVideoCodec                (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    bool   RetrieveFrame                 (AVCodecContext*, VideoFrame* Frame, AVFrame* AvFrame) override;
    int    FilteredReceiveFrame          (AVCodecContext *Context, AVFrame *Frame) override;
    void   PostProcessFrame              (AVCodecContext *Context, VideoFrame *Frame) override;
    bool   IsDeinterlacing               (bool &DoubleRate, bool StreamChange = false) override;
    bool   DecoderWillResetOnFlush       (void) override;
    bool   DecoderWillResetOnAspect      (void) override;

    static MythCodecID GetSupportedCodec (AVCodecContext **Context,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const AVPixelFormat *PixFmt);
    static enum AVPixelFormat GetFormat2 (AVCodecContext *Context,
                                          const AVPixelFormat *PixFmt);
    static bool HaveVAAPI                (bool ReCheck = false);

  private:
    static int  InitialiseContext        (AVCodecContext *Context);
    static int  InitialiseContext2       (AVCodecContext *Context);
    static VAProfile VAAPIProfileForCodec(const AVCodecContext *Codec);
    static AVPixelFormat FramesFormat    (AVPixelFormat Format);
    void        DestroyDeinterlacer      (void);

    MythDeintType    m_deinterlacer      { DEINT_NONE };
    bool             m_deinterlacer2x    { false      };
    int              m_lastInterlaced    { 0 };
    int              m_lastTopFieldFirst { 0 };
    AVFilterContext *m_filterSink        { nullptr };
    AVFilterContext *m_filterSource      { nullptr };
    AVFilterGraph   *m_filterGraph       { nullptr };
    bool             m_filterError       { false   };
    AVBufferRef     *m_framesCtx         { nullptr };
    int64_t          m_filterPriorPTS[2] { 0 };
    int64_t          m_filterPTSUsed     { 0 };
    int              m_filterWidth       { 0 };
    int              m_filterHeight      { 0 };
};

#endif // MYTHVAAPICONTEXT_H
