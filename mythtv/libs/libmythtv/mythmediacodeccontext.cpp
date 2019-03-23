// MythTV
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "avformatdecoder.h"
#include "mythmediacodecinterop.h"
#include "mythmediacodeccontext.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext_mediacodec.h"
#include "libavcodec/mediacodec.h"
#include "libavcodec/avcodec.h"
}

#define LOC QString("MediaCodec: ")

void MythMediaCodecContext::DestroyInteropCallback(void *Wait, void *Interop, void*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Destroy interop callback");
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Interop);
    if (interop)
        interop->DecrRef();
    if (wait)
        wait->wakeAll();
}

void MythMediaCodecContext::DeviceContextFinished(AVHWDeviceContext *Context)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Device context finished");
    MythMediaCodecInterop *interop = reinterpret_cast<MythMediaCodecInterop*>(Context->user_opaque);
    if (!interop)
        return;

    if (gCoreContext->IsUIThread())
        interop->DecrRef();
    else
        MythMainWindow::HandleCallback("Destroy MediaCodec interop", &DestroyInteropCallback, interop, nullptr);
}

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
    hwdevicectx->free = &MythMediaCodecContext::DeviceContextFinished;
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

void MythMediaCodecContext::CreateDecoderCallback(void *Wait, void *Context, void*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Create decoder callback");
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    AVCodecContext *context = reinterpret_cast<AVCodecContext*>(Context);
    if (context)
        MythMediaCodecContext::InitialiseDecoder(context);
    if (wait)
        wait->wakeAll();
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
    // Create hardware device and interop
    if (gCoreContext->IsUIThread())
        InitialiseDecoder(Context);
    else
        MythMainWindow::HandleCallback("Create MediaCodec decoder", &CreateDecoderCallback, Context, nullptr);

    if (!Context->hw_device_ctx)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Failed to create HW device type '%'")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)));
        return -1;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created HW device type '%1'")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)));
    return 0;
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

int MythMediaCodecContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int)
{
    if (!Frame || !Context)
        return -1;

    AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    Frame->opaque           = videoframe;
    videoframe->pix_fmt     = Context->pix_fmt;
    Frame->reordered_opaque = Context->reordered_opaque;

    // AVMediaCodecBuffer is stored in Frame->data[3]
    videoframe->buf = Frame->data[3];

    // Frame->buf[0] contains the release method. Take another reference to ensure the frame
    // is not released before it is displayed.
    videoframe->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->buf[0]));

    // Retrieve and set the interop class
    AVHWDeviceContext *devicectx = reinterpret_cast<AVHWDeviceContext*>(Context->hw_device_ctx->data);
    videoframe->priv[1] = reinterpret_cast<unsigned char*>(devicectx->user_opaque);

    // Set release method
    Frame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(videoframe), 0,
                                     MythMediaCodecContext::ReleaseBuffer, avfd, 0);
    return 0;
}

void MythMediaCodecContext::ReleaseBuffer(void *Opaque, uint8_t *Data)
{
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Opaque);
    VideoFrame *frame = reinterpret_cast<VideoFrame*>(Data);
    if (decoder && decoder->GetPlayer())
        decoder->GetPlayer()->DeLimboFrame(frame);
}
