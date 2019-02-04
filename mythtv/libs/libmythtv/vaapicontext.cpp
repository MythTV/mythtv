// Qt
#include <QWaitCondition>

// Mythtv
#include "mythmainwindow.h"
#include "mythlogging.h"
#include "mythcodecid.h"
#include "mythframe.h"
#include "videooutbase.h"
#include "mythrender_opengl.h"
#include "videocolourspace.h"
#include "vaapicontext.h"

extern "C" {
#include "libavutil/hwcontext_vaapi.h"
}

#define LOC QString("VAAPIHWCtx: ")
#define ERR QString("VAAPIHWCtx Error: ")

#define INIT_ST \
  VAStatus va_status; \
  bool ok = true

#define CHECK_ST \
  ok &= (va_status == VA_STATUS_SUCCESS); \
  if (!ok) \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(va_status) \
              .arg(vaErrorStr(va_status)))

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
    LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI display closed");
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

    MythVAAPIDisplay* display = reinterpret_cast<MythVAAPIDisplay*>(Context->user_opaque);
    if (display)
        display->DecrRef();
    LOG(VB_GENERAL, LOG_INFO, LOC + "VAAPI frames context destroyed");
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
    LOG(VB_GENERAL, LOG_INFO, LOC + "Setting up VAAPI hw frames context");

    // the frames context now holds the reference to MythVAAPIDisplay.
    // Set the callback to ensure it is released
    AVHWFramesContext* hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(Context->hw_frames_ctx->data);
    hw_frames_ctx->sw_format         = Context->sw_pix_fmt;
    hw_frames_ctx->initial_pool_size = NUM_VAAPI_BUFFERS;
    hw_frames_ctx->format            = AV_PIX_FMT_VAAPI;
    hw_frames_ctx->width             = FFALIGN(Context->coded_width, 32);
    hw_frames_ctx->height            = FFALIGN(Context->coded_height, 32);
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

    LOG(VB_GENERAL, LOG_INFO, QString("FFmpeg buffer pool created with %1 surfaces").arg(hw_frames_ctx->initial_pool_size));
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

VAAPIContext::VAAPIContext(void)
  : m_glxSurface(nullptr),
    m_glxSurfaceTexture(0),
    m_glxSurfaceTextureType(GL_TEXTURE_2D),
    m_glxSurfaceSize(),
    m_glxSurfaceDisplay(nullptr),
    m_pictureAttributes(nullptr),
    m_pictureAttributeCount(0),
    m_hueBase(0)
{
}

VAAPIContext::~VAAPIContext()
{
    delete [] m_pictureAttributes;
    DeleteGLXSurface();
}

void VAAPIContext::InitPictureAttributes(VADisplay Display, VideoColourSpace &Colourspace)
{
    if (!Display)
        return;

    m_hueBase = VideoOutput::CalcHueBase(vaQueryVendorString(Display));

    delete [] m_pictureAttributes;
    m_pictureAttributeCount = 0;
    int supported_controls = kPictureAttributeSupported_None;
    QList<VADisplayAttribute> supported;
    int num = vaMaxNumDisplayAttributes(Display);
    VADisplayAttribute* attribs = new VADisplayAttribute[num];

    int actual = 0;
    INIT_ST;
    va_status = vaQueryDisplayAttributes(Display, attribs, &actual);
    CHECK_ST;

    for (int i = 0; i < actual; i++)
    {
        int type = attribs[i].type;
        if ((attribs[i].flags & VA_DISPLAY_ATTRIB_SETTABLE) &&
            (type == VADisplayAttribBrightness ||
             type == VADisplayAttribContrast ||
             type == VADisplayAttribHue ||
             type == VADisplayAttribSaturation))
        {
            supported.push_back(attribs[i]);
            if (type == VADisplayAttribBrightness)
                supported_controls += kPictureAttributeSupported_Brightness;
            if (type == VADisplayAttribHue)
                supported_controls += kPictureAttributeSupported_Hue;
            if (type == VADisplayAttribContrast)
                supported_controls += kPictureAttributeSupported_Contrast;
            if (type == VADisplayAttribSaturation)
                supported_controls += kPictureAttributeSupported_Colour;
        }
    }

    Colourspace.SetSupportedAttributes((PictureAttributeSupported)supported_controls);
    delete [] attribs;

    if (supported.isEmpty())
        return;

    m_pictureAttributeCount = supported.size();
    m_pictureAttributes = new VADisplayAttribute[m_pictureAttributeCount];
    for (int i = 0; i < m_pictureAttributeCount; i++)
        m_pictureAttributes[i] = supported.at(i);

    if (supported_controls & kPictureAttributeSupported_Brightness)
        SetPictureAttribute(kPictureAttribute_Brightness,
            Colourspace.GetPictureAttribute(kPictureAttribute_Brightness));
    if (supported_controls & kPictureAttributeSupported_Hue)
        SetPictureAttribute(kPictureAttribute_Hue,
            Colourspace.GetPictureAttribute(kPictureAttribute_Hue));
    if (supported_controls & kPictureAttributeSupported_Contrast)
        SetPictureAttribute(kPictureAttribute_Contrast,
            Colourspace.GetPictureAttribute(kPictureAttribute_Contrast));
    if (supported_controls & kPictureAttributeSupported_Colour)
        SetPictureAttribute(kPictureAttribute_Colour,
            Colourspace.GetPictureAttribute(kPictureAttribute_Colour));
}

