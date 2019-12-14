// Qt
#include <QCoreApplication>
#include <QWaitCondition>

// Mythtv
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythlogging.h"
#include "decoders/avformatdecoder.h"
#include "opengl/mythrenderopengl.h"
#include "videobuffers.h"
#include "mythvaapiinterop.h"
#include "mythvaapicontext.h"

extern "C" {
#include "libavutil/hwcontext_vaapi.h"
#include "libavutil/pixdesc.h"
#include "libavfilter/buffersink.h"
}

#define LOC QString("VAAPIDec: ")

/*! \class MythVAAPIContext
 *
 * \todo Scaling errors when deinterlacing H.264 1080 and for 10bit HEVC 1080p.
 * \todo Fix crash when skipping to the end of an H.264 stream. Appears to be
 * because the decoder is partially initialised but we never feed it any packets
 * to complete the setup (as we have reached the end of the file).
 * Should be a simple null pointer check in FFmpeg.
*/
MythVAAPIContext::MythVAAPIContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

MythVAAPIContext::~MythVAAPIContext(void)
{
    DestroyDeinterlacer();
}

VAProfile MythVAAPIContext::VAAPIProfileForCodec(const AVCodecContext *Codec)
{
    if (!Codec)
        return VAProfileNone;

    switch (Codec->codec_id)
    {
        case AV_CODEC_ID_MPEG2VIDEO:
            switch (Codec->profile)
            {
                case FF_PROFILE_MPEG2_SIMPLE: return VAProfileMPEG2Simple;
                case FF_PROFILE_MPEG2_MAIN: return VAProfileMPEG2Main;
                default: break;
            }
            break;
        case AV_CODEC_ID_H263: return VAProfileH263Baseline;
        case AV_CODEC_ID_MPEG4:
            switch (Codec->profile)
            {
                case FF_PROFILE_MPEG4_SIMPLE: return VAProfileMPEG4Simple;
                case FF_PROFILE_MPEG4_ADVANCED_SIMPLE: return VAProfileMPEG4AdvancedSimple;
                case FF_PROFILE_MPEG4_MAIN: return VAProfileMPEG4Main;
                default: break;
            }
            break;
        case AV_CODEC_ID_H264:
            switch (Codec->profile)
            {
                case FF_PROFILE_H264_CONSTRAINED_BASELINE: return VAProfileH264ConstrainedBaseline;
                case FF_PROFILE_H264_MAIN: return VAProfileH264Main;
                case FF_PROFILE_H264_HIGH: return VAProfileH264High;
                default: break;
            }
            break;
        case AV_CODEC_ID_HEVC:
#if VA_CHECK_VERSION(0, 37, 0)
            switch (Codec->profile)
            {
                case FF_PROFILE_HEVC_MAIN: return VAProfileHEVCMain;
                case FF_PROFILE_HEVC_MAIN_10: return VAProfileHEVCMain10;
                default: break;
            }
#endif
            break;
        case AV_CODEC_ID_MJPEG: return VAProfileJPEGBaseline;
        case AV_CODEC_ID_WMV3:
        case AV_CODEC_ID_VC1:
            switch (Codec->profile)
            {
                case FF_PROFILE_VC1_SIMPLE: return VAProfileVC1Simple;
                case FF_PROFILE_VC1_MAIN: return VAProfileVC1Main;
                case FF_PROFILE_VC1_ADVANCED:
                case FF_PROFILE_VC1_COMPLEX: return VAProfileVC1Advanced;
                default: break;
            }
            break;
        case AV_CODEC_ID_VP8: return VAProfileVP8Version0_3;
        case AV_CODEC_ID_VP9:
            switch (Codec->profile)
            {
#if VA_CHECK_VERSION(0, 38, 0)
                case FF_PROFILE_VP9_0: return VAProfileVP9Profile0;
#endif
#if VA_CHECK_VERSION(0, 39, 0)
                case FF_PROFILE_VP9_2: return VAProfileVP9Profile2;
#endif
                default: break;
            }
            break;
        default: break;
    }

    return VAProfileNone;
}

