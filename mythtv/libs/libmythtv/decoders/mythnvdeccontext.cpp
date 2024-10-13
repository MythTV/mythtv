// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"

#include "avformatdecoder.h"
#include "decoders/mythnvdeccontext.h"
#include "mythplayerui.h"
#include "mythvideoout.h"
#include "opengl/mythnvdecinterop.h"

extern "C" {
#include "libavutil/log.h"
#define FFNV_LOG_FUNC(logctx, msg, ...) av_log(logctx, AV_LOG_ERROR, msg,  __VA_ARGS__)
#define FFNV_DEBUG_LOG_FUNC(logctx, msg, ...) av_log(logctx, AV_LOG_DEBUG, msg,  __VA_ARGS__)
#include <ffnvcodec/dynlink_loader.h>
}

extern "C" {
#include "libavutil/hwcontext_cuda.h"
#include "libavutil/opt.h"
}

#define LOC QString("NVDEC: ")


MythNVDECContext::MythNVDECContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

MythNVDECContext::~MythNVDECContext()
{
    av_buffer_unref(&m_framesContext);
}

/*! \brief Determine whether NVDEC decoding is supported for this codec.
 *
 * The objective here is, as for other decoders, to fail fast so that we can fallback
 * to another decoder as soon as possible if necessary.
*/
MythCodecID MythNVDECContext::GetSupportedCodec(AVCodecContext **Context,
                                                const AVCodec **Codec,
                                                const QString  &Decoder,
                                                AVStream       *Stream,
                                                uint            StreamType)
{
    bool decodeonly = Decoder == "nvdec-dec";
    auto success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_NVDEC_DEC : kCodec_MPEG1_NVDEC) + (StreamType - 1));
    auto failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    // no brainers
    if (!Decoder.startsWith("nvdec") || qEnvironmentVariableIsSet("NO_NVDEC") || !HaveNVDEC() || IsUnsupportedProfile(*Context))
        return failure;

    if (!decodeonly)
        if (!FrameTypeIsSupported(*Context, FMT_NVDEC))
            return failure;

    QString codecstr = avcodec_get_name((*Context)->codec_id);
    QString profile  = avcodec_profile_name((*Context)->codec_id, (*Context)->profile);
    QString pixfmt   = av_get_pix_fmt_name((*Context)->pix_fmt);

    cudaVideoCodec cudacodec = cudaVideoCodec_NumCodecs;
    switch ((*Context)->codec_id)
    {
        case AV_CODEC_ID_MPEG1VIDEO: cudacodec = cudaVideoCodec_MPEG1; break;
        case AV_CODEC_ID_MPEG2VIDEO: cudacodec = cudaVideoCodec_MPEG2; break;
        case AV_CODEC_ID_MPEG4:      cudacodec = cudaVideoCodec_MPEG4; break;
        case AV_CODEC_ID_VC1:        cudacodec = cudaVideoCodec_VC1;   break;
        case AV_CODEC_ID_H264:       cudacodec = cudaVideoCodec_H264;  break;
        case AV_CODEC_ID_HEVC:       cudacodec = cudaVideoCodec_HEVC;  break;
        case AV_CODEC_ID_VP8:        cudacodec = cudaVideoCodec_VP8;   break;
        case AV_CODEC_ID_VP9:        cudacodec = cudaVideoCodec_VP9;   break;
        default:  break;
    }

    cudaVideoChromaFormat cudaformat = cudaVideoChromaFormat_Monochrome;
    VideoFrameType type = MythAVUtil::PixelFormatToFrameType((*Context)->pix_fmt);
    uint depth = static_cast<uint>(MythVideoFrame::ColorDepth(type) - 8);
    QString desc = QString("'%1 %2 %3 Depth:%4 %5x%6'")
            .arg(codecstr, profile, pixfmt).arg(depth + 8)
            .arg((*Context)->width).arg((*Context)->height);

    // N.B. on stream changes format is set to CUDA/NVDEC. This may break if the new
    // stream has an unsupported chroma but the decoder should fail gracefully - just later.
    if ((FMT_NVDEC == type) || (MythVideoFrame::FormatIs420(type)))
        cudaformat = cudaVideoChromaFormat_420;
    else if (MythVideoFrame::FormatIs422(type))
        cudaformat = cudaVideoChromaFormat_422;
    else if (MythVideoFrame::FormatIs444(type))
        cudaformat = cudaVideoChromaFormat_444;

    if ((cudacodec == cudaVideoCodec_NumCodecs) || (cudaformat == cudaVideoChromaFormat_Monochrome))
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "Unknown codec or format");
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("NVDEC does NOT support %1").arg(desc));
        return failure;
    }

    // iterate over known decoder capabilities
    const auto & profiles = MythNVDECContext::GetProfiles();
    auto capcheck = [&](const MythNVDECCaps& Cap)
        { return Cap.Supports(cudacodec, cudaformat, depth, (*Context)->width, (*Context)->height); };
    if (!std::any_of(profiles.cbegin(), profiles.cend(), capcheck))
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "No matching profile support");
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("NVDEC does NOT support %1").arg(desc));
        return failure;
    }

    auto *decoder = dynamic_cast<AvFormatDecoder*>(reinterpret_cast<DecoderBase*>((*Context)->opaque));
    // and finally try and retrieve the actual FFmpeg decoder
    QString name = QString((*Codec)->name) + "_cuvid";
    if (name == "mpeg2video_cuvid")
        name = "mpeg2_cuvid";
    for (int i = 0; ; i++)
    {
        const AVCodecHWConfig *config = avcodec_get_hw_config(*Codec, i);
        if (!config)
            break;

        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
            (config->device_type == AV_HWDEVICE_TYPE_CUDA))
        {
            const AVCodec *codec = avcodec_find_decoder_by_name(name.toLocal8Bit());
            if (codec)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("NVDEC supports decoding %1").arg(desc));
                *Codec = codec;
                decoder->CodecMap()->FreeCodecContext(Stream);
                *Context = decoder->CodecMap()->GetCodecContext(Stream, *Codec);
                return success;
            }
            break;
        }
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find decoder '%1'").arg(name));
    return failure;
}

int MythNVDECContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!gCoreContext->IsUIThread() || !Context)
        return -1;

    // We need a player to release the interop. As we are using direct rendering
    // it must be a MythPlayerUI instance
    auto * player = GetPlayerUI(Context);
    if (!player)
        return -1;

    // Retrieve OpenGL render context
    auto * render = dynamic_cast<MythRenderOpenGL*>(player->GetRender());
    if (!render)
        return -1;
    OpenGLLocker locker(render);

    // Create interop (and CUDA context)
    auto * interop = MythNVDECInterop::CreateNVDEC(player, render);
    if (!interop)
        return -1;
    if (!interop->IsValid())
    {
        interop->DecrRef();
        return -1;
    }

    // Allocate the device context
    AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    if (!hwdeviceref)
    {
        interop->DecrRef();
        return -1;
    }

    // Set release method, interop and CUDA context
    auto* hwdevicecontext = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    hwdevicecontext->free = &MythCodecContext::DeviceContextFinished;
    hwdevicecontext->user_opaque = interop;
    auto *devicehwctx = reinterpret_cast<AVCUDADeviceContext*>(hwdevicecontext->hwctx);
    devicehwctx->cuda_ctx = interop->GetCUDAContext();
    devicehwctx->stream = nullptr;

    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init CUDA hw device");
        av_buffer_unref(&hwdeviceref);
        return -1;
    }

    Context->hw_device_ctx = hwdeviceref;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created CUDA device context");
    return 0;
}

void MythNVDECContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (codec_is_nvdec_dec(m_codecID) || codec_is_nvdec(m_codecID))
    {
        Context->get_format = MythNVDECContext::GetFormat;
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

int MythNVDECContext::HwDecoderInit(AVCodecContext *Context)
{
    if (codec_is_nvdec(m_codecID))
    {
        return MythCodecContext::InitialiseDecoder2(Context, MythNVDECContext::InitialiseDecoder,
                                                    "Create NVDEC decoder");
    }
    if (codec_is_nvdec_dec(m_codecID))
    {
        AVBufferRef *context = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_CUDA, nullptr,
                                                              gCoreContext->GetSetting("NVDECDevice"));
        if (context)
        {
            Context->hw_device_ctx = context;
            return 0;
        }
    }
    return -1;
}

