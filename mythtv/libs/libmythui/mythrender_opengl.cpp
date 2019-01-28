// Std
#include <algorithm>
using std::min;

// Qt
#include <QLibrary>
#include <QPainter>
#include <QWindow>
#include <QWidget>
#include <QGuiApplication>

// MythTV
#include "mythcorecontext.h"
#include "mythrender_opengl.h"
#include "mythrender_opengl_shaders.h"
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
static const GLuint kVertexSize    = 16 * sizeof(GLfloat);

static inline int __glCheck__(const QString &loc, const char* fileName, int n)
{
    GLenum error;
    while((error = glGetError()) != GL_NO_ERROR)
        LOG(VB_GENERAL, LOG_ERR, QString("%1: %2 @ %3, %4").arg(loc).arg(error).arg(fileName).arg(n));
    return error;
}

#define MAX_VERTEX_CACHE 500
#define glCheck() __glCheck__(LOC, __FILE__, __LINE__)

MythGLTexture::MythGLTexture(QOpenGLTexture *Texture)
  : m_data(nullptr),
    m_bufferSize(0),
    m_textureId(0),
    m_texture(Texture),
    m_pixelFormat(QOpenGLTexture::RGBA),
    m_pixelType(QOpenGLTexture::UInt8),
    m_vbo(nullptr),
    m_size(0,0),
    m_totalSize(0,0),
    m_flip(true),
    m_crop(false),
    m_source(),
    m_destination()
{
    memset(&m_vertexData, 0, sizeof(m_vertexData));
}

MythGLTexture::MythGLTexture(GLuint Texture)
  : m_data(nullptr),
    m_bufferSize(0),
    m_textureId(Texture),
    m_texture(nullptr),
    m_pixelFormat(QOpenGLTexture::RGBA),
    m_pixelType(QOpenGLTexture::UInt8),
    m_vbo(nullptr),
    m_size(0,0),
    m_totalSize(0,0),
    m_flip(true),
    m_crop(false),
    m_source(),
    m_destination()
{
    memset(&m_vertexData, 0, sizeof(m_vertexData));
}

OpenGLLocker::OpenGLLocker(MythRenderOpenGL *render) : m_render(render)
{
    if (m_render)
        m_render->makeCurrent();
}

OpenGLLocker::~OpenGLLocker()
{
    if (m_render)
        m_render->doneCurrent();
}

MythRenderOpenGL* MythRenderOpenGL::Create(const QString&, QPaintDevice* device)
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
     && display.contains(':'))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL is disabled for Remote X Session");
        return nullptr;
    }

    QSurfaceFormat format;
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(1);
#ifdef USING_OPENGLES
    format.setRenderableType(QSurfaceFormat::OpenGLES);
#endif
    if (qApp->platformName().contains("egl"))
        format.setRenderableType(QSurfaceFormat::OpenGLES);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        format.setOption(QSurfaceFormat::DebugContext);

    return new MythRenderOpenGL(format, device);
}

MythRenderOpenGL::MythRenderOpenGL(const QSurfaceFormat& format, QPaintDevice* device,
                                   RenderType type)
  : QOpenGLContext(),
    QOpenGLFunctions(),
    MythRender(type),
    m_lock(QMutex::Recursive),
    m_openglDebugger(nullptr)
{
    QWidget *w = dynamic_cast<QWidget*>(device);
    m_window = (w) ? w->windowHandle() : nullptr;
    ResetVars();
    ResetProcs();
    setFormat(format);
}

MythRenderOpenGL::~MythRenderOpenGL()
{
    if (!isValid())
        return;
    makeCurrent();
    DeleteOpenGLResources();
    if (m_openglDebugger)
        delete m_openglDebugger;
    doneCurrent();
}

void MythRenderOpenGL::messageLogged(const QOpenGLDebugMessage &Message)
{
    // filter out our own messages
    if (Message.source() == QOpenGLDebugMessage::ApplicationSource)
        return;

    QString message = Message.message();
    QString type("Unknown");
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
    LOG(VB_GPU, LOG_INFO, QString("%1 %2: %3").arg(LOC).arg(type).arg(message));
}

