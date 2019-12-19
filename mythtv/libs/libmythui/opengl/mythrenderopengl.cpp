// Std
#include <algorithm>
using std::min;

// Qt
#include <QLibrary>
#include <QPainter>
#include <QWindow>
#include <QWidget>
#include <QGuiApplication>
#include <QOpenGLExtraFunctions>

// MythTV
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "mythrenderopengl.h"
#include "mythrenderopenglshaders.h"
#include "mythlogging.h"
#include "mythuitype.h"
#include "mythxdisplay.h"
#define LOC QString("OpenGL: ")

#ifdef Q_OS_ANDROID
#include <android/log.h>
#include <QWindow>
#endif

#define VERTEX_INDEX  0
#define COLOR_INDEX   1
#define TEXTURE_INDEX 2
#define VERTEX_SIZE   2
#define TEXTURE_SIZE  2

static const GLuint kVertexOffset  = 0;
static const GLuint kTextureOffset = 8 * sizeof(GLfloat);
const GLuint MythRenderOpenGL::kVertexSize = 16 * sizeof(GLfloat);

#define MAX_VERTEX_CACHE 500

MythGLTexture::MythGLTexture(QOpenGLTexture *Texture)
  : m_texture(Texture)
{
}

MythGLTexture::MythGLTexture(GLuint Texture)
  : m_textureId(Texture)
{
}

OpenGLLocker::OpenGLLocker(MythRenderOpenGL *Render)
  : m_render(Render)
{
    if (m_render)
        m_render->makeCurrent();
}

OpenGLLocker::~OpenGLLocker()
{
    if (m_render)
        m_render->doneCurrent();
}

MythRenderOpenGL* MythRenderOpenGL::GetOpenGLRender(void)
{
    // Don't try and create the window
    if (!HasMythMainWindow())
        return nullptr;

    MythMainWindow* window = MythMainWindow::getMainWindow();
    if (!window)
        return nullptr;

    auto* result = dynamic_cast<MythRenderOpenGL*>(window->GetRenderDevice());
    if (result)
        return result;
    return nullptr;
}

MythRenderOpenGL* MythRenderOpenGL::Create(const QString& /*Painter*/, QPaintDevice* Device)
{
    QString display = getenv("DISPLAY");
    // Determine if we are running a remote X11 session
    // DISPLAY=:x or DISPLAY=unix:x are local
    // DISPLAY=hostname:x is remote
    // DISPLAY=/xxx/xxx/.../org.macosforge.xquartz:x is local OS X
    // x can be numbers n or n.n
    // Anything else including DISPLAY not set is assumed local,
    // in that case we are probably not running under X11
    if (!display.isEmpty()
     && !display.startsWith(":")
     && !display.startsWith("unix:")
     && !display.startsWith("/")
     &&  display.contains(':'))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL is disabled for Remote X Session");
        return nullptr;
    }

    // N.B the core profiles below are designed to target compute shader availability
    bool opengles = !qgetenv("MYTHTV_OPENGL_ES").isEmpty();
    bool core     = !qgetenv("MYTHTV_OPENGL_CORE").isEmpty();
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    if (core)
    {
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setMajorVersion(4);
        format.setMinorVersion(3);
    }

    if (opengles)
    {
        if (core)
        {
            format.setProfile(QSurfaceFormat::CoreProfile);
            format.setMajorVersion(3);
            format.setMinorVersion(1);
        }
        format.setRenderableType(QSurfaceFormat::OpenGLES);
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        format.setOption(QSurfaceFormat::DebugContext);

    return new MythRenderOpenGL(format, Device);
}

MythRenderOpenGL::MythRenderOpenGL(const QSurfaceFormat& Format, QPaintDevice* Device,
                                   RenderType Type)
  : MythEGL(this),
    MythRender(Type),
    m_fullRange(gCoreContext->GetBoolSetting("GUIRGBLevels", true))
{
    memset(m_defaultPrograms, 0, sizeof(m_defaultPrograms));
    m_projection.fill(0);
    m_parameters.fill(0);
    m_transforms.push(QMatrix4x4());

    auto *w = dynamic_cast<QWidget*>(Device);
    m_window = (w) ? w->windowHandle() : nullptr;

    setFormat(Format);

    connect(this, &QOpenGLContext::aboutToBeDestroyed, this, &MythRenderOpenGL::contextToBeDestroyed);
}

MythRenderOpenGL::~MythRenderOpenGL()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "MythRenderOpenGL closing");
    if (!isValid())
        return;
    disconnect(this, &QOpenGLContext::aboutToBeDestroyed, this, &MythRenderOpenGL::contextToBeDestroyed);
    MythRenderOpenGL::ReleaseResources();
}

void MythRenderOpenGL::messageLogged(const QOpenGLDebugMessage &Message)
{
    // filter unwanted messages
    if (m_openGLDebuggerFilter & Message.type())
        return;

    QString source("Unknown");
    QString type("Unknown");

    switch (Message.source())
    {
        case QOpenGLDebugMessage::ApplicationSource: return; // filter out our own messages
        case QOpenGLDebugMessage::APISource:            source = "API";        break;
        case QOpenGLDebugMessage::WindowSystemSource:   source = "WinSys";     break;
        case QOpenGLDebugMessage::ShaderCompilerSource: source = "ShaderComp"; break;
        case QOpenGLDebugMessage::ThirdPartySource:     source = "3rdParty";   break;
        case QOpenGLDebugMessage::OtherSource:          source = "Other";      break;
        default: break;
    }

    // N.B. each break is on a separate line to allow setting individual break points
    // when using synchronous logging
        switch (Message.type())
    {
        case QOpenGLDebugMessage::ErrorType:
            type = "Error"; break;
        case QOpenGLDebugMessage::DeprecatedBehaviorType:
            type = "Deprecated"; break;
        case QOpenGLDebugMessage::UndefinedBehaviorType:
            type = "Undef behaviour"; break;
        case QOpenGLDebugMessage::PortabilityType:
            type = "Portability"; break;
        case QOpenGLDebugMessage::PerformanceType:
            type = "Performance"; break;
        case QOpenGLDebugMessage::OtherType:
            type = "Other"; break;
        case QOpenGLDebugMessage::MarkerType:
            type = "Marker"; break;
        case QOpenGLDebugMessage::GroupPushType:
            type = "GroupPush"; break;
        case QOpenGLDebugMessage::GroupPopType:
            type = "GroupPop"; break;
        default: break;
    }
    LOG(VB_GPU, LOG_INFO, LOC + QString("Src: %1 Type: %2 Msg: %3").arg(source).arg(type).arg(Message.message()));
}

void MythRenderOpenGL:: logDebugMarker(const QString &Message)
{
    if (m_openglDebugger)
    {
        QOpenGLDebugMessage message = QOpenGLDebugMessage::createApplicationMessage(
            Message, 0, QOpenGLDebugMessage::NotificationSeverity, QOpenGLDebugMessage::MarkerType);
        m_openglDebugger->logMessage(message);
    }
}