/*! \brief Enable NVDEC/CUDA deinterlacing if necessary
 *
 * \note We can only set this once just after codec initialisation. We do not
 * have control over the scan type - control is effectively passed to CUDA to detect
 * and handle all deinterlacing.
*/
void MythNVDECContext::SetDeinterlacing(AVCodecContext *Context,
                                        MythVideoProfile *Profile, bool DoubleRate)
{
    if (!Context)
        return;

    // Don't enable for anything that cannot be interlaced
    // We could use frame rate here but initial frame rate detection is not always accurate
    // and we lose little by enabling deinterlacing. NVDEC will not deinterlace a
    // progressive stream and any CUDA capable video card has memory to spare
    // (assuming it even sets up deinterlacing for a progressive stream)
    if (Context->height == 720) // 720P
        return;

    MythDeintType deinterlacer = DEINT_NONE;
    MythDeintType singlepref = DEINT_NONE;
    MythDeintType doublepref = DEINT_NONE;
    if (Profile)
    {
        singlepref = MythVideoFrame::ParseDeinterlacer(Profile->GetSingleRatePreferences());
        if (DoubleRate)
            doublepref = MythVideoFrame::ParseDeinterlacer(Profile->GetDoubleRatePreferences());
    }

    // Deinterlacers are not filtered (as we have no frame) - so mask appropriately
    MythDeintType singledeint = singlepref & (DEINT_BASIC | DEINT_MEDIUM | DEINT_HIGH);
    MythDeintType doubledeint = doublepref & (DEINT_BASIC | DEINT_MEDIUM | DEINT_HIGH);

    // With decode only/copy back, we can deinterlace here or the frames can use
    // either CPU or shader deints. So only accept a driver deinterlacer preference.

    // When direct rendering, there is no CPU option but shaders can be used.
    // Assume driver deints are higher quality so accept a driver preference or CPU
    // if shader is not preferred
    bool other = false;
    if (codec_is_nvdec_dec(m_codecID))
    {
        if (DoubleRate)
        {
            if (doubledeint)
            {
                if (doublepref & (DEINT_DRIVER))
                    deinterlacer = doubledeint;
                else if (doublepref & (DEINT_CPU | DEINT_SHADER))
                    other = true;
            }
            else
            {
                DoubleRate = false;
            }
        }

        if (!deinterlacer && !other && (singlepref & DEINT_DRIVER))
            deinterlacer = singledeint;
    }
    else if (codec_is_nvdec(m_codecID))
    {
        if (DoubleRate)
        {
            if (doubledeint)
            {
                if (doublepref & (DEINT_DRIVER | DEINT_CPU))
                    deinterlacer = doubledeint;
                else if (doublepref & DEINT_SHADER)
                    other = true;
            }
            else
            {
                DoubleRate = false;
            }
        }

        if (!deinterlacer && !other && singledeint)
        {
            if (singlepref & (DEINT_DRIVER | DEINT_CPU))
                deinterlacer = singledeint;
            else if (singlepref & DEINT_SHADER)
                { }
        }
    }

    if (deinterlacer == DEINT_NONE)
        return;

    QString mode = "adaptive";
    if (DEINT_BASIC == deinterlacer)
        mode = "bob";
    int result = av_opt_set(Context->priv_data, "deint", mode.toLocal8Bit(), 0);
    if (result == 0)
    {
        if (av_opt_set_int(Context->priv_data, "drop_second_field", static_cast<int>(!DoubleRate), 0) == 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Setup decoder deinterlacer '%1'")
                .arg(MythVideoFrame::DeinterlacerName(deinterlacer | DEINT_DRIVER, DoubleRate, FMT_NVDEC)));
            m_deinterlacer = deinterlacer;
            m_deinterlacer2x = DoubleRate;
        }
    }
}

void MythNVDECContext::PostProcessFrame(AVCodecContext* /*Context*/, MythVideoFrame *Frame)
{
    // Remove interlacing flags and set deinterlacer if necessary
    if (Frame && m_deinterlacer)
    {
        Frame->m_interlaced = false;
        Frame->m_interlacedReverse = false;
        Frame->m_topFieldFirst = false;
        Frame->m_deinterlaceInuse = m_deinterlacer | DEINT_DRIVER;
        Frame->m_deinterlaceInuse2x = m_deinterlacer2x;
        Frame->m_alreadyDeinterlaced = true;
    }
}

