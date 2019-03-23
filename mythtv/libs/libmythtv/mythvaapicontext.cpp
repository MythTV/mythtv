// Qt
#include <QCoreApplication>
#include <QWaitCondition>

// Mythtv
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythlogging.h"
#include "mythcodecid.h"
#include "mythframe.h"
#include "avformatdecoder.h"
#include "mythrender_opengl.h"
#include "videobuffers.h"
#include "mythvaapiinterop.h"
#include "mythvaapicontext.h"

extern "C" {
#include "libavutil/hwcontext_vaapi.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("VAAPIDec: ")

void MythVAAPIContext::DestroyInteropCallback(void *Wait, void *Interop, void *)
{
    MythOpenGLInterop *interop = static_cast<MythOpenGLInterop*>(Interop);
    QWaitCondition    *wait    = static_cast<QWaitCondition*>(Wait);
    if (interop)
        interop->DecrRef();
    if (wait)
        wait->wakeAll();
}

void MythVAAPIContext::FramesContextFinished(AVHWFramesContext *Context)
{
    if (!Context)
        return;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "VAAPI frames context destroyed");

    MythVAAPIInterop *interop = reinterpret_cast<MythVAAPIInterop*>(Context->user_opaque);
    if (interop && gCoreContext->IsUIThread())
        interop->DecrRef();
    else if (interop)
        MythMainWindow::HandleCallback("Destroy VAAPI interop", &DestroyInteropCallback, interop, nullptr);
}

void MythVAAPIContext::DeviceContextFinished(AVHWDeviceContext*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "VAAPI device context destroyed");
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

MythCodecID MythVAAPIContext::GetSupportedCodec(AVCodecContext *CodecContext,
                                            AVCodec **Codec,
                                            const QString &Decoder,
                                            uint StreamType,
                                            AVPixelFormat &PixFmt)
{
    MythCodecID success = static_cast<MythCodecID>(kCodec_MPEG1_VAAPI + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (Decoder != "vaapi")
        return failure;

    if (MythOpenGLInterop::GetInteropType(success) == MythOpenGLInterop::Unsupported)
        return failure;

    // check for actual decoder support
    AVBufferRef *hwdevicectx = nullptr;
    if (av_hwdevice_ctx_create(&hwdevicectx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0) < 0)
        return failure;

    bool foundhwfmt   = false;
    bool foundswfmt   = false;
    bool foundprofile = false;
    bool foundentry   = false;
    bool sizeok       = true;

    AVHWFramesConstraints *constraints = av_hwdevice_get_hwframe_constraints(hwdevicectx, nullptr);
    if (constraints)
    {
        if ((constraints->min_width > CodecContext->width) || (constraints->min_height > CodecContext->height))
            sizeok = false;
        if ((constraints->max_width < CodecContext->width) || (constraints->max_height < CodecContext->height))
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
    AVHWDeviceContext    *device = reinterpret_cast<AVHWDeviceContext*>(hwdevicectx->data);
    AVVAAPIDeviceContext *hwctx  = reinterpret_cast<AVVAAPIDeviceContext*>(device->hwctx);
    int profilecount = vaMaxNumProfiles(hwctx->display);
    VAProfile desired = VAAPIProfileForCodec(CodecContext);
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
        av_freep(&entrylist);
    }

    av_buffer_unref(&hwdevicectx);

    if (foundhwfmt && foundswfmt && foundprofile && sizeok && foundentry)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2'")
                .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VAAPI)).arg((*Codec)->name));
        PixFmt = AV_PIX_FMT_VAAPI;
        return success;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support '%2' (format: %3 size: %4 profile: %5 entry: %6)")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VAAPI)).arg((*Codec)->name)
            .arg(foundswfmt).arg(sizeok).arg(foundprofile).arg(foundentry));
    return failure;
}

enum AVPixelFormat MythVAAPIContext::GetFormat(struct AVCodecContext *Context,
                                           const enum AVPixelFormat *PixFmt)
{
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_VAAPI)
            if (MythVAAPIContext::InitialiseDecoder(Context) >= 0)
                return *PixFmt;
        PixFmt++;
    }
    return ret;
}

int MythVAAPIContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int Flags)
{
    AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    // set fields required for directrendering
    for (int i = 0; i < 4; i++)
    {
        Frame->data[i]     = nullptr;
        Frame->linesize[i] = 0;
    }
    Frame->opaque           = videoframe;
    videoframe->pix_fmt     = Context->pix_fmt;
    Frame->reordered_opaque = Context->reordered_opaque;

    int ret = avcodec_default_get_buffer2(Context, Frame, Flags);
    if (ret < 0)
        return ret;

    // avcodec_default_get_buffer2 will retrieve an AVBufferRef from the pool of
    // VAAPI surfaces stored within AVHWFramesContext. The pointer to VASurfaceID is stored
    // in Frame->data[3]. Store this in VideoFrame::buf for MythVAAPIInterop to use.
    videoframe->buf = Frame->data[3];
    // Frame->buf(0) also contains a reference to the buffer. Take an additional reference to this
    // buffer to retain the surface until it has been displayed (otherwise it is
    // reused once the decoder is finished with it).
    videoframe->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->buf[0]));
    // frame->hw_frames_ctx contains a reference to the AVHWFramesContext. Take an additional
    // reference to ensure AVHWFramesContext is not released until we are finished with it.
    // This also contains the underlying MythOpenGLInterop class reference.
    videoframe->priv[1] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->hw_frames_ctx));

    // Set release method
    Frame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(videoframe), 0,
                                     MythVAAPIContext::ReleaseBuffer, avfd, 0);
    return ret;
}

void MythVAAPIContext::ReleaseBuffer(void *Opaque, uint8_t *Data)
{
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Opaque);
    VideoFrame *frame = reinterpret_cast<VideoFrame*>(Data);
    if (decoder && decoder->GetPlayer())
        decoder->GetPlayer()->DeLimboFrame(frame);
}

void MythVAAPIContext::InitialiseContext(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return;

    MythCodecID vaapiid = static_cast<MythCodecID>(kCodec_MPEG1_VAAPI + (mpeg_version(Context->codec_id) - 1));
    MythOpenGLInterop::Type type = MythOpenGLInterop::GetInteropType(vaapiid);
    if (type == MythOpenGLInterop::Unsupported)
        return;

    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return;

    AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!hwdeviceref || (hwdeviceref && !hwdeviceref->data))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware device context");
        return;
    }

    // set hardware device context - just needs a display
    AVHWDeviceContext* hwdevicecontext  = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || (hwdevicecontext && !hwdevicecontext->hwctx))
        return;
    AVVAAPIDeviceContext *vaapidevicectx = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevicecontext->hwctx);
    if (!vaapidevicectx)
        return;

    MythVAAPIInterop *interop = MythVAAPIInterop::Create(render, type);
    if (!interop->GetDisplay())
    {
        interop->DecrRef();
        av_buffer_unref(&hwdeviceref);
        return;
    }

    // set the display and ensure it is closed when the device context is finished
    hwdevicecontext->free   = &MythVAAPIContext::DeviceContextFinished;
    vaapidevicectx->display = interop->GetDisplay();

    // initialise hardware device context
    int res = av_hwdevice_ctx_init(hwdeviceref);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI hardware context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return;
    }

    // allocate the hardware frames context for FFmpeg
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(hwdeviceref);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware frames context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return;
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

    hw_frames_ctx->sw_format         = Context->sw_pix_fmt;
    hw_frames_ctx->initial_pool_size = static_cast<int>(VideoBuffers::GetNumBuffers(FMT_VAAPI));
    hw_frames_ctx->format            = AV_PIX_FMT_VAAPI;
    hw_frames_ctx->width             = Context->coded_width;
    hw_frames_ctx->height            = Context->coded_height;
    hw_frames_ctx->user_opaque       = interop;
    hw_frames_ctx->free              = &MythVAAPIContext::FramesContextFinished;
    res = av_hwframe_ctx_init(Context->hw_frames_ctx);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI frames context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        return;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("VAAPI FFmpeg buffer pool created with %1 surfaces")
        .arg(vaapi_frames_ctx->nb_surfaces));
    av_buffer_unref(&hwdeviceref);
}

void MythVAAPIContext::InitialiseDecoderCallback(void* Wait, void* Context, void*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Hardware decoder callback");
    QWaitCondition* wait    = reinterpret_cast<QWaitCondition*>(Wait);
    AVCodecContext* context = reinterpret_cast<AVCodecContext*>(Context);
    InitialiseContext(context);
    if (wait)
        wait->wakeAll();
}

int MythVAAPIContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!Context)
        return -1;

    if (gCoreContext->IsUIThread())
        InitialiseContext(Context);
    else
        MythMainWindow::HandleCallback("VAAPI context creation", &InitialiseDecoderCallback, Context, nullptr);

    return Context->hw_frames_ctx ? 0 : -1;
}