inline AVPixelFormat MythVAAPIContext::FramesFormat(AVPixelFormat Format)
{
    switch (Format)
    {
        case AV_PIX_FMT_YUV420P10: return AV_PIX_FMT_P010;
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16: return AV_PIX_FMT_P016;
        default: return AV_PIX_FMT_NV12;
    }
}

/*! \brief Confirm whether VAAPI support is available given Decoder and Context
*/
MythCodecID MythVAAPIContext::GetSupportedCodec(AVCodecContext **Context,
                                                AVCodec ** /*Codec*/,
                                                const QString &Decoder,
                                                uint StreamType)
{
    bool decodeonly = Decoder == "vaapi-dec";
    auto success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_VAAPI_DEC : kCodec_MPEG1_VAAPI) + (StreamType - 1));
    auto failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!Decoder.startsWith("vaapi") || !HaveVAAPI() || getenv("NO_VAAPI"))
        return failure;

    QString codec   = ff_codec_id_string((*Context)->codec_id);
    QString profile = avcodec_profile_name((*Context)->codec_id, (*Context)->profile);
    QString pixfmt  = av_get_pix_fmt_name((*Context)->pix_fmt);

    // Simple check for known profile
    VAProfile desired = VAAPIProfileForCodec(*Context);
    if (desired == VAProfileNone)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VAAPI does not support decoding '%1 %2 %3'")
            .arg(codec).arg(profile).arg(pixfmt));
        return failure;
    }

    if (!decodeonly)
    {
        // If called from outside of the main thread, we need a MythPlayer instance to
        // process the interop check callback - which may fail otherwise (depending
        // on whether the result is cached).
        MythPlayer* player = nullptr;
        if (!gCoreContext->IsUIThread())
        {
            auto decoder = reinterpret_cast<AvFormatDecoder*>((*Context)->opaque);
            if (decoder)
                player = decoder->GetPlayer();
        }

        // Direct rendering needs interop support
        if (MythOpenGLInterop::GetInteropType(FMT_VAAPI, player) == MythOpenGLInterop::Unsupported)
            return failure;
    }

    // Check for actual decoder support
    AVBufferRef *hwdevicectx = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VAAPI, nullptr,
                                                              gCoreContext->GetSetting("VAAPIDevice"));
    if(!hwdevicectx)
        return failure;

    // Check for ironlake decode only - which won't work due to FFmpeg frame format
    // constraints. May apply to other platforms.
    auto *device = reinterpret_cast<AVHWDeviceContext*>(hwdevicectx->data);
    auto *hwctx  = reinterpret_cast<AVVAAPIDeviceContext*>(device->hwctx);

    if (decodeonly)
    {
        QString vendor = vaQueryVendorString(hwctx->display);
        if (vendor.contains("ironlake", Qt::CaseInsensitive))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Disallowing VAAPI decode only for Ironlake");
            av_buffer_unref(&hwdevicectx);
            return failure;
        }
    }

    bool foundprofile = false;
    bool foundentry   = false;
    bool sizeok       = true;

    VAConfigID config;
    if (vaCreateConfig(hwctx->display, desired, VAEntrypointVLD, nullptr, 0, &config) == VA_STATUS_SUCCESS)
    {
        auto *hwconfig = reinterpret_cast<AVVAAPIHWConfig*>(av_hwdevice_hwconfig_alloc(hwdevicectx));
        hwconfig->config_id = config;
        AVHWFramesConstraints *constraints = av_hwdevice_get_hwframe_constraints(hwdevicectx, hwconfig);
        vaDestroyConfig(hwctx->display, config);
        av_free(hwconfig);

        if (constraints)
        {
            if ((constraints->min_width > (*Context)->width) || (constraints->min_height > (*Context)->height))
                sizeok = false;
            if ((constraints->max_width < (*Context)->width) || (constraints->max_height < (*Context)->height))
                sizeok = false;
            av_hwframe_constraints_free(&constraints);
        }
    }

    // FFmpeg checks profiles very late and never checks entrypoints.
    int profilecount = vaMaxNumProfiles(hwctx->display);
    auto *profilelist = static_cast<VAProfile*>(av_malloc_array(static_cast<size_t>(profilecount), sizeof(VAProfile)));
    if (vaQueryConfigProfiles(hwctx->display, profilelist, &profilecount) == VA_STATUS_SUCCESS)
    {
        for (int i = 0; i < profilecount; ++i)
        {
            if (profilelist[i] == desired)
            {
                foundprofile = true;
                break;
            }
        }
    }
    av_freep(&profilelist);

    if (VAProfileNone != desired)
    {
        int count = 0;
        int entrysize = vaMaxNumEntrypoints(hwctx->display);
        auto *entrylist = static_cast<VAEntrypoint*>(av_malloc_array(static_cast<size_t>(entrysize), sizeof(VAEntrypoint)));
        if (vaQueryConfigEntrypoints(hwctx->display, desired, entrylist, &count) == VA_STATUS_SUCCESS)
        {
            for (int i = 0; i < count; ++i)
            {
                if (entrylist[i] == VAEntrypointVLD)
                {
                    foundentry = true;
                    break;
                }
            }
        }

        // use JPEG support as a proxy for MJPEG (full range YUV)
        if (foundentry && ((AV_PIX_FMT_YUVJ420P == (*Context)->pix_fmt || AV_PIX_FMT_YUVJ422P == (*Context)->pix_fmt ||
                            AV_PIX_FMT_YUVJ444P == (*Context)->pix_fmt)))
        {
            bool jpeg = false;
            if (vaQueryConfigEntrypoints(hwctx->display, VAProfileJPEGBaseline, entrylist, &count) == VA_STATUS_SUCCESS)
            {
                for (int i = 0; i < count; ++i)
                {
                    if (entrylist[i] == VAEntrypointVLD)
                    {
                        jpeg = true;
                        break;
                    }
                }
            }
            if (!jpeg)
                foundentry = false;
        }

        av_freep(&entrylist);
    }

    av_buffer_unref(&hwdevicectx);

    if (foundprofile && sizeok && foundentry)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2 %3 %4'")
                .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VAAPI)).arg(codec)
                .arg(profile).arg(pixfmt));
        (*Context)->pix_fmt = AV_PIX_FMT_VAAPI;
        return success;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("HW device type '%1' does not support '%2 %3 %4' (Size:%5 Profile:%6 Entry: %7)")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VAAPI))
            .arg(codec).arg(profile).arg(pixfmt)
            .arg(sizeok).arg(foundprofile).arg(foundentry));
    return failure;
}