bool MythNVDECContext::DecoderWillResetOnFlush(void)
{
    return codec_is_nvdec(m_codecID);
}

bool MythNVDECContext::IsDeinterlacing(bool &DoubleRate, bool /*unused*/)
{
    if (m_deinterlacer != DEINT_NONE)
    {
        DoubleRate = m_deinterlacer2x;
        return true;
    }
    DoubleRate = false;
    return false;
}

enum AVPixelFormat MythNVDECContext::GetFormat(AVCodecContext* Context, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_CUDA)
        {
            auto * decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
            if (decoder)
            {
                auto * me = dynamic_cast<MythNVDECContext*>(decoder->GetMythCodecContext());
                if (me)
                    me->InitFramesContext(Context);
            }
            return AV_PIX_FMT_CUDA;
        }
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

bool MythNVDECContext::RetrieveFrame(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame)
{
    if (AvFrame->format != AV_PIX_FMT_CUDA)
        return false;
    if (codec_is_nvdec_dec(m_codecID))
        return RetrieveHWFrame(Frame, AvFrame);
    if (codec_is_nvdec(m_codecID))
        return GetBuffer(Context, Frame, AvFrame, 0);
    return false;
}

/*! \brief Convert AVFrame data to MythFrame
 *
 * \note The CUDA decoder returns a complete AVFrame which should represent an NV12
 * frame held in device (GPU) memory. There is no need to call avcodec_default_get_buffer2.
*/
bool MythNVDECContext::GetBuffer(struct AVCodecContext *Context, MythVideoFrame *Frame,
                                 AVFrame *AvFrame, int /*Flags*/)
{
    if ((AvFrame->format != AV_PIX_FMT_CUDA) || !AvFrame->data[0])
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not a valid CUDA hw frame");
        return false;
    }

    if (!Context || !Frame)
        return false;

    auto *decoder = static_cast<AvFormatDecoder*>(Context->opaque);

    for (int i = 0; i < 3; i++)
    {
        Frame->m_pitches[i] = AvFrame->linesize[i];
        Frame->m_offsets[i] = AvFrame->data[i] ? (static_cast<int>(AvFrame->data[i] - AvFrame->data[0])) : 0;
        Frame->m_priv[i] = nullptr;
    }

    Frame->m_width = AvFrame->width;
    Frame->m_height = AvFrame->height;
    Frame->m_pixFmt = Context->pix_fmt;
    Frame->m_directRendering = true;

    AvFrame->opaque = Frame;

    // set the pixel format - normally NV12 but P010 for 10bit etc. Set here rather than guessing later.
    if (AvFrame->hw_frames_ctx)
    {
        auto *context = reinterpret_cast<AVHWFramesContext*>(AvFrame->hw_frames_ctx->data);
        if (context)
            Frame->m_swPixFmt = context->sw_format;
    }

    // NVDEC 'fixes' 10/12/16bit colour values
    Frame->m_colorshifted = true;

    // Frame->data[0] holds CUdeviceptr for the frame data - offsets calculated above
    Frame->m_buffer = AvFrame->data[0];

    // Retain the buffer so it is not released before we display it
    Frame->m_priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));

    // We need the CUDA device context in the interop class and it also holds the reference
    // to the interop class itself
    Frame->m_priv[1] = reinterpret_cast<unsigned char*>(av_buffer_ref(Context->hw_device_ctx));

    // Set the release method
    AvFrame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0,
                                       MythCodecContext::ReleaseBuffer, decoder, 0);
    return true;
}

