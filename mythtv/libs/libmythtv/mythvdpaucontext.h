#ifndef MYTHVDPAUCONTEXT_H
#define MYTHVDPAUCONTEXT_H

// MythTV
#include "mythcodeccontext.h"

class MythVDPAUContext
{
  public:
    MythVDPAUContext(MythCodecID CodecID);
    static MythCodecID GetSupportedCodec (AVCodecContext *CodecContext,
                                          AVCodec       **Codec,
                                          const QString  &Decoder,
                                          uint            StreamType,
                                          AVPixelFormat  &PixFmt);
    static enum AVPixelFormat GetFormat  (AVCodecContext *Context,
                                          const enum AVPixelFormat *PixFmt);

    // Direct rendering only
    static int  GetBuffer                (struct AVCodecContext *Context,
                                          AVFrame *Frame, int Flags);
    static void ReleaseBuffer            (void *Opaque, uint8_t *Data);
    static void FramesContextFinished    (AVHWFramesContext *Context);
    static void DeviceContextFinished    (AVHWDeviceContext *Context);
    static void InitialiseDecoderCallback(void *Wait, void *Context, void*);
    static void DestroyInteropCallback   (void *Wait, void *Interop, void*);

  private:
    static int  InitialiseDecoder        (AVCodecContext *Context);
    static void InitialiseContext        (AVCodecContext *Context);
    MythCodecID m_codecID;
};

#endif // MYTHVDPAUCONTEXT_H
