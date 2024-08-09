// Std
#include <algorithm>
#include <cmath>

// Qt
#include <QLibrary>
#include <QPainter>
#include <QWindow>
#include <QWidget>
#include <QGuiApplication>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "mythmainwindow.h"
#include "mythrenderopengl.h"
#include "mythrenderopenglshaders.h"
#include "mythuitype.h"
#ifdef USING_X11
#include "platforms/mythxdisplay.h"
#endif

#define LOC QString("OpenGL: ")

#ifdef Q_OS_ANDROID
#include <android/log.h>
#include <QWindow>
#endif

static constexpr GLuint VERTEX_INDEX  { 0 };
static constexpr GLuint COLOR_INDEX   { 1 };
static constexpr GLuint TEXTURE_INDEX { 2 };
static constexpr GLint  VERTEX_SIZE   { 2 };
static constexpr GLint  TEXTURE_SIZE  { 2 };

static constexpr GLuint kVertexOffset  { 0 };
static constexpr GLuint kTextureOffset { 8 * sizeof(GLfloat) };

static constexpr int MAX_VERTEX_CACHE { 500 };

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

MythRenderOpenGL* MythRenderOpenGL::Create(QWidget *Widget)
{
    if (!Widget)
        return nullptr;

#ifdef USING_X11
    if (MythXDisplay::DisplayIsRemote())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL is disabled for Remote X Session");
        return nullptr;
    }
#endif

    // N.B the core profiles below are designed to target compute shader availability
    bool opengles = !qEnvironmentVariableIsEmpty("MYTHTV_OPENGL_ES");
    bool core     = !qEnvironmentVariableIsEmpty("MYTHTV_OPENGL_CORE");
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

    return new MythRenderOpenGL(format, Widget);
}

MythRenderOpenGL::MythRenderOpenGL(const QSurfaceFormat& Format, QWidget *Widget)
  : MythEGL(this),
    MythRender(kRenderOpenGL),
    m_fullRange(gCoreContext->GetBoolSetting("GUIRGBLevels", true))
{
    m_projection.fill(0);
    m_parameters.fill(0);
    m_transforms.push(QMatrix4x4());
    setFormat(Format);
    connect(this, &QOpenGLContext::aboutToBeDestroyed, this, &MythRenderOpenGL::contextToBeDestroyed);
    SetWidget(Widget);
}

MythRenderOpenGL::~MythRenderOpenGL()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "MythRenderOpenGL closing");
    if (!isValid())
        return;
    disconnect(this, &QOpenGLContext::aboutToBeDestroyed, this, &MythRenderOpenGL::contextToBeDestroyed);
    if (m_ready)
        MythRenderOpenGL::ReleaseResources();
}

