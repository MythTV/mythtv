#ifndef VAAPICONTEXT_H
#define VAAPICONTEXT_H

// MythTV
#include "mythcodecid.h"

// VAAPI
#include "va/va.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
}

class VAAPIContext
{
  public:
    static MythCodecID GetSupportedCodec (AVCodecContext *CodecContext,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (struct AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);
    static int  GetBuffer                (struct AVCodecContext *Context,
                                          AVFrame *Frame, int Flags);
    static void ReleaseBuffer            (void *Opaque, uint8_t *Data);

    static void FramesContextFinished    (struct AVHWFramesContext *Context);
    static void DeviceContextFinished    (struct AVHWDeviceContext *Context);
    static void InitialiseDecoderCallback(void* Wait, void* Context, void*);
    static void DestroyInteropCallback   (void* Wait, void* Interop, void*);

  private:
    static int  InitialiseDecoder        (AVCodecContext *Context);
    static void InitialiseContext        (AVCodecContext *Context);
    static VAProfile VAAPIProfileForCodec(const AVCodecContext *Codec);
};

#endif // VAAPICONTEXT_H
