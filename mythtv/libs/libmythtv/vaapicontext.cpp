// Qt
#include <QCoreApplication>
#include <QWaitCondition>

// Mythtv
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythlogging.h"
#include "mythcodecid.h"
#include "mythframe.h"
#include "mythrender_opengl.h"
#include "mythopenglinterop.h"
#include "videobuffers.h"
#include "vaapicontext.h"

extern "C" {
#include "libavutil/hwcontext_vaapi.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("VAAPIHWCtx: ")
#define ERR QString("VAAPIHWCtx Error: ")

MythVAAPIDisplay::MythVAAPIDisplay(MythRenderOpenGL *Context)
  : ReferenceCounter("VAAPIDisplay"),
    m_context(Context),
    m_vaDisplay(nullptr)
{
    if (!m_context)
      return;
    m_context->IncrRef();

    OpenGLLocker locker(m_context);
    Display *display = glXGetCurrentDisplay();

    if (!display)
    {
      LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open GLX display");
      return;
    }

    m_vaDisplay = vaGetDisplayGLX(display);

    if (!m_vaDisplay)
    {
      LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VADisplay");
      return;
    }

    int major_ver, minor_ver;
    if (vaInitialize(m_vaDisplay, &major_ver, &minor_ver) != VA_STATUS_SUCCESS)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI GLX display");
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created VAAPI GLX display");
}

MythVAAPIDisplay::~MythVAAPIDisplay()
{
    if (!m_context)
        return;

    if (m_vaDisplay)
    {
        if (gCoreContext->IsUIThread())
        {
            MythVAAPIDisplayDestroy(m_context, m_vaDisplay);
        }
        else
        {
            MythMainWindow *window = MythMainWindow::getMainWindow();
            if (!window)
            {
                QWaitCondition wait;
                QMutex lock;
                lock.lock();
                MythCallbackEvent *event = new MythCallbackEvent(&MythVAAPIDisplayDestroyCallback, m_context, &wait, m_vaDisplay);
                QCoreApplication::postEvent(window, event, Qt::HighEventPriority);
                int count = 0;
                while (!wait.wait(&lock, 100) && ((count += 100) < 1100))
                    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Waited %1ms to destroy display").arg(count));
                lock.unlock();
            }
        }
    }
    m_context->DecrRef();
}

void MythVAAPIDisplay::MythVAAPIDisplayDestroy(MythRenderOpenGL *Context, VADisplay Display)
{
    if (!Context || !Display || !gCoreContext->IsUIThread())
        return;

    Context->makeCurrent();
    if(vaTerminate(Display) != VA_STATUS_SUCCESS)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Error closing VAAPI GLX display");
    Context->doneCurrent();
    LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI GLX display closed");
}

void MythVAAPIDisplay::MythVAAPIDisplayDestroyCallback(void *Context, void *Wait, void *Display)
{
    MythRenderOpenGL *context = static_cast<MythRenderOpenGL*>(Context);
    QWaitCondition   *wait    = static_cast<QWaitCondition*>(Wait);
    VADisplay        *display = static_cast<VADisplay*>(Display);
    MythVAAPIDisplayDestroy(context, display);
    if (wait)
        wait->wakeAll();
}

void VAAPIContext::HWFramesContextFinished(AVHWFramesContext *Context)
{
    if (!Context)
        return;
    LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI frames context destroyed");
    MythVAAPIDisplay* display = reinterpret_cast<MythVAAPIDisplay*>(Context->user_opaque);
    if (display)
        display->DecrRef();
}

void VAAPIContext::HWDeviceContextFinished(AVHWDeviceContext*)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI device context destroyed");
}

VAProfile VAAPIContext::VAAPIProfileForCodec(const AVCodecContext *Codec)
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

MythCodecID VAAPIContext::GetBestSupportedCodec(AVCodecContext *CodecContext,
                                                AVCodec **Codec,
                                                const QString &Decoder,
                                                uint StreamType,
                                                AVPixelFormat &PixFmt)
{
    MythCodecID result = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));
    if (Decoder != "vaapi")
        return result;

    if (!MythOpenGLInterop::IsCodecSupported(kCodec_MPEG2_VAAPI))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Render device does not support VAAPI");
        return result;
    }

    // check for actual decoder support
    AVBufferRef *hwdevicectx = nullptr;
    if (av_hwdevice_ctx_create(&hwdevicectx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0) < 0)
        return result;

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
        return static_cast<MythCodecID>(kCodec_MPEG1_VAAPI + (StreamType - 1));
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support '%2' (format: %3 size: %4 profile: %5 entry: %6)")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VAAPI)).arg((*Codec)->name)
            .arg(foundswfmt).arg(sizeok).arg(foundprofile).arg(foundentry));
    return result;
}