void MythRenderOpenGL::MessageLogged(const QOpenGLDebugMessage &Message)
{
    // filter unwanted messages
    if ((m_openGLDebuggerFilter & Message.type()) != 0U)
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
    LOG(VB_GPU, LOG_INFO, LOC + QString("Src: %1 Type: %2 Msg: %3")
        .arg(source, type, Message.message()));
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
    m_ready = true;
    m_features = openGLFeatures();

    // don't enable this by default - it can generate a lot of detail
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
    {
        m_openglDebugger = new QOpenGLDebugLogger();
        if (m_openglDebugger->initialize())
        {
            connect(m_openglDebugger, &QOpenGLDebugLogger::messageLogged, this, &MythRenderOpenGL::MessageLogged);
            QOpenGLDebugLogger::LoggingMode mode = QOpenGLDebugLogger::AsynchronousLogging;

            // this will impact performance but can be very useful
            if (!qEnvironmentVariableIsEmpty("MYTHTV_OPENGL_SYNCHRONOUS"))
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

    // check for core profile N.B. not OpenGL ES
    if (fmt.profile() == QSurfaceFormat::OpenGLContextProfile::CoreProfile)
    {
        // if we have a core profile then we need a VAO bound - this is just a
        // workaround for the time being
        extraFunctions()->glGenVertexArrays(1, &m_vao);
        extraFunctions()->glBindVertexArray(m_vao);
    }

    // For (embedded) GPUs that use tile based rendering, it is faster to use
    // glClear e.g. on the Pi3 it improves video frame rate significantly. Using
    // glClear tells the GPU it doesn't have to retrieve the old framebuffer and will
    // also clear existing draw calls.
    // For now this just includes Broadcom VideoCoreIV.
    // Other Tile Based Deferred Rendering GPUS - PowerVR5/6/7, Apple (PowerVR as well?)
    // Other Tile Based Immediate Mode Rendering GPUS - ARM Mali, Qualcomm Adreno
    static const std::array<const QByteArray,3> kTiled { "videocore", "vc4", "v3d" };
    auto renderer = QByteArray(reinterpret_cast<const char*>(glGetString(GL_RENDERER))).toLower();
    for (const auto & name : kTiled)
    {
        if (renderer.contains(name))
        {
            m_extraFeatures |= kGLTiled;
            break;
        }
    }

    // Check for memory extensions
    if (hasExtension("GL_NVX_gpu_memory_info"))
        m_extraFeatures |= kGLNVMemory;

    // Check 16 bit FBOs
    Check16BitFBO();

    // Check for compute and geometry shaders
    if (QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Compute))
        m_extraFeatures |= kGLComputeShaders;
    if (QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Geometry))
        m_extraFeatures |= kGLGeometryShaders;

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

static constexpr QLatin1String GLYesNo (bool v)
{
    return v ? QLatin1String("Yes") : QLatin1String("No");
}

void MythRenderOpenGL::DebugFeatures(void)
{
    QSurfaceFormat fmt = format();
    QString qtglversion = QString("OpenGL%1 %2.%3")
            .arg(fmt.renderableType() == QSurfaceFormat::OpenGLES ? "ES" : "")
            .arg(fmt.majorVersion()).arg(fmt.minorVersion());
    QString qtglsurface = QString("RGBA: %1:%2:%3:%4 Depth: %5 Stencil: %6")
            .arg(fmt.redBufferSize()).arg(fmt.greenBufferSize())
            .arg(fmt.blueBufferSize()).arg(fmt.alphaBufferSize())
            .arg(fmt.depthBufferSize()).arg(fmt.stencilBufferSize());
    QStringList shaders {"None"};
    if (m_features & Shaders)
    {
        shaders = QStringList { "Vertex", "Fragment" };
        if (m_extraFeatures & kGLGeometryShaders)
            shaders << "Geometry";
        if (m_extraFeatures & kGLComputeShaders)
            shaders << "Compute";
    }
    const QString module = QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL ? "OpenGL (not ES)" : "OpenGL ES";
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL vendor        : %1").arg(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL renderer      : %1").arg(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL version       : %1").arg(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt platform          : %1").arg(QGuiApplication::platformName()));
#ifdef USING_EGL
    bool eglfuncs = IsEGL();
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("EGL display          : %1").arg(GLYesNo(GetEGLDisplay() != nullptr)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("EGL images           : %1").arg(GLYesNo(eglfuncs)));
#endif
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL module     : %1").arg(module));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL format     : %1").arg(qtglversion));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL surface    : %1").arg(qtglsurface));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture size     : %1").arg(m_maxTextureSize));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Shaders              : %1").arg(shaders.join(",")));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("16bit framebuffers   : %1").arg(GLYesNo(m_extraFeatures & kGL16BitFBO)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Unpack Subimage      : %1").arg(GLYesNo(m_extraFeatures & kGLExtSubimage)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Buffer mapping       : %1").arg(GLYesNo(m_extraFeatures & kGLBufferMap)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Rectangular textures : %1").arg(GLYesNo(m_extraFeatures & kGLExtRects)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("NPOT textures        : %1").arg(GLYesNo(m_features & NPOTTextures)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Max texture units    : %1").arg(m_maxTextureUnits));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("GL_RED/GL_R8         : %1").arg(GLYesNo(!(m_extraFeatures & kGLLegacyTextures))));
    // warnings
    if (m_maxTextureUnits < 3)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Warning: Insufficient texture units for some features.");
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
    if (!IsReady())
        return false;

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

    if (!recommended)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "OpenGL not recommended with this system's hardware/drivers.");
    }

    return recommended;
}

