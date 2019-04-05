// MythTV
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "avformatdecoder.h"
#include "mythmediacodecinterop.h"
#include "mythhwcontext.h"
#include "mythmediacodeccontext.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext_mediacodec.h"
#include "libavcodec/mediacodec.h"
#include "libavcodec/avcodec.h"
}

#define LOC QString("MediaCodec: ")

int MythMediaCodecContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return -1;

    // Create interop class
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;

    QSize size(Context->width, Context->height);
    MythMediaCodecInterop *interop = MythMediaCodecInterop::Create(render, size);
    if (!interop)
        return -1;
    if (!interop->GetSurface())
    {
        interop->DecrRef();
        return -1;
    }

    // Create the hardware context
    AVBufferRef *hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
    AVHWDeviceContext *hwdevicectx = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    hwdevicectx->free = &MythHWContext::DeviceContextFinished;
    hwdevicectx->user_opaque = interop;
    AVMediaCodecDeviceContext *hwctx = reinterpret_cast<AVMediaCodecDeviceContext*>(hwdevicectx->hwctx);
    hwctx->surface = interop->GetSurface();
    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "av_hwdevice_ctx_init failed");
        av_buffer_unref(&hwdeviceref);
        return -1;
    }

    Context->hw_device_ctx = hwdeviceref;
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created MediaCodec hardware device context");
    return 0;
}

MythCodecID MythMediaCodecContext::GetBestSupportedCodec(AVCodecContext*,
                                                         AVCodec       **Codec,
                                                         const QString  &Decoder,
                                                         uint            StreamType,
                                                         AVPixelFormat  &PixFmt)
{
    AVPixelFormat format = AV_PIX_FMT_NONE;
    if (Decoder == "mediacodec")
    {
        QString decodername = QString((*Codec)->name) + "_mediacodec";
        if (decodername == "mpeg2video_mediacodec")
            decodername = "mpeg2_mediacodec";
        AVCodec *newCodec = avcodec_find_decoder_by_name (decodername.toLocal8Bit());
        if (newCodec)
        {
            *Codec = newCodec;
            format = AV_PIX_FMT_MEDIACODEC;
        }
    }

    if (format == AV_PIX_FMT_NONE)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support decoding '%2'")
                .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)).arg((*Codec)->name));
        return (MythCodecID)(kCodec_MPEG1 + (StreamType - 1));
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2'")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)).arg((*Codec)->name));
    PixFmt = AV_PIX_FMT_MEDIACODEC;
    return (MythCodecID)(kCodec_MPEG1_MEDIACODEC + (StreamType - 1));
}

MythMediaCodecContext::MythMediaCodecContext()
  : MythCodecContext()
{
}

int MythMediaCodecContext::HwDecoderInit(AVCodecContext *Context)
{
    return MythHWContext::InitialiseDecoder2(Context, MythMediaCodecContext::InitialiseDecoder, "Create MediaCodec decoder");
}

AVPixelFormat MythMediaCodecContext::GetFormat(AVCodecContext*, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_MEDIACODEC)
            return AV_PIX_FMT_MEDIACODEC;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}
