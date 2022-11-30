// MythTV other libs
#include "libmythui/mythmainwindow.h"

// MythTV
#include "avformatdecoder.h"
#include "decoders/mythvdpaucontext.h"
#include "decoders/mythvdpauhelper.h"
#include "mythplayerui.h"
#include "opengl/mythvdpauinterop.h"

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
 * \sa MythCodecContext
*/
MythVDPAUContext::MythVDPAUContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

/// \brief Create a VDPAU device for use with direct rendering.
int MythVDPAUContext::InitialiseContext(AVCodecContext* Context)
{
    if (!gCoreContext->IsUIThread() || !Context)
        return -1;

    // The interop must have a reference to the ui player so it can be deleted
    // from the main thread.
    auto * player = GetPlayerUI(Context);
    if (!player)
        return -1;

    // Retrieve OpenGL render context
    auto * render = dynamic_cast<MythRenderOpenGL*>(player->GetRender());
    if (!render)
        return -1;
    OpenGLLocker locker(render);

    // Create interop
    auto vdpauid = static_cast<MythCodecID>(kCodec_MPEG1_VDPAU + (mpeg_version(Context->codec_id) - 1));
    auto * interop = MythVDPAUInterop::CreateVDPAU(player, render, vdpauid);
    if (!interop)
        return -1;

    // Allocate the device context
    auto * hwdeviceref = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VDPAU, interop);
    if (!hwdeviceref)
        return -1;

    auto * hwdevicecontext = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || !hwdevicecontext->hwctx)
        return -1;

    // Initialise device context
    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise device context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return -1;
    }

    // Allocate the hardware frames context
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(hwdeviceref);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VDPAU hardware frames context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return -1;
    }

    // Add our interop class and set the callback for its release
    auto * hwframesctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    hwframesctx->user_opaque = interop;
    hwframesctx->free = &MythCodecContext::FramesContextFinished;

    // Initialise frames context
    hwframesctx->sw_format = Context->sw_pix_fmt == AV_PIX_FMT_YUVJ420P ? AV_PIX_FMT_YUV420P : Context->sw_pix_fmt;
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

    auto * vdpaudevicectx = static_cast<AVVDPAUDeviceContext*>(hwdevicecontext->hwctx);
    if (av_vdpau_bind_context(Context, vdpaudevicectx->device,
                              vdpaudevicectx->get_proc_address, AV_HWACCEL_FLAG_IGNORE_LEVEL) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to bind VDPAU context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return -1;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VDPAU buffer pool created"));
    av_buffer_unref(&hwdeviceref);

    NewHardwareFramesContext();

    return 0;
}

MythCodecID MythVDPAUContext::GetSupportedCodec(AVCodecContext **Context,
                                                const AVCodec ** /*Codec*/,
                                                const QString &Decoder,
                                                uint StreamType)
{
    bool decodeonly = Decoder == "vdpau-dec";
    auto success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_VDPAU_DEC : kCodec_MPEG1_VDPAU) + (StreamType - 1));
    auto failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!Decoder.startsWith("vdpau") || qEnvironmentVariableIsSet("NO_VDPAU") || IsUnsupportedProfile(*Context))
        return failure;

    if (!decodeonly)
        if (!FrameTypeIsSupported(*Context, FMT_VDPAU))
            return failure;

    QString codec   = avcodec_get_name((*Context)->codec_id);
    QString profile = avcodec_profile_name((*Context)->codec_id, (*Context)->profile);
    QString pixfmt  = av_get_pix_fmt_name((*Context)->pix_fmt);

    // VDPAU only supports 8bit 420p:(
    VideoFrameType type = MythAVUtil::PixelFormatToFrameType((*Context)->pix_fmt);
    bool vdpau = (type == FMT_YV12 || type == FMT_VDPAU) && MythVDPAUHelper::HaveVDPAU() &&
                 (decodeonly ? codec_is_vdpau_dechw(success) : codec_is_vdpau_hw(success));

    if (vdpau)
    {
        MythCodecContext::CodecProfile mythprofile =
                MythCodecContext::FFmpegToMythProfile((*Context)->codec_id, (*Context)->profile);
        const VDPAUProfiles& profiles = MythVDPAUHelper::GetProfiles();
        vdpau = false;
        for (const auto& vdpauprofile : profiles)
        {
            bool match = vdpauprofile.first == mythprofile;
            if (match)
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Trying %1")
                    .arg(MythCodecContext::GetProfileDescription(mythprofile, QSize())));
                if (vdpauprofile.second.Supported((*Context)->width, (*Context)->height, (*Context)->level))
                {
                    vdpau = true;
                    break;
                }
            }
        }
    }

    // H264 needs additional checks for old hardware
    if (vdpau && (success == kCodec_H264_VDPAU || success == kCodec_H264_VDPAU_DEC))
    {
        vdpau = MythVDPAUHelper::CheckH264Decode(*Context);
        if (!vdpau)
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "H264 decode check failed");
    }

    QString desc = QString("'%1 %2 %3 %4x%5'")
        .arg(codec, profile, pixfmt).arg((*Context)->width).arg((*Context)->height);

    if (!vdpau)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VDPAU does not support decoding %1").arg(desc));
        return failure;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VDPAU supports decoding %1").arg(desc));
    (*Context)->pix_fmt = AV_PIX_FMT_VDPAU;
    return success;
}