bool MythRenderOpenGL::IsReady(void)
{
    return isValid() && m_ready;
}

void MythRenderOpenGL::swapBuffers()
{
    QOpenGLContext::swapBuffers(m_window);
    m_swapCount++;
}

uint64_t MythRenderOpenGL::GetSwapCount()
{
    auto result = m_swapCount;
    m_swapCount = 0;
    return result;
}

void MythRenderOpenGL::SetWidget(QWidget *Widget)
{
    if (!Widget)
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + "No widget!");
        return;
    }

    // We must have a window/surface.
    m_window = Widget->windowHandle();
    QWidget* native = Widget->nativeParentWidget();
    if (!m_window && native)
        m_window = native->windowHandle();

    if (!m_window)
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + "No window surface!");
        return;
    }

#ifdef Q_OS_ANDROID
    // Ensure surface type is always OpenGL
    m_window->setSurfaceType(QWindow::OpenGLSurface);
    if (native && native->windowHandle())
        native->windowHandle()->setSurfaceType(QWindow::OpenGLSurface);
#endif

#ifdef USING_QTWEBENGINE
    auto * globalcontext = QOpenGLContext::globalShareContext();
    if (globalcontext)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Using global shared OpenGL context");
        setShareContext(globalcontext);
    }
#endif

    if (!create())
        LOG(VB_GENERAL, LOG_CRIT, LOC + "Failed to create OpenGLContext!");
    else
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