// Can't be static because its connected to a signal and passed "this".
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void MythRenderOpenGL::contextToBeDestroyed(void)
{
    LOG(VB_GENERAL, LOG_WARNING, LOC + "Context about to be destroyed");
}

bool MythRenderOpenGL::Init(void)
{
    if (!isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "MythRenderOpenGL is not a valid OpenGL rendering context");
        return false;
    }

    OpenGLLocker locker(this);
    initializeOpenGLFunctions();
    m_features = openGLFeatures();

    // don't enable this by default - it can generate a lot of detail
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
    {
        m_openglDebugger = new QOpenGLDebugLogger();
        if (m_openglDebugger->initialize())
        {
            connect(m_openglDebugger, SIGNAL(messageLogged(QOpenGLDebugMessage)), this, SLOT(messageLogged(QOpenGLDebugMessage)));
            QOpenGLDebugLogger::LoggingMode mode = QOpenGLDebugLogger::AsynchronousLogging;

            // this will impact performance but can be very useful
            if (!qgetenv("MYTHTV_OPENGL_SYNCHRONOUS").isEmpty())
                mode = QOpenGLDebugLogger::SynchronousLogging;

            m_openglDebugger->startLogging(mode);
            if (mode == QOpenGLDebugLogger::AsynchronousLogging)
                LOG(VB_GENERAL, LOG_INFO, LOC + "GPU debug logging started (async)");
            else
                LOG(VB_GENERAL, LOG_INFO, LOC + "Started synchronous GPU debug logging (will hurt performance)");

            // filter messages. Some drivers can be extremely verbose for certain issues.
            QStringList debug;
            QString filter = qgetenv("MYTHTV_OPENGL_LOGFILTER");
            if (filter.contains("other", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::OtherType;
                debug << "Other";
            }
            if (filter.contains("error", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::ErrorType;
                debug << "Error";
            }
            if (filter.contains("deprecated", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::DeprecatedBehaviorType;
                debug << "Deprecated";
            }
            if (filter.contains("undefined", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::UndefinedBehaviorType;
                debug << "Undefined";
            }
            if (filter.contains("portability", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::PortabilityType;
                debug << "Portability";
            }
            if (filter.contains("performance", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::PerformanceType;
                debug << "Performance";
            }
            if (filter.contains("grouppush", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::GroupPushType;
                debug << "GroupPush";
            }
            if (filter.contains("grouppop", Qt::CaseInsensitive))
            {
                m_openGLDebuggerFilter |= QOpenGLDebugMessage::GroupPopType;
                debug << "GroupPop";
            }

            if (!debug.isEmpty())
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Filtering out GPU messages for: %1")
                    .arg(debug.join(", ")));
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to initialise OpenGL logging");
            delete m_openglDebugger;
            m_openglDebugger = nullptr;
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        logDebugMarker("RENDER_INIT_START");

    Init2DState();

    // basic features
    GLint maxtexsz = 0;
    GLint maxunits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxunits);
    m_maxTextureUnits = maxunits;
    m_maxTextureSize  = (maxtexsz) ? maxtexsz : 512;
    QSurfaceFormat fmt = format();
    m_colorDepth = qMin(fmt.redBufferSize(), qMin(fmt.greenBufferSize(), fmt.blueBufferSize()));

    // RGBA16 - available on ES via extension
    m_extraFeatures |= isOpenGLES() ? hasExtension("GL_EXT_texture_norm16") ? kGLExtRGBA16 : kGLFeatNone : kGLExtRGBA16;

    // Pixel buffer objects
    bool buffer_procs = reinterpret_cast<MYTH_GLMAPBUFFERPROC>(GetProcAddress("glMapBuffer")) &&
                        reinterpret_cast<MYTH_GLUNMAPBUFFERPROC>(GetProcAddress("glUnmapBuffer"));

    // Buffers are available by default (GL and GLES).
    // Buffer mapping is available by extension
    if ((isOpenGLES() && hasExtension("GL_OES_mapbuffer") && buffer_procs) ||
        (hasExtension("GL_ARB_vertex_buffer_object") && buffer_procs))
        m_extraFeatures |= kGLBufferMap;

    // Rectangular textures
    if (!isOpenGLES() && (hasExtension("GL_NV_texture_rectangle") ||
                          hasExtension("GL_ARB_texture_rectangle") ||
                          hasExtension("GL_EXT_texture_rectangle")))
        m_extraFeatures |= kGLExtRects;

    // GL_RED etc texure formats. Not available on GLES2.0 or GL < 2
    if ((isOpenGLES() && format().majorVersion() < 3) ||
        (!isOpenGLES() && format().majorVersion() < 2))
        m_extraFeatures |= kGLLegacyTextures;

    // GL_UNPACK_ROW_LENGTH - for uploading video textures
    // Note: Should also be available on GL1.4 per specification
    if (!isOpenGLES() || (isOpenGLES() && ((fmt.majorVersion() >= 3) || hasExtension("GL_EXT_unpack_subimage"))))
        m_extraFeatures |= kGLExtSubimage;

    // check for core profile
    m_coreProfile = fmt.profile() == QSurfaceFormat::OpenGLContextProfile::CoreProfile;

    // if we have a core profile then we need a VAO bound - this is just a
    // workaround for the time being
    if (m_coreProfile)
    {
        QOpenGLExtraFunctions extra(nullptr);
        extra.initializeOpenGLFunctions();
        extra.glGenVertexArrays(1, &m_vao);
        extra.glBindVertexArray(m_vao);
    }

    // For (embedded) GPUs that use tile based rendering, it is faster to use
    // glClear e.g. on the Pi3 it improves video frame rate significantly. Using
    // glClear tells the GPU it doesn't have to retrieve the old framebuffer and will
    // also clear existing draw calls.
    // For now this just includes Broadcom VideoCoreIV.
    // Other Tile Based Deferred Rendering GPUS - PowerVR5/6/7, Apple (PowerVR as well?)
    // Other Tile Based Immediate Mode Rendering GPUS - ARM Mali, Qualcomm Adreno
    static const QByteArray kTiled[3] = { "videocore", "vc4", "v3d" };
    auto renderer = QByteArray(reinterpret_cast<const char*>(glGetString(GL_RENDERER))).toLower();
    for (int i = 0 ; i < 3; ++i)
    {
        if (renderer.contains(kTiled[i]))
        {
            m_extraFeatures |= kGLTiled;
            break;
        }
    }

    DebugFeatures();

    m_extraFeaturesUsed = m_extraFeatures;
    if (!CreateDefaultShaders())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create default shaders");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialised MythRenderOpenGL");
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using %1 range output").arg(m_fullRange ? "full" : "limited"));
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        logDebugMarker("RENDER_INIT_END");
    return true;
}

#define GLYesNo(arg) ((arg) ? "Yes" : "No")

void MythRenderOpenGL::DebugFeatures(void)
{
    static bool s_debugged = false;
    if (s_debugged)
        return;
    s_debugged = true;
    QSurfaceFormat fmt = format();
    QString qtglversion = QString("OpenGL%1 %2.%3")
            .arg(fmt.renderableType() == QSurfaceFormat::OpenGLES ? "ES" : "")
            .arg(fmt.majorVersion()).arg(fmt.minorVersion());
    QString qtglsurface = QString("RGBA: %1%2%3%4 Depth: %5 Stencil: %6")
            .arg(fmt.redBufferSize()).arg(fmt.greenBufferSize())
            .arg(fmt.greenBufferSize()).arg(fmt.alphaBufferSize())
            .arg(fmt.depthBufferSize()).arg(fmt.stencilBufferSize());
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL vendor        : %1").arg(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL renderer      : %1").arg(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL version       : %1").arg(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt platform          : %1").arg(QGuiApplication::platformName()));    
#ifdef USING_EGL
    bool eglfuncs = IsEGL();
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("EGL display          : %1").arg(GLYesNo(GetEGLDisplay() != nullptr)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("EGL images           : %1").arg(GLYesNo(eglfuncs)));
#endif
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL format     : %1").arg(qtglversion));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL surface    : %1").arg(qtglsurface));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture size     : %1").arg(m_maxTextureSize));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture units    : %1").arg(m_maxTextureUnits));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Shaders              : %1").arg(GLYesNo(m_features & Shaders)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("NPOT textures        : %1").arg(GLYesNo(m_features & NPOTTextures)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Multitexturing       : %1").arg(GLYesNo(m_features & Multitexture)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Rectangular textures : %1").arg(GLYesNo(m_extraFeatures & kGLExtRects)));
    //LOG(VB_GENERAL, LOG_INFO, LOC + QString("RGBA16 textures      : %1").arg(GLYesNo(m_extraFeatures & kGLExtRGBA16)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Buffer mapping       : %1").arg(GLYesNo(m_extraFeatures & kGLBufferMap)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Framebuffer objects  : %1").arg(GLYesNo(m_features & Framebuffers)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Unpack Subimage      : %1").arg(GLYesNo(m_extraFeatures & kGLExtSubimage)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("GL_RED/GL_R8         : %1").arg(GLYesNo(!(m_extraFeatures & kGLLegacyTextures))));
    //LOG(VB_GENERAL, LOG_INFO, LOC + QString("Compute shaders      : %1").arg(GLYesNo(QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Compute))));

    // warnings
    if (m_maxTextureUnits < 3)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Warning: Insufficient texture units for some features.");
}