///\ brief Confirm pixel format and create VDPAU device for direct rendering (MythVDPAUInterop required)
enum AVPixelFormat MythVDPAUContext::GetFormat(struct AVCodecContext* Context, const enum AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VDPAU)
            if (MythCodecContext::InitialiseDecoder(Context, MythVDPAUContext::InitialiseContext, "VDPAU context creation") >= 0)
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
            AVBufferRef *device = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VDPAU, nullptr);
            if (device)
            {
                NewHardwareFramesContext();
                if (Context->sw_pix_fmt == AV_PIX_FMT_YUVJ420P)
                    Context->sw_pix_fmt = AV_PIX_FMT_YUV420P;
                Context->hw_device_ctx = device;
                return AV_PIX_FMT_VDPAU;
            }
        }
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

bool MythVDPAUContext::RetrieveFrame(AVCodecContext* /*unused*/, MythVideoFrame *Frame, AVFrame *AvFrame)
{
    if (AvFrame->format != AV_PIX_FMT_VDPAU)
        return false;
    if (codec_is_vdpau_dec(m_codecID))
        return RetrieveHWFrame(Frame, AvFrame);
    return false;
}

bool MythVDPAUContext::DecoderWillResetOnFlush(void)
{
    return m_codecID == kCodec_H264_VDPAU;
}

bool MythVDPAUContext::DecoderWillResetOnAspect(void)
{
    return (m_codecID == kCodec_MPEG2_VDPAU) || (m_codecID == kCodec_MPEG2_VDPAU_DEC);
}

/*! \brief Report whether the decoder is known to be errored.
 *
 * This is used to determine whether the VDPAU display has been preempted. FFmpeg
 * does not deal with preemption so we need to detect it ourselves in the interop
 * class. We do not necessarily have a frame at this point, so need to access the interop
 * through AVHWFramesContext.
 *
 * \note May be called without a current AVCodecContext to confirm any previous reset request.
*/
bool MythVDPAUContext::DecoderNeedsReset(AVCodecContext* Context)
{
    if (m_resetRequired)
        return true;

    if (!codec_is_vdpau_hw(m_codecID))
        return false;
    if (!Context)
        return false;
    if (!Context->hw_frames_ctx)
        return false;

    auto* hwframesctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    auto* interop = reinterpret_cast<MythVDPAUInterop*>(hwframesctx->user_opaque);
    if (interop && interop->IsPreempted())
    {
        m_resetRequired = true;
        return true;
    }
    return false;
}

void MythVDPAUContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (codec_is_vdpau_hw(m_codecID))
    {
        Context->get_buffer2 = MythCodecContext::GetBuffer;
        Context->get_format  = MythVDPAUContext::GetFormat;
        Context->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
        return;
    }
    if (codec_is_vdpau_dechw(m_codecID))
    {
        Context->get_format   = MythVDPAUContext::GetFormat2;
        Context->slice_flags  = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}