void MythRenderOpenGL::logDebugMarker(const QString &Message)
{
    if (m_openglDebugger)
    {
        QOpenGLDebugMessage message = QOpenGLDebugMessage::createApplicationMessage(
            Message, 0, QOpenGLDebugMessage::NotificationSeverity, QOpenGLDebugMessage::MarkerType);
        m_openglDebugger->logMessage(message);
    }
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
#if 0
            // this will impact performance but can be very useful
            mode = QOpenGLDebugLogger::SynchronousLogging;
#endif
            m_openglDebugger->startLogging(mode);
            LOG(VB_GENERAL, LOG_INFO, LOC + "GPU debug logging started");
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to initialise OpenGL logging");
            delete m_openglDebugger;
            m_openglDebugger = nullptr;
        }
    }

    InitProcs();
    Init2DState();

    // basic features
    GLint maxtexsz = 0;
    GLint maxunits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxunits);
    m_max_units = maxunits;
    m_max_tex_size = (maxtexsz) ? maxtexsz : 512;

    // RGBA16 - available on ES via extension
    m_extraFeatures |= isOpenGLES() ? hasExtension("GL_EXT_texture_norm16") ? kGLExtRGBA16 : kGLFeatNone : kGLExtRGBA16;

    // Pixel buffer objects
    bool buffer_procs = (MYTH_GLMAPBUFFERPROC)GetProcAddress("glMapBuffer") &&
                        (MYTH_GLUNMAPBUFFERPROC)GetProcAddress("glUnmapBuffer");

    // Buffers are available by default (GL and GLES).
    // Buffer mapping is available by extension
    if ((isOpenGLES() && hasExtension("GL_OES_mapbuffer") && buffer_procs) ||
        (hasExtension("GL_ARB_vertex_buffer_object") && buffer_procs))
        m_extraFeatures |= kGLBufferMap;

    // TODO combine fence functionality
    // NVidia fence
    if(hasExtension("GL_NV_fence") && m_glGenFencesNV && m_glDeleteFencesNV &&
        m_glSetFenceNV  && m_glFinishFenceNV)
        m_extraFeatures |= kGLNVFence;

    // Apple fence
    if(hasExtension("GL_APPLE_fence") && m_glGenFencesAPPLE && m_glDeleteFencesAPPLE &&
        m_glSetFenceAPPLE  && m_glFinishFenceAPPLE)
        m_extraFeatures |= kGLAppleFence;

    // Framebuffer discard - GLES only
    if (isOpenGLES() && hasExtension("GL_EXT_discard_framebuffer") && m_glDiscardFramebuffer)
        m_extraFeatures |= kGLExtFBDiscard;

    DebugFeatures();

    m_extraFeaturesUsed = m_extraFeatures;
    if (!CreateDefaultShaders())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create default shaders");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialised MythRenderOpenGL");
    return true;
}

#define GLYesNo(arg) (arg ? "Yes" : "No")

void MythRenderOpenGL::DebugFeatures(void)
{
    static bool debugged = false;
    if (debugged)
        return;
    debugged = true;
    QSurfaceFormat fmt = format();
    QString qtglversion = QString("OpenGL%1 %2.%3")
            .arg(fmt.renderableType() == QSurfaceFormat::OpenGLES ? "ES" : "")
            .arg(fmt.majorVersion()).arg(fmt.minorVersion());
    QString qtglsurface = QString("RGBA: %1%2%3%4 Depth: %5 Stencil: %6")
            .arg(fmt.redBufferSize()).arg(fmt.greenBufferSize())
            .arg(fmt.greenBufferSize()).arg(fmt.alphaBufferSize())
            .arg(fmt.depthBufferSize()).arg(fmt.stencilBufferSize());
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL vendor        : %1").arg((const char*)glGetString(GL_VENDOR)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL renderer      : %1").arg((const char*)glGetString(GL_RENDERER)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL version       : %1").arg((const char*)glGetString(GL_VERSION)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt platform          : %1").arg(qApp->platformName()));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL format     : %1").arg(qtglversion));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL surface    : %1").arg(qtglsurface));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture size     : %1").arg(m_max_tex_size));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture units    : %1").arg(m_max_units));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Shaders              : %1").arg(GLYesNo(m_features & Shaders)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("NPOT textures        : %1").arg(GLYesNo(m_features & NPOTTextures)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Multitexturing       : %1").arg(GLYesNo(m_features & Multitexture)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("RGBA16 textures      : %1").arg(GLYesNo(m_extraFeatures & kGLExtRGBA16)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Buffer mapping       : %1").arg(GLYesNo(m_extraFeatures & kGLBufferMap)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Framebuffer objects  : %1").arg(GLYesNo(m_features & Framebuffers)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Framebuffer discard  : %1").arg(GLYesNo(m_extraFeatures & kGLExtFBDiscard)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Fence                : %1")
        .arg(GLYesNo((m_extraFeatures & kGLAppleFence) || (m_extraFeatures & kGLNVFence))));

    // warnings
    if (m_max_units < 3)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Warning: Insufficient texture units for some features.");
}

uint MythRenderOpenGL::GetExtraFeatures(void) const
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
    QString renderer = (const char*)glGetString(GL_RENDERER);

    if (!(openGLFeatures() & Shaders))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL has no shader support");
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
    QWidget* wNativeParent = w->nativeParentWidget();
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
    if (!m_lock_level++)
        QOpenGLContext::makeCurrent(m_window);
}