/*! \note Creating the decoder here still causes issues on occasion (VP8) where multiple
 * contexts are created. They do appear to be properly released however. FFmpeg will simply
 * not accpet a context created before GetFormat
*/
AVPixelFormat MythVAAPIContext::GetFormat(AVCodecContext *Context, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VAAPI)
            if (MythCodecContext::InitialiseDecoder(Context, MythVAAPIContext::InitialiseContext, "VAAPI context creation") >= 0)
                return AV_PIX_FMT_VAAPI;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

AVPixelFormat MythVAAPIContext::GetFormat2(AVCodecContext *Context, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VAAPI)
            if (InitialiseContext2(Context) >= 0)
                return AV_PIX_FMT_VAAPI;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

/*! \brief Create a VAAPI hardware context with appropriate OpenGL interop.
*/
int MythVAAPIContext::InitialiseContext(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return -1;

    // We need a render device
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;

    // The interop must have a reference to the player so it can be deleted
    // from the main thread.
    MythPlayer *player = nullptr;
    auto *decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
    if (decoder)
        player = decoder->GetPlayer();
    if (!player)
        return -1;

    // Check interop support
    MythOpenGLInterop::Type type = MythOpenGLInterop::GetInteropType(FMT_VAAPI, player);
    if (type == MythOpenGLInterop::Unsupported)
        return -1;

    // Create interop
    MythVAAPIInterop *interop = MythVAAPIInterop::Create(render, type);
    if (!interop)
        return -1;
    if (!interop->GetDisplay())
    {
        interop->DecrRef();
        return -1;
    }

    // Set the player required to process interop release
    interop->SetPlayer(player);

    // Create hardware device context
    AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!hwdeviceref || (hwdeviceref && !hwdeviceref->data))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware device context");
        return -1;
    }

    auto* hwdevicecontext  = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || (hwdevicecontext && !hwdevicecontext->hwctx))
        return -1;
    auto *vaapidevicectx = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevicecontext->hwctx);
    if (!vaapidevicectx)
        return -1;

    // Set the display
    vaapidevicectx->display = interop->GetDisplay();

    // Initialise hardware device context
    int res = av_hwdevice_ctx_init(hwdeviceref);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI hardware context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return res;
    }

    // Allocate the hardware frames context for FFmpeg
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(hwdeviceref);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware frames context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return -1;
    }

    // Setup the frames context
    auto* hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    auto* vaapi_frames_ctx = reinterpret_cast<AVVAAPIFramesContext*>(hw_frames_ctx->hwctx);

    // Workarounds for specific drivers, surface formats and codecs
    // NV12 seems to work best across GPUs and codecs with the exception of
    // MPEG2 on Ironlake where it seems to return I420 labelled as NV12. I420 is
    // buggy on Sandybridge (stride?) and produces a mixture of I420/NV12 frames
    // for H.264 on Ironlake.
    int  format    = VA_FOURCC_NV12;
    QString vendor = interop->GetVendor();
    if (vendor.contains("ironlake", Qt::CaseInsensitive))
        if (CODEC_IS_MPEG(Context->codec_id))
            format = VA_FOURCC_I420;

    if (format != VA_FOURCC_NV12)
    {
        auto vaapiid = static_cast<MythCodecID>(kCodec_MPEG1_VAAPI + (mpeg_version(Context->codec_id) - 1));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Forcing surface format for %1 and %2 with driver '%3'")
            .arg(toString(vaapiid)).arg(MythOpenGLInterop::TypeToString(type)).arg(vendor));
    }

    VASurfaceAttrib prefs[3] = {
        { VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, { VAGenericValueTypeInteger, { format } } },
        { VASurfaceAttribUsageHint,   VA_SURFACE_ATTRIB_SETTABLE, { VAGenericValueTypeInteger, { VA_SURFACE_ATTRIB_USAGE_HINT_DISPLAY } } },
        { VASurfaceAttribMemoryType,  VA_SURFACE_ATTRIB_SETTABLE, { VAGenericValueTypeInteger, { VA_SURFACE_ATTRIB_MEM_TYPE_VA} } } };
    vaapi_frames_ctx->attributes = prefs;
    vaapi_frames_ctx->nb_attributes = 3;
    hw_frames_ctx->sw_format         = FramesFormat(Context->sw_pix_fmt);
    int referenceframes = AvFormatDecoder::GetMaxReferenceFrames(Context);
    hw_frames_ctx->initial_pool_size = static_cast<int>(VideoBuffers::GetNumBuffers(FMT_VAAPI, referenceframes, true));
    hw_frames_ctx->format            = AV_PIX_FMT_VAAPI;
    hw_frames_ctx->width             = Context->coded_width;
    hw_frames_ctx->height            = Context->coded_height;
    // The frames context now holds the reference to MythVAAPIInterop
    hw_frames_ctx->user_opaque       = interop;
    // Set the callback to ensure it is released
    hw_frames_ctx->free              = &MythCodecContext::FramesContextFinished;

    // Initialise hardwar frames context
    res = av_hwframe_ctx_init(Context->hw_frames_ctx);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI frames context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return res;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VAAPI FFmpeg buffer pool created with %1 %2x%3 surfaces (%4 references)")
        .arg(vaapi_frames_ctx->nb_surfaces).arg(Context->coded_width).arg(Context->coded_height)
        .arg(referenceframes));
    av_buffer_unref(&hwdeviceref);

    NewHardwareFramesContext();

    return 0;
}

