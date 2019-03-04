#ifndef MYTHMEDIACODECCONTEXT_H
#define MYTHMEDIACODECCONTEXT_H

// MythTV
#include "mythcodeccontext.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
}

class MythMediaCodecContext : public MythCodecContext
{
  public:
    // MythCodecContext
    MythMediaCodecContext();
    int HwDecoderInit(AVCodecContext *Context) override;

    static MythCodecID GetBestSupportedCodec(AVCodecContext *AvCtx,
                                             AVCodec       **Codec,
                                             const QString  &Decoder,
                                             uint            StreamType,
                                             AVPixelFormat  &PixFmt);
    static AVPixelFormat GetFormat          (AVCodecContext *AvCtx,
                                             const AVPixelFormat *PixFmt);
    static int  GetBuffer                   (struct AVCodecContext *Context,
                                             AVFrame *Frame, int Flags);
    static void ReleaseBuffer               (void *Opaque, uint8_t *Data);
    static void DeviceContextFinished       (AVHWDeviceContext *Context);
    static void CreateDecoderCallback       (void *Wait, void *Context, void*);
    static void DestroyInteropCallback      (void *Wait, void *Interop, void*);

  private:
    static int  InitialiseDecoder           (AVCodecContext *Context);
};

#endif // MYTHMEDIACODECCONTEXT_H
