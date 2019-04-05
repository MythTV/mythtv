// MythTV
#include "mythmainwindow.h"
#include "avformatdecoder.h"
#include "mythvdpauinterop.h"
#include "mythvdpauhelper.h"
#include "mythvdpaucontext.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_vdpau.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/vdpau.h"
}

#define LOC QString("VDPAUDec: ")

MythVDPAUContext::MythVDPAUContext(MythCodecID CodecID)
  : m_codecID(CodecID)
{
}

void MythVDPAUContext::DestroyInteropCallback(void *Wait, void *Interop, void*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Destroy interop callback");
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Interop);
    if (interop)
        interop->DecrRef();
    if (wait)
        wait->wakeAll();
}

void MythVDPAUContext::FramesContextFinished(AVHWFramesContext *Context)
{
    MythVDPAUInterop *interop = reinterpret_cast<MythVDPAUInterop*>(Context->user_opaque);
    if (!interop)
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Frames context destroyed");
    if (gCoreContext->IsUIThread())
        interop->DecrRef();
    else
        MythMainWindow::HandleCallback("Destroy VDPAU interop", &DestroyInteropCallback, interop, nullptr);
}

void MythVDPAUContext::DeviceContextFinished(AVHWDeviceContext*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Device context finished");
}

void MythVDPAUContext::InitialiseContext(AVCodecContext* Context)
{
    if (!gCoreContext->IsUIThread() || !Context)
        return;

    // Retrieve OpenGL render context
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return;

    // Lock
    OpenGLLocker locker(render);

    MythCodecID vdpauid = static_cast<MythCodecID>(kCodec_MPEG1_VDPAU + (mpeg_version(Context->codec_id) - 1));
    MythOpenGLInterop::Type type = MythVDPAUInterop::GetInteropType(vdpauid, render);
    if (type == MythOpenGLInterop::Unsupported)
        return;

    // Allocate the device context
    AVBufferRef* hwdeviceref = nullptr;
    if (av_hwdevice_ctx_create(&hwdeviceref, AV_HWDEVICE_TYPE_VDPAU, nullptr, nullptr, 0) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create device context");
        return;
    }
    if (!hwdeviceref || (hwdeviceref && !hwdeviceref->data))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create device context");
        return;
    }

    // Set release method
    AVHWDeviceContext* hwdevicecontext = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || (hwdevicecontext && !hwdevicecontext->hwctx))
        return;
    hwdevicecontext->free = &MythVDPAUContext::DeviceContextFinished;

    // Initialise device context
    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise device context");
        av_buffer_unref(&hwdeviceref);
        return;
    }

    // Create interop
    MythVDPAUInterop *interop = MythVDPAUInterop::Create(render);
    if (!interop)
    {
        av_buffer_unref(&hwdeviceref);
        return;
    }

    // allocate the hardware frames context
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(hwdeviceref);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VDPAU hardware frames context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return;
    }

    // Add our interop class and set the callback for its release
    AVHWFramesContext* hwframesctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    hwframesctx->user_opaque = interop;
    hwframesctx->free = &MythVDPAUContext::FramesContextFinished;

    // Initialise frames context
    hwframesctx->sw_format = Context->sw_pix_fmt;
    hwframesctx->format    = AV_PIX_FMT_VDPAU;
    hwframesctx->width     = Context->coded_width;
    hwframesctx->height    = Context->coded_height;
    int res = av_hwframe_ctx_init(Context->hw_frames_ctx);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VDPAU frames context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return;
    }

    AVVDPAUDeviceContext* vdpaudevicectx = static_cast<AVVDPAUDeviceContext*>(hwdevicecontext->hwctx);
    if (av_vdpau_bind_context(Context, vdpaudevicectx->device, vdpaudevicectx->get_proc_address, 0) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VDPAU frames context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VDPAU buffer pool created"));
    av_buffer_unref(&hwdeviceref);
}

void MythVDPAUContext::InitialiseDecoderCallback(void *Wait, void *Context, void*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Create decoder callback");
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    AVCodecContext *context = reinterpret_cast<AVCodecContext*>(Context);
    MythVDPAUContext::InitialiseContext(context);
    if (wait)
        wait->wakeAll();
}

int MythVDPAUContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!Context)
        return -1;
    if (gCoreContext->IsUIThread())
        InitialiseContext(Context);
    else
        MythMainWindow::HandleCallback("VDPAU context creation", &InitialiseDecoderCallback, Context, nullptr);
    return Context->hw_frames_ctx ? 0 : -1;
}

MythCodecID MythVDPAUContext::GetSupportedCodec(AVCodecContext *Context, AVCodec **, const QString &Decoder,
                                                uint StreamType, AVPixelFormat &PixFmt)
{
    bool decodeonly = Decoder == "vdpau-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_VDPAU_DEC : kCodec_MPEG1_VDPAU) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (((Decoder != "vdpau") && (Decoder != "vdpau-dec")) || getenv("NO_VDPAU"))
        return failure;

    bool vdpau = MythVDPAUHelper::HaveVDPAU() && (decodeonly ? codec_is_vdpau_dechw(success) : codec_is_vdpau_hw(success));
    if (vdpau && (success == kCodec_MPEG4_VDPAU || success == kCodec_MPEG4_VDPAU_DEC))
        vdpau = MythVDPAUHelper::HaveMPEG4Decode();
    if (vdpau && (success == kCodec_H264_VDPAU || success == kCodec_H264_VDPAU_DEC))
        vdpau = MythVDPAUHelper::CheckH264Decode(Context);
    if (vdpau && (success == kCodec_HEVC_VDPAU || success == kCodec_HEVC_VDPAU_DEC))
        vdpau = MythVDPAUHelper::CheckHEVCDecode(Context);

    if (!vdpau)
        return failure;

    PixFmt = AV_PIX_FMT_VDPAU;
    return success;
}

enum AVPixelFormat MythVDPAUContext::GetFormat(struct AVCodecContext* Context, const enum AVPixelFormat *PixFmt)
{
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VDPAU)
            if (MythVDPAUContext::InitialiseDecoder(Context) >= 0)
                return *PixFmt;
        PixFmt++;
    }
    return ret;
}

int MythVDPAUContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int Flags)
{
    AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    // set fields required for directrendering
    for (int i = 0; i < 4; i++)
    {
        Frame->data[i]     = nullptr;
        Frame->linesize[i] = 0;
    }
    Frame->opaque           = videoframe;
    videoframe->pix_fmt     = Context->pix_fmt;
    Frame->reordered_opaque = Context->reordered_opaque;

    int ret = avcodec_default_get_buffer2(Context, Frame, Flags);
    if (ret < 0)
        return ret;

    // Retrieve the VdpVideoSurface and retain it to ensure it is not released before
    // we have used it for display
    videoframe->buf = Frame->data[3];
    videoframe->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->buf[0]));
    videoframe->priv[1] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->hw_frames_ctx));

    // Set release method
    Frame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(videoframe), 0,
                                     MythVDPAUContext::ReleaseBuffer, avfd, 0);
    return ret;
}

void MythVDPAUContext::ReleaseBuffer(void *Opaque, uint8_t *Data)
{
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Opaque);
    VideoFrame *frame = reinterpret_cast<VideoFrame*>(Data);
    if (decoder && decoder->GetPlayer())
        decoder->GetPlayer()->DeLimboFrame(frame);
}
