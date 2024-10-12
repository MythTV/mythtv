// MythTV
#include "decoders/avformatdecoder.h"
#include "mythplayerui.h"
#include "mythmmalcontext.h"

// FFmpeg
extern "C" {
#include "libavutil/opt.h"
}

#define LOC QString("MMAL: ")

MythMMALContext::MythMMALContext(DecoderBase *Parent, MythCodecID Codec)
  : MythCodecContext(Parent, Codec)
{
}

MythMMALContext::~MythMMALContext()
{
    if (m_interop)
        DestroyInterop(m_interop);
}

bool MythMMALContext::CheckCodecSize(int Width, int Height, MythCodecContext::CodecProfile Profile)
{
    switch (Profile)
    {
        case MythCodecContext::MPEG2:
        case MythCodecContext::MPEG4:
        case MythCodecContext::VC1:
        case MythCodecContext::H264:
            if (Width > 1920 || Height > 1088)
                return false;
            break;
        default: break;
    }
    return true;
}

MythCodecID MythMMALContext::GetSupportedCodec(AVCodecContext **Context,
                                               const AVCodec **Codec,
                                               const QString &Decoder,
                                               AVStream *Stream,
                                               uint StreamType)
{
    bool decodeonly = Decoder == "mmal-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_MMAL_DEC : kCodec_MPEG1_MMAL) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!Decoder.startsWith("mmal"))
        return failure;

    // Only MPEG2, MPEG4, VC1 and H264 supported (and HEVC will never be supported)
    MythCodecContext::CodecProfile mythprofile = MythCodecContext::NoProfile;
    switch ((*Codec)->id)
    {
        case AV_CODEC_ID_MPEG2VIDEO: mythprofile = MythCodecContext::MPEG2; break;
        case AV_CODEC_ID_MPEG4:      mythprofile = MythCodecContext::MPEG4; break;
        case AV_CODEC_ID_VC1:        mythprofile = MythCodecContext::VC1;   break;
        case AV_CODEC_ID_H264:
            if ((*Context)->profile == FF_PROFILE_H264_HIGH_10 ||
                (*Context)->profile == FF_PROFILE_H264_HIGH_10_INTRA)
            {
                return failure;
            }
            mythprofile = MythCodecContext::H264; break;
        default: break;
    }

    if (mythprofile == MythCodecContext::NoProfile)
        return failure;

    // Check size
    if (!MythMMALContext::CheckCodecSize((*Context)->width, (*Context)->height, mythprofile))
        return failure;

    // check actual decoder support
    const MMALProfiles& profiles = MythMMALContext::GetProfiles();
    if (!profiles.contains(mythprofile))
        return failure;

    if (!decodeonly)
        if (!FrameTypeIsSupported(*Context, FMT_MMAL))
            return failure;

    // look for a decoder
    QString name = QString((*Codec)->name) + "_mmal";
    if (name == "mpeg2video_mmal")
        name = "mpeg2_mmal";
    const AVCodec *codec = avcodec_find_decoder_by_name(name.toLocal8Bit());
    AvFormatDecoder *decoder = dynamic_cast<AvFormatDecoder*>(reinterpret_cast<DecoderBase*>((*Context)->opaque));
    if (!codec || !decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find %1").arg(name));
        return failure;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found MMAL/FFmpeg decoder '%1'").arg(name));
    *Codec = codec;    
    decoder->CodecMap()->FreeCodecContext(Stream);
    *Context = decoder->CodecMap()->GetCodecContext(Stream, *Codec);
    (*Context)->pix_fmt = decodeonly ? (*Context)->pix_fmt : AV_PIX_FMT_MMAL;
    return success;
}

void MythMMALContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (codec_is_mmal_dec(m_codecID))
    {
        DirectRendering = false;
        return;
    }
    else if (codec_is_mmal(m_codecID))
    {
        Context->get_format  = MythMMALContext::GetFormat;
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythMMALContext::RetrieveFrame(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame)
{
    if (codec_is_mmal_dec(m_codecID))
        return GetBuffer(Context, Frame, AvFrame, 0);
    else if (codec_is_mmal(m_codecID))
        return GetBuffer2(Context, Frame, AvFrame, 0);
    return false;
}


int MythMMALContext::HwDecoderInit(AVCodecContext *Context)
{
    if (!Context)
        return -1;

    if (codec_is_mmal_dec(m_codecID))
        return 0;

    if (!codec_is_mmal(m_codecID) || Context->pix_fmt != AV_PIX_FMT_MMAL)
        return -1;

    if (auto * player = GetPlayerUI(Context); player != nullptr)
        if (FrameTypeIsSupported(Context, FMT_MMAL))
            m_interop = MythMMALInterop::CreateMMAL(dynamic_cast<MythRenderOpenGL*>(player->GetRender()));

    return m_interop ? 0 : -1;
}

void MythMMALContext::SetDecoderOptions(AVCodecContext *Context, const AVCodec *Codec)
{
    if (!(codec_is_mmal(m_codecID)))
        return;
    if (!(Context && Codec))
        return;
    if (!(Codec->priv_class && Context->priv_data))
        return;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Setting number of extra buffers to 8");
    av_opt_set(Context->priv_data, "extra_buffers", "8", 0);
}

bool MythMMALContext::GetBuffer(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame, int)
{
    // Sanity checks
    if (!Context || !AvFrame || !Frame)
        return false;

    // Ensure we can render this format
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrameType type = MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(AvFrame->format));
    const VideoFrameTypes* supported = Frame->m_renderFormats;
    auto foundIt = std::find(supported->cbegin(), supported->cend(), type);
    // No fallback currently (unlikely)
    if (foundIt == supported->end())
        return false;

    // Re-allocate if necessary
    if ((Frame->m_type != type) || (Frame->m_width != AvFrame->width) || (Frame->m_height != AvFrame->height))
        if (!VideoBuffers::ReinitBuffer(Frame, type, decoder->GetVideoCodecID(), AvFrame->width, AvFrame->height))
            return false;

    // Copy data
    uint count = MythVideoFrame::GetNumPlanes(Frame->m_type);
    for (uint plane = 0; plane < count; ++plane)
    {
        MythVideoFrame::CopyPlane(Frame->m_buffer + Frame->m_offsets[plane], Frame->m_pitches[plane],
                                  AvFrame->data[plane], AvFrame->linesize[plane],
                                  MythVideoFrame::GetPitchForPlane(Frame->m_type, AvFrame->width, plane),
                                  MythVideoFrame::GetHeightForPlane(Frame->m_type, AvFrame->height, plane));
    }

    return true;
}

bool MythMMALContext::GetBuffer2(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame, int)
{
    // Sanity checks
    if (!Context || !AvFrame || !Frame || !m_interop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error");
        return false;
    }

    // MMAL?
    if (Frame->m_type != FMT_MMAL || static_cast<AVPixelFormat>(AvFrame->format) != AV_PIX_FMT_MMAL)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not an MMAL frame");
        return false;
    }

    Frame->m_width = AvFrame->width;
    Frame->m_height = AvFrame->height;
    Frame->m_pixFmt = Context->pix_fmt;
    Frame->m_swPixFmt = Context->sw_pix_fmt;
    Frame->m_directRendering = 1;
    AvFrame->opaque = Frame;

    // Frame->data[3] holds MMAL_BUFFER_HEADER_T
    Frame->m_buffer = AvFrame->data[3];
    // Retain the buffer so it is not released before we display it
    Frame->m_priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));
    // Add interop
    Frame->m_priv[1] = reinterpret_cast<unsigned char*>(m_interop);
    // Set the release method
    AvFrame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0, MythCodecContext::ReleaseBuffer,
                                       static_cast<AvFormatDecoder*>(Context->opaque), 0);
    return true;
}

