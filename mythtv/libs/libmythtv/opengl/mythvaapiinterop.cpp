// MythTV
#include "videooutbase.h"
#include "videocolourspace.h"
#include "fourcc.h"
#include "mythvaapiinterop.h"

extern "C" {
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/hwcontext_vaapi.h"
}

#define LOC QString("VAAPIInterop: ")

#define INIT_ST \
  VAStatus va_status; \
  bool ok = true

#define CHECK_ST \
  ok &= (va_status == VA_STATUS_SUCCESS); \
  if (!ok) \
      LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(va_status) \
              .arg(vaErrorStr(va_status)))

/*! \brief Return an 'interoperability' method that is supported by the current render device.
 *
 * DRM interop is the preferred option as it is copy free but requires EGL.
 * DRM returns raw YUV frames which gives us full colourspace and deinterlacing control.
 *
 * GLX Pixmap interop will copy the frame to an RGBA texture. VAAPI functionality is
 * used for colourspace conversion and deinterlacing. It is not supported
 * when EGL and/or Wayland are running.
 *
 * GLX Copy is only available when GLX is running (no OpenGLES or Wayland) and,
 * under the hood, performs the same Pixmap copy as GLXPixmap support plus an
 * additional render to texture via a FramebufferObject. As it is less performant
 * and less widely available than GLX Pixmap, it may be removed in the future.
*/
MythOpenGLInterop::Type MythVAAPIInterop::GetInteropType(MythCodecID CodecId,
                                                         MythRenderOpenGL *Context)
{
    if (!codec_is_vaapi(CodecId) || getenv("NO_VAAPI"))
        return Unsupported;

    if (!Context)
        Context = MythRenderOpenGL::GetOpenGLRender();
    if (!Context)
        return Unsupported;

    OpenGLLocker locker(Context);
    bool egl = Context->IsEGL();
    bool opengles = Context->isOpenGLES();
    bool wayland = qgetenv("XDG_SESSION_TYPE").contains("wayland");
    // best first
    if (egl && MythVAAPIInteropDRM::IsSupported(Context)) // zero copy
        return VAAPIEGLDRM;
    else if (!egl && !wayland && MythVAAPIInteropGLXPixmap::IsSupported(Context)) // copy
        return VAAPIGLXPIX;
    else if (!egl && !opengles && !wayland) // 2 * copy
        return VAAPIGLXCOPY;
    return Unsupported;
}

MythVAAPIInterop* MythVAAPIInterop::Create(MythRenderOpenGL *Context, Type InteropType)
{
    if (!Context)
        return nullptr;

    if (InteropType == VAAPIEGLDRM)
        return new MythVAAPIInteropDRM(Context);
    else if (InteropType == VAAPIGLXPIX)
        return new MythVAAPIInteropGLXPixmap(Context);
    else if (InteropType == VAAPIGLXCOPY)
        return new MythVAAPIInteropGLXCopy(Context);
    return nullptr;
}

/*! \class MythVAAPIInterop
 *
 * \todo Fix pause frame deinterlacing
 * \todo Deinterlacing 'breaks' after certain stream changes (e.g. aspect ratio)
 * \todo Scaling of some 1080 H.264 material (garbage line at bottom - presumably
 * scaling from 1088 to 1080 - but only some files). Same effect on all VAAPI interop types.
*/
MythVAAPIInterop::MythVAAPIInterop(MythRenderOpenGL *Context, Type InteropType)
  : MythOpenGLInterop(Context, InteropType)
{
}

MythVAAPIInterop::~MythVAAPIInterop()
{
    DestroyDeinterlacer();
    if (m_vaDisplay)
        if (vaTerminate(m_vaDisplay) != VA_STATUS_SUCCESS)
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Error closing VAAPI display");
}

VADisplay MythVAAPIInterop::GetDisplay(void)
{
    return m_vaDisplay;
}

QString MythVAAPIInterop::GetVendor(void)
{
    return m_vaVendor;
}

void MythVAAPIInterop::IniitaliseDisplay(void)
{
    if (!m_vaDisplay)
        return;
    int major, minor;
    if (vaInitialize(m_vaDisplay, &major, &minor) != VA_STATUS_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI display");
        vaTerminate(m_vaDisplay);
        m_vaDisplay = nullptr;
    }
    else
    {
        m_vaVendor = vaQueryVendorString(m_vaDisplay);
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created VAAPI %1.%2 display for %3 (%4)")
            .arg(major).arg(minor).arg(TypeToString(m_type)).arg(m_vaVendor));
    }
}

void MythVAAPIInterop::DestroyDeinterlacer(void)
{
    if (m_filterGraph)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Destroying VAAPI deinterlacer");
    avfilter_graph_free(&m_filterGraph);
    m_filterGraph = nullptr;
    m_filterSink = nullptr;
    m_filterSource = nullptr;
    m_deinterlacer = DEINT_NONE;
    m_deinterlacer2x = false;
    m_firstField = true;
    m_lastFilteredFrame = 0;
    m_lastFilteredFrameCount = 0;
    av_buffer_unref(&m_vppFramesContext);
}

VASurfaceID MythVAAPIInterop::VerifySurface(MythRenderOpenGL *Context, VideoFrame *Frame)
{
    VASurfaceID result = 0;
    if (!Frame)
        return result;

    if ((Frame->pix_fmt != AV_PIX_FMT_VAAPI) || (Frame->codec != FMT_VAAPI) ||
        !Frame->buf || !Frame->priv[1])
        return result;

    // Sanity check the context
    if (m_context != Context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mismatched OpenGL contexts!");
        return result;
    }

    // Check size
    QSize surfacesize(Frame->width, Frame->height);
    if (m_openglTextureSize != surfacesize)
    {
        if (!m_openglTextureSize.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Video texture size changed!");
        m_openglTextureSize = surfacesize;
    }

    // Retrieve surface
    VASurfaceID id = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(Frame->buf));
    if (id)
        result = id;
    return result;
}