/*! \brief Create a VAAPI hardware context without OpenGL interop.
 *
 * \note Testing with Debian Buster, 10bit copyback requires the i965-va-driver-shaders package
 * instead of i965-va-driver package. Expect a purple screen otherwise:) I suspect this
 * is due to some unnecessary scaling somewhere - as the relevant code should only be hit
 * if scaling is required.
*/
int MythVAAPIContext::InitialiseContext2(AVCodecContext *Context)
{
    if (!Context)
        return -1;

    AVBufferRef *device = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VAAPI, nullptr,
                                                      gCoreContext->GetSetting("VAAPIDevice"));
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(device);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware frames context");
        av_buffer_unref(&device);
        return -1;
    }

    int referenceframes = AvFormatDecoder::GetMaxReferenceFrames(Context);
    auto* hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    auto* vaapi_frames_ctx = reinterpret_cast<AVVAAPIFramesContext*>(hw_frames_ctx->hwctx);
    hw_frames_ctx->sw_format         = FramesFormat(Context->sw_pix_fmt);
    hw_frames_ctx->format            = AV_PIX_FMT_VAAPI;
    hw_frames_ctx->width             = Context->coded_width;
    hw_frames_ctx->height            = Context->coded_height;
    hw_frames_ctx->initial_pool_size = static_cast<int>(VideoBuffers::GetNumBuffers(FMT_VAAPI, referenceframes));
    hw_frames_ctx->free              = &MythCodecContext::FramesContextFinished;
    if (av_hwframe_ctx_init(Context->hw_frames_ctx) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI frames context");
        av_buffer_unref(&device);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return -1;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VAAPI FFmpeg buffer pool created with %1 %2x%3 surfaces (%4 references)")
        .arg(vaapi_frames_ctx->nb_surfaces).arg(Context->coded_width).arg(Context->coded_height)
        .arg(referenceframes));
    av_buffer_unref(&device);

    NewHardwareFramesContext();

    return 0;
}