void MythRenderOpenGL::doneCurrent()
{
    // TODO add back QOpenGLContext::doneCurrent call
    // once calls are better pipelined
    m_lock_level--;
    if (m_lock_level < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mis-matched calls to makeCurrent()");
    m_lock.unlock();
}

void MythRenderOpenGL::Release(void)
{
#if !defined(Q_OS_WIN)
    while (m_lock_level > 0)
        doneCurrent();
#endif
}

void MythRenderOpenGL::MoveResizeWindow(const QRect &rect)
{
    if (m_window)
        m_window->setGeometry(rect);
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

void MythRenderOpenGL::Flush(bool use_fence)
{
    if (!m_flushEnabled)
        return;

    makeCurrent();

    if ((m_extraFeaturesUsed & kGLAppleFence) &&
        (m_fence && use_fence))
    {
        m_glSetFenceAPPLE(m_fence);
        m_glFinishFenceAPPLE(m_fence);
    }
    else if ((m_extraFeaturesUsed & kGLNVFence) &&
             (m_fence && use_fence))
    {
        m_glSetFenceNV(m_fence, GL_ALL_COMPLETED_NV);
        m_glFinishFenceNV(m_fence);
    }
    else
    {
        glFlush();
    }

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
    uint32_t tmp = (r << 24) + (g << 16) + (b << 8) + a;
    if (tmp == m_background)
        return;

    m_background = tmp;
    makeCurrent();
    glClearColor(r / 255.0, g / 255.0, b / 255.0, a / 255.0);
    doneCurrent();
}

void MythRenderOpenGL::SetFence(void)
{
    if (!m_flushEnabled)
        return;

    makeCurrent();
    if (m_extraFeaturesUsed & kGLAppleFence)
    {
        m_glGenFencesAPPLE(1, &m_fence);
        if (m_fence)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Using GL_APPLE_fence");
    }
    else if (m_extraFeaturesUsed & kGLNVFence)
    {
        m_glGenFencesNV(1, &m_fence);
        if (m_fence)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Using GL_NV_fence");
    }
    doneCurrent();
}

void MythRenderOpenGL::DeleteFence(void)
{
    if (m_fence)
    {
        makeCurrent();
        if (m_extraFeaturesUsed & kGLAppleFence)
            m_glDeleteFencesAPPLE(1, &m_fence);
        else if (m_extraFeaturesUsed & kGLNVFence)
            m_glDeleteFencesNV(1, &m_fence);
        m_fence = 0;
        doneCurrent();
    }
}

MythGLTexture* MythRenderOpenGL::CreateTexture(QSize Size,
                                               QOpenGLTexture::PixelType PixelType,
                                               QOpenGLTexture::PixelFormat PixelFormat,
                                               QOpenGLTexture::Filter Filter,
                                               QOpenGLTexture::WrapMode Wrap,
                                               QOpenGLTexture::TextureFormat Format)
{
    OpenGLLocker locker(this);
    uint datasize = GetBufferSize(Size, PixelFormat, PixelType);
    if (!datasize)
        return nullptr;

    QOpenGLTexture *texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture->setAutoMipMapGenerationEnabled(false);
    texture->setMipLevels(1);
    texture->setSize(Size.width(), Size.height()); // this triggers creation
    if (!texture->textureId())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to create texure");
        delete texture;
        return nullptr;
    }

    if (Format == QOpenGLTexture::NoFormat)
    {
        if (isOpenGLES() && format().majorVersion() < 3)
            Format = QOpenGLTexture::RGBAFormat;
        else
            Format = QOpenGLTexture::RGBA8_UNorm;
    }
    texture->setFormat(Format);
    texture->allocateStorage(PixelFormat, PixelType);
    texture->setMinMagFilters(Filter, Filter);
    texture->setWrapMode(Wrap);

    MythGLTexture *result = new MythGLTexture(texture);
    result->m_pixelFormat = PixelFormat;
    result->m_pixelType   = PixelType;
    result->m_vbo         = CreateVBO(kVertexSize);
    result->m_totalSize   = GetTextureSize(Size);
    result->m_bufferSize  = datasize;
    result->m_size        = Size;
    return result;
}

MythGLTexture* MythRenderOpenGL::CreateTextureFromQImage(QImage *Image)
{
    if (!Image)
        return nullptr;

    OpenGLLocker locker(this);
    QOpenGLTexture *texture = new QOpenGLTexture(*Image, QOpenGLTexture::DontGenerateMipMaps);
    if (!texture->textureId())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to create texure");
        delete texture;
        return nullptr;
    }
    texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    MythGLTexture *result = new MythGLTexture(texture);
    result->m_texture     = texture;
    result->m_vbo         = CreateVBO(kVertexSize);
    result->m_totalSize   = GetTextureSize(Image->size());
    // N.B. Format and type per qopengltexure.cpp
    result->m_pixelFormat = QOpenGLTexture::RGBA;
    result->m_pixelType   = QOpenGLTexture::UInt8;
    result->m_bufferSize  = GetBufferSize(result->m_totalSize, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    result->m_size        = Image->size();
    result->m_crop        = true;
    return result;
}

MythGLTexture* MythRenderOpenGL::CreateHelperTexture(void)
{
    makeCurrent();

    uint width = m_max_tex_size;
    MythGLTexture *texture = CreateTexture(QSize(width, 1),
                                           QOpenGLTexture::Float32,
                                           QOpenGLTexture::RGBA,
                                           QOpenGLTexture::Linear,
                                           QOpenGLTexture::Repeat,
                                           QOpenGLTexture::RGBA16_UNorm);

    float *buf = nullptr;
    buf = new float[texture->m_bufferSize];
    float *ref = buf;

    for (uint i = 0; i < width; i++)
    {
        float x = (((float)i) + 0.5f) / (float)width;
        StoreBicubicWeights(x, ref);
        ref += 4;
    }
    StoreBicubicWeights(0, buf);
    StoreBicubicWeights(1, &buf[(width - 1) << 2]);

    EnableTextures();
    texture->m_texture->bind();
    texture->m_texture->setData(texture->m_pixelFormat, texture->m_pixelType, buf);
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Created bicubic helper texture (%1 samples)") .arg(width));
    delete [] buf;
    doneCurrent();
    return texture;
}

QSize MythRenderOpenGL::GetTextureSize(const QSize &size)
{
    if (m_features & NPOTTextures)
        return size;

    int w = 64;
    int h = 64;
    while (w < size.width())
        w *= 2;
    while (h < size.height())
        h *= 2;
    return QSize(w, h);
}

int MythRenderOpenGL::GetTextureDataSize(MythGLTexture *Texture)
{
    if (Texture)
        return Texture->m_bufferSize;
    return 0;
}

void MythRenderOpenGL::SetTextureFilters(MythGLTexture *Texture, QOpenGLTexture::Filter Filter, QOpenGLTexture::WrapMode Wrap)
{
    if (!Texture || (Texture && !Texture->m_texture))
        return;

    makeCurrent();
    EnableTextures();
    Texture->m_texture->bind();
    Texture->m_texture->setWrapMode(Wrap);
    Texture->m_texture->setMinMagFilters(Filter, Filter);
    doneCurrent();
}

/*! \brief Set the texture filters for the given OpenGL texture object
 *
 * \note This is used to set filters for textures attached to QOpenGLFramebufferObjects.
 * By default, these have GL_NEAREST set for the min/mag filters and there is no
 * method to change them. Furthermore we cannot wrap these textures in a QOpenGLTexture
 * and use the functionality in that class. So back to basics... and convert between
 * QOpenGLTexture and GL enums.
*/
void MythRenderOpenGL::SetTextureFilters(GLuint Texture, QOpenGLTexture::Filter Filter, QOpenGLTexture::WrapMode)
{
    if (!Texture)
        return;

    GLenum wrap   = GL_CLAMP_TO_EDGE;
    GLenum minmag = GL_NEAREST;
    if (Filter == QOpenGLTexture::Linear)
        minmag = GL_LINEAR;

    makeCurrent();
    EnableTextures();
    glBindTexture(GL_TEXTURE_2D, Texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minmag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, minmag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     wrap);
    doneCurrent();
}

void MythRenderOpenGL::ActiveTexture(int active_tex)
{
    if (!(m_features & Multitexture))
        return;

    makeCurrent();
    if (m_active_tex != active_tex)
    {
        glActiveTexture(active_tex);
        m_active_tex = active_tex;
    }
    doneCurrent();
}

void MythRenderOpenGL::StoreBicubicWeights(float x, float *dst)
{
    float w0 = (((-1 * x + 3) * x - 3) * x + 1) / 6;
    float w1 = ((( 3 * x - 6) * x + 0) * x + 4) / 6;
    float w2 = (((-3 * x + 3) * x + 3) * x + 1) / 6;
    float w3 = ((( 1 * x + 0) * x + 0) * x + 0) / 6;
    *dst++ = 1 + x - w1 / (w0 + w1);
    *dst++ = 1 - x + w3 / (w2 + w3);
    *dst++ = w0 + w1;
    *dst++ = 0;
}

void MythRenderOpenGL::EnableTextures(void)
{
    if (isOpenGLES() || m_activeTextureTarget)
        return;

    makeCurrent();
    glEnable(GL_TEXTURE_2D);
    m_activeTextureTarget = GL_TEXTURE_2D;
    doneCurrent();
}

void MythRenderOpenGL::DisableTextures(void)
{
    if (isOpenGLES() || !m_activeTextureTarget)
        return;

    makeCurrent();
    glDisable(m_activeTextureTarget);
    m_activeTextureTarget = 0;
    doneCurrent();
}

void MythRenderOpenGL::DeleteTexture(MythGLTexture *Texture)
{
    if (!Texture)
        return;

    makeCurrent();
    // N.B. Don't delete m_textureId - it is owned externally
    if (Texture->m_texture)
        delete Texture->m_texture;
    if (Texture->m_data)
        delete [] Texture->m_data;
    if (Texture->m_vbo)
        delete Texture->m_vbo;
    delete Texture;
    Flush(true);
    doneCurrent();
}

QOpenGLFramebufferObject* MythRenderOpenGL::CreateFramebuffer(QSize &Size)
{
    if (!(m_features & Framebuffers))
        return nullptr;

    OpenGLLocker locker(this);
    QOpenGLFramebufferObject *framebuffer = new QOpenGLFramebufferObject(Size);
    if (framebuffer->isValid())
    {
        if (framebuffer->isBound())
        {
            m_activeFramebuffer = framebuffer;
            BindFramebuffer(nullptr);
        }
        Flush(true);
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

    MythGLTexture *texture = new MythGLTexture(Framebuffer->texture());
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

void MythRenderOpenGL::DiscardFramebuffer(QOpenGLFramebufferObject *Framebuffer)
{
    if (!(m_extraFeaturesUsed & kGLExtFBDiscard))
        return;

    makeCurrent();
    static const GLenum fbdiscards[] = { GL_COLOR_ATTACHMENT0 };
    static const GLenum dfdiscards[] = { GL_COLOR_EXT };
    BindFramebuffer(Framebuffer);
    m_glDiscardFramebuffer(GL_FRAMEBUFFER, 1, (Framebuffer ? true : defaultFramebufferObject()) ? fbdiscards : dfdiscards);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(MythGLTexture *Texture, QOpenGLFramebufferObject *Target,
                                  const QRect &Source, const QRect &Destination,
                                  QOpenGLShaderProgram *Program, int alpha,
                                  int red, int green, int blue)
{
    makeCurrent();
    BindFramebuffer(Target);
    DrawBitmapPriv(Texture, Source, Destination, Program, alpha, red, green, blue);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(MythGLTexture **Textures, uint TextureCount,
                                  QOpenGLFramebufferObject *Target,
                                  const QRect &Source, const QRect &Destination,
                                  QOpenGLShaderProgram *Program)
{
    if (!Textures || !TextureCount)
        return;

    makeCurrent();
    BindFramebuffer(Target);
    DrawBitmapPriv(Textures, TextureCount, Source, Destination, Program);
    doneCurrent();
}

void MythRenderOpenGL::DrawRect(QOpenGLFramebufferObject *target,
                                const QRect &area, const QBrush &fillBrush,
                                const QPen &linePen, int alpha)
{
    makeCurrent();
    BindFramebuffer(target);
    DrawRectPriv(area, fillBrush, linePen, alpha);
    doneCurrent();
}

void MythRenderOpenGL::DrawRoundRect(QOpenGLFramebufferObject *target,
                                     const QRect &area, int cornerRadius,
                                     const QBrush &fillBrush,
                                     const QPen &linePen, int alpha)
{
    makeCurrent();
    BindFramebuffer(target);
    DrawRoundRectPriv(area, cornerRadius, fillBrush, linePen, alpha);
    doneCurrent();
}

inline void MythRenderOpenGL::glVertexAttribPointerI(
    GLuint index, GLint size, GLenum type, GLboolean normalize,
    GLsizei stride, const GLuint value)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    glVertexAttribPointer(index, size, type, normalize,
                            stride, (const char *)value);
#pragma GCC diagnostic pop
}

void MythRenderOpenGL::DrawBitmapPriv(MythGLTexture *Texture, const QRect &Source,
                                      const QRect &Destination,
                                      QOpenGLShaderProgram *Program,
                                      int alpha, int red, int green, int blue)
{
    if (!Texture || (Texture && !((Texture->m_texture || Texture->m_textureId) && Texture->m_vbo)))
        return;

    if (Program == nullptr)
        Program = m_defaultPrograms[kShaderDefault];

    SetShaderProgramParams(Program, m_projection, "u_projection");
    SetShaderProgramParams(Program, m_transforms.top(), "u_transform");

    EnableTextures();
    Program->setUniformValue("s_texture0", 0);
    ActiveTexture(GL_TEXTURE0);
    if (Texture->m_texture)
        Texture->m_texture->bind();
    else
        glBindTexture(GL_TEXTURE_2D, Texture->m_textureId);

    QOpenGLBuffer* buffer = Texture->m_vbo;
    buffer->bind();
    if (UpdateTextureVertices(Texture, Source, Destination))
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

    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            kVertexOffset);
    glVertexAttrib4f(COLOR_INDEX, red / 255.0, green / 255.0, blue / 255.0, alpha / 255.0);
    glVertexAttribPointerI(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(TEXTURE_INDEX);
    glDisableVertexAttribArray(VERTEX_INDEX);
    buffer->release(QOpenGLBuffer::VertexBuffer);
}

void MythRenderOpenGL::DrawBitmapPriv(MythGLTexture **Textures, uint TextureCount,
                                      const QRect &Source, const QRect &Destination,
                                      QOpenGLShaderProgram *Program)
{
    if (Program == nullptr)
        Program = m_defaultPrograms[kShaderDefault];

    MythGLTexture* first = Textures[0];
    if (!first || (first && !((first->m_texture || first->m_textureId) && first->m_vbo)))
        return;

    SetShaderProgramParams(Program, m_projection, "u_projection");
    SetShaderProgramParams(Program, m_transforms.top(), "u_transform");

    EnableTextures();
    uint active_tex = 0;
    for (uint i = 0; i < TextureCount; i++)
    {
        QString uniform = QString("s_texture%1").arg(active_tex);
        Program->setUniformValue(qPrintable(uniform), active_tex);
        ActiveTexture(GL_TEXTURE0 + active_tex++);
        if (Textures[i]->m_texture)
            Textures[i]->m_texture->bind();
        else
            glBindTexture(GL_TEXTURE_2D, Textures[i]->m_textureId);
    }

    QOpenGLBuffer* buffer = first->m_vbo;
    buffer->bind();
    if (UpdateTextureVertices(first, Source, Destination))
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

    glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            kVertexOffset);
    glVertexAttrib4f(COLOR_INDEX, 1.0, 1.0, 1.0, 1.0);
    glVertexAttribPointerI(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(TEXTURE_INDEX);
    glDisableVertexAttribArray(VERTEX_INDEX);
    buffer->release(QOpenGLBuffer::VertexBuffer);
}

void MythRenderOpenGL::DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                                     const QPen &linePen, int alpha)
{
    DrawRoundRectPriv(area, 1, fillBrush, linePen, alpha);
}

void MythRenderOpenGL::DrawRoundRectPriv(const QRect &area, int cornerRadius,
                                          const QBrush &fillBrush,
                                          const QPen &linePen, int alpha)
{
    int lineWidth = linePen.width();
    int halfline  = lineWidth / 2;
    int rad = cornerRadius - halfline;

    if ((area.width() / 2) < rad)
        rad = area.width() / 2;

    if ((area.height() / 2) < rad)
        rad = area.height() / 2;
    int dia = rad * 2;

    QRect r(area.left(), area.top(), area.width(), area.height());

    QRect tl(r.left(),  r.top(), rad, rad);
    QRect tr(r.left() + r.width() - rad, r.top(), rad, rad);
    QRect bl(r.left(),  r.top() + r.height() - rad, rad, rad);
    QRect br(r.left() + r.width() - rad, r.top() + r.height() - rad, rad, rad);

    DisableTextures();

    glEnableVertexAttribArray(VERTEX_INDEX);

    if (fillBrush.style() != Qt::NoBrush)
    {
        // Get the shaders
        QOpenGLShaderProgram* elip = m_defaultPrograms[kShaderCircle];
        QOpenGLShaderProgram* fill = m_defaultPrograms[kShaderSimple];

        // Set the fill color
        glVertexAttrib4f(COLOR_INDEX,
                           fillBrush.color().red() / 255.0,
                           fillBrush.color().green() / 255.0,
                           fillBrush.color().blue() / 255.0,
                          (fillBrush.color().alpha() / 255.0) * (alpha / 255.0));

        // Set the radius
        m_parameters(2,0) = rad;
        m_parameters(3,0) = rad - 1.0;

        // Enable the Circle shader
        SetShaderProgramParams(elip, m_projection, "u_projection");
        SetShaderProgramParams(elip, m_transforms.top(), "u_transform");

        // Draw the top left segment
        m_parameters(0,0) = tl.left() + rad;
        m_parameters(1,0) = tl.top() + rad;

        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the top right segment
        m_parameters(0,0) = tr.left();
        m_parameters(1,0) = tr.top() + rad;
        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tr);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom left segment
        m_parameters(0,0) = bl.left() + rad;
        m_parameters(1,0) = bl.top();
        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, bl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom right segment
        m_parameters(0,0) = br.left();
        m_parameters(1,0) = br.top();
        SetShaderProgramParams(elip, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, br);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Fill the remaining areas
        QRect main(r.left() + rad, r.top(), r.width() - dia, r.height());
        QRect left(r.left(), r.top() + rad, rad, r.height() - dia);
        QRect right(r.left() + r.width() - rad, r.top() + rad, rad, r.height() - dia);

        SetShaderProgramParams(fill, m_projection, "u_projection");
        SetShaderProgramParams(fill, m_transforms.top(), "u_transform");

        GetCachedVBO(GL_TRIANGLE_STRIP, main);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GetCachedVBO(GL_TRIANGLE_STRIP, left);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GetCachedVBO(GL_TRIANGLE_STRIP, right);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    }

    if (linePen.style() != Qt::NoPen)
    {
        // Get the shaders
        QOpenGLShaderProgram* edge = m_defaultPrograms[kShaderCircleEdge];
        QOpenGLShaderProgram* vline = m_defaultPrograms[kShaderVertLine];
        QOpenGLShaderProgram* hline = m_defaultPrograms[kShaderHorizLine];

        // Set the line color
        glVertexAttrib4f(COLOR_INDEX,
                           linePen.color().red() / 255.0,
                           linePen.color().green() / 255.0,
                           linePen.color().blue() / 255.0,
                          (linePen.color().alpha() / 255.0) * (alpha / 255.0));

        // Set the radius and width
        m_parameters(2,0) = rad - lineWidth / 2.0;
        m_parameters(3,0) = lineWidth / 2.0;

        // Enable the edge shader
        SetShaderProgramParams(edge, m_projection, "u_projection");
        SetShaderProgramParams(edge, m_transforms.top(), "u_transform");

        // Draw the top left edge segment
        m_parameters(0,0) = tl.left() + rad;
        m_parameters(1,0) = tl.top() + rad;
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the top right edge segment
        m_parameters(0,0) = tr.left();
        m_parameters(1,0) = tr.top() + rad;
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, tr);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom left edge segment
        m_parameters(0,0) = bl.left() + rad;
        m_parameters(1,0) = bl.top();
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, bl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom right edge segment
        m_parameters(0,0) = br.left();
        m_parameters(1,0) = br.top();
        SetShaderProgramParams(edge, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, br);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Vertical lines
        SetShaderProgramParams(vline, m_projection, "u_projection");
        SetShaderProgramParams(vline, m_transforms.top(), "u_transform");

        m_parameters(1,0) = lineWidth / 2.0;
        QRect vl(r.left(), r.top() + rad,
                 lineWidth, r.height() - dia);

        // Draw the left line segment
        m_parameters(0,0) = vl.left() + lineWidth;
        SetShaderProgramParams(vline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, vl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the right line segment
        vl.translate(r.width() - lineWidth, 0);
        m_parameters(0,0) = vl.left();
        SetShaderProgramParams(vline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, vl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Horizontal lines
        SetShaderProgramParams(hline, m_projection, "u_projection");
        SetShaderProgramParams(hline, m_transforms.top(), "u_transform");
        QRect hl(r.left() + rad, r.top(),
                 r.width() - dia, lineWidth);

        // Draw the top line segment
        m_parameters(0,0) = hl.top() + lineWidth;
        SetShaderProgramParams(hline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, hl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw the bottom line segment
        hl.translate(0, r.height() - lineWidth);
        m_parameters(0,0) = hl.top();
        SetShaderProgramParams(hline, m_parameters, "u_parameters");
        GetCachedVBO(GL_TRIANGLE_STRIP, hl);
        glVertexAttribPointerI(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    }

    glDisableVertexAttribArray(VERTEX_INDEX);
}

void MythRenderOpenGL::Init2DState(void)
{
    SetBlend(true);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Flush(true);
}

void MythRenderOpenGL::InitProcs(void)
{
    m_glGenFencesNV = (MYTH_GLGENFENCESNVPROC)GetProcAddress("glGenFencesNV");
    m_glDeleteFencesNV = (MYTH_GLDELETEFENCESNVPROC)GetProcAddress("glDeleteFencesNV");
    m_glSetFenceNV = (MYTH_GLSETFENCENVPROC)GetProcAddress("glSetFenceNV");
    m_glFinishFenceNV = (MYTH_GLFINISHFENCENVPROC)GetProcAddress("glFinishFenceNV");
    m_glGenFencesAPPLE = (MYTH_GLGENFENCESAPPLEPROC)GetProcAddress("glGenFencesAPPLE");
    m_glDeleteFencesAPPLE = (MYTH_GLDELETEFENCESAPPLEPROC)GetProcAddress("glDeleteFencesAPPLE");
    m_glSetFenceAPPLE = (MYTH_GLSETFENCEAPPLEPROC)GetProcAddress("glSetFenceAPPLE");
    m_glFinishFenceAPPLE = (MYTH_GLFINISHFENCEAPPLEPROC)GetProcAddress("glFinishFenceAPPLE");
    m_glDiscardFramebuffer = (MYTH_GLDISCARDFRAMEBUFFER)GetProcAddress("glDiscardFramebufferEXT");
}

QFunctionPointer MythRenderOpenGL::GetProcAddress(const QString &proc) const
{
    static const QString exts[4] = { "", "ARB", "EXT", "OES" };
    QFunctionPointer result = nullptr;
    for (int i = 0; i < 4; i++)
    {
        result = getProcAddress((proc + exts[i]).toLocal8Bit().constData());
        if (result)
            break;
    }
    if (result == nullptr)
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Extension not found: %1").arg(proc));
    return result;
}

void MythRenderOpenGL::ResetVars(void)
{
    m_fence           = 0;

    m_lock_level      = 0;

    m_features        = Multitexture;
    m_extraFeatures   = kGLFeatNone;
    m_extraFeaturesUsed = kGLFeatNone;
    m_max_tex_size    = 0;
    m_max_units       = 0;

    m_viewport        = QRect();
    m_active_tex      = 0;
    m_activeTextureTarget = 0;
    m_activeFramebuffer = nullptr;
    m_blend           = false;
    m_background      = 0x00000000;
    m_flushEnabled    = true;
    m_projection.fill(0);
    m_parameters.fill(0);
    memset(m_defaultPrograms, 0, sizeof(m_defaultPrograms));
    m_activeProgram = nullptr;
    m_transforms.clear();
    m_transforms.push(QMatrix4x4());
    m_cachedMatrixUniforms.clear();
}

void MythRenderOpenGL::ResetProcs(void)
{
    m_glGenFencesNV = nullptr;
    m_glDeleteFencesNV = nullptr;
    m_glSetFenceNV = nullptr;
    m_glFinishFenceNV = nullptr;
    m_glGenFencesAPPLE = nullptr;
    m_glDeleteFencesAPPLE = nullptr;
    m_glSetFenceAPPLE = nullptr;
    m_glFinishFenceAPPLE = nullptr;
    m_glDiscardFramebuffer = nullptr;
}

QOpenGLBuffer* MythRenderOpenGL::CreateVBO(uint Size, bool Release /*=true*/)
{
    OpenGLLocker locker(this);
    QOpenGLBuffer* buffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (buffer->create())
    {
        buffer->setUsagePattern(QOpenGLBuffer::StreamDraw);
        buffer->bind();
        buffer->allocate(Size);
        if (Release)
            buffer->release(QOpenGLBuffer::VertexBuffer);
        return buffer;
    }
    delete buffer;
    return nullptr;
}

void MythRenderOpenGL::DeleteOpenGLResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting OpenGL Resources");
    DeleteDefaultShaders();
    DeleteFence();
    ExpireVertices();
    ExpireVBOS();
    Flush(false);

    if (m_cachedVertices.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString(" %1 unexpired vertices")
            .arg(m_cachedVertices.size()));
    }

    if (m_cachedVBOS.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString(" %1 unexpired VBOs")
            .arg(m_cachedVertices.size()));
    }
}

bool MythRenderOpenGL::UpdateTextureVertices(MythGLTexture *Texture, const QRect &Source,
                                             const QRect &Destination)
{
    if (!Texture || (Texture && Texture->m_size.isEmpty()))
        return false;

    if ((Texture->m_source == Source) && (Texture->m_destination == Destination))
        return false;

    Texture->m_source = Source;
    Texture->m_destination = Destination;

    GLfloat *data = Texture->m_vertexData;
    QSize    size = Texture->m_size;

    int width  = Texture->m_crop ? min(Source.width(),  size.width())  : Source.width();
    int height = Texture->m_crop ? min(Source.height(), size.height()) : Source.height();

    data[0 + TEX_OFFSET] = Source.left() / (GLfloat)size.width();
    data[(Texture->m_flip ? 7 : 1) + TEX_OFFSET] = (Source.top() + height) / (GLfloat)size.height();
    data[6 + TEX_OFFSET] = (Source.left() + width) / (GLfloat)size.width();
    data[(Texture->m_flip ? 1 : 7) + TEX_OFFSET] = Source.top() / (GLfloat)size.height();

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
    return true;
}

GLfloat* MythRenderOpenGL::GetCachedVertices(GLuint type, const QRect &area)
{
    uint64_t ref = ((uint64_t)area.left() & 0xfff) +
                  (((uint64_t)area.top() & 0xfff) << 12) +
                  (((uint64_t)area.width() & 0xfff) << 24) +
                  (((uint64_t)area.height() & 0xfff) << 36) +
                  (((uint64_t)type & 0xfff) << 48);

    if (m_cachedVertices.contains(ref))
    {
        m_vertexExpiry.removeOne(ref);
        m_vertexExpiry.append(ref);
        return m_cachedVertices[ref];
    }

    GLfloat *vertices = new GLfloat[8];

    vertices[2] = vertices[0] = area.left();
    vertices[5] = vertices[1] = area.top();
    vertices[4] = vertices[6] = area.left() + area.width();
    vertices[3] = vertices[7] = area.top() + area.height();

    if (type == GL_LINE_LOOP)
    {
        vertices[7] = vertices[1];
        vertices[5] = vertices[3];
    }

    m_cachedVertices.insert(ref, vertices);
    m_vertexExpiry.append(ref);
    ExpireVertices(MAX_VERTEX_CACHE);

    return vertices;
}

void MythRenderOpenGL::ExpireVertices(uint max)
{
    while ((uint)m_vertexExpiry.size() > max)
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

void MythRenderOpenGL::GetCachedVBO(GLuint type, const QRect &area)
{
    uint64_t ref = ((uint64_t)area.left() & 0xfff) +
                  (((uint64_t)area.top() & 0xfff) << 12) +
                  (((uint64_t)area.width() & 0xfff) << 24) +
                  (((uint64_t)area.height() & 0xfff) << 36) +
                  (((uint64_t)type & 0xfff) << 48);

    if (m_cachedVBOS.contains(ref))
    {
        m_vboExpiry.removeOne(ref);
        m_vboExpiry.append(ref);
        m_cachedVBOS.value(ref)->bind();
        return;
    }

    GLfloat *vertices = GetCachedVertices(type, area);
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
    return;
}

void MythRenderOpenGL::ExpireVBOS(uint max)
{
    while ((uint)m_vboExpiry.size() > max)
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

uint MythRenderOpenGL::GetBufferSize(QSize Size, QOpenGLTexture::PixelFormat Format, QOpenGLTexture::PixelType Type)
{
    uint bytes = 0;
    uint bpp = 0;;

    switch (Format)
    {
        case QOpenGLTexture::BGRA:
        case QOpenGLTexture::RGBA: bpp = 4; break;
        case QOpenGLTexture::BGR:
        case QOpenGLTexture::RGB:  bpp = 3; break;
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
    if (fx.hzoom != 1.0f || fx.vzoom != 1.0f || fx.angle != 0.0f)
    {
        newtop.translate(center.x(), center.y());
        newtop.scale(fx.hzoom, fx.vzoom);
        newtop.rotate(fx.angle, 0, 0, 1);
        newtop.translate(-center.x(), -center.y());
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
    QOpenGLShaderProgram *program = new QOpenGLShaderProgram();
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

void MythRenderOpenGL::SetShaderProgramParams(QOpenGLShaderProgram *Program, const QMatrix4x4 &Value, const char *Uniform)
{
    OpenGLLocker locker(this);
    if (!Uniform || !EnableShaderProgram(Program))
        return;

    // Uniform cacheing
    QString tag = QStringLiteral("%1-%2").arg(Program->programId()).arg(Uniform);
    QHash<QString,QMatrix4x4>::iterator it = m_cachedMatrixUniforms.find(tag);
    if (it == m_cachedMatrixUniforms.end())
        m_cachedMatrixUniforms.insert(tag, Value);
    else if (!qFuzzyCompare(Value, it.value()))
        it.value() = Value;
    else
        return;

    Program->setUniformValue(Uniform, Value);
}

bool MythRenderOpenGL::CreateDefaultShaders(void)
{
    m_defaultPrograms[kShaderSimple]     = CreateShaderProgram(kSimpleVertexShader, kSimpleFragmentShader);
    m_defaultPrograms[kShaderDefault]    = CreateShaderProgram(kDefaultVertexShader, kDefaultFragmentShader);
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