MythVAAPIInteropGLX::MythVAAPIInteropGLX(MythRenderOpenGL *Context, Type InteropType)
  : MythVAAPIInterop(Context, InteropType),
    m_vaapiPictureAttributes(nullptr),
    m_vaapiPictureAttributeCount(0),
    m_vaapiHueBase(0),
    m_vaapiColourSpace(0),
    m_basicDeinterlacer(DEINT_NONE)
{
}

MythVAAPIInteropGLX::~MythVAAPIInteropGLX()
{
    delete [] m_vaapiPictureAttributes;
}

uint MythVAAPIInteropGLX::GetFlagsForFrame(VideoFrame *Frame, FrameScanType Scan)
{
    uint flags = VA_FRAME_PICTURE;
    if (!Frame)
        return flags;

    // Set deinterlacing flags if VPP is not available
    if (m_deinterlacer)
    {
        flags = VA_FRAME_PICTURE;
    }
    else if (is_interlaced(Scan))
    {
        // As for VDPAU, only VAAPI can deinterlace these frames - so accept any deinterlacer
        bool doublerate = true;
        MythDeintType driverdeint = GetDoubleRateOption(Frame, DEINT_DRIVER | DEINT_CPU | DEINT_SHADER);
        if (!driverdeint)
        {
            doublerate = false;
            driverdeint = GetSingleRateOption(Frame, DEINT_DRIVER | DEINT_CPU | DEINT_SHADER);
        }

        if (driverdeint)
        {
            driverdeint = DEINT_BASIC;
            if (m_basicDeinterlacer != driverdeint)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Enabled deinterlacer '%1'")
                    .arg(DeinterlacerName(driverdeint | DEINT_DRIVER, doublerate, FMT_VAAPI)));
            }

            bool top = Frame->interlaced_reversed ? !Frame->top_field_first : Frame->top_field_first;
            if (Scan == kScan_Interlaced)
            {
                Frame->deinterlace_inuse = driverdeint | DEINT_DRIVER;
                Frame->deinterlace_inuse2x = doublerate;
                flags = top ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
            }
            else if (Scan == kScan_Intr2ndField)
            {
                Frame->deinterlace_inuse = driverdeint | DEINT_DRIVER;
                Frame->deinterlace_inuse2x = doublerate;
                flags = top ? VA_BOTTOM_FIELD : VA_TOP_FIELD;
            }
            m_basicDeinterlacer = driverdeint;
        }
        else if (m_basicDeinterlacer)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled basic VAAPI deinterlacer");
            m_basicDeinterlacer = DEINT_NONE;
        }
    }

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
    flags |= m_vaapiColourSpace;
    return flags;
}

void MythVAAPIInteropGLX::InitPictureAttributes(VideoColourSpace *ColourSpace)
{
    m_vaapiColourSpace = 0;
    if (!ColourSpace || !m_vaDisplay)
        return;

    OpenGLLocker locker(m_context);
    m_vaapiHueBase = VideoOutput::CalcHueBase(m_vaVendor);

    delete [] m_vaapiPictureAttributes;
    m_vaapiPictureAttributeCount = 0;
    int supported_controls = kPictureAttributeSupported_None;
    QList<VADisplayAttribute> supported;
    int num = vaMaxNumDisplayAttributes(m_vaDisplay);
    VADisplayAttribute* attribs = new VADisplayAttribute[num];

    int actual = 0;
    INIT_ST;
    va_status = vaQueryDisplayAttributes(m_vaDisplay, attribs, &actual);
    CHECK_ST;

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
        }
    }

    // Set the supported attributes
    ColourSpace->SetSupportedAttributes(static_cast<PictureAttributeSupported>(supported_controls));
    // and listen for changes
    connect(ColourSpace, &VideoColourSpace::PictureAttributeChanged, this, &MythVAAPIInteropGLX::SetPictureAttribute);

    // create
    delete [] attribs;

    if (supported.isEmpty())
        return;

    m_vaapiPictureAttributeCount = supported.size();
    m_vaapiPictureAttributes = new VADisplayAttribute[m_vaapiPictureAttributeCount];
    for (int i = 0; i < m_vaapiPictureAttributeCount; i++)
        m_vaapiPictureAttributes[i] = supported.at(i);

    if (supported_controls & kPictureAttributeSupported_Brightness)
        SetPictureAttribute(kPictureAttribute_Brightness, ColourSpace->GetPictureAttribute(kPictureAttribute_Brightness));
    if (supported_controls & kPictureAttributeSupported_Hue)
        SetPictureAttribute(kPictureAttribute_Hue, ColourSpace->GetPictureAttribute(kPictureAttribute_Hue));
    if (supported_controls & kPictureAttributeSupported_Contrast)
        SetPictureAttribute(kPictureAttribute_Contrast, ColourSpace->GetPictureAttribute(kPictureAttribute_Contrast));
    if (supported_controls & kPictureAttributeSupported_Colour)
        SetPictureAttribute(kPictureAttribute_Colour, ColourSpace->GetPictureAttribute(kPictureAttribute_Colour));
}

