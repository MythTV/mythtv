// MythTV
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "avformatdecoder.h"
#include "mythnvdecinterop.h"
#include "mythvideoout.h"
#include "mythnvdeccontext.h"

extern "C" {
    #include "libavutil/opt.h"
}

#define LOC QString("NVDEC: ")

QMutex* MythNVDECContext::s_NVDECLock = new QMutex(QMutex::Recursive);
bool MythNVDECContext::s_NVDECAvailable = false;
vector<MythNVDECContext::MythNVDECCaps> MythNVDECContext::s_NVDECDecoderCaps = vector<MythNVDECContext::MythNVDECCaps>();

MythNVDECContext::MythNVDECContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

/*! \brief Determine whether NVDEC decoding is supported for this codec.
 *
 * The objective here is, as for other decoders, to fail fast so that we can fallback
 * to another decoder as soon as possible if necessary.
 *
 * \note Interop support for rendering is implicitly assumed as it only
 * requires an OpenGL context which is checked in HaveNVDEC()
*/
MythCodecID MythNVDECContext::GetSupportedCodec(AVCodecContext **Context,
                                                AVCodec       **Codec,
                                                const QString  &Decoder,
                                                AVStream       *Stream,
                                                uint            StreamType)
{
    bool decodeonly = Decoder == "nvdec-dec";
    auto success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_NVDEC_DEC : kCodec_MPEG1_NVDEC) + (StreamType - 1));
    auto failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    // no brainers
    if (!Decoder.startsWith("nvdec") || getenv("NO_NVDEC") || !HaveNVDEC() || IsUnsupportedProfile(*Context))
        return failure;

    QString codecstr = ff_codec_id_string((*Context)->codec_id);
    QString profile  = avcodec_profile_name((*Context)->codec_id, (*Context)->profile);
    QString pixfmt   = av_get_pix_fmt_name((*Context)->pix_fmt);

    // Check actual decoder capabilities. These are loaded statically and in a thread safe
    // manner in HaveNVDEC
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
    VideoFrameType type = PixelFormatToFrameType((*Context)->pix_fmt);
    // N.B. on stream changes format is set to CUDA/NVDEC. This may break if the new
    // stream has an unsupported chroma but the decoder should fail gracefully - just later.
    if ((FMT_NVDEC == type) || (format_is_420(type)))
        cudaformat = cudaVideoChromaFormat_420;
    else if (format_is_422(type))
        cudaformat = cudaVideoChromaFormat_422;
    else if (format_is_444(type))
        cudaformat = cudaVideoChromaFormat_444;

    uint depth = static_cast<uint>(ColorDepth(type) - 8);
    bool supported = false;

    if ((cudacodec != cudaVideoCodec_NumCodecs) && (cudaformat != cudaVideoChromaFormat_Monochrome))
    {
        // iterate over known decoder capabilities
        s_NVDECLock->lock();
        auto it = s_NVDECDecoderCaps.cbegin();
        for ( ; it != s_NVDECDecoderCaps.cend(); ++it)
        {
            if (((*it).m_codec == cudacodec) && ((*it).m_depth == depth) && ((*it).m_format == cudaformat))
            {
                // match - now check restrictions
                int width = (*Context)->width;
                int height = (*Context)->height;
                uint mblocks = static_cast<uint>((width * height) / 256);
                if (((*it).m_maximum.width() >= width) && ((*it).m_maximum.height() >= height) &&
                    ((*it).m_minimum.width() <= width) && ((*it).m_minimum.height() <= height) &&
                    ((*it).m_macroBlocks >= mblocks))
                {
                    supported = true;
                }
                else
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("Codec '%9' failed size constraints: source: %1x%2 min: %3x%4 max: %5x%6 mbs: %7, max %8")
                        .arg(width).arg(height).arg((*it).m_minimum.width()).arg((*it).m_minimum.height())
                        .arg((*it).m_maximum.width()).arg((*it).m_maximum.height()).arg(mblocks).arg((*it).m_macroBlocks)
                        .arg(get_encoding_type(success)));

                }
                break;
            }
        }
        s_NVDECLock->unlock();
    }

    // and finally try and retrieve the actual FFmpeg decoder
    if (supported)
    {
        for (int i = 0; ; i++)
        {
            const AVCodecHWConfig *config = avcodec_get_hw_config(*Codec, i);
            if (!config)
                break;

            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                (config->device_type == AV_HWDEVICE_TYPE_CUDA))
            {
                QString name = QString((*Codec)->name) + "_cuvid";
                if (name == "mpeg2video_cuvid")
                    name = "mpeg2_cuvid";
                AVCodec *codec = avcodec_find_decoder_by_name(name.toLocal8Bit());
                if (codec)
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2 %3 %4' depth %5")
                            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_CUDA)).arg(codecstr)
                            .arg(profile).arg(pixfmt).arg(depth + 8));
                    *Codec = codec;
                    gCodecMap->freeCodecContext(Stream);
                    *Context = gCodecMap->getCodecContext(Stream, *Codec);
                    return success;
                }
                break;
            }
        }
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support decoding '%2 %3 %4' depth %5")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_CUDA)).arg(codecstr)
            .arg(profile).arg(pixfmt).arg(depth + 8));
    return failure;
}

int MythNVDECContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!gCoreContext->IsUIThread() || !Context)
        return -1;

    // We need a player to release the interop
    MythPlayer *player = nullptr;
    auto *decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
    if (decoder)
        player = decoder->GetPlayer();
    if (!player)
        return -1;

    // Retrieve OpenGL render context
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;
    OpenGLLocker locker(render);

    // Check interop type
    if (MythOpenGLInterop::GetInteropType(FMT_NVDEC, player) == MythOpenGLInterop::Unsupported)
        return -1;

    // Create interop (and CUDA context)
    MythNVDECInterop *interop = MythNVDECInterop::Create(render);
    if (!interop)
        return -1;
    if (!interop->IsValid())
    {
        interop->DecrRef();
        return -1;
    }

    // Set player
    interop->SetPlayer(player);

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
                                        VideoDisplayProfile *Profile, bool DoubleRate)
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
        singlepref = MythVideoOutput::ParseDeinterlacer(Profile->GetSingleRatePreferences());
        if (DoubleRate)
            doublepref = MythVideoOutput::ParseDeinterlacer(Profile->GetDoubleRatePreferences());
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
                .arg(DeinterlacerName(deinterlacer | DEINT_DRIVER, DoubleRate, FMT_NVDEC)));
            m_deinterlacer = deinterlacer;
            m_deinterlacer2x = DoubleRate;
        }
    }
}

void MythNVDECContext::PostProcessFrame(AVCodecContext* /*Context*/, VideoFrame *Frame)
{
    // Remove interlacing flags and set deinterlacer if necessary
    if (Frame && m_deinterlacer)
    {
        Frame->interlaced_frame = 0;
        Frame->interlaced_reversed = 0;
        Frame->top_field_first = 0;
        Frame->deinterlace_inuse = m_deinterlacer | DEINT_DRIVER;
        Frame->deinterlace_inuse2x = m_deinterlacer2x;
    }
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

enum AVPixelFormat MythNVDECContext::GetFormat(AVCodecContext* /*Contextconst*/, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_CUDA)
            return AV_PIX_FMT_CUDA;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

bool MythNVDECContext::RetrieveFrame(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame)
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
bool MythNVDECContext::GetBuffer(struct AVCodecContext *Context, VideoFrame *Frame,
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
        Frame->pitches[i] = AvFrame->linesize[i];
        Frame->offsets[i] = AvFrame->data[i] ? (static_cast<int>(AvFrame->data[i] - AvFrame->data[0])) : 0;
        Frame->priv[i] = nullptr;
    }

    Frame->width = AvFrame->width;
    Frame->height = AvFrame->height;
    Frame->pix_fmt = Context->pix_fmt;
    Frame->directrendering = 1;

    AvFrame->opaque = Frame;
    AvFrame->reordered_opaque = Context->reordered_opaque;

    // set the pixel format - normally NV12 but P010 for 10bit etc. Set here rather than guessing later.
    if (AvFrame->hw_frames_ctx)
    {
        auto *context = reinterpret_cast<AVHWFramesContext*>(AvFrame->hw_frames_ctx->data);
        if (context)
            Frame->sw_pix_fmt = context->sw_format;
    }

    // NVDEC 'fixes' 10/12/16bit colour values
    Frame->colorshifted = 1;

    // Frame->data[0] holds CUdeviceptr for the frame data - offsets calculated above
    Frame->buf = AvFrame->data[0];

    // Retain the buffer so it is not released before we display it
    Frame->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));

    // We need the CUDA device context in the interop class and it also holds the reference
    // to the interop class itself
    Frame->priv[1] = reinterpret_cast<unsigned char*>(av_buffer_ref(Context->hw_device_ctx));

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
}

