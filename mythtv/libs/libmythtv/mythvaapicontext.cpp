// Qt
#include <QCoreApplication>
#include <QWaitCondition>

// Mythtv
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythlogging.h"
#include "mythrender_opengl.h"
#include "videobuffers.h"
#include "mythvaapiinterop.h"
#include "mythvaapicontext.h"

extern "C" {
#include "libavutil/hwcontext_vaapi.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("VAAPIDec: ")

MythVAAPIContext::MythVAAPIContext(MythCodecID CodecID)
  : MythCodecContext(CodecID)
{
}

int MythVAAPIContext::HwDecoderInit(AVCodecContext *Context)
{
    if (codec_is_vaapi(m_codecID))
    {
        return 0;
    }
    else if (codec_is_vaapi_dec(m_codecID))
    {

        AVBufferRef *device = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VAAPI,
                                                          gCoreContext->GetSetting("VAAPIDevice"));
        if (device)
        {
            Context->hw_device_ctx = device;
            return 0;
        }
    }
    return -1;
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

/*! \brief Confirm whether VAAPI support is available given Decoder and Context
 *
 * \todo Fix comparision of PixFmt against valid software formats - as PixFmt is
 * hard coded to YV420P in AvFormatDecoder
*/
MythCodecID MythVAAPIContext::GetSupportedCodec(AVCodecContext *Context,
                                                AVCodec **Codec,
                                                const QString &Decoder,
                                                int StreamType,
                                                AVPixelFormat &PixFmt)
{
    bool decodeonly = Decoder == "vaapi-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_VAAPI_DEC : kCodec_MPEG1_VAAPI) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!(Decoder == "vaapi" || Decoder == "vaapi-dec") || !HaveVAAPI() || getenv("NO_VAAPI"))
        return failure;

    // direct rendering needs interop support
    if (!decodeonly && (MythOpenGLInterop::GetInteropType(success) == MythOpenGLInterop::Unsupported))
        return failure;

    // check for actual decoder support
    AVBufferRef *hwdevicectx = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VAAPI, gCoreContext->GetSetting("VAAPIDevice"));
    if(!hwdevicectx)
        return failure;

    // Check for ironlake decode only - which won't work due to FFmpeg frame format
    // constraints. May apply to other platforms.
    AVHWDeviceContext    *device = reinterpret_cast<AVHWDeviceContext*>(hwdevicectx->data);
    AVVAAPIDeviceContext *hwctx  = reinterpret_cast<AVVAAPIDeviceContext*>(device->hwctx);

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

    bool foundhwfmt   = false;
    bool foundswfmt   = false;
    bool foundprofile = false;
    bool foundentry   = false;
    bool sizeok       = true;

    AVHWFramesConstraints *constraints = av_hwdevice_get_hwframe_constraints(hwdevicectx, nullptr);
    if (constraints)
    {
        if ((constraints->min_width > Context->width) || (constraints->min_height > Context->height))
            sizeok = false;
        if ((constraints->max_width < Context->width) || (constraints->max_height < Context->height))
            sizeok = false;

        enum AVPixelFormat *valid = constraints->valid_sw_formats;
        while (*valid != AV_PIX_FMT_NONE)
        {
            if (*valid == PixFmt)
            {
                foundswfmt = true;
                break;
            }
            valid++;
        }
        valid = constraints->valid_hw_formats;
        while (*valid != AV_PIX_FMT_NONE)
        {
            if (*valid == AV_PIX_FMT_VAAPI)
            {
                foundhwfmt = true;
                break;
            }
            valid++;
        }
        av_hwframe_constraints_free(&constraints);
    }

    // FFmpeg checks profiles very late and never checks entrypoints.
    int profilecount = vaMaxNumProfiles(hwctx->display);
    VAProfile desired = VAAPIProfileForCodec(Context);
    VAProfile *profilelist = static_cast<VAProfile*>(av_malloc_array(static_cast<size_t>(profilecount), sizeof(VAProfile)));
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
        VAEntrypoint *entrylist = static_cast<VAEntrypoint*>(av_malloc_array(static_cast<size_t>(entrysize), sizeof(VAEntrypoint)));
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
        if (foundentry && ((AV_PIX_FMT_YUVJ420P == Context->pix_fmt || AV_PIX_FMT_YUVJ422P == Context->pix_fmt ||
                            AV_PIX_FMT_YUVJ444P == Context->pix_fmt)))
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

    if (foundhwfmt && foundswfmt && foundprofile && sizeok && foundentry)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2 %3'")
                .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VAAPI)).arg((*Codec)->name)
                .arg(av_get_pix_fmt_name(Context->pix_fmt)));
        PixFmt = AV_PIX_FMT_VAAPI;
        return success;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("HW device type '%1' does not support '%2 %7' (format: %3 size: %4 profile: %5 entry: %6)")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VAAPI)).arg((*Codec)->name)
            .arg(foundswfmt).arg(sizeok).arg(foundprofile).arg(foundentry)
            .arg(av_get_pix_fmt_name(Context->pix_fmt)));
    return failure;
}

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