int MythVAAPIInteropGLX::SetPictureAttribute(PictureAttribute Attribute, int Value)
{
    if (!m_vaDisplay)
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
            Value = std::min(std::max(Value, 0), 100);
            int newval = Value + adjustment;
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
        m_context->makeCurrent();
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_vaDisplay,
                                           m_vaapiPictureAttributes,
                                           m_vaapiPictureAttributeCount);
        CHECK_ST;
        m_context->doneCurrent();
        return Value;
    }
    return -1;
}

MythVAAPIInteropGLXCopy::MythVAAPIInteropGLXCopy(MythRenderOpenGL *Context)
  : MythVAAPIInteropGLX(Context, VAAPIGLXCOPY)
{
    Display *display = glXGetCurrentDisplay();
    if (!display)
    {
      LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open GLX display");
      return;
    }

    m_vaDisplay = vaGetDisplayGLX(display);
    if (!m_vaDisplay)
    {
      LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create GLX VADisplay");
      return;
    }
    IniitaliseDisplay();
}

MythVAAPIInteropGLXCopy::~MythVAAPIInteropGLXCopy()
{
    if (m_glxSurface && m_vaDisplay)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting GLX surface");
        OpenGLLocker locker(m_context);
        INIT_ST;
        va_status = vaDestroySurfaceGLX(m_vaDisplay, m_glxSurface);
        CHECK_ST;
    }
}

vector<MythVideoTexture*> MythVAAPIInteropGLXCopy::Acquire(MythRenderOpenGL *Context,
                                                           VideoColourSpace *ColourSpace,
                                                           VideoFrame *Frame,
                                                           FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    // Retrieve the VASurface
    VASurfaceID id = VerifySurface(Context, Frame);
    if (!id || !m_vaDisplay)
        return result;

    // Initialise colourspace on first frame
    if (ColourSpace && m_openglTextures.isEmpty())
        InitPictureAttributes(ColourSpace);

    // Lock
    OpenGLLocker locker(m_context);

    // we only ever use one glx surface which is updated on each call
    if (m_openglTextures.isEmpty())
    {
        // create a texture
        // N.B. No apparent 10/12/16bit support here. Can't encourage vaCreateSurfaceGLX
        // to work with a 16bit texture
        MythVideoTexture *texture = MythVideoTexture::CreateTexture(m_context, m_openglTextureSize);
        if (!texture)
            return result;

        texture->m_plane = 0;
        texture->m_planeCount = 1;
        texture->m_frameType = FMT_VAAPI;
        texture->m_frameFormat = FMT_RGBA32;

        // create an associated GLX surface
        INIT_ST;
        va_status = vaCreateSurfaceGLX(m_vaDisplay, texture->m_texture->target(),
                                       texture->m_texture->textureId(), &m_glxSurface);
        CHECK_ST;
        if (!m_glxSurface)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create GLX surface.");
            MythVideoTexture::DeleteTexture(m_context, texture);
            return result;
        }

        result.push_back(texture);
        m_openglTextures.insert(DUMMY_INTEROP_ID, result);
    }

    if (m_openglTextures.isEmpty() || !m_glxSurface)
        return result;
    result = m_openglTextures[DUMMY_INTEROP_ID];

    // VPP deinterlacing
    id = Deinterlace(Frame, id, Scan);

    // Copy surface to texture
    INIT_ST;
    va_status = vaCopySurfaceGLX(m_vaDisplay, m_glxSurface, id, GetFlagsForFrame(Frame, Scan));
    CHECK_ST;
    return result;
}

MythVAAPIInteropGLXPixmap::MythVAAPIInteropGLXPixmap(MythRenderOpenGL *Context)
  : MythVAAPIInteropGLX(Context, VAAPIGLXPIX),
    m_pixmap(0),
    m_glxPixmap(0),
    m_glxBindTexImageEXT(nullptr),
    m_glxReleaseTexImageEXT(nullptr)
{
    m_vaDisplay = vaGetDisplay(glXGetCurrentDisplay());
    if (!m_vaDisplay)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create X11 VADisplay");
    else
        IniitaliseDisplay();
    InitPixmaps();
}

MythVAAPIInteropGLXPixmap::~MythVAAPIInteropGLXPixmap()
{
    OpenGLLocker locker(m_context);
    Display* display = glXGetCurrentDisplay();
    if (!InitPixmaps() || !display)
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting GLX Pixmaps");
    if (m_glxPixmap)
    {
        m_glxReleaseTexImageEXT(display, m_glxPixmap, GLX_FRONT_EXT);
        XSync(display, False);
        glXDestroyPixmap(display, m_glxPixmap);
    }

    if (m_pixmap)
        XFreePixmap(display, m_pixmap);
}