bool MythNVDECContext::HaveNVDEC(void)
{
    QMutexLocker locker(s_NVDECLock);
    static bool s_checked = false;
    if (!s_checked)
    {
        if (gCoreContext->IsUIThread())
            NVDECCheck();
        else
            LOG(VB_GENERAL, LOG_WARNING, LOC + "HaveNVDEC must be initialised from the main thread");
    }
    s_checked = true;
    return s_NVDECAvailable;
}

inline MythCodecID cuda_to_myth(cudaVideoCodec Codec)
{
    switch (Codec)
    {
        case cudaVideoCodec_MPEG1: return kCodec_MPEG1;
        case cudaVideoCodec_MPEG2: return kCodec_MPEG2;
        case cudaVideoCodec_MPEG4: return kCodec_MPEG4;
        case cudaVideoCodec_VC1:   return kCodec_VC1;
        case cudaVideoCodec_H264:  return kCodec_H264;
        case cudaVideoCodec_HEVC:  return kCodec_HEVC;
        case cudaVideoCodec_VP8:   return kCodec_VP8;
        case cudaVideoCodec_VP9:   return kCodec_VP9;
        default: break;
    }
    return kCodec_NONE;
}

inline VideoFrameType cuda_to_myth(cudaVideoChromaFormat Format)
{
    switch (Format)
    {
        case cudaVideoChromaFormat_420: return FMT_YV12;
        case cudaVideoChromaFormat_422: return FMT_YUV422P;
        case cudaVideoChromaFormat_444: return FMT_YUV444P;
        default: break;
    }
    return FMT_NONE;
}

/*! \brief Perform the actual NVDEC availability and capability check
 * \note lock is held in HaveNVDEC
*/
void MythNVDECContext::NVDECCheck(void)
{
    MythRenderOpenGL *opengl = MythRenderOpenGL::GetOpenGLRender();
    CUcontext     context = nullptr;
    CudaFunctions   *cuda = nullptr;
    if (MythNVDECInterop::CreateCUDAContext(opengl, cuda, context))
    {
        OpenGLLocker locker(opengl);
        CuvidFunctions *cuvid = nullptr;
        CUcontext dummy;
        cuda->cuCtxPushCurrent(context);

        if (cuvid_load_functions(&cuvid, nullptr) == 0)
        {
            // basic check passed
            LOG(VB_GENERAL, LOG_INFO, LOC + "NVDEC is available");
            s_NVDECAvailable = true;
            s_NVDECDecoderCaps.clear();

            if (cuvid->cuvidGetDecoderCaps)
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Decoder support check:");
            else
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
                            s_NVDECDecoderCaps.emplace_back(
                                    MythNVDECCaps(cudacodec, depth, cudaformat,
                                        QSize(caps.nMinWidth, caps.nMinHeight),
                                        QSize(static_cast<int>(caps.nMaxWidth), static_cast<int>(caps.nMaxHeight)),
                                        caps.nMaxMBCount));
                            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                                QString("Codec: %1: Depth: %2 Format: %3 Min: %4x%5 Max: %6x%7 MBs: %8")
                                    .arg(toString(cuda_to_myth(cudacodec))).arg(depth + 8)
                                    .arg(format_description(cuda_to_myth(cudaformat)))
                                    .arg(caps.nMinWidth).arg(caps.nMinHeight)
                                    .arg(caps.nMaxWidth).arg(caps.nMaxHeight).arg(caps.nMaxMBCount));
                        }
                        else if (!cuvid->cuvidGetDecoderCaps)
                        {
                            // dummy - just support everything:)
                            s_NVDECDecoderCaps.emplace_back(MythNVDECCaps(cudacodec, depth, cudaformat,
                                                                       QSize(32, 32), QSize(8192, 8192),
                                                                       (8192 * 8192) / 256));
                        }
                    }
                }
            }
            cuvid_free_functions(&cuvid);
        }
        cuda->cuCtxPopCurrent(&dummy);
    }
    MythNVDECInterop::CleanupContext(opengl, cuda, context);

    if (!s_NVDECAvailable)
        LOG(VB_GENERAL, LOG_INFO, LOC + "NVDEC functionality checked failed");
}