int MythRenderOpenGL::GetColorDepth(void) const
{
    return m_colorDepth;
}

int MythRenderOpenGL::GetMaxTextureSize(void) const
{
    return m_maxTextureSize;
}

int MythRenderOpenGL::GetMaxTextureUnits(void) const
{
    return m_maxTextureUnits;
}

int MythRenderOpenGL::GetExtraFeatures(void) const
{
    return m_extraFeaturesUsed;
}

QOpenGLFunctions::OpenGLFeatures MythRenderOpenGL::GetFeatures(void) const
{
    return m_features;
}

bool MythRenderOpenGL::IsRecommendedRenderer(void)
{
    bool recommended = true;
    OpenGLLocker locker(this);
    QString renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    if (!(openGLFeatures() & Shaders))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL has no shader support");
        recommended = false;
    }
    else if (!(openGLFeatures() & Framebuffers))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL has no framebuffer support");
        recommended = false;
    }
    else if (renderer.contains("Software Rasterizer", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL is using software rasterizer.");
        recommended = false;
    }
    else if (renderer.contains("softpipe", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL seems to be using software "
            "fallback. Please check your OpenGL driver installation, "
            "configuration, and device permissions.");
        recommended = false;
    }
    return recommended;
}

void MythRenderOpenGL::swapBuffers()
{
    QOpenGLContext::swapBuffers(m_window);
}

void MythRenderOpenGL::setWidget(QWidget *Widget)
{
    if (!Widget)
        return;

    m_window = Widget->windowHandle();
    if (!m_window)
    {
        Widget = Widget->nativeParentWidget();
        if (Widget)
            m_window = Widget->windowHandle();
    }

#ifdef ANDROID
    // change all window surfacetypes to OpenGLSurface
    // otherwise the raster gets painted on top of the GL surface
    m_window->setSurfaceType(QWindow::OpenGLSurface);
    QWidget* wNativeParent = Widget->nativeParentWidget();
    if (wNativeParent)
        wNativeParent->windowHandle()->setSurfaceType(QWindow::OpenGLSurface);
#endif

    if (!create())
        LOG(VB_GENERAL, LOG_WARNING, LOC + "setWidget create failed");
    else if (Widget)
        Widget->setAttribute(Qt::WA_PaintOnScreen);
}

void MythRenderOpenGL::makeCurrent()
{
    m_lock.lock();
    if (!m_lockLevel++)
        if (!QOpenGLContext::makeCurrent(m_window))
            LOG(VB_GENERAL, LOG_ERR, LOC + "makeCurrent failed");
}

