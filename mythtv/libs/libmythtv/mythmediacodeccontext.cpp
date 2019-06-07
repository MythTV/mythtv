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
    hwdevicectx->free = &MythCodecContext::DeviceContextFinished;
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
    bool decodeonly = Decoder == "mediacodec-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_MEDIACODEC_DEC : kCodec_MPEG1_MEDIACODEC) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if ((Decoder != "mediacodec") && (Decoder != "mediacodec-dec"))
        return failure;

    QString decodername = QString((*Codec)->name) + "_mediacodec";
    if (decodername == "mpeg2video_mediacodec")
        decodername = "mpeg2_mediacodec";
    AVCodec *newCodec = avcodec_find_decoder_by_name (decodername.toLocal8Bit());
    if (newCodec)
    {
        *Codec = newCodec;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2'")
                .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)).arg((*Codec)->name));
        PixFmt = AV_PIX_FMT_MEDIACODEC;
        return success;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support decoding '%2'")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)).arg((*Codec)->name));
    return failure;
}

MythMediaCodecContext::MythMediaCodecContext(MythCodecID CodecID)
  : MythCodecContext(CodecID)
{
}

int MythMediaCodecContext::HwDecoderInit(AVCodecContext *Context)
{
    if (codec_is_mediacodec_dec(m_codecID))
        return 0;
    else if (codec_is_mediacodec(m_codecID))
        return MythCodecContext::InitialiseDecoder2(Context, MythMediaCodecContext::InitialiseDecoder, "Create MediaCodec decoder");
    return -1;
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

/*! \brief Mark all MediaCodec decoded frames as progressive,
 *
 * \note This may not be appropriate for all devices
*/
void MythMediaCodecContext::PostProcessFrame(AVCodecContext*, VideoFrame* Frame)
{
    if (!Frame)
        return;
    Frame->interlaced_frame = 0;
    Frame->interlaced_reversed = 0;
    Frame->top_field_first = 0;
    Frame->deinterlace_inuse = DEINT_BASIC | DEINT_DRIVER;
    Frame->deinterlace_inuse2x = 0;
}
