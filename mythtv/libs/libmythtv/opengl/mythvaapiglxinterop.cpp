// MythTV
#include "mythvideoout.h"
#include "opengl/mythvaapiglxinterop.h"

#define LOC QString("VAAPIGLX: ")

MythVAAPIInteropGLX::MythVAAPIInteropGLX(MythPlayerUI* Player, MythRenderOpenGL* Context, InteropType Type)
  : MythVAAPIInterop(Player, Context, Type)
{
}

MythVAAPIInteropGLX::~MythVAAPIInteropGLX()
{
    delete [] m_vaapiPictureAttributes;
}

uint MythVAAPIInteropGLX::GetFlagsForFrame(MythVideoFrame* Frame, FrameScanType Scan)
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
        MythDeintType driverdeint = Frame->GetDoubleRateOption(DEINT_DRIVER | DEINT_CPU | DEINT_SHADER);
        if (!driverdeint)
        {
            doublerate = false;
            driverdeint = Frame->GetSingleRateOption(DEINT_DRIVER | DEINT_CPU | DEINT_SHADER);
        }

        if (driverdeint)
        {
            driverdeint = DEINT_BASIC;
            if (m_basicDeinterlacer != driverdeint)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Enabled deinterlacer '%1'")
                    .arg(MythVideoFrame::DeinterlacerName(driverdeint | DEINT_DRIVER, doublerate, FMT_VAAPI)));
            }

            bool top = Frame->m_interlacedReverse ? !Frame->m_topFieldFirst : Frame->m_topFieldFirst;
            if (Scan == kScan_Interlaced)
            {
                Frame->m_deinterlaceInuse = driverdeint | DEINT_DRIVER;
                Frame->m_deinterlaceInuse2x = doublerate;
                flags = top ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
            }
            else if (Scan == kScan_Intr2ndField)
            {
                Frame->m_deinterlaceInuse = driverdeint | DEINT_DRIVER;
                Frame->m_deinterlaceInuse2x = doublerate;
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
        switch (Frame->m_colorspace)
        {
            case AVCOL_SPC_BT709:     m_vaapiColourSpace = VA_SRC_BT709; break;
            case AVCOL_SPC_SMPTE170M:
            case AVCOL_SPC_BT470BG:   m_vaapiColourSpace = VA_SRC_BT601; break;
            case AVCOL_SPC_SMPTE240M: m_vaapiColourSpace = VA_SRC_SMPTE_240; break;
            default:
            m_vaapiColourSpace = ((Frame->m_width < 1280) ? VA_SRC_BT601 : VA_SRC_BT709); break;
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using '%1' VAAPI colourspace")
            .arg((m_vaapiColourSpace == VA_SRC_BT709) ? "bt709" : ((m_vaapiColourSpace == VA_SRC_BT601) ? "bt601" : "smpte240")));
    }
    flags |= m_vaapiColourSpace;
    return flags;
}

void MythVAAPIInteropGLX::InitPictureAttributes(MythVideoColourSpace* ColourSpace)
{
    m_vaapiColourSpace = 0;
    if (!ColourSpace || !m_vaDisplay)
        return;

    OpenGLLocker locker(m_openglContext);

    delete [] m_vaapiPictureAttributes;
    m_vaapiPictureAttributeCount = 0;
    int supported_controls = kPictureAttributeSupported_None;
    QVector<VADisplayAttribute> supported;
    int num = vaMaxNumDisplayAttributes(m_vaDisplay);
    auto* attribs = new VADisplayAttribute[static_cast<unsigned int>(num)];

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
    connect(ColourSpace, &MythVideoColourSpace::PictureAttributeChanged, this, &MythVAAPIInteropGLX::SetPictureAttribute);

    // create
    delete [] attribs;

    if (supported.isEmpty())
        return;

    m_vaapiPictureAttributeCount = supported.size();
    m_vaapiPictureAttributes = new VADisplayAttribute[static_cast<unsigned int>(m_vaapiPictureAttributeCount)];
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
            adjustment = 50;
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
            Value = std::clamp(Value, 0, 100);
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
        m_openglContext->makeCurrent();
        INIT_ST;
        va_status = vaSetDisplayAttributes(m_vaDisplay,
                                           m_vaapiPictureAttributes,
                                           m_vaapiPictureAttributeCount);
        CHECK_ST;
        m_openglContext->doneCurrent();
        return Value;
    }
    return -1;
}

MythVAAPIInteropGLXCopy::MythVAAPIInteropGLXCopy(MythPlayerUI* Player, MythRenderOpenGL* Context)
  : MythVAAPIInteropGLX(Player, Context, GL_VAAPIGLXCOPY)
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
    InitaliseDisplay();
}

MythVAAPIInteropGLXCopy::~MythVAAPIInteropGLXCopy()
{
    if (m_glxSurface && m_vaDisplay)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting GLX surface");
        OpenGLLocker locker(m_openglContext);
        INIT_ST;
        va_status = vaDestroySurfaceGLX(m_vaDisplay, m_glxSurface);
        CHECK_ST;
    }
}