/*! \brief Check whether VAAPI is available and not emulated via VDPAU
 *
 * The VDPAU backend appears to be largely unmaintained, does not expose the full
 * range of VDPAU functionality (deinterlacing, full colourspace handling etc) and in
 * testing fails when used by FFmpeg. So disallow VAAPI over VDPAU - VDPAU should just
 * be used directly.
*/
bool MythVAAPIContext::HaveVAAPI(bool ReCheck /*= false*/)
{
    static bool s_haveVaapi = false;
    static bool s_checked   = false;
    if (s_checked && !ReCheck)
        return s_haveVaapi;
    s_checked = true;

    AVBufferRef *context = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VAAPI, nullptr,
                                                          gCoreContext->GetSetting("VAAPIDevice"));
    if (context)
    {
        auto *hwdevice = reinterpret_cast<AVHWDeviceContext*>(context->data);
        auto *hwctx    = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevice->hwctx);
        QString vendor(vaQueryVendorString(hwctx->display));
        if (vendor.contains("vdpau", Qt::CaseInsensitive))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI is using a VDPAU backend - ignoring VAAPI");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI is available");
            s_haveVaapi = true;
        }
        av_buffer_unref(&context);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI functionality checked failed");
    }

    return s_haveVaapi;
}

void MythVAAPIContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (codec_is_vaapi(m_codecID))
    {
        Context->get_buffer2 = MythCodecContext::GetBuffer;
        Context->get_format  = MythVAAPIContext::GetFormat;
        return;
    }
    if (codec_is_vaapi_dec(m_codecID))
    {
        Context->get_format = MythVAAPIContext::GetFormat2;
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythVAAPIContext::RetrieveFrame(AVCodecContext* /*unused*/, VideoFrame *Frame, AVFrame *AvFrame)
{
    if (AvFrame->format != AV_PIX_FMT_VAAPI)
        return false;
    if (codec_is_vaapi_dec(m_codecID))
        return RetrieveHWFrame(Frame, AvFrame);
    return false;
}

/*! \brief Retrieve decoded frame and optionally deinterlace.
 *
 * \note Deinterlacing is setup in PostProcessFrame which has
 * access to deinterlacer preferences.
 * \note In testing, A/V sync is broken for a stream with timestamps that increment
 * by 1 (when doublerate deinterlacing). This is presumably an anomaly. NVDEC
 * doublerate deinterlacers produce the same problem.
*/
int MythVAAPIContext::FilteredReceiveFrame(AVCodecContext *Context, AVFrame *Frame)
{
    int ret = 0;

    while (true)
    {
        if (m_filterGraph && ((m_filterWidth != Context->coded_width) || (m_filterHeight != Context->coded_height)))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Input changed - deleting filter");
            DestroyDeinterlacer();
        }

        if (m_filterGraph)
        {
            ret = av_buffersink_get_frame(m_filterSink, Frame);
            if  (ret >= 0)
            {
                if (m_filterPriorPTS[0] && m_filterPTSUsed == m_filterPriorPTS[1])
                {
                    Frame->pts = m_filterPriorPTS[1] + (m_filterPriorPTS[1] - m_filterPriorPTS[0]) / 2;
                    Frame->scte_cc_len = 0;
                    Frame->atsc_cc_len = 0;
                    av_frame_remove_side_data(Frame, AV_FRAME_DATA_A53_CC);
                }
                else
                {
                    Frame->pts = m_filterPriorPTS[1];
                    m_filterPTSUsed = m_filterPriorPTS[1];
                }
            }
            if (ret != AVERROR(EAGAIN))
                break;
        }

        // EAGAIN or no filter graph
        ret = avcodec_receive_frame(Context, Frame);
        if (ret == 0)
        {
            // preserve interlaced flags
            m_lastInterlaced = Frame->interlaced_frame;
            m_lastTopFieldFirst = Frame->top_field_first;
        }

        if (ret < 0)
            break;

        // some streams with missing timestamps break frame timing when doublerate
        // so replace with dts if available
        int64_t pts = Frame->pts;
        if (pts == AV_NOPTS_VALUE && Frame->pkt_dts != AV_NOPTS_VALUE && m_deinterlacer2x)
            pts = Frame->pkt_dts;
        m_filterPriorPTS[0] = m_filterPriorPTS[1];
        m_filterPriorPTS[1] = pts;

        if (!m_filterGraph)
            break;

        ret = av_buffersrc_add_frame(m_filterSource, Frame);
        if (ret < 0)
            break;
    }

    return ret;
}