MythNVDECContext::MythNVDECCaps::MythNVDECCaps(cudaVideoCodec Codec, uint Depth, cudaVideoChromaFormat Format,
                                               QSize Minimum, QSize Maximum, uint MacroBlocks)
  : m_codec(Codec),
    m_depth(Depth),
    m_format(Format),
    m_minimum(Minimum),
    m_maximum(Maximum),
    m_macroBlocks(MacroBlocks)
{
    auto ToMythProfile = [](cudaVideoCodec CudaCodec)
    {
        switch (CudaCodec)
        {
            case cudaVideoCodec_MPEG1: return MythCodecContext::MPEG1;
            case cudaVideoCodec_MPEG2: return MythCodecContext::MPEG2;
            case cudaVideoCodec_MPEG4: return MythCodecContext::MPEG4;
            case cudaVideoCodec_VC1:   return MythCodecContext::VC1;
            case cudaVideoCodec_H264:  return MythCodecContext::H264;
            case cudaVideoCodec_HEVC:  return MythCodecContext::HEVC;
            case cudaVideoCodec_VP8:   return MythCodecContext::VP8;
            case cudaVideoCodec_VP9:   return MythCodecContext::VP9;
            default: break;
        }
        return MythCodecContext::NoProfile;
    };

    auto ToMythFormat = [](cudaVideoChromaFormat CudaFormat)
    {
        switch (CudaFormat)
        {
            case cudaVideoChromaFormat_420: return FMT_YV12;
            case cudaVideoChromaFormat_422: return FMT_YUV422P;
            case cudaVideoChromaFormat_444: return FMT_YUV444P;
            default: break;
        }
        return FMT_NONE;
    };
    m_profile = ToMythProfile(m_codec);
    m_type = ToMythFormat(m_format);
}

bool MythNVDECContext::MythNVDECCaps::Supports(cudaVideoCodec Codec, cudaVideoChromaFormat Format,
                                               uint Depth, int Width, int Height) const
{
    uint mblocks = static_cast<uint>((Width * Height) / 256);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Trying to match: Codec %1 Format %2 Depth %3 Width %4 Height %5 MBs %6")
            .arg(Codec).arg(Format).arg(Depth).arg(Width).arg(Height).arg(mblocks));
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("to this profile: Codec %1 Format %2 Depth %3 Width %4<->%5 Height %6<->%7 MBs %8")
            .arg(m_codec).arg(m_format).arg(m_depth)
            .arg(m_minimum.width()).arg(m_maximum.width())
            .arg(m_minimum.height()).arg(m_maximum.height()).arg(m_macroBlocks));

    bool result = (Codec == m_codec) && (Format == m_format) && (Depth == m_depth) &&
                  (m_maximum.width() >= Width) && (m_maximum.height() >= Height) &&
                  (m_minimum.width() <= Width) && (m_minimum.height() <= Height) &&
                  (m_macroBlocks >= mblocks);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("%1 Match").arg(result ? "" : "NO"));
    return result;
}

bool MythNVDECContext::HaveNVDEC(bool Reinit /*=false*/)
{
    static QRecursiveMutex lock;
    QMutexLocker locker(&lock);
    static bool s_checked = false;
    static bool s_available = false;
    if (!s_checked || Reinit)
    {
        if (gCoreContext->IsUIThread())
        {
            const std::vector<MythNVDECCaps>& profiles = MythNVDECContext::GetProfiles();
            if (profiles.empty())
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + "No NVDEC decoders found");
            }
            else
            {
                s_available = true;
                LOG(VB_GENERAL, LOG_INFO, LOC + "Supported/available NVDEC decoders:");
                for (const auto& profile : profiles)
                {
                    QString desc = MythCodecContext::GetProfileDescription(profile.m_profile,profile.m_maximum,
                                                                           profile.m_type, profile.m_depth + 8);
                    LOG(VB_GENERAL, LOG_INFO, LOC + desc + QString(" MBs: %1").arg(profile.m_macroBlocks));
                }
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "HaveNVDEC must be initialised from the main thread");
        }
    }
    s_checked = true;
    return s_available;
}


void MythNVDECContext::GetDecoderList(QStringList &Decoders)
{
    const std::vector<MythNVDECCaps>& profiles = MythNVDECContext::GetProfiles();
    if (profiles.empty())
        return;
    Decoders.append("NVDEC:");
    for (const auto& profile : profiles)
    {
        if (!(profile.m_depth % 2)) // Ignore 9/11bit etc
            Decoders.append(MythCodecContext::GetProfileDescription(profile.m_profile, profile.m_maximum,
                                                                    profile.m_type, profile.m_depth + 8));
    }
}