void MythRenderOpenGL::doneCurrent()
{
    // TODO add back QOpenGLContext::doneCurrent call
    // once calls are better pipelined
    m_lockLevel--;
    if (m_lockLevel < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mis-matched calls to makeCurrent()");
    m_lock.unlock();
}

void MythRenderOpenGL::SetViewPort(const QRect &rect, bool viewportonly)
{
    if (rect == m_viewport)
        return;
    makeCurrent();
    m_viewport = rect;
    glViewport(m_viewport.left(), m_viewport.top(),
               m_viewport.width(), m_viewport.height());
    if (!viewportonly)
        SetMatrixView();
    doneCurrent();
}

void MythRenderOpenGL::Flush(void)
{
    if (!m_flushEnabled)
        return;

    makeCurrent();
    glFlush();
    doneCurrent();
}

void MythRenderOpenGL::SetBlend(bool enable)
{
    makeCurrent();
    if (enable && !m_blend)
        glEnable(GL_BLEND);
    else if (!enable && m_blend)
        glDisable(GL_BLEND);
    m_blend = enable;
    doneCurrent();
}

void MythRenderOpenGL::SetBackground(int r, int g, int b, int a)
{
    int32_t tmp = (r << 24) + (g << 16) + (b << 8) + a;
    if (tmp == m_background)
        return;

    m_background = tmp;
    makeCurrent();
    glClearColor(r / 255.0F, g / 255.0F, b / 255.0F, a / 255.0F);
    doneCurrent();
}

MythGLTexture* MythRenderOpenGL::CreateTextureFromQImage(QImage *Image)
{
    if (!Image)
        return nullptr;

    OpenGLLocker locker(this);
    auto *texture = new QOpenGLTexture(*Image, QOpenGLTexture::DontGenerateMipMaps);
    if (!texture->textureId())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to create texure");
        delete texture;
        return nullptr;
    }
    texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    auto *result = new MythGLTexture(texture);
    result->m_texture     = texture;
    result->m_vbo         = CreateVBO(kVertexSize);
    result->m_totalSize   = GetTextureSize(Image->size(), result->m_target != QOpenGLTexture::TargetRectangle);
    // N.B. Format and type per qopengltexure.cpp
    result->m_pixelFormat = QOpenGLTexture::RGBA;
    result->m_pixelType   = QOpenGLTexture::UInt8;
    result->m_bufferSize  = GetBufferSize(result->m_totalSize, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    result->m_size        = Image->size();
    result->m_crop        = true;
    return result;
}

QSize MythRenderOpenGL::GetTextureSize(const QSize &size, bool Normalised)
{
    if ((m_features & NPOTTextures) || !Normalised)
        return size;

    int w = 64;
    int h = 64;
    while (w < size.width())
        w *= 2;
    while (h < size.height())
        h *= 2;
    return {w, h};
}

int MythRenderOpenGL::GetTextureDataSize(MythGLTexture *Texture)
{
    if (Texture)
        return Texture->m_bufferSize;
    return 0;
}

void MythRenderOpenGL::SetTextureFilters(MythGLTexture *Texture, QOpenGLTexture::Filter Filter, QOpenGLTexture::WrapMode Wrap)
{
    if (!Texture || (Texture && !(Texture->m_texture || Texture->m_textureId)))
        return;

    makeCurrent();
    if (Texture->m_texture)
    {
        Texture->m_texture->bind();
        Texture->m_texture->setWrapMode(Wrap);
        Texture->m_texture->setMinMagFilters(Filter, Filter);
    }
    else
    {
        glBindTexture(Texture->m_target, Texture->m_textureId);
        glTexParameteri(Texture->m_target, GL_TEXTURE_MIN_FILTER, Filter);
        glTexParameteri(Texture->m_target, GL_TEXTURE_MAG_FILTER, Filter);
        glTexParameteri(Texture->m_target, GL_TEXTURE_WRAP_S,     Wrap);
        glTexParameteri(Texture->m_target, GL_TEXTURE_WRAP_T,     Wrap);
    }
    doneCurrent();
}

void MythRenderOpenGL::ActiveTexture(GLuint ActiveTex)
{
    if (!(m_features & Multitexture))
        return;

    makeCurrent();
    if (m_activeTexture != ActiveTex)
    {
        glActiveTexture(ActiveTex);
        m_activeTexture = ActiveTex;
    }
    doneCurrent();
}

void MythRenderOpenGL::DeleteTexture(MythGLTexture *Texture)
{
    if (!Texture)
        return;

    makeCurrent();
    // N.B. Don't delete m_textureId - it is owned externally
    delete Texture->m_texture;
    delete [] Texture->m_data;
    delete Texture->m_vbo;
    delete Texture;
    Flush();
    doneCurrent();
}

QOpenGLFramebufferObject* MythRenderOpenGL::CreateFramebuffer(QSize &Size, GLenum InternalFormat)
{
    if (!(m_features & Framebuffers))
        return nullptr;

    OpenGLLocker locker(this);
    QOpenGLFramebufferObject *framebuffer;
    if (InternalFormat)
        framebuffer = new QOpenGLFramebufferObject(Size, QOpenGLFramebufferObject::NoAttachment,
                                                   GL_TEXTURE_2D, InternalFormat);
    else
        framebuffer = new QOpenGLFramebufferObject(Size);
    if (framebuffer->isValid())
    {
        if (framebuffer->isBound())
        {
            m_activeFramebuffer = framebuffer;
            BindFramebuffer(nullptr);
        }
        Flush();
        return framebuffer;
    }
    LOG(VB_GENERAL, LOG_ERR, "Failed to create framebuffer object");
    delete framebuffer;
    return nullptr;
}

MythGLTexture* MythRenderOpenGL::CreateFramebufferTexture(QOpenGLFramebufferObject *Framebuffer)
{
    if (!Framebuffer)
        return nullptr;

    auto *texture = new MythGLTexture(Framebuffer->texture());
    texture->m_size        = texture->m_totalSize = Framebuffer->size();
    texture->m_vbo         = CreateVBO(kVertexSize);
    texture->m_flip        = false;
    return texture;
}

void MythRenderOpenGL::DeleteFramebuffer(QOpenGLFramebufferObject *Framebuffer)
{
    if (Framebuffer)
    {
        makeCurrent();
        delete Framebuffer;
        doneCurrent();
    }
}

void MythRenderOpenGL::BindFramebuffer(QOpenGLFramebufferObject *Framebuffer)
{
    if (Framebuffer == m_activeFramebuffer)
        return;

    makeCurrent();
    if (Framebuffer == nullptr)
        QOpenGLFramebufferObject::bindDefault();
    else
        Framebuffer->bind();
    doneCurrent();
    m_activeFramebuffer = Framebuffer;
}

void MythRenderOpenGL::ClearFramebuffer(void)
{
    makeCurrent();
    glClear(GL_COLOR_BUFFER_BIT);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(MythGLTexture *Texture, QOpenGLFramebufferObject *Target,
                                  const QRect &Source, const QRect &Destination,
                                  QOpenGLShaderProgram *Program, int Alpha)
{
    makeCurrent();

    if (!Texture || (Texture && !((Texture->m_texture || Texture->m_textureId) && Texture->m_vbo)))
        return;

    if (Program == nullptr)
        Program = m_defaultPrograms[kShaderDefault];

    BindFramebuffer(Target);
    SetShaderProjection(Program);

    GLenum textarget = Texture->m_target;
    Program->setUniformValue("s_texture0", 0);
    ActiveTexture(GL_TEXTURE0);
    if (Texture->m_texture)
        Texture->m_texture->bind();
    else
        glBindTexture(textarget, Texture->m_textureId);

    QOpenGLBuffer* buffer = Texture->m_vbo;
    buffer->bind();
    if (UpdateTextureVertices(Texture, Source, Destination, 0))
    {
        if (m_extraFeaturesUsed & kGLBufferMap)
        {
            void* target = buffer->map(QOpenGLBuffer::WriteOnly);
            if (target)
                memcpy(target, Texture->m_vertexData, kVertexSize);
            buffer->unmap();
        }
        else
        {
            buffer->write(0, Texture->m_vertexData, kVertexSize);
        }
    }

    glEnableVertexAttribArray(VERTEX_INDEX);
    glEnableVertexAttribArray(TEXTURE_INDEX);
    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
    glVertexAttrib4f(COLOR_INDEX, 1.0F, 1.0F, 1.0F, Alpha / 255.0F);
    glVertexAttribPointerI(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE, TEXTURE_SIZE * sizeof(GLfloat), kTextureOffset);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(TEXTURE_INDEX);
    glDisableVertexAttribArray(VERTEX_INDEX);
    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(MythGLTexture **Textures, uint TextureCount,
                                  QOpenGLFramebufferObject *Target,
                                  const QRect &Source, const QRect &Destination,
                                  QOpenGLShaderProgram *Program,
                                  int Rotation)
{
    if (!Textures || !TextureCount)
        return;

    makeCurrent();
    BindFramebuffer(Target);

    if (Program == nullptr)
        Program = m_defaultPrograms[kShaderDefault];

    MythGLTexture* first = Textures[0];
    if (!first || (first && !((first->m_texture || first->m_textureId) && first->m_vbo)))
        return;

    SetShaderProjection(Program);

    GLenum textarget = first->m_target;
    uint active_tex = 0;
    for (uint i = 0; i < TextureCount; i++)
    {
        QString uniform = QString("s_texture%1").arg(active_tex);
        Program->setUniformValue(qPrintable(uniform), active_tex);
        ActiveTexture(GL_TEXTURE0 + active_tex++);
        if (Textures[i]->m_texture)
            Textures[i]->m_texture->bind();
        else
            glBindTexture(textarget, Textures[i]->m_textureId);
    }

    QOpenGLBuffer* buffer = first->m_vbo;
    buffer->bind();
    if (UpdateTextureVertices(first, Source, Destination, Rotation))
    {
        if (m_extraFeaturesUsed & kGLBufferMap)
        {
            void* target = buffer->map(QOpenGLBuffer::WriteOnly);
            if (target)
                memcpy(target, first->m_vertexData, kVertexSize);
            buffer->unmap();
        }
        else
        {
            buffer->write(0, first->m_vertexData, kVertexSize);
        }
    }

    glEnableVertexAttribArray(VERTEX_INDEX);
    glEnableVertexAttribArray(TEXTURE_INDEX);
    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
    glVertexAttrib4f(COLOR_INDEX, 1.0, 1.0, 1.0, 1.0);
    glVertexAttribPointerI(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE, TEXTURE_SIZE * sizeof(GLfloat), kTextureOffset);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(TEXTURE_INDEX);
    glDisableVertexAttribArray(VERTEX_INDEX);
    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    doneCurrent();
}

static const float kLimitedRangeOffset = (16.0F / 255.0F);
static const float kLimitedRangeScale  = (219.0F / 255.0F);

/// \brief An optimised method to clear a QRect to the given color
void MythRenderOpenGL::ClearRect(QOpenGLFramebufferObject *Target, const QRect &Area, int Color)
{
    makeCurrent();
    BindFramebuffer(Target);
    glEnableVertexAttribArray(VERTEX_INDEX);

    // Set the fill color
    float color = m_fullRange ? Color / 255.0F : (Color * kLimitedRangeScale) + kLimitedRangeOffset;
    glVertexAttrib4f(COLOR_INDEX, color, color, color, 255.0F);
    SetShaderProjection(m_defaultPrograms[kShaderSimple]);

    GetCachedVBO(GL_TRIANGLE_STRIP, Area);
    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    glDisableVertexAttribArray(VERTEX_INDEX);
    doneCurrent();
}

void MythRenderOpenGL::DrawRect(QOpenGLFramebufferObject *Target,
                                const QRect &Area, const QBrush &FillBrush,
                                const QPen &LinePen, int Alpha)
{
    DrawRoundRect(Target, Area, 1, FillBrush, LinePen, Alpha);
}

void MythRenderOpenGL::DrawRoundRect(QOpenGLFramebufferObject *Target,
                                     const QRect &Area, int CornerRadius,
                                     const QBrush &FillBrush,
                                     const QPen &LinePen, int Alpha)
{
    makeCurrent();
    BindFramebuffer(Target);

    int lineWidth = LinePen.width();
    int halfline  = lineWidth / 2;
    int rad = CornerRadius - halfline;

    if ((Area.width() / 2) < rad)
        rad = Area.width() / 2;

    if ((Area.height() / 2) < rad)
        rad = Area.height() / 2;
    int dia = rad * 2;

    QRect r(Area.left(), Area.top(), Area.width(), Area.height());

    QRect tl(r.left(),  r.top(), rad, rad);
    QRect tr(r.left() + r.width() - rad, r.top(), rad, rad);
    QRect bl(r.left(),  r.top() + r.height() - rad, rad, rad);
    QRect br(r.left() + r.width() - rad, r.top() + r.height() - rad, rad, rad);

    glEnableVertexAttribArray(VERTEX_INDEX);

    if (FillBrush.style() != Qt::NoBrush)
    {
        // Get the shaders
        QOpenGLShaderProgram* elip = m_defaultPrograms[kShaderCircle];
        QOpenGLShaderProgram* fill = m_defaultPrograms[kShaderSimple];

        // Set the fill color
        if (m_fullRange)
        {
            glVertexAttrib4f(COLOR_INDEX,
                               FillBrush.color().red() / 255.0F,
                               FillBrush.color().green() / 255.0F,
                               FillBrush.color().blue() / 255.0F,
                              (FillBrush.color().alpha() / 255.0F) * (Alpha / 255.0F));
        }
        else
        {
            glVertexAttrib4f(COLOR_INDEX,
                              (FillBrush.color().red() * kLimitedRangeScale) + kLimitedRangeOffset,
                              (FillBrush.color().blue() * kLimitedRangeScale) + kLimitedRangeOffset,
                              (FillBrush.color().green() * kLimitedRangeScale) + kLimitedRangeOffset,
                              (FillBrush.color().alpha() / 255.0F) * (Alpha / 255.0F));
        }

        // Set the radius
        m_parameters(2,0) = rad;
        m_parameters(3,0) = rad - 1.0F;

        // Enable the Circle shader
        SetShaderProjection(elip);

        // Draw the top left segment
        m_parameters(0,0) = tl.left() + rad;
        m_parameters(1,0) = tl.top() + rad;

        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the top right segment
        m_parameters(0,0) = tr.left();
        m_parameters(1,0) = tr.top() + rad;
        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tr);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom left segment
        m_parameters(0,0) = bl.left() + rad;
        m_parameters(1,0) = bl.top();
        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, bl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom right segment
        m_parameters(0,0) = br.left();
        m_parameters(1,0) = br.top();
        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, br);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Fill the remaining areas
        QRect main(r.left() + rad, r.top(), r.width() - dia, r.height());
        QRect left(r.left(), r.top() + rad, rad, r.height() - dia);
        QRect right(r.left() + r.width() - rad, r.top() + rad, rad, r.height() - dia);

        SetShaderProjection(fill);

        GetCachedVBO(GL_TRIANGLE_STRIP, main);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GetCachedVBO(GL_TRIANGLE_STRIP, left);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GetCachedVBO(GL_TRIANGLE_STRIP, right);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    }

    if (LinePen.style() != Qt::NoPen)
    {
        // Get the shaders
        QOpenGLShaderProgram* edge  = m_defaultPrograms[kShaderCircleEdge];
        QOpenGLShaderProgram* vline = m_defaultPrograms[kShaderVertLine];
        QOpenGLShaderProgram* hline = m_defaultPrograms[kShaderHorizLine];

        // Set the line color
        if (m_fullRange)
        {
            glVertexAttrib4f(COLOR_INDEX,
                               LinePen.color().red() / 255.0F,
                               LinePen.color().green() / 255.0F,
                               LinePen.color().blue() / 255.0F,
                              (LinePen.color().alpha() / 255.0F) * (Alpha / 255.0F));
        }
        else
        {
            glVertexAttrib4f(COLOR_INDEX,
                              (LinePen.color().red() * kLimitedRangeScale) + kLimitedRangeOffset,
                              (LinePen.color().blue() * kLimitedRangeScale) + kLimitedRangeOffset,
                              (LinePen.color().green() * kLimitedRangeScale) + kLimitedRangeOffset,
                              (FillBrush.color().alpha() / 255.0F) * (Alpha / 255.0F));
        }

        // Set the radius and width
        m_parameters(2,0) = rad - lineWidth / 2.0F;
        m_parameters(3,0) = lineWidth / 2.0F;

        // Enable the edge shader
        SetShaderProjection(edge);

        // Draw the top left edge segment
        m_parameters(0,0) = tl.left() + rad;
        m_parameters(1,0) = tl.top() + rad;
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the top right edge segment
        m_parameters(0,0) = tr.left();
        m_parameters(1,0) = tr.top() + rad;
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tr);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat),kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom left edge segment
        m_parameters(0,0) = bl.left() + rad;
        m_parameters(1,0) = bl.top();
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, bl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom right edge segment
        m_parameters(0,0) = br.left();
        m_parameters(1,0) = br.top();
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, br);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Vertical lines
        SetShaderProjection(vline);

        m_parameters(1,0) = lineWidth / 2.0F;
        QRect vl(r.left(), r.top() + rad, lineWidth, r.height() - dia);

        // Draw the left line segment
        m_parameters(0,0) = vl.left() + lineWidth;
        SetShaderProgramParams(vline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, vl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the right line segment
        vl.translate(r.width() - lineWidth, 0);
        m_parameters(0,0) = vl.left();
        SetShaderProgramParams(vline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, vl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Horizontal lines
        SetShaderProjection(hline);
        QRect hl(r.left() + rad, r.top(), r.width() - dia, lineWidth);

        // Draw the top line segment
        m_parameters(0,0) = hl.top() + lineWidth;
        SetShaderProgramParams(hline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, hl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom line segment
        hl.translate(0, r.height() - lineWidth);
        m_parameters(0,0) = hl.top();
        SetShaderProgramParams(hline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, hl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    }
    glDisableVertexAttribArray(VERTEX_INDEX);
    doneCurrent();
}

inline void MythRenderOpenGL::glVertexAttribPointerI(GLuint Index, GLint Size, GLenum Type, GLboolean Normalize,
                                                     GLsizei Stride, const GLuint Value)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    glVertexAttribPointer(Index, Size, Type, Normalize, Stride, reinterpret_cast<const char *>(Value));
#pragma GCC diagnostic pop
}

void MythRenderOpenGL::Init2DState(void)
{
    SetBlend(true);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    Flush();
}

QFunctionPointer MythRenderOpenGL::GetProcAddress(const QString &Proc) const
{
    static const QString kExts[4] = { "", "ARB", "EXT", "OES" };
    QFunctionPointer result = nullptr;
    for (int i = 0; i < 4; i++)
    {
        result = getProcAddress((Proc + kExts[i]).toLocal8Bit().constData());
        if (result)
            break;
    }
    if (result == nullptr)
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Extension not found: %1").arg(Proc));
    return result;
}

QOpenGLBuffer* MythRenderOpenGL::CreateVBO(int Size, bool Release /*=true*/)
{
    OpenGLLocker locker(this);
    auto* buffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (buffer->create())
    {
        buffer->setUsagePattern(QOpenGLBuffer::StreamDraw);
        buffer->bind();
        buffer->allocate(Size);
        if (Release)
            QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
        return buffer;
    }
    delete buffer;
    return nullptr;
}

void MythRenderOpenGL::ReleaseResources(void)
{
    OpenGLLocker locker(this);
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        logDebugMarker("RENDER_RELEASE_START");
    DeleteDefaultShaders();
    ExpireVertices();
    ExpireVBOS();
    if (m_coreProfile && m_vao)
    {
        QOpenGLExtraFunctions extra(nullptr);
        extra.initializeOpenGLFunctions();
        extra.glDeleteVertexArrays(1, &m_vao);
    }
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        logDebugMarker("RENDER_RELEASE_END");
    delete m_openglDebugger;
    m_openglDebugger = nullptr;
    Flush();

    if (!m_cachedVertices.empty())
        LOG(VB_GENERAL, LOG_ERR, LOC + QString(" %1 unexpired vertices").arg(m_cachedVertices.size()));

    if (!m_cachedVBOS.empty())
        LOG(VB_GENERAL, LOG_ERR, LOC + QString(" %1 unexpired VBOs").arg(m_cachedVertices.size()));
}

bool MythRenderOpenGL::UpdateTextureVertices(MythGLTexture *Texture, const QRect &Source,
                                             const QRect &Destination, int Rotation)
{
    if (!Texture || (Texture && Texture->m_size.isEmpty()))
        return false;

    if ((Texture->m_source == Source) && (Texture->m_destination == Destination) &&
        (Texture->m_rotation == Rotation))
        return false;

    Texture->m_source      = Source;
    Texture->m_destination = Destination;
    Texture->m_rotation    = Rotation;

    GLfloat *data = Texture->m_vertexData;
    QSize    size = Texture->m_size;

    int width  = Texture->m_crop ? min(Source.width(),  size.width())  : Source.width();
    int height = Texture->m_crop ? min(Source.height(), size.height()) : Source.height();

    if (Texture->m_target != QOpenGLTexture::TargetRectangle)
    {
        data[0 + TEX_OFFSET] = Source.left() / static_cast<GLfloat>(size.width());
        data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET] = (Source.top() + height) / static_cast<GLfloat>(size.height());
        data[6 + TEX_OFFSET] = (Source.left() + width) / static_cast<GLfloat>(size.width());
        data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET] = Source.top() / static_cast<GLfloat>(size.height());
    }
    else
    {
        data[0 + TEX_OFFSET] = Source.left();
        data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET] = (Source.top() + height);
        data[6 + TEX_OFFSET] = (Source.left() + width);
        data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET] = Source.top();
    }

    data[2 + TEX_OFFSET] = data[0 + TEX_OFFSET];
    data[3 + TEX_OFFSET] = data[7 + TEX_OFFSET];
    data[4 + TEX_OFFSET] = data[6 + TEX_OFFSET];
    data[5 + TEX_OFFSET] = data[1 + TEX_OFFSET];

    width  = Texture->m_crop ? min(width, Destination.width())   : Destination.width();
    height = Texture->m_crop ? min(height, Destination.height()) : Destination.height();

    data[2] = data[0] = Destination.left();
    data[5] = data[1] = Destination.top();
    data[4] = data[6] = Destination.left() + width;
    data[3] = data[7] = Destination.top()  + height;

    if (Texture->m_rotation != 0)
    {
        GLfloat temp;
        if (Texture->m_rotation == 90)
        {
            temp = data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET];
            data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET] = data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET];
            data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET] = temp;
            data[2 + TEX_OFFSET] = data[6 + TEX_OFFSET];
            data[4 + TEX_OFFSET] = data[0 + TEX_OFFSET];
        }
        else if (Texture->m_rotation == -90)
        {
            temp = data[0 + TEX_OFFSET];
            data[0 + TEX_OFFSET] = data[6 + TEX_OFFSET];
            data[6 + TEX_OFFSET] = temp;
            data[3 + TEX_OFFSET] = data[1 + TEX_OFFSET];
            data[5 + TEX_OFFSET] = data[7 + TEX_OFFSET];
        }
        else if (abs(Texture->m_rotation) == 180)
        {
            temp = data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET];
            data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET] = data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET];
            data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET] = temp;
            data[3 + TEX_OFFSET] = data[7 + TEX_OFFSET];
            data[5 + TEX_OFFSET] = data[1 + TEX_OFFSET];
            temp = data[0 + TEX_OFFSET];
            data[0 + TEX_OFFSET] = data[6 + TEX_OFFSET];
            data[6 + TEX_OFFSET] = temp;
            data[2 + TEX_OFFSET] = data[0 + TEX_OFFSET];
            data[4 + TEX_OFFSET] = data[6 + TEX_OFFSET];
        }
    }

    return true;
}