int VAAPIContext::SetPictureAttribute(PictureAttribute Attribute, int NewValue)
{
    (void)Attribute;
    (void)NewValue;
    /*
    int adj = 0;
    VADisplayAttribType attrib = VADisplayAttribBrightness;
    switch (Attribute)
    {
        case kPictureAttribute_Brightness:
            attrib = VADisplayAttribBrightness;
            break;
        case kPictureAttribute_Contrast:
            attrib = VADisplayAttribContrast;
            break;
        case kPictureAttribute_Hue:
            attrib = VADisplayAttribHue;
            adj = m_hueBase;
            break;
        case kPictureAttribute_Colour:
            attrib = VADisplayAttribSaturation;
            break;
        default:
            return -1;
    }

    bool found = false;
    for (int i = 0; i < m_pictureAttributeCount; i++)
    {
        if (m_pictureAttributes[i].type == attrib)
        {
            int min = m_pictureAttributes[i].min_value;
            int max = m_pictureAttributes[i].max_value;
            int val = min + (int)(((float)((NewValue + adj) % 100) / 100.0f) * (max - min));
            m_pictureAttributes[i].value = val;
            found = true;
            break;
        }
    }


    if (found && m_context)
    {
        m_context->makeCurrent();
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_display->m_vaDisplay,
                                           m_pictureAttributes,
                                           m_pictureAttributeCount);
        CHECK_ST;
        m_context->doneCurrent();
        return NewValue;
    }
    return -1;
*/
    return 0;
}

bool VAAPIContext::CopySurfaceToTexture(MythRenderOpenGL *Context,
                                        VideoFrame *Frame, uint Texture,
                                        uint TextureType, FrameScanType Scan)
{
    if (!Frame)
        return false;

    if ((Frame->pix_fmt != AV_PIX_FMT_VAAPI) || (Frame->codec != FMT_VAAPI) ||
        !Frame->buf || !Frame->priv[1])
        return false;

    AVBufferRef* buffer = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
    if (!buffer || (buffer && !buffer->data))
        return false;
    AVHWFramesContext* vaapiframes = reinterpret_cast<AVHWFramesContext*>(buffer->data);
    if (!vaapiframes || (vaapiframes && !vaapiframes->user_opaque))
        return false;
    MythVAAPIDisplay* display = reinterpret_cast<MythVAAPIDisplay*>(vaapiframes->user_opaque);
    if (!display || (display && !(display->m_context && display->m_vaDisplay)))
        return false;

    if (Context != display->m_context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mismatched OpenGL contexts");
        return false;
    }

    OpenGLLocker locker(display->m_context);

    VASurfaceID id = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(Frame->buf));
    void* glx_surface = GetGLXSurface(Texture, TextureType, Frame->width, Frame->height, display);
    if (!glx_surface)
    {
        LOG(VB_GENERAL, LOG_ERR, "No GLX surface");
        return false;
    }

    uint field = VA_FRAME_PICTURE;
    if (Scan == kScan_Interlaced)
        field = Frame->top_field_first ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
    else if (Scan == kScan_Intr2ndField)
        field = Frame->top_field_first ? VA_BOTTOM_FIELD : VA_TOP_FIELD;

    INIT_ST;
    va_status = vaCopySurfaceGLX(display->m_vaDisplay, glx_surface, id, field);
    CHECK_ST;
    return true;
}

void* VAAPIContext::GetGLXSurface(uint Texture, uint TextureType, int Width, int Height, MythVAAPIDisplay *Display)
{
    QSize surfacesize(Width, Height);
    if (m_glxSurface && (m_glxSurfaceSize == surfacesize) &&
       (m_glxSurfaceTexture == Texture) && (m_glxSurfaceTextureType == TextureType) &&
       (m_glxSurfaceDisplay == Display))
    {
        return m_glxSurface;
    }

    DeleteGLXSurface();

    void *glxsurface = nullptr;
    INIT_ST;
    va_status = vaCreateSurfaceGLX(Display->m_vaDisplay, TextureType, Texture, &glxsurface);
    CHECK_ST;
    if (!glxsurface)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create GLX surface.");
        return nullptr;
    }

    m_glxSurface            = glxsurface;
    m_glxSurfaceSize        = surfacesize;
    m_glxSurfaceTexture     = Texture;
    m_glxSurfaceTextureType = TextureType;
    m_glxSurfaceDisplay     = Display;
    m_glxSurfaceDisplay->IncrRef();
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created GLX surface");
    return m_glxSurface;
}

void VAAPIContext::DeleteGLXSurface(void)
{
    if (!m_glxSurface || !m_glxSurfaceDisplay)
        return;

    if (m_glxSurfaceDisplay && m_glxSurfaceDisplay->m_context)
        m_glxSurfaceDisplay->m_context->makeCurrent();

    INIT_ST;
    va_status = vaDestroySurfaceGLX(m_glxSurfaceDisplay->m_vaDisplay, m_glxSurface);
    CHECK_ST;

    m_glxSurface            = nullptr;
    m_glxSurfaceSize        = QSize();
    m_glxSurfaceTexture     = 0;
    m_glxSurfaceTextureType = 0;
    if (m_glxSurfaceDisplay && m_glxSurfaceDisplay->m_context)
        m_glxSurfaceDisplay->m_context->doneCurrent();

    m_glxSurfaceDisplay->DecrRef();
    m_glxSurfaceDisplay = nullptr;
    LOG(VB_GENERAL, LOG_INFO, LOC + "Destroyed GLX surface");
}
