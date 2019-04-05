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

int MythVDPAUContext::InitialiseContext(AVCodecContext* Context)
{
    if (!gCoreContext->IsUIThread() || !Context)
        return -1;

    // Retrieve OpenGL render context
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;

    // Lock
    OpenGLLocker locker(render);

    MythCodecID vdpauid = static_cast<MythCodecID>(kCodec_MPEG1_VDPAU + (mpeg_version(Context->codec_id) - 1));
    MythOpenGLInterop::Type type = MythVDPAUInterop::GetInteropType(vdpauid, render);
    if (type == MythOpenGLInterop::Unsupported)
        return -1;

    // Allocate the device context
    AVBufferRef* hwdeviceref = nullptr;
    if (av_hwdevice_ctx_create(&hwdeviceref, AV_HWDEVICE_TYPE_VDPAU, nullptr, nullptr, 0) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create device context");
        return -1;
    }
    if (!hwdeviceref || (hwdeviceref && !hwdeviceref->data))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create device context");
        return -1;
    }

    // Set release method
    AVHWDeviceContext* hwdevicecontext = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || (hwdevicecontext && !hwdevicecontext->hwctx))
        return -1;

    // Initialise device context
    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise device context");
        av_buffer_unref(&hwdeviceref);
        return -1;
    }

    // Create interop
    MythVDPAUInterop *interop = MythVDPAUInterop::Create(render);
    if (!interop)
    {
        av_buffer_unref(&hwdeviceref);
        return -1;
    }

    // allocate the hardware frames context
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(hwdeviceref);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VDPAU hardware frames context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return -1;
    }

    // Add our interop class and set the callback for its release
    AVHWFramesContext* hwframesctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    hwframesctx->user_opaque = interop;
    hwframesctx->free = &MythHWContext::FramesContextFinished;

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
        return res;
    }

    AVVDPAUDeviceContext* vdpaudevicectx = static_cast<AVVDPAUDeviceContext*>(hwdevicecontext->hwctx);
    if (av_vdpau_bind_context(Context, vdpaudevicectx->device, vdpaudevicectx->get_proc_address, 0) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VDPAU frames context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return -1;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VDPAU buffer pool created"));
    av_buffer_unref(&hwdeviceref);
    return 0;
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
            if (MythHWContext::InitialiseDecoder(Context, MythVDPAUContext::InitialiseContext, "VDPAU context creation") >= 0)
                return *PixFmt;
        PixFmt++;
    }
    return ret;
}