AVPixelFormat MythVAAPIContext::GetFormat2(AVCodecContext*, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VAAPI)
            return AV_PIX_FMT_VAAPI;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

int MythVAAPIContext::InitialiseContext(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return -1;

    MythCodecID vaapiid = static_cast<MythCodecID>(kCodec_MPEG1_VAAPI + (mpeg_version(Context->codec_id) - 1));
    MythOpenGLInterop::Type type = MythOpenGLInterop::GetInteropType(vaapiid);
    if (type == MythOpenGLInterop::Unsupported)
        return -1;

    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;

    AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!hwdeviceref || (hwdeviceref && !hwdeviceref->data))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware device context");
        return -1;
    }

    // set hardware device context - just needs a display
    AVHWDeviceContext* hwdevicecontext  = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || (hwdevicecontext && !hwdevicecontext->hwctx))
        return -1;
    AVVAAPIDeviceContext *vaapidevicectx = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevicecontext->hwctx);
    if (!vaapidevicectx)
        return -1;

    MythVAAPIInterop *interop = MythVAAPIInterop::Create(render, type);
    if (!interop->GetDisplay())
    {
        interop->DecrRef();
        av_buffer_unref(&hwdeviceref);
        return -1;
    }

    // set the display
    vaapidevicectx->display = interop->GetDisplay();

    // initialise hardware device context
    int res = av_hwdevice_ctx_init(hwdeviceref);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI hardware context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return res;
    }

    // allocate the hardware frames context for FFmpeg
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(hwdeviceref);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware frames context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return -1;
    }

    // setup the frames context
    // the frames context now holds the reference to MythVAAPIInterop
    // Set the callback to ensure it is released
    AVHWFramesContext* hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    AVVAAPIFramesContext* vaapi_frames_ctx = reinterpret_cast<AVVAAPIFramesContext*>(hw_frames_ctx->hwctx);

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
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Forcing surface format for %1 and %2 with driver '%3'")
            .arg(toString(vaapiid)).arg(MythOpenGLInterop::TypeToString(type)).arg(vendor));
    }

    VASurfaceAttrib prefs[3] = {
        { VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, { VAGenericValueTypeInteger, { format } } },
        { VASurfaceAttribUsageHint,   VA_SURFACE_ATTRIB_SETTABLE, { VAGenericValueTypeInteger, { VA_SURFACE_ATTRIB_USAGE_HINT_DISPLAY } } },
        { VASurfaceAttribMemoryType,  VA_SURFACE_ATTRIB_SETTABLE, { VAGenericValueTypeInteger, { VA_SURFACE_ATTRIB_MEM_TYPE_VA} } } };
    vaapi_frames_ctx->attributes = prefs;
    vaapi_frames_ctx->nb_attributes = 3;
    hw_frames_ctx->sw_format         = (Context->sw_pix_fmt == AV_PIX_FMT_YUV420P10) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
    hw_frames_ctx->initial_pool_size = static_cast<int>(VideoBuffers::GetNumBuffers(FMT_VAAPI));
    hw_frames_ctx->format            = AV_PIX_FMT_VAAPI;
    hw_frames_ctx->width             = Context->coded_width;
    hw_frames_ctx->height            = Context->coded_height;
    hw_frames_ctx->user_opaque       = interop;
    hw_frames_ctx->free              = &MythCodecContext::FramesContextFinished;
    res = av_hwframe_ctx_init(Context->hw_frames_ctx);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI frames context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return res;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VAAPI FFmpeg buffer pool created with %1 surfaces")
        .arg(vaapi_frames_ctx->nb_surfaces));
    av_buffer_unref(&hwdeviceref);
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
    static bool havevaapi = false;
    static bool checked   = false;
    if (checked && !ReCheck)
        return havevaapi;
    checked = true;

    AVBufferRef *context = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_VAAPI, gCoreContext->GetSetting("VAAPIDevice"));
    if (context)
    {
        AVHWDeviceContext    *hwdevice = reinterpret_cast<AVHWDeviceContext*>(context->data);
        AVVAAPIDeviceContext *hwctx    = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevice->hwctx);
        QString vendor(vaQueryVendorString(hwctx->display));
        if (vendor.contains("vdpau", Qt::CaseInsensitive))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI is using a VDPAU backend - ignoring VAAPI");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI is available");
            havevaapi = true;
        }
        av_buffer_unref(&context);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI functionality checked failed");
    }

    return havevaapi;
}