vector<MythVideoTexture*> MythVAAPIInteropGLXPixmap::Acquire(MythRenderOpenGL *Context,
                                                             VideoColourSpace *ColourSpace,
                                                             VideoFrame *Frame,
                                                             FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    // Retrieve the VASurface
    VASurfaceID id = VerifySurface(Context, Frame);
    if (!id || !m_vaDisplay)
        return result;

    // Initialise colourspace on first frame
    if (ColourSpace && m_openglTextures.isEmpty())
        InitPictureAttributes(ColourSpace);

    // Lock
    OpenGLLocker locker(m_context);
    if (!InitPixmaps())
        return result;

    // we only ever use one pixmap which is updated on each call
    if (m_openglTextures.isEmpty())
    {
        // Get display
        Display* display = glXGetCurrentDisplay();
        if (!display)
            return result;

        // Get a framebuffer config
        int fbattribs[] = {
                GLX_RENDER_TYPE, GLX_RGBA_BIT,
                GLX_X_RENDERABLE, True,
                GLX_BIND_TO_TEXTURE_RGBA_EXT, True,
                GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
                GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
                GLX_Y_INVERTED_EXT, True,
                GLX_DOUBLEBUFFER, False,
                GLX_RED_SIZE, 8,
                GLX_GREEN_SIZE, 8,
                GLX_BLUE_SIZE, 8,
                GLX_ALPHA_SIZE, 8,
                None };
        int fbcount;
        GLXFBConfig *fbs = glXChooseFBConfig(display, DefaultScreen(display), fbattribs, &fbcount);
        if (!fbcount)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to retrieve GLX framebuffer config");
            return result;
        }

        GLXFBConfig fbconfig = fbs[0];
        XFree(fbs);

        // create pixmaps
        uint width  = static_cast<uint>(m_openglTextureSize.width());
        uint height = static_cast<uint>(m_openglTextureSize.height());
        XWindowAttributes xwattribs;
        XGetWindowAttributes(display, DefaultRootWindow(display), &xwattribs);
        m_pixmap = XCreatePixmap(display, DefaultRootWindow(display),
                                 width, height, static_cast<uint>(xwattribs.depth));
        if (!m_pixmap)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Pixmap");
            return result;
        }

        const int attribs[] = {
                GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
                GLX_TEXTURE_FORMAT_EXT, xwattribs.depth == 32 ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT,
                GLX_MIPMAP_TEXTURE_EXT, False, None};

        m_glxPixmap = glXCreatePixmap(display, fbconfig, m_pixmap, attribs);
        if (!m_glxPixmap)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create GLXPixmap");
            return result;
        }

        // Create a texture
        // N.B. as for GLX Copy there is no obvious 10/12/16bit support here.
        // too many unknowns in this pipeline
        vector<QSize> size;
        size.push_back(m_openglTextureSize);
        vector<MythVideoTexture*> textures = MythVideoTexture::CreateTextures(m_context, FMT_VAAPI, FMT_RGBA32, size);
        if (textures.empty())
            return result;
        result.push_back(textures[0]);
        m_openglTextures.insert(DUMMY_INTEROP_ID, result);
    }

    if (m_openglTextures.isEmpty() || !m_glxPixmap || !m_pixmap)
        return result;
    result = m_openglTextures[DUMMY_INTEROP_ID];

    // VPP deinterlacing
    id = Deinterlace(Frame, id, Scan);

    // Copy the surface to the texture
    INIT_ST;
    va_status = vaSyncSurface(m_vaDisplay, id);
    CHECK_ST;
    unsigned short width  = static_cast<unsigned short>(m_openglTextureSize.width());
    unsigned short height = static_cast<unsigned short>(m_openglTextureSize.height());
    va_status = vaPutSurface(m_vaDisplay, id, m_pixmap,
                             0, 0, width, height, 0, 0, width, height,
                             nullptr, 0, GetFlagsForFrame(Frame, Scan));
    CHECK_ST;

    Display* glxdisplay = glXGetCurrentDisplay();
    if (glxdisplay)
    {
        XSync(glxdisplay, False);
        m_context->glBindTexture(QOpenGLTexture::Target2D, result[0]->m_textureId);
        m_glxBindTexImageEXT(glxdisplay, m_glxPixmap, GLX_FRONT_EXT, nullptr);
        m_context->glBindTexture(QOpenGLTexture::Target2D, 0);
    }
    return result;
}

bool MythVAAPIInteropGLXPixmap::InitPixmaps(void)
{
    if (m_glxBindTexImageEXT && m_glxReleaseTexImageEXT)
        return true;

    OpenGLLocker locker(m_context);
    m_glxBindTexImageEXT = reinterpret_cast<MYTH_GLXBINDTEXIMAGEEXT>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXBindTexImageEXT")));
    m_glxReleaseTexImageEXT = reinterpret_cast<MYTH_GLXRELEASETEXIMAGEEXT>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXReleaseTexImageEXT")));
    if (!m_glxBindTexImageEXT || !m_glxReleaseTexImageEXT)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to resolve 'texture_from_pixmap' functions");
        return false;
    }
    return true;
}

bool MythVAAPIInteropGLXPixmap::IsSupported(MythRenderOpenGL *Context)
{
    if (!Context)
        return false;

    OpenGLLocker locker(Context);
    Display* display = glXGetCurrentDisplay();
    if (!display)
        return false;
    int screen = DefaultScreen(display);
    QByteArray extensions(glXQueryExtensionsString(display, screen));
    return extensions.contains("GLX_EXT_texture_from_pixmap");
}

MythVAAPIInteropDRM::MythVAAPIInteropDRM(MythRenderOpenGL *Context)
  : MythVAAPIInterop(Context, VAAPIEGLDRM),
    m_drmFile(),
    m_eglImageTargetTexture2DOES(nullptr),
    m_eglCreateImageKHR(nullptr),
    m_eglDestroyImageKHR(nullptr),
    m_referenceFrames()
{
    QString device = gCoreContext->GetSetting("VAAPIDevice");
    if (device.isEmpty())
        device = "/dev/dri/renderD128";
    m_drmFile.setFileName(device);
    if (m_drmFile.open(QIODevice::ReadWrite))
    {
        m_vaDisplay = vaGetDisplayDRM(m_drmFile.handle());
        if (!m_vaDisplay)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create DRM VADisplay");
            return;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open %1").arg(device));
        return;
    }
    IniitaliseDisplay();
    InitEGL();
}