void MythRenderOpenGL::SetViewPort(QRect Rect, bool ViewportOnly)
{
    if (Rect == m_viewport)
        return;
    makeCurrent();
    m_viewport = Rect;
    glViewport(m_viewport.left(), m_viewport.top(),
               m_viewport.width(), m_viewport.height());
    if (!ViewportOnly)
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

void MythRenderOpenGL::SetBlend(bool Enable)
{
    makeCurrent();
    if (Enable && !m_blend)
        glEnable(GL_BLEND);
    else if (!Enable && m_blend)
        glDisable(GL_BLEND);
    m_blend = Enable;
    doneCurrent();
}

void MythRenderOpenGL::SetBackground(uint8_t Red, uint8_t Green, uint8_t Blue, uint8_t Alpha)
{
    int32_t tmp = (Red << 24) + (Green << 16) + (Blue << 8) + Alpha;
    if (tmp == m_background)
        return;

    m_background = tmp;
    makeCurrent();
    glClearColor(Red / 255.0F, Green / 255.0F, Blue / 255.0F, Alpha / 255.0F);
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

QSize MythRenderOpenGL::GetTextureSize(const QSize Size, bool Normalised)
{
    if (((m_features & NPOTTextures) != 0U) || !Normalised)
        return Size;

    int w = 64;
    int h = 64;
    while (w < Size.width())
        w *= 2;
    while (h < Size.height())
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
    if (!Texture || !(Texture->m_texture || Texture->m_textureId))
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

QOpenGLFramebufferObject* MythRenderOpenGL::CreateFramebuffer(QSize &Size, bool SixteenBit)
{
    if (!(m_features & Framebuffers))
        return nullptr;

    OpenGLLocker locker(this);
    QOpenGLFramebufferObject *framebuffer = nullptr;
    if (SixteenBit)
    {
        framebuffer = new QOpenGLFramebufferObject(Size, QOpenGLFramebufferObject::NoAttachment,
                                                   GL_TEXTURE_2D, QOpenGLTexture::RGBA16_UNorm);
    }
    else
    {
        framebuffer = new QOpenGLFramebufferObject(Size);
    }
    if (framebuffer->isValid())
    {
        if (framebuffer->isBound())
        {
            m_activeFramebuffer = framebuffer->handle();
            BindFramebuffer(nullptr);
        }
        Flush();
        return framebuffer;
    }
    LOG(VB_GENERAL, LOG_ERR, "Failed to create framebuffer object");
    delete framebuffer;
    return nullptr;
}

/// This is no longer used but will probably be needed for future UI enhancements.
MythGLTexture* MythRenderOpenGL::CreateFramebufferTexture(QOpenGLFramebufferObject *Framebuffer)
{
    if (!Framebuffer)
        return nullptr;

    auto *texture = new MythGLTexture(Framebuffer->texture());
    texture->m_size = texture->m_totalSize = Framebuffer->size();
    texture->m_vbo  = CreateVBO(kVertexSize);
    texture->m_flip = false;
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
    if ((Framebuffer && Framebuffer->handle() == m_activeFramebuffer) ||
        (!Framebuffer && defaultFramebufferObject() == m_activeFramebuffer))
        return;

    makeCurrent();
    if (Framebuffer == nullptr)
    {
        QOpenGLFramebufferObject::bindDefault();
        m_activeFramebuffer = defaultFramebufferObject();
    }
    else
    {
        Framebuffer->bind();
        m_activeFramebuffer = Framebuffer->handle();
    }
    doneCurrent();
}

void MythRenderOpenGL::ClearFramebuffer(void)
{
    makeCurrent();
    glClear(GL_COLOR_BUFFER_BIT);
    doneCurrent();
}

void MythRenderOpenGL::DrawProcedural(QRect Area, int Alpha, QOpenGLFramebufferObject* Target,
                                      QOpenGLShaderProgram *Program, float TimeVal)
{
    if (!Program)
        return;

    makeCurrent();
    BindFramebuffer(Target);
    glEnableVertexAttribArray(VERTEX_INDEX);
    GetCachedVBO(GL_TRIANGLE_STRIP, Area);
    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
    SetShaderProjection(Program);
    Program->setUniformValue("u_time",  TimeVal);
    Program->setUniformValue("u_alpha", static_cast<float>(Alpha / 255.0F));
    Program->setUniformValue("u_res",   QVector2D(m_window->width(), m_window->height()));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    glDisableVertexAttribArray(VERTEX_INDEX);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(MythGLTexture *Texture, QOpenGLFramebufferObject *Target,
                                  const QRect Source, const QRect Destination,
                                  QOpenGLShaderProgram *Program, int Alpha, qreal Scale)
{
    makeCurrent();

    if (!Texture || !((Texture->m_texture || Texture->m_textureId) && Texture->m_vbo))
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
    if (UpdateTextureVertices(Texture, Source, Destination, 0, Scale))
    {
        if (m_extraFeaturesUsed & kGLBufferMap)
        {
            void* target = buffer->map(QOpenGLBuffer::WriteOnly);
            if (target)
            {
                std::copy(Texture->m_vertexData.cbegin(),
                          Texture->m_vertexData.cend(),
                          static_cast<GLfloat*>(target));
            }
            buffer->unmap();
        }
        else
        {
            buffer->write(0, Texture->m_vertexData.data(), kVertexSize);
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

void MythRenderOpenGL::DrawBitmap(std::vector<MythGLTexture *> &Textures,
                                  QOpenGLFramebufferObject *Target,
                                  const QRect Source, const QRect Destination,
                                  QOpenGLShaderProgram *Program,
                                  int Rotation)
{
    if (Textures.empty())
        return;

    makeCurrent();
    BindFramebuffer(Target);

    if (Program == nullptr)
        Program = m_defaultPrograms[kShaderDefault];

    MythGLTexture* first = Textures[0];
    if (!first || !((first->m_texture || first->m_textureId) && first->m_vbo))
        return;

    SetShaderProjection(Program);

    GLenum textarget = first->m_target;
    for (uint i = 0; i < Textures.size(); i++)
    {
        QString uniform = QString("s_texture%1").arg(i);
        Program->setUniformValue(qPrintable(uniform), i);
        ActiveTexture(GL_TEXTURE0 + i);
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
            {
                std::copy(first->m_vertexData.cbegin(),
                          first->m_vertexData.cend(),
                          static_cast<GLfloat*>(target));
            }
            buffer->unmap();
        }
        else
        {
            buffer->write(0, first->m_vertexData.data(), kVertexSize);
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
void MythRenderOpenGL::ClearRect(QOpenGLFramebufferObject *Target, const QRect Area, int Color, int Alpha)
{
    makeCurrent();
    BindFramebuffer(Target);
    glEnableVertexAttribArray(VERTEX_INDEX);

    // Set the fill color
    float color = m_fullRange ? Color / 255.0F : (Color * kLimitedRangeScale) + kLimitedRangeOffset;
    glVertexAttrib4f(COLOR_INDEX, color, color, color, Alpha / 255.0F);
    SetShaderProjection(m_defaultPrograms[kShaderSimple]);

    GetCachedVBO(GL_TRIANGLE_STRIP, Area);
    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    glDisableVertexAttribArray(VERTEX_INDEX);
    doneCurrent();
}

void MythRenderOpenGL::DrawRect(QOpenGLFramebufferObject *Target,
                                const QRect Area, const QBrush &FillBrush,
                                const QPen &LinePen, int Alpha)
{
    DrawRoundRect(Target, Area, 1, FillBrush, LinePen, Alpha);
}


void MythRenderOpenGL::DrawRoundRect(QOpenGLFramebufferObject *Target,
                                     const QRect Area, int CornerRadius,
                                     const QBrush &FillBrush,
                                     const QPen &LinePen, int Alpha)
{
    bool fill = FillBrush.style() != Qt::NoBrush;
    bool edge = LinePen.style() != Qt::NoPen;
    if (!(fill || edge))
        return;

    auto SetColor = [&](const QColor& Color)
    {
        if (m_fullRange)
        {
            glVertexAttrib4f(COLOR_INDEX, Color.red() / 255.0F, Color.green() / 255.0F,
                             Color.blue() / 255.0F, (Color.alpha() / 255.0F) * (Alpha / 255.0F));
            return;
        }
        glVertexAttrib4f(COLOR_INDEX, (Color.red() * kLimitedRangeScale) + kLimitedRangeOffset,
                        (Color.blue() * kLimitedRangeScale) + kLimitedRangeOffset,
                        (Color.green() * kLimitedRangeScale) + kLimitedRangeOffset,
                        (Color.alpha() / 255.0F) * (Alpha / 255.0F));
    };

    float halfwidth = Area.width() / 2.0F;
    float halfheight = Area.height() / 2.0F;
    float radius = CornerRadius;
    radius = std::max(radius, 1.0F);
    radius = std::min(radius, halfwidth);
    radius = std::min(radius, halfheight);

    // Set shader parameters
    // Centre of the rectangle
    m_parameters(0,0) = Area.left() + halfwidth;
    m_parameters(1,0) = Area.top() + halfheight;
    m_parameters(2,0) = radius;
    // Rectangle 'size' - distances from the centre to the edge
    m_parameters(0,1) = halfwidth;
    m_parameters(1,1) = halfheight;

    makeCurrent();
    BindFramebuffer(Target);
    glEnableVertexAttribArray(VERTEX_INDEX);
    GetCachedVBO(GL_TRIANGLE_STRIP, Area);
    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(GLfloat), kVertexOffset);

    if (fill)
    {
        SetColor(FillBrush.color());
        SetShaderProjection(m_defaultPrograms[kShaderRect]);
        SetShaderProgramParams(m_defaultPrograms[kShaderRect], m_parameters, "u_parameters");
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    if (edge)
    {
        float innerradius = radius - LinePen.width();
        innerradius = std::max(innerradius, 1.0F);
        m_parameters(3,0) = innerradius;
        // Adjust the size for the inner radius (edge)
        m_parameters(2,1) = halfwidth - LinePen.width();
        m_parameters(3,1) = halfheight - LinePen.width();
        SetColor(LinePen.color());
        SetShaderProjection(m_defaultPrograms[kShaderEdge]);
        SetShaderProgramParams(m_defaultPrograms[kShaderEdge], m_parameters, "u_parameters");
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
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
    QOpenGLFramebufferObject::bindDefault();
    m_activeFramebuffer = defaultFramebufferObject();
    Flush();
}

QFunctionPointer MythRenderOpenGL::GetProcAddress(const QString &Proc) const
{
    static const std::array<const QString,4> kExts { "", "ARB", "EXT", "OES" };
    QFunctionPointer result = nullptr;
    for (const auto & ext : kExts)
    {
        result = getProcAddress((Proc + ext).toLocal8Bit().constData());
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
    if (m_vao)
    {
        extraFunctions()->glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
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

QStringList MythRenderOpenGL::GetDescription(void)
{
    QStringList result;
    result.append(tr("QPA platform")    + "\t: " + QGuiApplication::platformName());
    result.append(tr("OpenGL vendor")   + "\t: " + reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    result.append(tr("OpenGL renderer") + "\t: " + reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    result.append(tr("OpenGL version")  + "\t: " + reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    QSurfaceFormat fmt = format();
    result.append(tr("Color depth (RGBA)")   + "\t: " + QString("%1:%2:%3:%4")
                  .arg(fmt.redBufferSize()).arg(fmt.greenBufferSize())
                  .arg(fmt.blueBufferSize()).arg(fmt.alphaBufferSize()));
    return result;
}

bool MythRenderOpenGL::UpdateTextureVertices(MythGLTexture *Texture, const QRect Source,
                                             const QRect Destination, int Rotation, qreal Scale)
{
    if (!Texture || Texture->m_size.isEmpty())
        return false;

    if ((Texture->m_source == Source) && (Texture->m_destination == Destination) &&
        (Texture->m_rotation == Rotation))
        return false;

    Texture->m_source      = Source;
    Texture->m_destination = Destination;
    Texture->m_rotation    = Rotation;

    GLfloat *data = Texture->m_vertexData.data();
    QSize    size = Texture->m_size;

    int width  = Texture->m_crop ? std::min(Source.width(),  size.width())  : Source.width();
    int height = Texture->m_crop ? std::min(Source.height(), size.height()) : Source.height();

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

    width  = Texture->m_crop ? std::min(static_cast<int>(width * Scale), Destination.width())   : Destination.width();
    height = Texture->m_crop ? std::min(static_cast<int>(height * Scale), Destination.height()) : Destination.height();

    data[2] = data[0] = Destination.left();
    data[5] = data[1] = Destination.top();
    data[4] = data[6] = Destination.left() + width;
    data[3] = data[7] = Destination.top()  + height;

    if (Texture->m_rotation != 0)
    {
        if (Texture->m_rotation == 90)
        {
            GLfloat temp = data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET];
            data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET] = data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET];
            data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET] = temp;
            data[2 + TEX_OFFSET] = data[6 + TEX_OFFSET];
            data[4 + TEX_OFFSET] = data[0 + TEX_OFFSET];
        }
        else if (Texture->m_rotation == -90)
        {
            GLfloat temp = data[0 + TEX_OFFSET];
            data[0 + TEX_OFFSET] = data[6 + TEX_OFFSET];
            data[6 + TEX_OFFSET] = temp;
            data[3 + TEX_OFFSET] = data[1 + TEX_OFFSET];
            data[5 + TEX_OFFSET] = data[7 + TEX_OFFSET];
        }
        else if (abs(Texture->m_rotation) == 180)
        {
            GLfloat temp = data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET];
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

GLfloat* MythRenderOpenGL::GetCachedVertices(GLuint Type, const QRect Area)
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

void MythRenderOpenGL::GetCachedVBO(GLuint Type, const QRect Area)
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
        case QOpenGLTexture::RGBA_Integer:
        case QOpenGLTexture::BGRA_Integer:
        case QOpenGLTexture::BGRA:
        case QOpenGLTexture::RGBA: bpp = 4; break;
        case QOpenGLTexture::RGB_Integer:
        case QOpenGLTexture::BGR_Integer:
        case QOpenGLTexture::BGR:
        case QOpenGLTexture::RGB:  bpp = 3; break;
        case QOpenGLTexture::RG_Integer:
        case QOpenGLTexture::RG:   bpp = 2; break;
        case QOpenGLTexture::Red:
        case QOpenGLTexture::Red_Integer:
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

void MythRenderOpenGL::PushTransformation(const UIEffects &Fx, QPointF &Center)
{
    QMatrix4x4 newtop = m_transforms.top();
    if (Fx.m_hzoom != 1.0F || Fx.m_vzoom != 1.0F || Fx.m_angle != 0.0F)
    {
        newtop.translate(static_cast<GLfloat>(Center.x()), static_cast<GLfloat>(Center.y()));
        newtop.scale(Fx.m_hzoom, Fx.m_vzoom);
        newtop.rotate(Fx.m_angle, 0, 0, 1);
        newtop.translate(static_cast<GLfloat>(-Center.x()), static_cast<GLfloat>(-Center.y()));
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
        for (QOpenGLShader* shader : std::as_const(shaders))
            LOG(VB_GENERAL, LOG_DEBUG, "\n" + shader->sourceCode());
    }
    program->bindAttributeLocation("a_position",  VERTEX_INDEX);
    program->bindAttributeLocation("a_color",     COLOR_INDEX);
    program->bindAttributeLocation("a_texcoord0", TEXTURE_INDEX);
    if (!program->link())
        return ShaderError(program, "");
    return program;
}

QOpenGLShaderProgram* MythRenderOpenGL::CreateComputeShader(const QString &Source)
{
    if (!(m_extraFeaturesUsed & kGLComputeShaders) || Source.isEmpty())
        return nullptr;

    OpenGLLocker locker(this);
    auto *program = new QOpenGLShaderProgram();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Compute, Source))
        return ShaderError(program, Source);

    if (VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_DEBUG))
    {
        QList<QOpenGLShader*> shaders = program->shaders();
        for (QOpenGLShader* shader : std::as_const(shaders))
            LOG(VB_GENERAL, LOG_DEBUG, "\n" + shader->sourceCode());
    }

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
    QString tag = QString("%1-%2").arg(Program->programId()).arg(Uniform);
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
    m_defaultPrograms[kShaderRect]       = CreateShaderProgram(kDrawVertexShader, kRoundedRectShader);
    m_defaultPrograms[kShaderEdge]       = CreateShaderProgram(kDrawVertexShader, kRoundedEdgeShader);
    return m_defaultPrograms[kShaderSimple] && m_defaultPrograms[kShaderDefault] &&
           m_defaultPrograms[kShaderRect] && m_defaultPrograms[kShaderEdge];
}

void MythRenderOpenGL::DeleteDefaultShaders(void)
{
    for (auto & program : m_defaultPrograms)
    {
        DeleteShaderProgram(program);
        program = nullptr;
    }
}

void MythRenderOpenGL::SetMatrixView(void)
{
    m_projection.setToIdentity();
    m_projection.ortho(m_viewport);
}

std::tuple<int, int, int> MythRenderOpenGL::GetGPUMemory()
{
    OpenGLLocker locker(this);
    if (m_extraFeaturesUsed & kGLNVMemory)
    {
        GLint total = 0;
        GLint dedicated = 0;
        GLint available = 0;
        glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total);
        glGetIntegerv(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicated);
        glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available);
        return { total / 1024, dedicated / 1024, available / 1024 };
    }
    return { 0, 0, 0 };
}

/*! \brief Check for 16bit framebufferobject support
 *
 * We don't restrict this test based on OpenGL type or version to try and give as
 * much support as possible. It will likely fail on all GL/ES 2.X versions.
 *
 * \note Qt support for 16bit framebuffers is broken until Qt5.12 on OpenGL ES3.X.
 * \note Qt only supports GL_RGBA16, GL_RGB16, GL_RGB10_A2 and GL_RGB10 but we
 * only test for RGBA16 as it should be widely supported and will not restrict
 * the use of alpha blending.
*/
void MythRenderOpenGL::Check16BitFBO(void)
{
    OpenGLLocker locker(this);
    QSize size{256, 256};
    QOpenGLFramebufferObject *fbo = CreateFramebuffer(size, true);
    if (fbo)
    {
        m_extraFeatures |= kGL16BitFBO;
        delete fbo;
    }
}
