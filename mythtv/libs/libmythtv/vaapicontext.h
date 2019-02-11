#ifndef VAAPICONTEXT_H
#define VAAPICONTEXT_H

// MythTV
#include "referencecounter.h"
#include "mythcodecid.h"

// VAAPI
#include "va/va.h"
#include "va/va_version.h"
#if VA_CHECK_VERSION(0,34,0)
#include "va/va_compat.h"
#endif
#include "va/va_glx.h"

class MythRenderOpenGL;

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
}

#define NUM_VAAPI_BUFFERS 24

class MythVAAPIDisplay : public ReferenceCounter
{
  public:
    explicit    MythVAAPIDisplay(MythRenderOpenGL *Context);
    static void MythVAAPIDisplayDestroy(MythRenderOpenGL *Context, VADisplay Display);
    static void MythVAAPIDisplayDestroyCallback(void* Context, void* Wait, void *Display);

    MythRenderOpenGL *m_context;
    VADisplay         m_vaDisplay;

  protected:
   ~MythVAAPIDisplay();
};

class VAAPIContext
{
  public:
    VAAPIContext();
   ~VAAPIContext();
    static MythCodecID GetBestSupportedCodec(AVCodecContext *CodecContext,
                                             AVCodec **Codec,
                                             const QString &Decoder,
                                             uint StreamType,
                                             AVPixelFormat &PixFmt);
    static void HWFramesContextFinished(struct AVHWFramesContext *Context);
    static void HWDeviceContextFinished(struct AVHWDeviceContext *Context);
    static void InitVAAPIContext(AVCodecContext *Context);
    static int  HwDecoderInit(AVCodecContext *Context);
    static void HWDecoderInitCallback(void*, void* Wait, void *Data);

  private:
    static VAProfile VAAPIProfileForCodec(const AVCodecContext *Codec);
};

#endif // VAAPICONTEXT_H
