// MythTV
#include "videooutbase.h"
#include "videocolourspace.h"
#include "mythrender_opengl.h"
#include "mythmainwindow.h"
#include "mythopenglinterop.h"

#define LOC QString("OpenGLInterop: ")

QStringList MythOpenGLInterop::GetAllowedRenderers(MythCodecID CodecId)
{
    QStringList result;
    if (codec_is_vaapi(CodecId) && IsCodecSupported(CodecId))
        result << "openglvaapi";
    return result;
}

bool MythOpenGLInterop::IsCodecSupported(MythCodecID CodecId)
{
    bool supported = false;

#ifdef USING_GLVAAPI
    if (codec_is_vaapi(CodecId))
    {
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
#else
    (void)CodecId;
#endif

    return supported;
}

MythOpenGLInterop::MythOpenGLInterop(void)
  : m_glTexture(nullptr),
    m_glTextureFormat(0),
    m_colourSpace(nullptr)
#ifdef USING_GLVAAPI
   ,m_vaapiDisplay(nullptr),
    m_vaapiPictureAttributes(nullptr),
    m_vaapiPictureAttributeCount(0),
    m_vaapiHueBase(0),
    m_vaapiColourSpace(0)
#endif
{
}

MythOpenGLInterop::~MythOpenGLInterop()
{
#ifdef USING_GLVAAPI
    delete [] m_vaapiPictureAttributes;
    DeleteVAAPIGLXSurface();
#endif
}

int MythOpenGLInterop::SetPictureAttribute(PictureAttribute Attribute, int NewValue)
{
#ifdef USING_GLVAAPI
    if (m_vaapiDisplay)
        return SetVAAPIPictureAttribute(Attribute, NewValue);
#else
    (void)Attribute;
    (void)NewValue;
#endif
    return -1;
}

/*! \brief Return a valid MythGLTexture that encapsulates a hardware surface.
 *
 * \note When paused Frame is null and the last valid texture should be returned.
 * This will already have been deinterlaced as necessary.
*/
MythGLTexture* MythOpenGLInterop::GetTexture(MythRenderOpenGL *Context, VideoColourSpace *ColourSpace,
                                             VideoFrame *Frame, FrameScanType Scan)
{
    if (!Frame)
        return m_glTexture;
    if (!Context || !ColourSpace)
        return nullptr;

#ifdef USING_GLVAAPI
    if (Frame->codec == FMT_VAAPI)
        return GetVAAPITexture(Context, ColourSpace, Frame, Scan);
#else
    (void)Scan;
#endif

    return nullptr;
}

#ifdef USING_GLVAAPI
#include "vaapicontext.h"
#define INIT_ST \
  VAStatus va_status; \
  bool ok = true

#define CHECK_ST \
  ok &= (va_status == VA_STATUS_SUCCESS); \
  if (!ok) \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(va_status) \
              .arg(vaErrorStr(va_status)))

MythGLTexture* MythOpenGLInterop::GetVAAPITexture(MythRenderOpenGL *Context,
                                                  VideoColourSpace *ColourSpace,
                                                  VideoFrame *Frame,
                                                  FrameScanType Scan)
{
    if ((Frame->pix_fmt != AV_PIX_FMT_VAAPI) || (Frame->codec != FMT_VAAPI) ||
        !Frame->buf || !Frame->priv[1])
        return nullptr;

    // Unpick
    AVBufferRef* buffer = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
    if (!buffer || (buffer && !buffer->data))
        return nullptr;
    AVHWFramesContext* vaapiframes = reinterpret_cast<AVHWFramesContext*>(buffer->data);
    if (!vaapiframes || (vaapiframes && !vaapiframes->user_opaque))
        return nullptr;
    MythVAAPIDisplay* display = reinterpret_cast<MythVAAPIDisplay*>(vaapiframes->user_opaque);
    if (!display || (display && !(display->m_context && display->m_vaDisplay)))
        return nullptr;

    if (Context != display->m_context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mismatched OpenGL contexts");
        return nullptr;
    }

    // Retrieve or create our texture
    OpenGLLocker locker(display->m_context);
    VASurfaceID id = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(Frame->buf));
    MythGLTexture* glxsurface = GetVAAPIGLXSurface(Frame, display, ColourSpace);
    if (!glxsurface)
        return nullptr;

    // Set deinterlacing
    uint flags = VA_FRAME_PICTURE;
    if (Scan == kScan_Interlaced)
        flags = Frame->top_field_first ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
    else if (Scan == kScan_Intr2ndField)
        flags = Frame->top_field_first ? VA_BOTTOM_FIELD : VA_TOP_FIELD;

    // Update colourspace
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

    // Copy surface to texture
    if (glxsurface->m_data)
    {
        INIT_ST;
        va_status = vaCopySurfaceGLX(display->m_vaDisplay, glxsurface->m_data, id, flags);
        CHECK_ST;
    }
    return glxsurface;
}