GLfloat* MythRenderOpenGL::GetCachedVertices(GLuint Type, const QRect &Area)
{
    uint64_t ref = (static_cast<uint64_t>(Area.left()) & 0xfff) +
                  ((static_cast<uint64_t>(Area.top()) & 0xfff) << 12) +
                  ((static_cast<uint64_t>(Area.width()) & 0xfff) << 24) +
                  ((static_cast<uint64_t>(Area.height()) & 0xfff) << 36) +
                  ((static_cast<uint64_t>(Type & 0xfff)) << 48);

    if (m_cachedVertices.contains(ref))
    {
        m_vertexExpiry.removeOne(ref);
        m_vertexExpiry.append(ref);
        return m_cachedVertices[ref];
    }

    auto *vertices = new GLfloat[8];

    vertices[2] = vertices[0] = Area.left();
    vertices[5] = vertices[1] = Area.top();
    vertices[4] = vertices[6] = Area.left() + Area.width();
    vertices[3] = vertices[7] = Area.top() + Area.height();

    if (Type == GL_LINE_LOOP)
    {
        vertices[7] = vertices[1];
        vertices[5] = vertices[3];
    }

    m_cachedVertices.insert(ref, vertices);
    m_vertexExpiry.append(ref);
    ExpireVertices(MAX_VERTEX_CACHE);

    return vertices;
}

