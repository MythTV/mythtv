// Mythtv
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/opengl/mythrenderopengl.h"

#include "avformatdecoder.h"
#include "mythcodecid.h"
#include "mythframe.h"
#include "mythplayerui.h"
#include "mythvtbcontext.h"
#include "opengl/mythvtbinterop.h"
#include "videobuffers.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_videotoolbox.h"
#include "libavcodec/videotoolbox.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("VTBDec: ")

MythVTBContext::MythVTBContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

MythVTBContext::~MythVTBContext()
{
    av_buffer_unref(&m_framesContext);
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

bool MythVTBContext::RetrieveFrame(AVCodecContext* Context, MythVideoFrame* Frame, AVFrame* AvFrame)
{
    if (AvFrame->format != AV_PIX_FMT_VIDEOTOOLBOX)
        return false;
    if (codec_is_vtb_dec(m_codecID))
        return RetrieveHWFrame(Frame, AvFrame);
    if (codec_is_vtb(m_codecID))
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

MythCodecID MythVTBContext::GetSupportedCodec(AVCodecContext **Context,
                                              const AVCodec ** Codec,
                                              const QString &Decoder,
                                              uint StreamType)
{
    bool decodeonly = Decoder == "vtb-dec"; 
    auto success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_VTB_DEC : kCodec_MPEG1_VTB) + (StreamType - 1));
    auto failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!Decoder.startsWith("vtb") || IsUnsupportedProfile(*Context))
        return failure;

    if (!decodeonly)
        if (!FrameTypeIsSupported(*Context, FMT_VTB))
            return failure;

    // Check decoder support
    MythCodecContext::CodecProfile mythprofile = MythCodecContext::NoProfile;
    switch ((*Codec)->id)
    {
        case AV_CODEC_ID_MPEG1VIDEO: mythprofile = MythCodecContext::MPEG1; break;
        case AV_CODEC_ID_MPEG2VIDEO: mythprofile = MythCodecContext::MPEG2; break;
        case AV_CODEC_ID_MPEG4:      mythprofile = MythCodecContext::MPEG4; break;
        case AV_CODEC_ID_H263:       mythprofile = MythCodecContext::H263;  break;
        case AV_CODEC_ID_H264:       mythprofile = MythCodecContext::H264;  break;
        case AV_CODEC_ID_VC1:        mythprofile = MythCodecContext::VC1;   break;
        case AV_CODEC_ID_VP8:        mythprofile = MythCodecContext::VP8;   break;
        case AV_CODEC_ID_VP9:        mythprofile = MythCodecContext::VP9;   break;
        case AV_CODEC_ID_HEVC:       mythprofile = MythCodecContext::HEVC;  break;
        default: break;
    }

    if (mythprofile == MythCodecContext::NoProfile)
        return failure;

    QString codec   = avcodec_get_name((*Context)->codec_id);
    QString profile = avcodec_profile_name((*Context)->codec_id, (*Context)->profile);
    QString pixfmt  = av_get_pix_fmt_name((*Context)->pix_fmt);

    const VTBProfiles& profiles = MythVTBContext::GetProfiles();
    if (!profiles.contains(mythprofile))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VideoToolbox does not support decoding '%1 %2 %3'")
                .arg(codec).arg(profile).arg(pixfmt));
        return failure;
    }

    // There is no guarantee (and no obvious way to check) whether the given
    // profile is actually supported.
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VideoToolbox MAY support decoding '%1 %2 %3'")
            .arg(codec).arg(profile).arg(pixfmt));
    (*Context)->pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
    return success;
}

int MythVTBContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!gCoreContext->IsUIThread())
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
    auto * interop = MythVTBInterop::CreateVTB(player, render);
    if (!interop)
        return -1;

    // Allocate the device context
    auto * deviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
    if (!deviceref)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create device context");
        interop->DecrRef();
        return -1;
    }

    // Add our interop class and set the callback for its release
    auto * devicectx = reinterpret_cast<AVHWDeviceContext*>(deviceref->data);
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

enum AVPixelFormat MythVTBContext::GetFormat(struct AVCodecContext* Context, const enum AVPixelFormat *PixFmt)
{
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VIDEOTOOLBOX)
        {
            auto* decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
            if (decoder)
            {
                auto* me = dynamic_cast<MythVTBContext*>(decoder->GetMythCodecContext());
                if (me)
                    me->InitFramesContext(Context);
            }
            return *PixFmt;
        }
        PixFmt++;
    }
    return ret;
}