const std::vector<MythNVDECContext::MythNVDECCaps> &MythNVDECContext::GetProfiles(void)
{
    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static std::vector<MythNVDECContext::MythNVDECCaps> s_profiles;

    QMutexLocker locker(&lock);
    if (s_initialised)
        return s_profiles;
    s_initialised = true;

    MythRenderOpenGL *opengl = MythRenderOpenGL::GetOpenGLRender();
    CUcontext     context = nullptr;
    CudaFunctions   *cuda = nullptr;
    if (MythNVDECInterop::CreateCUDAContext(opengl, cuda, context))
    {
        OpenGLLocker gllocker(opengl);
        CuvidFunctions *cuvid = nullptr;
        CUcontext dummy = nullptr;
        cuda->cuCtxPushCurrent(context);

        if (cuvid_load_functions(&cuvid, nullptr) == 0)
        {
            // basic check passed
            if (!cuvid->cuvidGetDecoderCaps)
                LOG(VB_GENERAL, LOG_WARNING, LOC + "Old driver - cannot check decoder capabilities");

            // now iterate over codecs, depths and formats to check support
            for (int codec = cudaVideoCodec_MPEG1; codec < cudaVideoCodec_NumCodecs; ++codec)
            {
                auto cudacodec = static_cast<cudaVideoCodec>(codec);
                if (cudacodec == cudaVideoCodec_JPEG)
                    continue;
                for (int format = cudaVideoChromaFormat_420; format < cudaVideoChromaFormat_444; ++format)
                {
                    auto cudaformat = static_cast<cudaVideoChromaFormat>(format);
                    for (uint depth = 0; depth < 9; ++depth)
                    {
                        CUVIDDECODECAPS caps;
                        caps.eCodecType = cudacodec;
                        caps.eChromaFormat = cudaformat;
                        caps.nBitDepthMinus8 = depth;
                        // N.B. cuvidGetDecoderCaps was not available on older drivers
                        if (cuvid->cuvidGetDecoderCaps && (cuvid->cuvidGetDecoderCaps(&caps) == CUDA_SUCCESS) &&
                            caps.bIsSupported)
                        {
                            s_profiles.emplace_back(
                                    cudacodec, depth, cudaformat,
                                        QSize(caps.nMinWidth, caps.nMinHeight),
                                        QSize(static_cast<int>(caps.nMaxWidth), static_cast<int>(caps.nMaxHeight)),
                                        caps.nMaxMBCount);
                        }
                        else if (!cuvid->cuvidGetDecoderCaps)
                        {
                            // dummy - just support everything:)
                            s_profiles.emplace_back(cudacodec, depth, cudaformat,
                                                                  QSize(32, 32), QSize(8192, 8192),
                                                                  (8192 * 8192) / 256);
                        }
                    }
                }
            }
            cuvid_free_functions(&cuvid);
        }
        cuda->cuCtxPopCurrent(&dummy);
    }
    MythNVDECInterop::CleanupContext(opengl, cuda, context);

    return s_profiles;
}

void MythNVDECContext::InitFramesContext(AVCodecContext *Context)
{
    if (!Context)
        return;

    if (m_framesContext)
    {
        auto *frames = reinterpret_cast<AVHWFramesContext*>(m_framesContext->data);
        if ((frames->sw_format == Context->sw_pix_fmt) && (frames->width == Context->coded_width) &&
            (frames->height == Context->coded_height))
        {
            Context->hw_frames_ctx = av_buffer_ref(m_framesContext);
            return;
        }
    }

    // If this is a 'spontaneous' callback from FFmpeg (i.e. not on a stream change)
    // then we must release any direct render buffers.
    if (codec_is_nvdec(m_codecID) && m_parent->GetPlayer())
        m_parent->GetPlayer()->DiscardVideoFrames(true, true);

    av_buffer_unref(&m_framesContext);

    AVBufferRef* framesref = av_hwframe_ctx_alloc(Context->hw_device_ctx);
    auto *frames = reinterpret_cast<AVHWFramesContext*>(framesref->data);
    frames->free = MythCodecContext::FramesContextFinished;
    frames->user_opaque = nullptr;
    frames->sw_format = Context->sw_pix_fmt;
    frames->format    = AV_PIX_FMT_CUDA;
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