void MythRenderOpenGL::ExpireVertices(int Max)
{
    while (m_vertexExpiry.size() > Max)
    {
        uint64_t ref = m_vertexExpiry.first();
        m_vertexExpiry.removeFirst();
        GLfloat *vertices = nullptr;
        if (m_cachedVertices.contains(ref))
            vertices = m_cachedVertices.value(ref);
        m_cachedVertices.remove(ref);
        delete [] vertices;
    }
}

void MythRenderOpenGL::GetCachedVBO(GLuint Type, const QRect &Area)
{
    uint64_t ref = (static_cast<uint64_t>(Area.left()) & 0xfff) +
                  ((static_cast<uint64_t>(Area.top()) & 0xfff) << 12) +
                  ((static_cast<uint64_t>(Area.width()) & 0xfff) << 24) +
                  ((static_cast<uint64_t>(Area.height()) & 0xfff) << 36) +
                  ((static_cast<uint64_t>(Type & 0xfff)) << 48);

    if (m_cachedVBOS.contains(ref))
    {
        m_vboExpiry.removeOne(ref);
        m_vboExpiry.append(ref);
        m_cachedVBOS.value(ref)->bind();
        return;
    }

    GLfloat *vertices = GetCachedVertices(Type, Area);
    QOpenGLBuffer *vbo = CreateVBO(kTextureOffset, false);
    m_cachedVBOS.insert(ref, vbo);
    m_vboExpiry.append(ref);

    if (m_extraFeaturesUsed & kGLBufferMap)
    {
        void* target = vbo->map(QOpenGLBuffer::WriteOnly);
        if (target)
            memcpy(target, vertices, kTextureOffset);
        vbo->unmap();
    }
    else
    {
        vbo->write(0, vertices, kTextureOffset);
    }
    ExpireVBOS(MAX_VERTEX_CACHE);
}

