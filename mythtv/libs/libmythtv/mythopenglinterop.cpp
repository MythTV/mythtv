// MythTV
#include "videooutbase.h"
#include "videocolourspace.h"
#include "mythrender_opengl.h"
#include "mythmainwindow.h"
#include "mythopenglinterop.h"

// VAAPI
#include "va/va.h"
#include "va/va_version.h"
#if VA_CHECK_VERSION(0,34,0)
#include "va/va_compat.h"
#endif
#include "va/va_glx.h"

#define LOC QString("OpenGLInterop: ")

#define INIT_ST \
  VAStatus va_status; \
  bool ok = true

#define CHECK_ST \
  ok &= (va_status == VA_STATUS_SUCCESS); \
  if (!ok) \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(va_status) \
              .arg(vaErrorStr(va_status)))

bool MythOpenGLInterop::IsCodecSupported(MythCodecID CodecId)
{
    bool supported = codec_sw_copy(CodecId);

#ifdef USING_GLVAAPI
    if (codec_is_vaapi(CodecId))
    {
        supported = false;
        MythMainWindow* win = MythMainWindow::getMainWindow();
        if (win && !getenv("NO_VAAPI"))
        {
            MythRenderOpenGL *render = static_cast<MythRenderOpenGL*>(win->GetRenderDevice());
            if (render)
            {
                supported = !render->IsEGL() && !render->isOpenGLES();
                if (!supported)
                    LOG(VB_GENERAL, LOG_INFO, "Disabling VAAPI - incompatible with current render device");
            }
        }
    }
#endif

    return supported;
}

MythOpenGLInterop::MythOpenGLInterop(void)
  : m_glxSurface(nullptr),
    m_glxSurfaceTexture(0),
    m_glxSurfaceTextureType(GL_TEXTURE_2D),
    m_glxSurfaceSize(),
    m_glxSurfaceDisplay(nullptr),
    m_colourSpace(nullptr),
    m_pictureAttributes(nullptr),
    m_pictureAttributeCount(0),
    m_hueBase(0),
    m_vaapiColourSpace(0)
{
}

MythOpenGLInterop::~MythOpenGLInterop()
{
    delete [] m_pictureAttributes;
    DeleteGLXSurface();
}

void MythOpenGLInterop::InitPictureAttributes(void)
{
    m_vaapiColourSpace = 0;
    if (!m_colourSpace || !m_glxSurfaceDisplay)
        return;
    if (!(m_glxSurfaceDisplay->m_vaDisplay && m_glxSurfaceDisplay->m_context))
        return;

    m_hueBase = VideoOutput::CalcHueBase(vaQueryVendorString(m_glxSurfaceDisplay->m_vaDisplay));

    delete [] m_pictureAttributes;
    m_pictureAttributeCount = 0;
    int supported_controls = kPictureAttributeSupported_None;
    QList<VADisplayAttribute> supported;
    int num = vaMaxNumDisplayAttributes(m_glxSurfaceDisplay->m_vaDisplay);
    VADisplayAttribute* attribs = new VADisplayAttribute[num];

    int actual = 0;
    INIT_ST;
    va_status = vaQueryDisplayAttributes(m_glxSurfaceDisplay->m_vaDisplay, attribs, &actual);
    CHECK_ST;

    int updatecscmatrix = -1;
    for (int i = 0; i < actual; i++)
    {
        int type = attribs[i].type;
        if ((attribs[i].flags & VA_DISPLAY_ATTRIB_SETTABLE) &&
            (type == VADisplayAttribBrightness ||
             type == VADisplayAttribContrast ||
             type == VADisplayAttribHue ||
             type == VADisplayAttribSaturation ||
             type == VADisplayAttribCSCMatrix))
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
            if (type == VADisplayAttribCSCMatrix)
                updatecscmatrix = i;
        }
    }

    m_colourSpace->SetSupportedAttributes(static_cast<PictureAttributeSupported>(supported_controls));
    delete [] attribs;

    if (supported.isEmpty())
        return;

    m_pictureAttributeCount = supported.size();
    m_pictureAttributes = new VADisplayAttribute[m_pictureAttributeCount];
    for (int i = 0; i < m_pictureAttributeCount; i++)
        m_pictureAttributes[i] = supported.at(i);

    if (supported_controls & kPictureAttributeSupported_Brightness)
        SetPictureAttribute(kPictureAttribute_Brightness, m_colourSpace->GetPictureAttribute(kPictureAttribute_Brightness));
    if (supported_controls & kPictureAttributeSupported_Hue)
        SetPictureAttribute(kPictureAttribute_Hue, m_colourSpace->GetPictureAttribute(kPictureAttribute_Hue));
    if (supported_controls & kPictureAttributeSupported_Contrast)
        SetPictureAttribute(kPictureAttribute_Contrast, m_colourSpace->GetPictureAttribute(kPictureAttribute_Contrast));
    if (supported_controls & kPictureAttributeSupported_Colour)
        SetPictureAttribute(kPictureAttribute_Colour, m_colourSpace->GetPictureAttribute(kPictureAttribute_Colour));

    if (updatecscmatrix > -1)
    {
        // FIXME - this is untested. Presumably available with the VDPAU backend.
        // If functioning correctly we need to turn off all of the other VA picture attributes
        // as this acts, as for OpenGL, as the master colourspace conversion matrix.
        // We can also enable Studio levels support.
        QMatrix4x4 yuv = static_cast<QMatrix4x4>(*m_colourSpace);
        float raw[9];
        raw[0] = yuv(0, 0); raw[1] = yuv(0, 1); raw[2] = yuv(0, 2);
        raw[3] = yuv(1, 0); raw[4] = yuv(1, 1); raw[5] = yuv(1, 2);
        raw[6] = yuv(2, 0); raw[7] = yuv(2, 1); raw[8] = yuv(2, 2);
        m_pictureAttributes[updatecscmatrix].value = static_cast<int32_t>(raw[0]);
        m_glxSurfaceDisplay->m_context->makeCurrent();
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_glxSurfaceDisplay->m_vaDisplay, &m_pictureAttributes[updatecscmatrix], 1);
        CHECK_ST;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Updated VAAPI colourspace matrix");
        m_glxSurfaceDisplay->m_context->doneCurrent();
        m_vaapiColourSpace = VA_SRC_COLOR_MASK;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "No CSC matrix support - using limited VAAPI colourspace types");
    }
}