MythVAAPIInteropDRM::~MythVAAPIInteropDRM()
{
    OpenGLLocker locker(m_context);

    CleanupReferenceFrames();
    DestroyDeinterlacer();
    DeleteTextures();

    if (m_drmFile.isOpen())
        m_drmFile.close();
}

void MythVAAPIInteropDRM::DeleteTextures(void)
{
    OpenGLLocker locker(m_context);

    EGLDisplay display = eglGetCurrentDisplay();
    if (!m_openglTextures.isEmpty() && InitEGL() && display)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting DRM buffers");
        QHash<unsigned long long, vector<MythVideoTexture*> >::const_iterator it = m_openglTextures.constBegin();
        for ( ; it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythVideoTexture*> textures = it.value();
            vector<MythVideoTexture*>::iterator it2 = textures.begin();
            for ( ; it2 != textures.end(); ++it2)
            {
                if ((*it2)->m_data)
                {
                    m_eglDestroyImageKHR(display, (*it2)->m_data);
                    (*it2)->m_data = nullptr;
                }
            }
        }
    }

    MythVAAPIInterop::DeleteTextures();
}

void MythVAAPIInteropDRM::DestroyDeinterlacer(void)
{
    if (m_filterGraph)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting deinterlacer frame cache");
        DeleteTextures();
    }
    MythVAAPIInterop::DestroyDeinterlacer();
}

void MythVAAPIInteropDRM::PostInitDeinterlacer(void)
{
    // remove the old, non-deinterlaced frame cache
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting progressive frame cache");
    DeleteTextures();
}

void MythVAAPIInteropDRM::CleanupReferenceFrames(void)
{
    while (!m_referenceFrames.isEmpty())
    {
        AVBufferRef* ref = m_referenceFrames.takeLast();
        av_buffer_unref(&ref);
    }
}

void MythVAAPIInteropDRM::RotateReferenceFrames(AVBufferRef *Buffer)
{
    if (!Buffer)
        return;

    // don't retain twice for double rate
    if ((m_referenceFrames.size() > 0) &&
            (static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[0]->data)) ==
             static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(Buffer->data))))
    {
        return;
    }

    m_referenceFrames.push_front(av_buffer_ref(Buffer));

    // release old frames
    while (m_referenceFrames.size() > 3)
    {
        AVBufferRef* ref = m_referenceFrames.takeLast();
        av_buffer_unref(&ref);
    }
}

vector<MythVideoTexture*> MythVAAPIInteropDRM::GetReferenceFrames(void)
{
    vector<MythVideoTexture*> result;
    int size = m_referenceFrames.size();
    if (size < 1)
        return result;

    VASurfaceID next = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[0]->data));
    VASurfaceID current = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[size > 1 ? 1 : 0]->data));
    VASurfaceID last = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(m_referenceFrames[size > 2 ? 2 : 0]->data));

    if (!m_openglTextures.contains(next) || !m_openglTextures.contains(current) ||
        !m_openglTextures.contains(last))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Reference frame error");
        return result;
    }

    result = m_openglTextures[last];
    foreach (MythVideoTexture* tex, m_openglTextures[current])
        result.push_back(tex);
    foreach (MythVideoTexture* tex, m_openglTextures[next])
        result.push_back(tex);
    return result;
}