std::vector<MythVideoTextureOpenGL*>
MythVAAPIInteropGLXCopy::Acquire(MythRenderOpenGL* Context,
                                 MythVideoColourSpace* ColourSpace,
                                 MythVideoFrame* Frame,
                                 FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
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
    OpenGLLocker locker(m_openglContext);

    // we only ever use one glx surface which is updated on each call
    if (m_openglTextures.isEmpty())
    {
        // create a texture
        // N.B. No apparent 10/12/16bit support here. Can't encourage vaCreateSurfaceGLX
        // to work with a 16bit texture
        MythVideoTextureOpenGL *texture = MythVideoTextureOpenGL::CreateTexture(m_openglContext, m_textureSize);
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
            MythVideoTextureOpenGL::DeleteTexture(m_openglContext, texture);
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

MythVAAPIInteropGLXPixmap::MythVAAPIInteropGLXPixmap(MythPlayerUI* Player, MythRenderOpenGL* Context)
  : MythVAAPIInteropGLX(Player, Context, GL_VAAPIGLXPIX)
{
    m_vaDisplay = vaGetDisplay(glXGetCurrentDisplay());
    if (!m_vaDisplay)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create X11 VADisplay");
    else
        InitaliseDisplay();
    InitPixmaps();
}

MythVAAPIInteropGLXPixmap::~MythVAAPIInteropGLXPixmap()
{
    OpenGLLocker locker(m_openglContext);
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

std::vector<MythVideoTextureOpenGL*>
MythVAAPIInteropGLXPixmap::Acquire(MythRenderOpenGL* Context,
                                   MythVideoColourSpace* ColourSpace,
                                   MythVideoFrame* Frame,
                                   FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
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
    OpenGLLocker locker(m_openglContext);
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
        const std::array<const int,23> fbattribs {
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
                0 };
        int fbcount = 0;
        GLXFBConfig *fbs = glXChooseFBConfig(display, DefaultScreen(display), fbattribs.data(), &fbcount);
        if (!fbcount)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to retrieve GLX framebuffer config");
            return result;
        }

        GLXFBConfig fbconfig = fbs[0];
        XFree(reinterpret_cast<void*>(fbs));

        // create pixmaps
        uint width  = static_cast<uint>(m_textureSize.width());
        uint height = static_cast<uint>(m_textureSize.height());
        XWindowAttributes xwattribs;
        XGetWindowAttributes(display, DefaultRootWindow(display), &xwattribs);
        m_pixmap = XCreatePixmap(display, DefaultRootWindow(display),
                                 width, height, static_cast<uint>(xwattribs.depth));
        if (!m_pixmap)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Pixmap");
            return result;
        }

        const std::array<const int,7> attribs {
                GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
                GLX_TEXTURE_FORMAT_EXT, xwattribs.depth == 32 ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT,
                GLX_MIPMAP_TEXTURE_EXT, False, 0};

        m_glxPixmap = glXCreatePixmap(display, fbconfig, m_pixmap, attribs.data());
        if (!m_glxPixmap)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create GLXPixmap");
            return result;
        }

        // Create a texture
        // N.B. as for GLX Copy there is no obvious 10/12/16bit support here.
        // too many unknowns in this pipeline
        std::vector<QSize> size;
        size.push_back(m_textureSize);
        std::vector<MythVideoTextureOpenGL*> textures = MythVideoTextureOpenGL::CreateTextures(m_openglContext, FMT_VAAPI, FMT_RGBA32, size);
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
    auto width  = static_cast<unsigned short>(m_textureSize.width());
    auto height = static_cast<unsigned short>(m_textureSize.height());
    va_status = vaPutSurface(m_vaDisplay, id, m_pixmap,
                             0, 0, width, height, 0, 0, width, height,
                             nullptr, 0, GetFlagsForFrame(Frame, Scan));
    CHECK_ST;

    Display* glxdisplay = glXGetCurrentDisplay();
    if (glxdisplay)
    {
        XSync(glxdisplay, False);
        m_openglContext->glBindTexture(QOpenGLTexture::Target2D, result[0]->m_textureId);
        m_glxBindTexImageEXT(glxdisplay, m_glxPixmap, GLX_FRONT_EXT, nullptr);
        m_openglContext->glBindTexture(QOpenGLTexture::Target2D, 0);
    }
    return result;
}

bool MythVAAPIInteropGLXPixmap::InitPixmaps()
{
    if (m_glxBindTexImageEXT && m_glxReleaseTexImageEXT)
        return true;

    OpenGLLocker locker(m_openglContext);
    m_glxBindTexImageEXT = reinterpret_cast<MYTH_GLXBINDTEXIMAGEEXT>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXBindTexImageEXT")));
    m_glxReleaseTexImageEXT = reinterpret_cast<MYTH_GLXRELEASETEXIMAGEEXT>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXReleaseTexImageEXT")));
    if (!m_glxBindTexImageEXT || !m_glxReleaseTexImageEXT)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to resolve 'texture_from_pixmap' functions");
        return false;
    }
    return true;
}

bool MythVAAPIInteropGLXPixmap::IsSupported(MythRenderOpenGL* Context)
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
