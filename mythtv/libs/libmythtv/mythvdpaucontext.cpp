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

/*! \class MythVDPAUContext
 *
 * \sa MythVDPAUHelper
 * \sa MythVDAPUInterop
 * \sa MythHWContext
*/
MythVDPAUContext::MythVDPAUContext(MythCodecID CodecID)
  : MythCodecContext(),
    m_codecID(CodecID)
{
}

/*! \brief Create a VDPAU device for use with direct rendering.
 *
 * \note For reasons I cannot understand, this must be performed as part of the GetFormat
 * call. Trying to initialise in HwDecoderInit fails to create the frames context
 * (sw_pix_fmt is not set) -  the same 'problem' appears in MythVAAPIContext
 */
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
    AVBufferRef* hwdeviceref = MythHWContext::CreateDevice(AV_HWDEVICE_TYPE_VDPAU);
    if (!hwdeviceref)
        return -1;

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
    MythVDPAUInterop *interop = MythVDPAUInterop::Create(render, vdpauid);
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to bind VDPAU context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return -1;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VDPAU buffer pool created"));
    av_buffer_unref(&hwdeviceref);
    return 0;
}

MythCodecID MythVDPAUContext::GetSupportedCodec(AVCodecContext *Context, AVCodec **Codec, const QString &Decoder,
                                                uint StreamType, AVPixelFormat &PixFmt)
{
    bool decodeonly = Decoder == "vdpau-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_VDPAU_DEC : kCodec_MPEG1_VDPAU) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (((Decoder != "vdpau") && (Decoder != "vdpau-dec")) || getenv("NO_VDPAU"))
        return failure;

    // VDPAU only supports 8bit 420p:(
    // N.B. It should probably be possible to force YUVJ420P support
    VideoFrameType type = PixelFormatToFrameType(Context->pix_fmt);
    bool vdpau = (type == FMT_YV12) && (AV_PIX_FMT_YUVJ420P != Context->pix_fmt) && MythVDPAUHelper::HaveVDPAU() &&
                 (decodeonly ? codec_is_vdpau_dechw(success) : codec_is_vdpau_hw(success));
    if (vdpau && (success == kCodec_MPEG4_VDPAU || success == kCodec_MPEG4_VDPAU_DEC))
        vdpau = MythVDPAUHelper::HaveMPEG4Decode();
    if (vdpau && (success == kCodec_H264_VDPAU || success == kCodec_H264_VDPAU_DEC))
        vdpau = MythVDPAUHelper::CheckH264Decode(Context);
    if (vdpau && (success == kCodec_HEVC_VDPAU || success == kCodec_HEVC_VDPAU_DEC))
        vdpau = MythVDPAUHelper::CheckHEVCDecode(Context);

    if (!vdpau)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support decoding '%2 %3'")
                .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VDPAU)).arg((*Codec)->name)
                .arg(format_description(type)));
        return failure;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2 %3'")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VDPAU)).arg((*Codec)->name)
            .arg(format_description(type)));
    PixFmt = AV_PIX_FMT_VDPAU;
    return success;
}

///\ brief Confirm pixel format and create VDPAU device for direct rendering (MythVDPAUInterop required)
enum AVPixelFormat MythVDPAUContext::GetFormat(struct AVCodecContext* Context, const enum AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VDPAU)
            if (MythHWContext::InitialiseDecoder(Context, MythVDPAUContext::InitialiseContext, "VDPAU context creation") >= 0)
                return AV_PIX_FMT_VDPAU;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

///\ brief Confirm pixel format and create VDPAU device for copy back (no MythVDPAUInterop required)
enum AVPixelFormat MythVDPAUContext::GetFormat2(struct AVCodecContext* Context, const enum AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VDPAU)
        {
            AVBufferRef *device = MythHWContext::CreateDevice(AV_HWDEVICE_TYPE_VDPAU);
            if (device)
            {
                Context->hw_device_ctx = device;
                return AV_PIX_FMT_VDPAU;
            }
        }
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}