vector<MythVideoTexture*> MythVAAPIInteropDRM::Acquire(MythRenderOpenGL *Context,
                                                       VideoColourSpace *ColourSpace,
                                                       VideoFrame *Frame,
                                                       FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    VASurfaceID id = VerifySurface(Context, Frame);
    if (!id || !m_vaDisplay)
        return result;

    // Update frame colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Deinterlacing
    bool needreferenceframes = false;

    if (is_interlaced(Scan))
    {
        // allow GLSL deinterlacers
        Frame->deinterlace_allowed = Frame->deinterlace_allowed | DEINT_SHADER;

        // is GLSL preferred - and if so do we need reference frames
        bool glsldeint = false;

        // we explicitly use a shader if preferred over driver. If CPU only
        // is preferred, the default will be to use the driver instead and if that
        // fails we fall back to GLSL
        MythDeintType shader = GetDoubleRateOption(Frame, DEINT_SHADER);
        MythDeintType driver = GetDoubleRateOption(Frame, DEINT_DRIVER);
        if (m_filterError)
            shader = GetDoubleRateOption(Frame, DEINT_SHADER | DEINT_CPU | DEINT_DRIVER, DEINT_ALL);
        if (shader && !driver)
        {
            glsldeint = true;
            needreferenceframes = shader == DEINT_HIGH;
            Frame->deinterlace_double = Frame->deinterlace_double | DEINT_SHADER;
        }
        else if (!shader && !driver) // singlerate
        {
            shader = GetSingleRateOption(Frame, DEINT_SHADER);
            driver = GetSingleRateOption(Frame, DEINT_DRIVER);
            if (m_filterError)
                shader = GetSingleRateOption(Frame, DEINT_SHADER | DEINT_CPU | DEINT_DRIVER, DEINT_ALL);
            if (shader && !driver)
            {
                glsldeint = true;
                needreferenceframes = shader == DEINT_HIGH;
                Frame->deinterlace_single = Frame->deinterlace_single | DEINT_SHADER;
            }
        }

        // driver deinterlacing
        if (!glsldeint)
            id = Deinterlace(Frame, id, Scan);

        // fallback to shaders if VAAPI deints fail
        if (m_filterError)
            Frame->deinterlace_allowed = Frame->deinterlace_allowed & ~DEINT_DRIVER;
    }
    else if (m_deinterlacer)
    {
        DestroyDeinterlacer();
    }

    if (needreferenceframes)
    {
        if (abs(Frame->frameCounter - m_discontinuityCounter) > 1)
            CleanupReferenceFrames();
        RotateReferenceFrames(reinterpret_cast<AVBufferRef*>(Frame->priv[0]));
    }
    else
    {
        CleanupReferenceFrames();
    }
    m_discontinuityCounter = Frame->frameCounter;

    // return cached texture if available
    if (m_openglTextures.contains(id))
    {
        if (needreferenceframes)
            return GetReferenceFrames();
        else
            return m_openglTextures[id];
    }

    OpenGLLocker locker(m_context);

    // Create new
    if (!InitEGL())
        return result;

    VAImage vaimage;
    memset(&vaimage, 0, sizeof(vaimage));
    vaimage.buf = vaimage.image_id = VA_INVALID_ID;
    INIT_ST;
    va_status = vaDeriveImage(m_vaDisplay, id, &vaimage);
    CHECK_ST;
    uint count = vaimage.num_planes;

    VABufferInfo vabufferinfo;
    memset(&vabufferinfo, 0, sizeof(vabufferinfo));
    vabufferinfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    va_status = vaAcquireBufferHandle(m_vaDisplay, vaimage.buf, &vabufferinfo);
    CHECK_ST;

    VideoFrameType format = VATypeToMythType(vaimage.format.fourcc);
    if (format == FMT_NONE)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported VA fourcc: %1")
            .arg(fourcc_str(static_cast<int32_t>(vaimage.format.fourcc))));
    }
    else
    {
        if (count != planes(format))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Inconsistent plane count %1 != %2")
                .arg(count).arg(planes(format)));
        }
        else
        {
            vector<QSize> sizes;
            for (uint plane = 0 ; plane < count; ++plane)
            {
                QSize size(vaimage.width, vaimage.height);
                if (plane > 0)
                    size = QSize(vaimage.width >> 1, vaimage.height >> 1);
                sizes.push_back(size);
            }

            vector<MythVideoTexture*> textures = MythVideoTexture::CreateTextures(m_context, FMT_VAAPI, format, sizes);
            if (textures.size() != count)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create all textures");
            }
            else
            {
                for (uint i = 0; i < textures.size(); ++i)
                    textures[i]->m_allowGLSLDeint = true;
                CreateDRMBuffers(format, textures, vabufferinfo.handle, vaimage);
                result = textures;
            }
        }
    }

    va_status = vaReleaseBufferHandle(m_vaDisplay, vaimage.buf);
    CHECK_ST;
    va_status = vaDestroyImage(m_vaDisplay, vaimage.image_id);
    CHECK_ST;

    m_openglTextures.insert(id, result);
    if (needreferenceframes)
        return GetReferenceFrames();
    return result;
}

VideoFrameType MythVAAPIInteropDRM::VATypeToMythType(uint32_t Fourcc)
{
    switch (Fourcc)
    {
        case VA_FOURCC_IYUV:
        case VA_FOURCC_I420: return FMT_YV12;
        case VA_FOURCC_NV12: return FMT_NV12;
        case VA_FOURCC_YUY2: return FMT_YUY2;
        case VA_FOURCC_UYVY: return FMT_YUY2; // ?
        case VA_FOURCC_P010: return FMT_P010;
        case VA_FOURCC_P016: return FMT_P016;
        case VA_FOURCC_ARGB: return FMT_ARGB32;
        case VA_FOURCC_RGBA: return FMT_RGBA32;
    }
    return FMT_NONE;
}

bool MythVAAPIInteropDRM::InitEGL(void)
{
    if (m_eglImageTargetTexture2DOES && m_eglCreateImageKHR && m_eglDestroyImageKHR)
        return true;

    if (!m_context)
        return false;

    OpenGLLocker locker(m_context);
    m_eglImageTargetTexture2DOES = reinterpret_cast<MYTH_EGLIMAGETARGET>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    m_eglCreateImageKHR          = reinterpret_cast<MYTH_EGLCREATEIMAGE>(eglGetProcAddress("eglCreateImageKHR"));
    m_eglDestroyImageKHR         = reinterpret_cast<MYTH_EGLDESTROYIMAGE>(eglGetProcAddress("eglDestroyImageKHR"));

    if (!(m_eglCreateImageKHR && m_eglDestroyImageKHR && m_eglImageTargetTexture2DOES))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to resolve EGL functions");
        return false;
    }
    return true;
}

#ifndef DRM_FORMAT_R8
#define DRM_FORMAT_R8       MKTAG('R', '8', ' ', ' ')
#define DRM_FORMAT_GR88     MKTAG('G', 'R', '8', '8')
#define DRM_FORMAT_R16      MKTAG('R', '1', '6', ' ')
#define DRM_FORMAT_GR32     MKTAG('G', 'R', '3', '2')
#endif