AVPixelFormat MythMMALContext::GetFormat(AVCodecContext*, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_MMAL)
            return AV_PIX_FMT_MMAL;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

bool MythMMALContext::HaveMMAL(bool Reinit /*=false*/)
{
    static QRecursiveMutex lock;
    QMutexLocker locker(&lock);
    static bool s_checked = false;
    static bool s_available = false;

    if (s_checked && !Reinit)
        return s_available;
    s_checked = true;

    const MMALProfiles& profiles = MythMMALContext::GetProfiles();
    if (profiles.isEmpty())
        return s_available;

    LOG(VB_GENERAL, LOG_INFO, LOC + "Supported/available MMAL decoders:");
    s_available = true;
    QSize size{0, 0};
    for (auto profile : std::as_const(profiles))
        LOG(VB_GENERAL, LOG_INFO, LOC + MythCodecContext::GetProfileDescription(profile, size));
    return s_available;
}

void MythMMALContext::GetDecoderList(QStringList &Decoders)
{
    const MMALProfiles& profiles = MythMMALContext::GetProfiles();
    if (profiles.isEmpty())
        return;

    QSize size(0, 0);
    Decoders.append("MMAL:");
    for (MythCodecContext::CodecProfile profile : profiles)
        Decoders.append(MythCodecContext::GetProfileDescription(profile, size));
}

// Broadcom
extern "C" {
#include "interface/vmcs_host/vc_vchi_gencmd.h"
}

const MMALProfiles& MythMMALContext::GetProfiles(void)
{
    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static MMALProfiles s_profiles;

    QMutexLocker locker(&lock);
    if (s_initialised)
        return s_profiles;
    s_initialised = true;

    static const QPair<QString, MythCodecContext::CodecProfile> s_map[] =
    {
        { "MPG2", MythCodecContext::MPEG2 },
        { "MPG4", MythCodecContext::MPEG4 },
        { "WVC1", MythCodecContext::VC1   },
        { "H264", MythCodecContext::H264  }
    };

    vcos_init();
    VCHI_INSTANCE_T vchi_instance;
    if (vchi_initialise(&vchi_instance) != 0)
        return s_profiles;
    if (vchi_connect(nullptr, 0, vchi_instance) != 0)
        return s_profiles;
    VCHI_CONNECTION_T *vchi_connection = nullptr;
    vc_vchi_gencmd_init(vchi_instance, &vchi_connection, 1 );

    for (auto profile : s_map)
    {
        char command[32];
        char* response = nullptr;
        int responsesize = 0;
        QString msg = QString("codec_enabled %1").arg(profile.first);
        if (!vc_gencmd(command, sizeof(command), msg.toLocal8Bit().constData()))
            vc_gencmd_string_property(command, profile.first.toLocal8Bit().constData(), &response, &responsesize);

        if (response && responsesize && qstrcmp(response, "enabled") == 0)
            s_profiles.append(profile.second);
    }

    vc_gencmd_stop();
    vchi_disconnect(vchi_instance);

    return s_profiles;
}