void MythRenderOpenGL::ExpireVBOS(int Max)
{
    while (m_vboExpiry.size() > Max)
    {
        uint64_t ref = m_vboExpiry.first();
        m_vboExpiry.removeFirst();
        if (m_cachedVBOS.contains(ref))
        {
            QOpenGLBuffer *vbo = m_cachedVBOS.value(ref);
            delete vbo;
            m_cachedVBOS.remove(ref);
        }
    }
}

int MythRenderOpenGL::GetBufferSize(QSize Size, QOpenGLTexture::PixelFormat Format, QOpenGLTexture::PixelType Type)
{
    int bytes = 0;
    int bpp = 0;;

    switch (Format)
    {
        case QOpenGLTexture::BGRA:
        case QOpenGLTexture::RGBA: bpp = 4; break;
        case QOpenGLTexture::BGR:
        case QOpenGLTexture::RGB:  bpp = 3; break;
        case QOpenGLTexture::RG:   bpp = 2; break;
        case QOpenGLTexture::Red:
        case QOpenGLTexture::Alpha:
        case QOpenGLTexture::Luminance: bpp = 1; break;
        default: break; // unsupported
    }

    switch (Type)
    {
        case QOpenGLTexture::Int8:    bytes = sizeof(GLbyte); break;
        case QOpenGLTexture::UInt8:   bytes = sizeof(GLubyte); break;
        case QOpenGLTexture::Int16:   bytes = sizeof(GLshort); break;
        case QOpenGLTexture::UInt16:  bytes = sizeof(GLushort); break;
        case QOpenGLTexture::Int32:   bytes = sizeof(GLint); break;
        case QOpenGLTexture::UInt32:  bytes = sizeof(GLuint); break;
        case QOpenGLTexture::Float32: bytes = sizeof(GLfloat); break;
        case QOpenGLTexture::UInt32_RGB10A2: bytes = sizeof(GLuint); break;
        default: break; // unsupported
    }

    if (!bpp || !bytes || Size.isEmpty())
        return 0;

    return Size.width() * Size.height() * bpp * bytes;
}