/*! \brief Create a set of EGL images/DRM buffers associated with the given textures
*/
void MythVAAPIInteropDRM::CreateDRMBuffers(VideoFrameType Format,
                                           vector<MythVideoTexture*> Textures,
                                           uintptr_t Handle, VAImage &Image)
{
    for (uint plane = 0; plane < Textures.size(); ++plane)
    {
        MythVideoTexture* texture = Textures[plane];
        int fourcc = (Format == FMT_P010) ? DRM_FORMAT_R16 : DRM_FORMAT_R8;
        if (plane > 0)
            fourcc = (Format == FMT_P010) ? DRM_FORMAT_GR32 : DRM_FORMAT_GR88;
        const EGLint attributes[] = {
                EGL_LINUX_DRM_FOURCC_EXT, fourcc,
                EGL_WIDTH,  texture->m_size.width(),
                EGL_HEIGHT, texture->m_size.height(),
                EGL_DMA_BUF_PLANE0_FD_EXT,     static_cast<EGLint>(Handle),
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(Image.offsets[plane]),
                EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(Image.pitches[plane]),
                EGL_NONE
        };

        EGLDisplay display = eglGetCurrentDisplay();
        if (!display)
            LOG(VB_GENERAL, LOG_ERR, LOC + "No EGLDisplay");

        EGLImageKHR image = m_eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2").arg(plane).arg(eglGetError()));

        m_context->glBindTexture(texture->m_target, texture->m_textureId);
        m_eglImageTargetTexture2DOES(texture->m_target, image);
        m_context->glBindTexture(texture->m_target, 0);
        texture->m_data = static_cast<unsigned char *>(image);
    }
}

bool MythVAAPIInteropDRM::IsSupported(MythRenderOpenGL *Context)
{
    if (!Context)
        return false;

    OpenGLLocker locker(Context);
    EGLDisplay egldisplay = eglGetCurrentDisplay();
    if (!egldisplay)
        return false;
    QByteArray extensions = QByteArray(eglQueryString(egldisplay, EGL_EXTENSIONS));
    return extensions.contains("EGL_EXT_image_dma_buf_import") &&
           Context->hasExtension("GL_OES_EGL_image");
}

bool MythVAAPIInterop::SetupDeinterlacer(MythDeintType Deinterlacer, bool DoubleRate,
                                         AVBufferRef *FramesContext,
                                         int Width, int Height,
                                         // Outputs
                                         AVFilterGraph *&Graph,
                                         AVFilterContext *&Source,
                                         AVFilterContext *&Sink)
{
    if (!FramesContext)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No hardware frames context");
        return false;
    }

    int ret;
    QString args;
    QString deinterlacer = "bob";
    if (DEINT_MEDIUM == Deinterlacer)
        deinterlacer = "motion_adaptive";
    else if (DEINT_HIGH == Deinterlacer)
        deinterlacer = "motion_compensated";

    // N.B. set auto to 0 otherwise we confuse playback if VAAPI does not deinterlace
    QString filters = QString("deinterlace_vaapi=mode=%1:rate=%2:auto=0")
            .arg(deinterlacer).arg(DoubleRate ? "field" : "frame");
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVBufferSrcParameters* params = nullptr;

    Graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !Graph)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    args = QString("video_size=%1x%2:pix_fmt=%3:time_base=1/1")
                .arg(Width).arg(Height).arg(AV_PIX_FMT_VAAPI);

    ret = avfilter_graph_create_filter(&Source, buffersrc, "in",
                                       args.toLocal8Bit().constData(), nullptr, Graph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer source");
        goto end;
    }

    params = av_buffersrc_parameters_alloc();
    params->hw_frames_ctx = FramesContext;
    ret = av_buffersrc_parameters_set(Source, params);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "av_buffersrc_parameters_set failed");
        goto end;
    }
    av_freep(&params);

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&Sink, buffersink, "out",
                                       nullptr, nullptr, Graph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer sink");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = Source;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = Sink;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    if ((ret = avfilter_graph_parse_ptr(Graph, filters.toLocal8Bit(),
                                        &inputs, &outputs, nullptr)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("avfilter_graph_parse_ptr failed for %1")
            .arg(filters));
        goto end;
    }

    if ((ret = avfilter_graph_config(Graph, nullptr)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("VAAPI deinterlacer config failed - '%1' unsupported?").arg(deinterlacer));
        goto end;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created deinterlacer '%1'")
        .arg(DeinterlacerName(Deinterlacer | DEINT_DRIVER, DoubleRate, FMT_VAAPI)));

end:
    if (ret < 0)
    {
        avfilter_graph_free(&Graph);
        Graph = nullptr;
    }
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret >= 0;
}