int MythOpenGLInterop::SetPictureAttribute(PictureAttribute Attribute, int NewValue)
{
    if (!m_glxSurfaceDisplay ||
       (m_glxSurfaceDisplay && !(m_glxSurfaceDisplay->m_vaDisplay && m_glxSurfaceDisplay->m_context)))
        return -1;

    int adjustment = 0;
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
            adjustment = m_hueBase;
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
            NewValue = std::min(std::max(NewValue, 0), 100);
            int newval = NewValue + adjustment;
            if (newval > 100) newval -= 100;
            qreal range = (m_pictureAttributes[i].max_value - m_pictureAttributes[i].min_value) / 100.0;
            int val = m_pictureAttributes[i].min_value + qRound(newval * range);
            m_pictureAttributes[i].value = val;
            found = true;
            break;
        }
    }


    if (found)
    {
        m_glxSurfaceDisplay->m_context->makeCurrent();
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_glxSurfaceDisplay->m_vaDisplay,
                                           m_pictureAttributes,
                                           m_pictureAttributeCount);
        CHECK_ST;
        m_glxSurfaceDisplay->m_context->doneCurrent();
        return NewValue;
    }
    return -1;
}

bool MythOpenGLInterop::CopySurfaceToTexture(MythRenderOpenGL *Context,
                                        VideoColourSpace *ColourSpace,
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
    void* glx_surface = GetGLXSurface(Texture, TextureType, Frame, display, ColourSpace);
    if (!glx_surface)
    {
        LOG(VB_GENERAL, LOG_ERR, "No GLX surface");
        return false;
    }

    uint flags = VA_FRAME_PICTURE;
    if (Scan == kScan_Interlaced)
        flags = Frame->top_field_first ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
    else if (Scan == kScan_Intr2ndField)
        flags = Frame->top_field_first ? VA_BOTTOM_FIELD : VA_TOP_FIELD;

    if (!m_vaapiColourSpace)
    {
        switch (Frame->colorspace)
        {
            case AVCOL_SPC_BT709:     m_vaapiColourSpace = VA_SRC_BT709; break;
            case AVCOL_SPC_SMPTE170M:
            case AVCOL_SPC_BT470BG:   m_vaapiColourSpace = VA_SRC_BT601; break;
            case AVCOL_SPC_SMPTE240M: m_vaapiColourSpace = VA_SRC_SMPTE_240; break;
            default:
            m_vaapiColourSpace = ((Frame->width < 1280) ? VA_SRC_BT601 : VA_SRC_BT709); break;
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using '%1' VAAPI colourspace")
            .arg((m_vaapiColourSpace == VA_SRC_BT709) ? "bt709" : ((m_vaapiColourSpace == VA_SRC_BT601) ? "bt601" : "smpte240")));
    }

    if (m_vaapiColourSpace)
        flags |= m_vaapiColourSpace;

    INIT_ST;
    va_status = vaCopySurfaceGLX(display->m_vaDisplay, glx_surface, id, flags);
    CHECK_ST;
    return true;
}

void* MythOpenGLInterop::GetGLXSurface(uint Texture, uint TextureType, VideoFrame *Frame,
                                  MythVAAPIDisplay *Display, VideoColourSpace *ColourSpace)
{
    if (!Frame)
        return nullptr;
    QSize surfacesize(Frame->width, Frame->height);
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

    if (ColourSpace)
    {
        ColourSpace->UpdateColourSpace(Frame);
        m_colourSpace = ColourSpace;
        m_colourSpace->IncrRef();
        InitPictureAttributes();
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Created GLX surface");
    return m_glxSurface;
}

void MythOpenGLInterop::DeleteGLXSurface(void)
{
    if (m_colourSpace)
    {
        m_colourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
        m_colourSpace->DecrRef();
    }
    m_colourSpace = nullptr;

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