void MythVAAPIContext::PostProcessFrame(AVCodecContext* Context, VideoFrame *Frame)
{
    if (!Frame || !codec_is_vaapi_dec(m_codecID) || !Context->hw_frames_ctx)
        return;

    // if VAAPI driver deints are errored or not available (older boards), then
    // allow CPU/GLSL
    if (m_filterError)
    {
        Frame->deinterlace_allowed = Frame->deinterlace_allowed & ~DEINT_DRIVER;
        return;
    }
    if (kCodec_HEVC_VAAPI_DEC == m_codecID || kCodec_VP9_VAAPI_DEC == m_codecID ||
        kCodec_VP8_VAAPI_DEC == m_codecID)
    {
        // enabling VPP deinterlacing with these codecs breaks decoding for some reason.
        // HEVC interlacing is not currently detected by FFmpeg and I can't find
        // any interlaced VP8/9 material. Shaders and/or CPU deints will be available
        // as appropriate
        m_filterError = true;
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Disabling VAAPI VPP deinterlacer for %1")
            .arg(toString(m_codecID)));
        return;
    }

    // if this frame has already been deinterlaced, then flag the deinterlacer and
    // that the frame has already been deinterlaced.
    // the FFmpeg vaapi deint filter will mark frames as progressive, so restore the
    // interlaced flags to ensure auto deinterlacing continues to work
    if (m_deinterlacer)
    {
        Frame->interlaced_frame = m_lastInterlaced;
        Frame->top_field_first = m_lastTopFieldFirst;
        Frame->deinterlace_inuse = m_deinterlacer | DEINT_DRIVER;
        Frame->deinterlace_inuse2x = m_deinterlacer2x;
        Frame->decoder_deinterlaced = 1;
    }

    // N.B. this picks up the scan tracking in MythPlayer. So we can
    // auto enable deinterlacing etc and override Progressive/Interlaced - but
    // no reversed interlaced.
    MythDeintType vaapideint = DEINT_NONE;
    MythDeintType singlepref = GetSingleRateOption(Frame, DEINT_DRIVER);
    MythDeintType doublepref = GetDoubleRateOption(Frame, DEINT_DRIVER);
    bool doublerate = true;
    bool other = false;

    // For decode only, a CPU or shader deint may also be used/preferred
    if (doublepref)
        vaapideint = doublepref;
    else if (GetDoubleRateOption(Frame, DEINT_CPU | DEINT_SHADER))
        other = true;

    if (!vaapideint && !other && singlepref)
    {
        doublerate = false;
        vaapideint = singlepref;
    }

    // nothing to see
    if (vaapideint == DEINT_NONE)
    {
        if (m_deinterlacer)
            DestroyDeinterlacer();
        return;
    }

    // already setup
    if ((m_deinterlacer == vaapideint) && (m_deinterlacer2x == doublerate))
        return;

    // Start from scratch
    DestroyDeinterlacer();
    m_framesCtx = av_buffer_ref(Context->hw_frames_ctx);
    if (!MythVAAPIInterop::SetupDeinterlacer(vaapideint, doublerate, Context->hw_frames_ctx,
                                             Context->coded_width, Context->coded_height,
                                             m_filterGraph, m_filterSource, m_filterSink))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create deinterlacer %1 - disabling")
            .arg(DeinterlacerName(vaapideint | DEINT_DRIVER, doublerate, FMT_VAAPI)));
        DestroyDeinterlacer();
        m_filterError = true;
    }
    else
    {
        m_deinterlacer = vaapideint;
        m_deinterlacer2x = doublerate;
        m_filterWidth = Context->coded_width;
        m_filterHeight = Context->coded_height;
    }
}