void MythRenderOpenGL::PushTransformation(const UIEffects &fx, QPointF &center)
{
    QMatrix4x4 newtop = m_transforms.top();
    if (fx.m_hzoom != 1.0F || fx.m_vzoom != 1.0F || fx.m_angle != 0.0F)
    {
        newtop.translate(static_cast<GLfloat>(center.x()), static_cast<GLfloat>(center.y()));
        newtop.scale(fx.m_hzoom, fx.m_vzoom);
        newtop.rotate(fx.m_angle, 0, 0, 1);
        newtop.translate(static_cast<GLfloat>(-center.x()), static_cast<GLfloat>(-center.y()));
    }
    m_transforms.push(newtop);
}

void MythRenderOpenGL::PopTransformation(void)
{
    m_transforms.pop();
}

inline QOpenGLShaderProgram* ShaderError(QOpenGLShaderProgram *Shader, const QString &Source)
{
    QString type = Source.isEmpty() ? "Shader link" : "Shader compile";
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("%1 error").arg(type));
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Log:"));
    LOG(VB_GENERAL, LOG_ERR, "\n" + Shader->log());
    if (!Source.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Source:"));
        LOG(VB_GENERAL, LOG_ERR, "\n" + Source);
    }
    delete Shader;
    return nullptr;
}

QOpenGLShaderProgram *MythRenderOpenGL::CreateShaderProgram(const QString &Vertex, const QString &Fragment)
{
    if (!(m_features & Shaders))
        return nullptr;

    OpenGLLocker locker(this);
    QString vertex   = Vertex.isEmpty()   ? kDefaultVertexShader  : Vertex;
    QString fragment = Fragment.isEmpty() ? kDefaultFragmentShader: Fragment;
    auto *program = new QOpenGLShaderProgram();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex))
        return ShaderError(program, vertex);
    if (!program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment))
        return ShaderError(program, fragment);
    if (VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_DEBUG))
    {
        QList<QOpenGLShader*> shaders = program->shaders();
        foreach (QOpenGLShader* shader, shaders)
            LOG(VB_GENERAL, LOG_DEBUG, "\n" + shader->sourceCode());
    }
    program->bindAttributeLocation("a_position",  VERTEX_INDEX);
    program->bindAttributeLocation("a_color",     COLOR_INDEX);
    program->bindAttributeLocation("a_texcoord0", TEXTURE_INDEX);
    if (!program->link())
        return ShaderError(program, "");
    return program;
}

void MythRenderOpenGL::DeleteShaderProgram(QOpenGLShaderProgram *Program)
{
    makeCurrent();
    delete Program;
    m_cachedMatrixUniforms.clear();
    m_activeProgram = nullptr;
    m_cachedUniformLocations.remove(Program);
    doneCurrent();
}

bool MythRenderOpenGL::EnableShaderProgram(QOpenGLShaderProgram* Program)
{
    if (!Program)
        return false;

    if (m_activeProgram == Program)
        return true;

    makeCurrent();
    Program->bind();
    m_activeProgram = Program;
    doneCurrent();
    return true;
}

void MythRenderOpenGL::SetShaderProjection(QOpenGLShaderProgram *Program)
{
    if (Program)
    {
        SetShaderProgramParams(Program, m_projection,       "u_projection");
        SetShaderProgramParams(Program, m_transforms.top(), "u_transform");
    }
}

void MythRenderOpenGL::SetShaderProgramParams(QOpenGLShaderProgram *Program, const QMatrix4x4 &Value, const char *Uniform)
{
    OpenGLLocker locker(this);
    if (!Uniform || !EnableShaderProgram(Program))
        return;

    // Uniform value cacheing
    QString tag = QStringLiteral("%1-%2").arg(Program->programId()).arg(Uniform);
    QHash<QString,QMatrix4x4>::iterator it = m_cachedMatrixUniforms.find(tag);
    if (it == m_cachedMatrixUniforms.end())
        m_cachedMatrixUniforms.insert(tag, Value);
    else if (!qFuzzyCompare(Value, it.value()))
        it.value() = Value;
    else
        return;

    // Uniform location cacheing
    QByteArray uniform(Uniform);
    GLint location = 0;
    QHash<QByteArray, GLint> &uniforms = m_cachedUniformLocations[Program];
    if (uniforms.contains(uniform))
    {
        location = uniforms[uniform];
    }
    else
    {
        location = Program->uniformLocation(Uniform);
        uniforms.insert(uniform, location);
    }

    Program->setUniformValue(location, Value);
}

bool MythRenderOpenGL::CreateDefaultShaders(void)
{
    m_defaultPrograms[kShaderSimple]     = CreateShaderProgram(kSimpleVertexShader, kSimpleFragmentShader);
    m_defaultPrograms[kShaderDefault]    = CreateShaderProgram(kDefaultVertexShader, m_fullRange ? kDefaultFragmentShader : kDefaultFragmentShaderLimited);
    m_defaultPrograms[kShaderCircle]     = CreateShaderProgram(kDrawVertexShader, kCircleFragmentShader);
    m_defaultPrograms[kShaderCircleEdge] = CreateShaderProgram(kDrawVertexShader, kCircleEdgeFragmentShader);
    m_defaultPrograms[kShaderVertLine]   = CreateShaderProgram(kDrawVertexShader, kVertLineFragmentShader);
    m_defaultPrograms[kShaderHorizLine]  = CreateShaderProgram(kDrawVertexShader, kHorizLineFragmentShader);
    return m_defaultPrograms[kShaderSimple] && m_defaultPrograms[kShaderDefault] &&
           m_defaultPrograms[kShaderCircle] && m_defaultPrograms[kShaderCircleEdge] &&
           m_defaultPrograms[kShaderVertLine] &&m_defaultPrograms[kShaderHorizLine];
}

void MythRenderOpenGL::DeleteDefaultShaders(void)
{
    for (int i = 0; i < kShaderCount; i++)
    {
        DeleteShaderProgram(m_defaultPrograms[i]);
        m_defaultPrograms[i] = nullptr;
    }
}

void MythRenderOpenGL::SetMatrixView(void)
{
    m_projection.setToIdentity();
    m_projection.ortho(m_viewport);
}
