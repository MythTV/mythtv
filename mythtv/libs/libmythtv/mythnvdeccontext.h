#ifndef MYTHNVDECCONTEXT_H
#define MYTHNVDECCONTEXT_H

// MythTV
#include "mythcodecid.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"
}

class MythNVDECContext
{
  public:
    static int         HwDecoderInit     (AVCodecContext *Context);
    static MythCodecID GetSupportedCodec (AVCodecContext *CodecContext,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Contextconst, const AVPixelFormat *PixFmt);
    static int  GetBuffer                (AVCodecContext *Context,
                                          AVFrame *Frame, int Flags);
    static void ReleaseBuffer            (void *Opaque, uint8_t *Data);
    static void DeviceContextFinished    (AVHWDeviceContext *Context);
    static void CreateDecoderCallback    (void *Wait, void *Context, void*);
    static void DestroyInteropCallback   (void *Wait, void *Interop, void*);
    static int  InitialiseDecoder        (AVCodecContext *Context);
};

#endif // MYTHNVDECCONTEXT_H
