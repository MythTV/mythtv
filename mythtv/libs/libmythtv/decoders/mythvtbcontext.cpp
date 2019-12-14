// Mythtv
#include "mythmainwindow.h"
#include "mythlogging.h"
#include "mythcodecid.h"
#include "mythframe.h"
#include "avformatdecoder.h"
#include "opengl/mythrenderopengl.h"
#include "videobuffers.h"
#include "mythvtbinterop.h"
#include "mythvtbcontext.h"

extern "C" {
#include "libavutil/hwcontext_videotoolbox.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("VTBDec: ")

MythVTBContext::MythVTBContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

void MythVTBContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (codec_is_vtb(m_codecID) || codec_is_vtb_dec(m_codecID))
    {
        Context->get_format  = MythVTBContext::GetFormat;
        Context->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythVTBContext::RetrieveFrame(AVCodecContext* Context, VideoFrame* Frame, AVFrame* AvFrame)
{
    if (AvFrame->format != AV_PIX_FMT_VIDEOTOOLBOX)
        return false;
    if (codec_is_vtb_dec(m_codecID))
        return RetrieveHWFrame(Frame, AvFrame);
    else if (codec_is_vtb(m_codecID))
        return GetBuffer2(Context, Frame, AvFrame, 0);
    return false;
}

int MythVTBContext::HwDecoderInit(AVCodecContext *Context)
{
    if (codec_is_vtb_dec(m_codecID))
    {
        AVBufferRef *device = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr);
        if (device)
        {
            Context->hw_device_ctx = device;
            return 0;
        }
    }
    else if (codec_is_vtb(m_codecID))
    {
        return MythCodecContext::InitialiseDecoder2(Context, MythVTBContext::InitialiseDecoder, "Create VTB decoder");
    }

    return -1;
}

bool MythVTBContext::CheckDecoderSupport(AVCodecContext **Context, uint StreamType)
{
    static bool initialised = false;
    static QVector<uint> supported;
    if (!initialised)
    {
        initialised = true;
        if (__builtin_available(macOS 10.13, *))
        {
            QStringList debug;
            if (VTIsHardwareDecodeSupported(kCMVideoCodecType_MPEG1Video)) { supported.append(1); debug << "MPEG1"; }
            if (VTIsHardwareDecodeSupported(kCMVideoCodecType_MPEG2Video)) { supported.append(2); debug << "MPEG2"; }
            if (VTIsHardwareDecodeSupported(kCMVideoCodecType_H263))       { supported.append(3); debug << "H.263"; }
            if (VTIsHardwareDecodeSupported(kCMVideoCodecType_MPEG4Video)) { supported.append(4); debug << "MPEG4"; }
            if (VTIsHardwareDecodeSupported(kCMVideoCodecType_H264))       { supported.append(5); debug << "H.264"; }
            if (VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC))       { supported.append(10); debug << "HEVC"; }
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Supported VideoToolBox codecs: %1").arg(debug.join(",")));
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Unable to check hardware decode support. Assuming all.");
            supported.append(1); supported.append(2); supported.append(3);
            supported.append(4); supported.append(5); supported.append(10);
        }
    }

    QString codec   = ff_codec_id_string((*Context)->codec_id);
    QString profile = avcodec_profile_name((*Context)->codec_id, (*Context)->profile);
    QString pixfmt  = av_get_pix_fmt_name((*Context)->pix_fmt);

    if (!supported.contains(StreamType))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support decoding '%2 %3 %4'")
                .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VIDEOTOOLBOX))
                .arg(codec).arg(profile).arg(pixfmt));
        return false;
    }

    // There is no guarantee (and no obvious way to check) whether the given H264 or HEVC
    // profile is actually supported.
    bool maybe = (StreamType == 5) || (StreamType == 10);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' %2 decoding '%3 %4 %5'")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VIDEOTOOLBOX))
            .arg(maybe ? "MAY support" : "supports")
            .arg(codec).arg(profile).arg(pixfmt));
    return true;
}

MythCodecID MythVTBContext::GetSupportedCodec(AVCodecContext **Context,
                                              AVCodec **,
                                              const QString &Decoder,
                                              uint StreamType)
{
    bool decodeonly = Decoder == "vtb-dec"; 
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_VTB_DEC : kCodec_MPEG1_VTB) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!Decoder.startsWith("vtb") || IsUnsupportedProfile(*Context))
        return failure;

    // Check decoder support
    if (!CheckDecoderSupport(Context, StreamType))
        return failure;

    (*Context)->pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
    return success;
}

int MythVTBContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!gCoreContext->IsUIThread())
        return -1;

    // Retrieve OpenGL render context
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;
    OpenGLLocker locker(render);

    // We need a player to release the interop
    MythPlayer *player = nullptr;
    auto *decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
    if (decoder)
        player = decoder->GetPlayer();
    if (!player)
        return -1;

    // Check interop support
    MythVTBInterop::Type type = MythOpenGLInterop::GetInteropType(FMT_VTB, player);
    if (type == MythOpenGLInterop::Unsupported)
        return -1;

    // Create interop
    MythVTBInterop* interop = MythVTBInterop::Create(render, type);
    if (!interop)
        return -1;

    // Set player
    interop->SetPlayer(player);

    // Allocate the device context
    AVBufferRef* deviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
    if (!deviceref)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create device context");
        interop->DecrRef();
        return -1;
    }
    
    // Add our interop class and set the callback for its release
    AVHWDeviceContext* devicectx = reinterpret_cast<AVHWDeviceContext*>(deviceref->data);
    devicectx->user_opaque = interop;
    devicectx->free        = MythCodecContext::DeviceContextFinished;

    // Create
    if (av_hwdevice_ctx_init(deviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise device context");
        av_buffer_unref(&deviceref);
        return -1;
    }

    Context->hw_device_ctx = deviceref;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created hw device '%1'")
        .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VIDEOTOOLBOX)));
    return 0;
}

enum AVPixelFormat MythVTBContext::GetFormat(struct AVCodecContext*, const enum AVPixelFormat *PixFmt)
{
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VIDEOTOOLBOX)
            return *PixFmt;
        PixFmt++;
    }
    return ret;
}
