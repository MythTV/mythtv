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

#define INIT_ST \
  VAStatus va_status; \
  bool ok = true

#define CHECK_ST \
  ok &= (va_status == VA_STATUS_SUCCESS); \
  if (!ok) \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(va_status) \
              .arg(vaErrorStr(va_status)))

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
    static MythCodecID GetBestSupportedCodec(AVCodec **Codec,
                                             const QString &Decoder,
                                             uint StreamType,
                                             AVPixelFormat &PixFmt);
    static void HWFramesContextFinished(struct AVHWFramesContext *Context);
    static void HWDeviceContextFinished(struct AVHWDeviceContext *Context);
    static void InitVAAPIContext(AVCodecContext *Context);
    static int  HwDecoderInit(AVCodecContext *Context);
    static void HWDecoderInitCallback(void*, void* Wait, void *Data);
};

#endif // VAAPICONTEXT_H