const VTBProfiles& MythVTBContext::GetProfiles(void)
{
    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static VTBProfiles s_profiles;

    QMutexLocker locker(&lock);
    if (s_initialised)
        return s_profiles;
    s_initialised = true;

    if (__builtin_available(macOS 10.13, *))
    {
        if (VTIsHardwareDecodeSupported(kCMVideoCodecType_MPEG1Video)) { s_profiles.append(MythCodecContext::MPEG1); }
        if (VTIsHardwareDecodeSupported(kCMVideoCodecType_MPEG2Video)) { s_profiles.append(MythCodecContext::MPEG2); }
        if (VTIsHardwareDecodeSupported(kCMVideoCodecType_H263))       { s_profiles.append(MythCodecContext::H263);  }
        if (VTIsHardwareDecodeSupported(kCMVideoCodecType_MPEG4Video)) { s_profiles.append(MythCodecContext::MPEG4); }
        if (VTIsHardwareDecodeSupported(kCMVideoCodecType_H264))       { s_profiles.append(MythCodecContext::H264);  }
        if (VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC))       { s_profiles.append(MythCodecContext::HEVC);  }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Unable to check hardware decode support. Assuming all.");
        s_profiles.append(MythCodecContext::MPEG1);
        s_profiles.append(MythCodecContext::MPEG2);
        s_profiles.append(MythCodecContext::H263);
        s_profiles.append(MythCodecContext::MPEG4);
        s_profiles.append(MythCodecContext::H264);
        s_profiles.append(MythCodecContext::HEVC);
    }

    return s_profiles;
}

bool MythVTBContext::HaveVTB(bool Reinit /*=false*/)
{
    static QRecursiveMutex lock;
    QMutexLocker locker(&lock);
    static bool s_checked = false;
    static bool s_available = false;
    if (!s_checked || Reinit)
    {
        const VTBProfiles& profiles = MythVTBContext::GetProfiles();
        if (profiles.empty())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "No VideoToolbox decoders found");
        }
        else
        {
            s_available = true;
            LOG(VB_GENERAL, LOG_INFO, LOC + "Supported/available VideoToolbox decoders:");
            QSize size{0, 0};
            for (auto profile : profiles)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    MythCodecContext::GetProfileDescription(profile, size));
            }
        }
    }
    s_checked = true;
    return s_available;
}

void MythVTBContext::GetDecoderList(QStringList &Decoders)
{
    const VTBProfiles& profiles = MythVTBContext::GetProfiles();
    if (profiles.isEmpty())
        return;

    QSize size(0, 0);
    Decoders.append("VideoToolbox:");
    for (MythCodecContext::CodecProfile profile : profiles)
        Decoders.append(MythCodecContext::GetProfileDescription(profile, size));
}

/*! \brief Create a hardware frames context if needed.
 *
 * \note We now use our own frames context to ensure stream changes are handled
 * properly. This still fails to recreate the hardware decoder context however
 * as the code in videotoolbox.c returns kVTVideoDecoderNotAvailableNowErr (i.e.
 * VideoToolbox takes an indeterminate amount of time to free the old decoder and does
 * not allow a new one to be created until it has done so).
 * This appears to be a common issue - though may work better on newer hardware that
 * supports multiple decoders (tested on an ancient MacBook). So hardware decoding
 * will fail ungracefully and we fall back to software decoding. It is possible
 * to handle this more gracefully by patching videotoolbox.c to use
 * kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder rather
 * than kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder. In this
 * case VideoToolbox will use software decoding instead and the switch is relatively
 * seemless. BUT the decoder no longer uses reference counted buffers and MythVTBInterop
 * will leak textures as it does not expect a unique CVPixelBufferRef for every new
 * frame.
 * Also note that the FFmpeg code does not 'require' hardware decoding for HEVC - so
 * software decoding may be used for HEVC - which will break the interop class as noted,
 * but I cannot test this with my current hardware (and I don't have an HEVC sample
 * with a resolution change).
*/
void MythVTBContext::InitFramesContext(AVCodecContext *Context)
{
    if (!Context)
        return;

    AVPixelFormat format = AV_PIX_FMT_NV12;
    if (MythVideoFrame::ColorDepth(MythAVUtil::PixelFormatToFrameType(Context->sw_pix_fmt)) > 8)
        format = AV_PIX_FMT_P010;

    if (m_framesContext)
    {
        auto *frames = reinterpret_cast<AVHWFramesContext*>(m_framesContext->data);
        if ((frames->sw_format == format) && (frames->width == Context->coded_width) &&
            (frames->height == Context->coded_height))
        {
            Context->hw_frames_ctx = av_buffer_ref(m_framesContext);
            return;
        }
    }

    // If this is a 'spontaneous' callback from FFmpeg (i.e. not on a stream change)
    // then we must release any direct render buffers.
    if (codec_is_vtb(m_codecID) && m_parent->GetPlayer())
        m_parent->GetPlayer()->DiscardVideoFrames(true, true);

    av_buffer_unref(&m_framesContext);

    AVBufferRef* framesref = av_hwframe_ctx_alloc(Context->hw_device_ctx);
    auto *frames = reinterpret_cast<AVHWFramesContext*>(framesref->data);
    frames->free = MythCodecContext::FramesContextFinished;
    frames->user_opaque = nullptr;
    frames->sw_format = format;
    frames->format    = AV_PIX_FMT_VIDEOTOOLBOX;
    frames->width     = Context->coded_width;
    frames->height    = Context->coded_height;
    if (av_hwframe_ctx_init(framesref) < 0)
    {
        av_buffer_unref(&framesref);
    }
    else
    {
        Context->hw_frames_ctx = framesref;
        m_framesContext = av_buffer_ref(framesref);
        NewHardwareFramesContext();
    }
}