void MythOpenGLInterop::InitVAAPIPictureAttributes(void)
{
    m_vaapiColourSpace = 0;
    if (!m_colourSpace || !m_vaapiDisplay)
        return;
    if (!(m_vaapiDisplay->m_vaDisplay && m_vaapiDisplay->m_context))
        return;

    m_vaapiHueBase = VideoOutput::CalcHueBase(vaQueryVendorString(m_vaapiDisplay->m_vaDisplay));

    delete [] m_vaapiPictureAttributes;
    m_vaapiPictureAttributeCount = 0;
    int supported_controls = kPictureAttributeSupported_None;
    QList<VADisplayAttribute> supported;
    int num = vaMaxNumDisplayAttributes(m_vaapiDisplay->m_vaDisplay);
    VADisplayAttribute* attribs = new VADisplayAttribute[num];

    int actual = 0;
    INIT_ST;
    va_status = vaQueryDisplayAttributes(m_vaapiDisplay->m_vaDisplay, attribs, &actual);
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

    m_vaapiPictureAttributeCount = supported.size();
    m_vaapiPictureAttributes = new VADisplayAttribute[m_vaapiPictureAttributeCount];
    for (int i = 0; i < m_vaapiPictureAttributeCount; i++)
        m_vaapiPictureAttributes[i] = supported.at(i);

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
        m_vaapiPictureAttributes[updatecscmatrix].value = static_cast<int32_t>(raw[0]);
        m_vaapiDisplay->m_context->makeCurrent();
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_vaapiDisplay->m_vaDisplay, &m_vaapiPictureAttributes[updatecscmatrix], 1);
        CHECK_ST;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Updated VAAPI colourspace matrix");
        m_vaapiDisplay->m_context->doneCurrent();
        m_vaapiColourSpace = VA_SRC_COLOR_MASK;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "No CSC matrix support - using limited VAAPI colourspace types");
    }
}

int MythOpenGLInterop::SetVAAPIPictureAttribute(PictureAttribute Attribute, int NewValue)
{
    if (!m_vaapiDisplay || (m_vaapiDisplay && !(m_vaapiDisplay->m_vaDisplay && m_vaapiDisplay->m_context)))
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
            adjustment = m_vaapiHueBase;
            break;
        case kPictureAttribute_Colour:
            attrib = VADisplayAttribSaturation;
            break;
        default:
            return -1;
    }

    bool found = false;
    for (int i = 0; i < m_vaapiPictureAttributeCount; i++)
    {
        if (m_vaapiPictureAttributes[i].type == attrib)
        {
            NewValue = std::min(std::max(NewValue, 0), 100);
            int newval = NewValue + adjustment;
            if (newval > 100) newval -= 100;
            qreal range = (m_vaapiPictureAttributes[i].max_value - m_vaapiPictureAttributes[i].min_value) / 100.0;
            int val = m_vaapiPictureAttributes[i].min_value + qRound(newval * range);
            m_vaapiPictureAttributes[i].value = val;
            found = true;
            break;
        }
    }


    if (found)
    {
        m_vaapiDisplay->m_context->makeCurrent();
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_vaapiDisplay->m_vaDisplay,
                                           m_vaapiPictureAttributes,
                                           m_vaapiPictureAttributeCount);
        CHECK_ST;
        m_vaapiDisplay->m_context->doneCurrent();
        return NewValue;
    }
    return -1;
}

MythGLTexture* MythOpenGLInterop::GetVAAPIGLXSurface(VideoFrame *Frame,
                                                     MythVAAPIDisplay *Display,
                                                     VideoColourSpace *ColourSpace)
{
    // return any cached texture
    QSize surfacesize(Frame->width, Frame->height);
    if (m_glTexture && (m_glTextureFormat == FMT_VAAPI) &&
       (m_glTexture->m_totalSize == surfacesize) && m_glTexture->m_data)
    {
        return m_glTexture;
    }

    // need new - delete old
    DeleteVAAPIGLXSurface();

    // create a texture
    MythGLTexture *texture = Display->m_context->CreateTexture(surfacesize);
    if (!texture)
        return nullptr;

    // create an associated GLX surface
    void *glxsurface = nullptr;
    INIT_ST;
    va_status = vaCreateSurfaceGLX(Display->m_vaDisplay, texture->m_texture->target(),
                                   texture->m_texture->textureId(), &glxsurface);
    CHECK_ST;
    if (!glxsurface)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create GLX surface.");
        Display->m_context->DeleteTexture(texture);
        return nullptr;
    }

    texture->m_data = static_cast<unsigned char*>(glxsurface);
    m_glTexture = texture;
    m_glTextureFormat = FMT_VAAPI;

    // Set display and colourspace
    m_vaapiDisplay = Display;
    m_vaapiDisplay->IncrRef();

    if (ColourSpace)
    {
        ColourSpace->UpdateColourSpace(Frame);
        m_colourSpace = ColourSpace;
        m_colourSpace->IncrRef();
        InitVAAPIPictureAttributes();
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Created VAAPI GLX surface");
    return m_glTexture;
}

void MythOpenGLInterop::DeleteVAAPIGLXSurface(void)
{
    // cleanup up VAAPI colourspace
    if (m_colourSpace)
    {
        m_colourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
        m_colourSpace->DecrRef();
    }
    m_colourSpace = nullptr;

    if (!m_vaapiDisplay)
        return;

    // warn against misuse
    if (m_glTextureFormat != FMT_VAAPI)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Deleting a non VAAPI surface");

    // cleanup texture and associated GLX surface
    if (m_vaapiDisplay->m_context)
        m_vaapiDisplay->m_context->makeCurrent();

    if (m_glTexture)
    {
        if (m_glTexture->m_data && m_vaapiDisplay->m_vaDisplay)
        {
            INIT_ST;
            va_status = vaDestroySurfaceGLX(m_vaapiDisplay->m_vaDisplay, static_cast<void*>(m_glTexture->m_data));
            CHECK_ST;
        }
        m_glTexture->m_data = nullptr;
        if (m_vaapiDisplay->m_context)
            m_vaapiDisplay->m_context->DeleteTexture(m_glTexture);
        m_glTexture = nullptr;
    }

    // release display
    if (m_vaapiDisplay->m_context)
        m_vaapiDisplay->m_context->doneCurrent();
    m_vaapiDisplay->DecrRef();
    m_vaapiDisplay = nullptr;

    // reset texture type
    m_glTextureFormat = 0;

    LOG(VB_GENERAL, LOG_INFO, LOC + "Destroyed VAAPI GLX texture");
}
#endif // USING_GLVAAPI