void VAAPIContext::InitVAAPIContext(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return;

    MythMainWindow* window = MythMainWindow::getMainWindow();
    if (!window)
        return;
    MythRenderOpenGL* render = dynamic_cast<MythRenderOpenGL*>(window->GetRenderDevice());
    if (!render)
        return;

    AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!hwdeviceref || (hwdeviceref && !hwdeviceref->data))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware device context");
        return;
    }

    // set hardware device context - just needs a display
    AVHWDeviceContext*    hwdevicecontext  = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || (hwdevicecontext && !hwdevicecontext->hwctx))
        return;
    AVVAAPIDeviceContext *vaapidevicectx = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevicecontext->hwctx);
    if (!vaapidevicectx)
        return;

    MythVAAPIDisplay* display = new MythVAAPIDisplay(render);
    if (!display->m_vaDisplay)
    {
        av_buffer_unref(&hwdeviceref);
        return;
    }

    // set the display and ensure it is closed when the device context is finished
    hwdevicecontext->free        = &VAAPIContext::HWDeviceContextFinished;
    vaapidevicectx->display    = display->m_vaDisplay;

    // initialise hardware device context
    int res = av_hwdevice_ctx_init(hwdeviceref);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI hardware context");
        av_buffer_unref(&hwdeviceref);
        display->DecrRef();
        return;
    }

    // allocate the hardware frames context for FFmpeg
    Context->hw_frames_ctx = av_hwframe_ctx_alloc(hwdeviceref);
    if (!Context->hw_frames_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VAAPI hardware frames context");
        av_buffer_unref(&hwdeviceref);
        display->DecrRef();
        return;
    }

    // setup the frames context
    // the frames context now holds the reference to MythVAAPIDisplay.
    // Set the callback to ensure it is released
    AVHWFramesContext* hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    hw_frames_ctx->sw_format         = Context->sw_pix_fmt;
    hw_frames_ctx->initial_pool_size = static_cast<int>(VideoBuffers::GetNumBuffers(FMT_VAAPI));
    hw_frames_ctx->format            = AV_PIX_FMT_VAAPI;
    hw_frames_ctx->width             = Context->coded_width;
    hw_frames_ctx->height            = Context->coded_height;
    hw_frames_ctx->user_opaque       = display;
    hw_frames_ctx->free              = &VAAPIContext::HWFramesContextFinished;
    res = av_hwframe_ctx_init(Context->hw_frames_ctx);
    if (res < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI frames context");
        av_buffer_unref(&hwdeviceref);
        av_buffer_unref(&(Context->hw_frames_ctx));
        display->DecrRef();
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("VAAPI FFmpeg buffer pool created with %1 surfaces").arg(hw_frames_ctx->initial_pool_size));
    av_buffer_unref(&hwdeviceref);
}

void VAAPIContext::HWDecoderInitCallback(void*, void* Wait, void *Data)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Hardware decoder callback");
    QWaitCondition* wait    = reinterpret_cast<QWaitCondition*>(Wait);
    AVCodecContext* context = reinterpret_cast<AVCodecContext*>(Data);
    InitVAAPIContext(context);
    if (wait)
        wait->wakeAll();
}

int VAAPIContext::HwDecoderInit(AVCodecContext *Context)
{
    if (!Context || !MythOpenGLInterop::IsCodecSupported(kCodec_MPEG2_VAAPI))
        return -1;

    if (gCoreContext->IsUIThread())
    {
        InitVAAPIContext(Context);
    }
    else
    {
        MythMainWindow *window = MythMainWindow::getMainWindow();
        if (window)
        {
            QWaitCondition wait;
            QMutex lock;
            lock.lock();
            MythCallbackEvent *event = new MythCallbackEvent(&HWDecoderInitCallback, nullptr, &wait, Context);
            QCoreApplication::postEvent(window, event, Qt::HighEventPriority);
            int count = 0;
            while (!wait.wait(&lock, 100) && ((count += 100) < 1100))
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Waited %1ms for context creation").arg(count));
            lock.unlock();
        }
    }

    return Context->hw_frames_ctx ? 0 : -1;
}