VASurfaceID MythVAAPIInterop::Deinterlace(VideoFrame *Frame, VASurfaceID Current, FrameScanType Scan)
{
    VASurfaceID result = Current;
    if (!Frame)
        return result;

    while (!m_filterError && is_interlaced(Scan))
    {
        // N.B. for DRM the use of a shader deint is checked before we get here
        MythDeintType deinterlacer = DEINT_NONE;
        bool doublerate = true;
        // no CPU or GLSL deinterlacing so pick up these options as well
        // N.B. Override deinterlacer_allowed to pick up any preference
        MythDeintType doublepref = GetDoubleRateOption(Frame, DEINT_DRIVER | DEINT_SHADER | DEINT_CPU, DEINT_ALL);
        MythDeintType singlepref = GetSingleRateOption(Frame, DEINT_DRIVER | DEINT_SHADER | DEINT_CPU, DEINT_ALL);

        if (doublepref)
        {
            deinterlacer = doublepref;
        }
        else if (singlepref)
        {
            deinterlacer = singlepref;
            doublerate = false;
        }

        if ((m_deinterlacer == deinterlacer) && (m_deinterlacer2x == doublerate))
            break;

        DestroyDeinterlacer();

        if (deinterlacer != DEINT_NONE)
        {
            AVBufferRef* frames = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
            if (!frames)
                break;

            AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
            if (!hwdeviceref)
                break;

            AVHWDeviceContext* hwdevicecontext  = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
            hwdevicecontext->free = [](AVHWDeviceContext*) { LOG(VB_PLAYBACK, LOG_INFO, LOC + "VAAPI VPP device context finished"); };

            AVVAAPIDeviceContext *vaapidevicectx = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevicecontext->hwctx);
            vaapidevicectx->display = m_vaDisplay; // re-use the existing display

            if (av_hwdevice_ctx_init(hwdeviceref) < 0)
            {
                av_buffer_unref(&hwdeviceref);
                m_filterError = true;
                break;
            }

            AVBufferRef *newframes = av_hwframe_ctx_alloc(hwdeviceref);
            if (!newframes)
            {
                m_filterError = true;
                av_buffer_unref(&hwdeviceref);
                break;
            }

            AVHWFramesContext* dstframes = reinterpret_cast<AVHWFramesContext*>(newframes->data);
            AVHWFramesContext* srcframes = reinterpret_cast<AVHWFramesContext*>(frames->data);

            m_filterWidth = srcframes->width;
            m_filterHeight = srcframes->height;
            static const int vpppoolsize = 2; // seems to be enough
            dstframes->sw_format = srcframes->sw_format;
            dstframes->width = m_filterWidth;
            dstframes->height = m_filterHeight;
            dstframes->initial_pool_size = vpppoolsize;
            dstframes->format = AV_PIX_FMT_VAAPI;
            dstframes->free = [](AVHWFramesContext*) { LOG(VB_PLAYBACK, LOG_INFO, LOC + "VAAPI VPP frames context finished"); };

            if (av_hwframe_ctx_init(newframes) < 0)
            {
                m_filterError = true;
                av_buffer_unref(&hwdeviceref);
                av_buffer_unref(&newframes);
                break;
            }

            m_vppFramesContext = newframes;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New VAAPI frame pool with %1 %2x%3 surfaces")
                .arg(vpppoolsize).arg(m_filterWidth).arg(m_filterHeight));
            av_buffer_unref(&hwdeviceref);

            if (!MythVAAPIInterop::SetupDeinterlacer(deinterlacer, doublerate, m_vppFramesContext,
                                                     m_filterWidth, m_filterHeight,
                                                     m_filterGraph, m_filterSource, m_filterSink))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create VAAPI deinterlacer %1 - disabling")
                    .arg(DeinterlacerName(deinterlacer | DEINT_DRIVER, doublerate, FMT_VAAPI)));
                DestroyDeinterlacer();
                m_filterError = true;
            }
            else
            {
                m_deinterlacer = deinterlacer;
                m_deinterlacer2x = doublerate;
                PostInitDeinterlacer();
                break;
            }
        }
        break;
    }

    if (!is_interlaced(Scan) && m_deinterlacer)
        DestroyDeinterlacer();

    if (m_deinterlacer)
    {
        // deinterlacing the pause frame repeatedly is unnecessary and, due to the
        // buffering in the VAAPI frames context, causes the image to 'jiggle' when using
        // double rate deinterlacing. If we are confident this is a pause frame we have seen,
        // return the last deinterlaced frame.
        if (Frame->pause_frame && m_lastFilteredFrame && (m_lastFilteredFrameCount == Frame->frameCounter))
            return m_lastFilteredFrame;

        Frame->deinterlace_inuse = m_deinterlacer | DEINT_DRIVER;
        Frame->deinterlace_inuse2x = m_deinterlacer2x;

        // 'pump' the filter with frames until it starts returning usefull output.
        // This minimises discontinuities at start up (where we would otherwise
        // show a couple of progressive frames first) and memory consumption for the DRM
        // interop as we only cache OpenGL textures for the deinterlacer's frame
        // pool.
        int retries = 3;
        while ((result == Current) && retries--)
        {
            while (true)
            {
                int ret = 0;
                MythAVFrame sinkframe;
                sinkframe->format =  AV_PIX_FMT_VAAPI;

                // only ask for a frame if we are expecting another
                if (m_deinterlacer2x && !m_firstField)
                {
                    ret = av_buffersink_get_frame(m_filterSink, sinkframe);
                    if  (ret >= 0)
                    {
                        // we have a filtered frame
                        result = m_lastFilteredFrame = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(sinkframe->data[3]));
                        m_lastFilteredFrameCount = Frame->frameCounter;
                        m_firstField = true;
                        break;
                    }
                    else if (ret != AVERROR(EAGAIN))
                        break;
                }

                // add another frame
                MythAVFrame sourceframe;
                sourceframe->top_field_first = Frame->interlaced_reversed ? !Frame->top_field_first : Frame->top_field_first;
                sourceframe->interlaced_frame = 1;
                sourceframe->data[3] = Frame->buf;
                sourceframe->buf[0] = av_buffer_ref(reinterpret_cast<AVBufferRef*>(Frame->priv[0]));
                sourceframe->width  = m_filterWidth;
                sourceframe->height = m_filterHeight;
                sourceframe->format = AV_PIX_FMT_VAAPI;
                ret = av_buffersrc_add_frame(m_filterSource, sourceframe);
                sourceframe->data[3] = nullptr;
                sourceframe->buf[0] = nullptr;
                if (ret < 0)
                    break;

                // try again
                ret = av_buffersink_get_frame(m_filterSink, sinkframe);
                if  (ret >= 0)
                {
                    // we have a filtered frame
                    result = m_lastFilteredFrame = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(sinkframe->data[3]));
                    m_lastFilteredFrameCount = Frame->frameCounter;
                    m_firstField = false;
                    break;
                }
                break;
            }
        }
    }
    return result;
}