bool MythVAAPIContext::IsDeinterlacing(bool &DoubleRate, bool StreamChange)
{
    // the VAAPI deinterlacer can be turned on and off, so on stream changes
    // return false to ensure auto deint works for the new format (the deinterlacer
    // refers to the current format)
    if (m_deinterlacer && !StreamChange)
    {
        DoubleRate = m_deinterlacer2x;
        return true;
    }
    DoubleRate = false;
    return false;
}

bool MythVAAPIContext::DecoderWillResetOnFlush(void)
{
    // HEVC appears to be OK
    return kCodec_H264_VAAPI == m_codecID;
}

bool MythVAAPIContext::DecoderWillResetOnAspect(void)
{
    // Only MPEG2 tested so far
    return (kCodec_MPEG2_VAAPI ==  m_codecID) || (kCodec_MPEG2_VAAPI_DEC == m_codecID);
}

void MythVAAPIContext::DestroyDeinterlacer(void)
{
    if (m_filterGraph)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Destroying VAAPI deinterlacer");
    avfilter_graph_free(&m_filterGraph);
    m_filterGraph = nullptr;
    m_filterSink = nullptr;
    m_filterSource = nullptr;
    m_filterPTSUsed = 0;
    m_filterPriorPTS[0] = 0;
    m_filterPriorPTS[1] = 0;
    m_filterWidth = 0;
    m_filterHeight = 0;
    if (m_framesCtx)
        av_buffer_unref(&m_framesCtx);
    m_deinterlacer = DEINT_NONE;
    m_deinterlacer2x = false;
}
