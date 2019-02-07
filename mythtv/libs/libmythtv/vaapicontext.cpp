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
#include "vaapicontext.h"

extern "C" {
#include "libavutil/hwcontext_vaapi.h"
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
    INIT_ST;
    va_status = vaInitialize(m_vaDisplay, &major_ver, &minor_ver);
    CHECK_ST;

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
    INIT_ST; va_status = vaTerminate(Display); CHECK_ST;
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

MythCodecID VAAPIContext::GetBestSupportedCodec(AVCodec **Codec,
                                                const QString &Decoder,
                                                uint StreamType,
                                                AVPixelFormat &PixFmt)
{
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_VAAPI;

    AVPixelFormat fmt = AV_PIX_FMT_NONE;
    if (Decoder == "vaapi")
    {
        for (int i = 0;; i++)
        {
            const AVCodecHWConfig *config = avcodec_get_hw_config(*Codec, i);
            if (!config)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Decoder %1 does not support device type %2.")
                        .arg((*Codec)->name).arg(av_hwdevice_get_type_name(type)));
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX && config->device_type == type)
            {
                fmt = config->pix_fmt;
                break;
            }
        }
    }
    if (fmt == AV_PIX_FMT_NONE)
    {
        return static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Decoder %1 supports device type %2.")
                .arg((*Codec)->name).arg(av_hwdevice_get_type_name(type)));
        PixFmt = fmt;
        return static_cast<MythCodecID>(kCodec_MPEG1_VAAPI + (StreamType - 1));
    }
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
    hw_frames_ctx->initial_pool_size = NUM_VAAPI_BUFFERS;
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
    if (!Context)
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
