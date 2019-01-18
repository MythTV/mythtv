// Std
#include <algorithm>
using std::min;

// Qt
#include <QLibrary>
#include <QPainter>
#include <QWindow>
#include <QGLFormat>
#include <QGuiApplication>

// MythTV
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

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
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
    if (Message.source() != QOpenGLDebugMessage::ApplicationSource)
        LOG(VB_GPU, LOG_INFO, LOC + Message.message());
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
    bool buffer_procs = m_glMapBuffer && m_glUnmapBuffer;
    if (!isOpenGLES() && hasExtension("GL_ARB_pixel_buffer_object") && buffer_procs)
        m_extraFeatures += kGLExtPBufObj;

    // Buffers are available by default (GL and GLES).
    // Buffer mapping is available by extension
    if ((isOpenGLES() && hasExtension("GL_OES_mapbuffer") && buffer_procs) ||
        (hasExtension("GL_ARB_vertex_buffer_object") && buffer_procs))
        m_extraFeatures += kGLBufferMap;

    // TODO combine fence functionality
    // NVidia fence
    if(hasExtension("GL_NV_fence") && m_glGenFencesNV && m_glDeleteFencesNV &&
        m_glSetFenceNV  && m_glFinishFenceNV)
        m_extraFeatures += kGLNVFence;

    // Apple fence
    if(hasExtension("GL_APPLE_fence") && m_glGenFencesAPPLE && m_glDeleteFencesAPPLE &&
        m_glSetFenceAPPLE  && m_glFinishFenceAPPLE)
        m_extraFeatures += kGLAppleFence;

    // MESA YCbCr
    if (hasExtension("GL_MESA_ycbcr_texture"))
        m_extraFeatures += kGLMesaYCbCr;

    // Apple YCbCr
    if (hasExtension("GL_APPLE_ycbcr_422"))
        m_extraFeatures += kGLAppleYCbCr;

    // Mipmapping - is this available by default on ES?
    if (hasExtension("GL_SGIS_generate_mipmap"))
        m_extraFeatures += kGLMipMaps;

    m_extraFeatures += (m_features & NPOTTextures) ? kGLFeatNPOT : kGLFeatNone;
    m_extraFeatures += (m_features & Framebuffers) ? kGLExtFBufObj : kGLFeatNone;
    m_extraFeatures += (m_features & Shaders) ? kGLSL : kGLFeatNone;

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
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Direct rendering     : %1").arg(GLYesNo(IsDirectRendering())));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt platform          : %1").arg(qApp->platformName()));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL format     : %1").arg(qtglversion));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt OpenGL surface    : %1").arg(qtglsurface));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture size     : %1").arg(m_max_tex_size));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture units    : %1").arg(m_max_units));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Shaders              : %1").arg(GLYesNo(m_features & Shaders)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("NPOT textures        : %1").arg(GLYesNo(m_features & NPOTTextures)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Multitexturing       : %1").arg(GLYesNo(m_features & Multitexture)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("RGBA16 textures      : %1").arg(GLYesNo(m_extraFeatures & kGLExtRGBA16)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Buffer mapping       : %1").arg(GLYesNo(m_features & Buffers)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Framebuffer objects  : %1").arg(GLYesNo(m_features & Framebuffers)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Framebuffer blitting : %1").arg(GLYesNo(QOpenGLFramebufferObject::hasOpenGLFramebufferBlit())));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Pixelbuffer objects  : %1").arg(GLYesNo(m_extraFeatures & kGLExtPBufObj)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("YCbCr textures       : %1")
        .arg(GLYesNo((m_extraFeatures & kGLMesaYCbCr) || (m_extraFeatures & kGLAppleYCbCr))));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Fence                : %1")
        .arg(GLYesNo((m_extraFeatures & kGLAppleFence) || (m_extraFeatures & kGLNVFence))));

    // warnings
    if (m_max_units < 3)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Warning: Insufficient texture units for some features.");
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
    else if (!IsDirectRendering())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL is using software rendering.");
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

bool MythRenderOpenGL::IsDirectRendering() const
{
    return QGLFormat::fromSurfaceFormat(format()).directRendering();
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

void* MythRenderOpenGL::GetTextureBuffer(uint tex, bool create_buffer)
{
    if (!m_textures.contains(tex))
        return nullptr;

    makeCurrent(); // associated doneCurrent() in UpdateTexture
    EnableTextures(tex);
    glBindTexture(m_textures[tex].m_type, tex);
    if (m_textures[tex].m_pbo)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_textures[tex].m_pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, m_textures[tex].m_data_size, nullptr, GL_STREAM_DRAW);
        return m_glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    }
    else if (!create_buffer)
    {
        return nullptr;
    }

    if (m_textures[tex].m_data)
        return m_textures[tex].m_data;

    unsigned char *scratch = new unsigned char[m_textures[tex].m_data_size];
    if (scratch)
    {
        memset(scratch, 0, m_textures[tex].m_data_size);
        m_textures[tex].m_data = scratch;
    }
    return scratch;
}

void MythRenderOpenGL::UpdateTexture(uint tex, void *buf)
{
    // N.B. GetTextureBuffer must be called first
    if (!m_textures.contains(tex))
        return;

    QSize size = m_textures[tex].m_act_size;

    if (m_textures[tex].m_pbo)
    {
        m_glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        glTexSubImage2D(m_textures[tex].m_type, 0, 0, 0, size.width(),
                        size.height(), m_textures[tex].m_data_fmt,
                        m_textures[tex].m_data_type, nullptr);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    else
    {
        glTexSubImage2D(m_textures[tex].m_type, 0, 0, 0, size.width(),
                        size.height(), m_textures[tex].m_data_fmt,
                        m_textures[tex].m_data_type, buf);
    }

    doneCurrent();
}

uint MythRenderOpenGL::CreateTexture(QSize act_size, bool use_pbo,
                                     uint type, uint data_type,
                                     uint data_fmt, uint internal_fmt,
                                     uint filter, uint wrap)
{
    // OpenGLES requires same formats for internal and external.
    if (isOpenGLES())
        internal_fmt = data_fmt;

    if (!type)
        type = m_default_texture_type;

    QSize tot_size = GetTextureSize(act_size);

    makeCurrent();

    EnableTextures(0, type);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(type, tex);

    if (tex)
    {
        MythGLTexture texture;
        texture.m_type         = type;
        texture.m_data_type    = data_type;
        texture.m_data_fmt     = data_fmt;
        texture.m_internal_fmt = internal_fmt;
        texture.m_size         = tot_size;
        texture.m_act_size     = act_size;
        texture.m_data_size    = GetBufferSize(act_size, data_fmt, data_type);
        m_textures.insert(tex, texture);

        if (ClearTexture(tex) && m_textures[tex].m_data_size)
        {
            SetTextureFilters(tex, filter, wrap);
            m_textures[tex].m_vbo = CreateVBO();
            if (use_pbo)
                m_textures[tex].m_pbo = CreatePBO(tex);
        }
        else
        {
            DeleteTexture(tex);
            tex = 0;
        }
    }

    Flush(true);
    doneCurrent();

    return tex;
}

uint MythRenderOpenGL::CreateHelperTexture(void)
{
    makeCurrent();

    uint width = m_max_tex_size;
    uint tmp_tex = CreateTexture(QSize(width, 1), false,
                                 GL_TEXTURE_2D, GL_FLOAT, GL_RGBA,
                                 GL_RGBA16, GL_NEAREST, GL_REPEAT);

    if (!tmp_tex)
    {
        DeleteTexture(tmp_tex);
        return 0;
    }

    float *buf = nullptr;
    buf = new float[m_textures[tmp_tex].m_data_size];
    float *ref = buf;

    for (uint i = 0; i < width; i++)
    {
        float x = (((float)i) + 0.5f) / (float)width;
        StoreBicubicWeights(x, ref);
        ref += 4;
    }
    StoreBicubicWeights(0, buf);
    StoreBicubicWeights(1, &buf[(width - 1) << 2]);

    EnableTextures(tmp_tex);
    glBindTexture(m_textures[tmp_tex].m_type, tmp_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, width, 1, 0, GL_RGBA, GL_FLOAT, buf);

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Created bicubic helper texture (%1 samples)") .arg(width));
    delete [] buf;
    doneCurrent();
    return tmp_tex;
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

int MythRenderOpenGL::GetTextureDataSize(uint tex)
{
    if (!m_textures.contains(tex))
        return 0;
    return m_textures[tex].m_data_size;
}

void MythRenderOpenGL::SetTextureFilters(uint tex, uint filt, uint wrap)
{
    if (!m_textures.contains(tex))
        return;

    if (filt == GL_LINEAR_MIPMAP_LINEAR && !(m_extraFeaturesUsed & kGLMipMaps))
        filt = GL_LINEAR;

    makeCurrent();
    EnableTextures(tex);
    m_textures[tex].m_filter = filt;
    m_textures[tex].m_wrap   = wrap;
    uint type = m_textures[tex].m_type;
    glBindTexture(type, tex);
    uint mag_filt = filt;
    if (filt == GL_LINEAR_MIPMAP_LINEAR)
    {
        mag_filt = GL_LINEAR;
#ifdef GL_GENERATE_MIPMAP_HINT_SGIS
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
#endif
#ifdef GL_GENERATE_MIPMAP_SGIS
        glTexParameteri(type, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
#endif
    }
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, mag_filt);
    glTexParameteri(type, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(type, GL_TEXTURE_WRAP_T, wrap);
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

void MythRenderOpenGL::EnableTextures(uint tex, uint tex_type)
{
    if (isOpenGLES())
        return;
    if (tex && !m_textures.contains(tex))
        return;

    makeCurrent();
    int type = tex ? m_textures[tex].m_type : tex_type;
    if (type != m_active_tex_type)
    {
        if (m_active_tex_type)
            glDisable(m_active_tex_type);
        glEnable(type);
        m_active_tex_type = type;
    }
    doneCurrent();
}

void MythRenderOpenGL::DisableTextures(void)
{
    if (isOpenGLES())
        return;
    if (!m_active_tex_type)
        return;
    makeCurrent();
    glDisable(m_active_tex_type);
    m_active_tex_type = 0;
    doneCurrent();
}

void MythRenderOpenGL::DeleteTexture(uint tex)
{
    if (!m_textures.contains(tex))
        return;

    makeCurrent();

    if (!m_textures[tex].m_external)
    {
        GLuint gltex = tex;
        glDeleteTextures(1, &gltex);
    }
    if (m_textures[tex].m_data)
        delete m_textures[tex].m_data;
    if (m_textures[tex].m_pbo)
        glDeleteBuffers(1, &(m_textures[tex].m_pbo));
    if (m_textures[tex].m_vbo)
        glDeleteBuffers(1, &(m_textures[tex].m_vbo));
    m_textures.remove(tex);

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
        LOG(VB_GENERAL, LOG_INFO, QString("Created FBO: %1x%2 id %3 texture %4 attach %5 bound %6")
            .arg(framebuffer->width()).arg(framebuffer->height())
            .arg(framebuffer->handle()).arg(framebuffer->texture())
            .arg(framebuffer->attachment()).arg(framebuffer->isBound()));
        if (framebuffer->isBound())
        {
            m_activeFramebuffer = framebuffer;
            BindFramebuffer(nullptr);
        }

        // Add the texture to our known list. This is probably temporary code
        // but we need a VBO to render the texture. We should not be attempting
        // to create any PBOs or texture memory so data size should not be
        // critical.
        MythGLTexture texture(true /*external - don't delete!*/);
        GLuint textureid    = framebuffer->texture();
        texture.m_type      = framebuffer->format().textureTarget();
        texture.m_size      = texture.m_act_size = framebuffer->size();
        texture.m_data_type = GL_UNSIGNED_BYTE; // NB Guess!
        texture.m_data_fmt  = texture.m_internal_fmt = framebuffer->format().internalTextureFormat();
        texture.m_data_size = GetBufferSize(texture.m_size, texture.m_data_fmt, texture.m_data_type);
        texture.m_vbo       = CreateVBO();
        m_textures.insert(textureid, texture);
        Flush(true);
        m_framebuffers.append(framebuffer);
        return framebuffer;
    }
    LOG(VB_GENERAL, LOG_ERR, "Failed to create framebuffer object");
    delete framebuffer;
    return nullptr;
}

void MythRenderOpenGL::DeleteFramebuffer(QOpenGLFramebufferObject *Framebuffer)
{
    if (!m_framebuffers.contains(Framebuffer))
        return;

    makeCurrent();
    m_framebuffers.removeAll(Framebuffer);
    DeleteTexture(Framebuffer->texture());
    delete Framebuffer;
    doneCurrent();
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

void MythRenderOpenGL::DrawBitmap(uint tex, QOpenGLFramebufferObject *target,
                                  const QRect *src, const QRect *dst,
                                  QOpenGLShaderProgram *Program, int alpha,
                                  int red, int green, int blue)
{
    if (!tex || !m_textures.contains(tex))
        return;

    makeCurrent();
    BindFramebuffer(target);
    DrawBitmapPriv(tex, src, dst, Program, alpha, red, green, blue);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(uint *textures, uint texture_count,
                                  QOpenGLFramebufferObject *target,
                                  const QRectF *src, const QRectF *dst,
                                  QOpenGLShaderProgram *Program)
{
    if (!textures || !texture_count)
        return;

    makeCurrent();
    BindFramebuffer(target);
    DrawBitmapPriv(textures, texture_count, src, dst, Program);
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

void MythRenderOpenGL::DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                                      QOpenGLShaderProgram *Program, int alpha,
                                     int red, int green, int blue)
{
    if (Program && !m_programs.contains(Program))
        Program = nullptr;
    if (Program == nullptr)
        Program = m_defaultPrograms[kShaderDefault];

    SetShaderProgramParams(Program, m_projection, "u_projection");
    SetShaderProgramParams(Program, m_transforms.top(), "u_transform");
    SetBlend(true);

    EnableTextures(tex);

    Program->setUniformValue("s_texture0", 0);
    ActiveTexture(GL_TEXTURE0);
    glBindTexture(m_textures[tex].m_type, tex);

    glBindBuffer(GL_ARRAY_BUFFER, m_textures[tex].m_vbo);
    UpdateTextureVertices(tex, src, dst);
    if (m_extraFeaturesUsed & kGLBufferMap)
    {
        glBufferData(GL_ARRAY_BUFFER, kVertexSize, nullptr, GL_STREAM_DRAW);
        void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        if (target)
            memcpy(target, m_textures[tex].m_vertex_data, kVertexSize);
        m_glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    else
    {
        glBufferData(GL_ARRAY_BUFFER, kVertexSize, m_textures[tex].m_vertex_data, GL_STREAM_DRAW);
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
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MythRenderOpenGL::DrawBitmapPriv(uint *textures, uint texture_count,
                                      const QRectF *src, const QRectF *dst,
                                      QOpenGLShaderProgram *Program)
{
    if (Program && !m_programs.contains(Program))
        Program = nullptr;
    if (Program == nullptr)
        Program = m_defaultPrograms[kShaderDefault];

    uint first = textures[0];

    SetShaderProgramParams(Program, m_projection, "u_projection");
    SetShaderProgramParams(Program, m_transforms.top(), "u_transform");
    SetBlend(false);

    EnableTextures(first);
    uint active_tex = 0;
    for (uint i = 0; i < texture_count; i++)
    {
        if (m_textures.contains(textures[i]))
        {
            QString uniform = QString("s_texture%1").arg(active_tex);
            Program->setUniformValue(qPrintable(uniform), active_tex);
            ActiveTexture(GL_TEXTURE0 + active_tex++);
            glBindTexture(m_textures[textures[i]].m_type, textures[i]);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_textures[first].m_vbo);
    UpdateTextureVertices(first, src, dst);
    if (m_extraFeaturesUsed & kGLBufferMap)
    {
        glBufferData(GL_ARRAY_BUFFER, kVertexSize, nullptr, GL_STREAM_DRAW);
        void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        if (target)
            memcpy(target, m_textures[first].m_vertex_data, kVertexSize);
        m_glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    else
    {
        glBufferData(GL_ARRAY_BUFFER, kVertexSize, m_textures[first].m_vertex_data, GL_STREAM_DRAW);
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
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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

    SetBlend(true);
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
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glDisableVertexAttribArray(VERTEX_INDEX);
}

void MythRenderOpenGL::Init2DState(void)
{
    SetBlend(false);
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
    m_glMapBuffer = (MYTH_GLMAPBUFFERPROC)GetProcAddress("glMapBuffer");
    m_glUnmapBuffer = (MYTH_GLUNMAPBUFFERPROC)GetProcAddress("glUnmapBuffer");
    m_glGenFencesNV = (MYTH_GLGENFENCESNVPROC)GetProcAddress("glGenFencesNV");
    m_glDeleteFencesNV = (MYTH_GLDELETEFENCESNVPROC)GetProcAddress("glDeleteFencesNV");
    m_glSetFenceNV = (MYTH_GLSETFENCENVPROC)GetProcAddress("glSetFenceNV");
    m_glFinishFenceNV = (MYTH_GLFINISHFENCENVPROC)GetProcAddress("glFinishFenceNV");
    m_glGenFencesAPPLE = (MYTH_GLGENFENCESAPPLEPROC)GetProcAddress("glGenFencesAPPLE");
    m_glDeleteFencesAPPLE = (MYTH_GLDELETEFENCESAPPLEPROC)GetProcAddress("glDeleteFencesAPPLE");
    m_glSetFenceAPPLE = (MYTH_GLSETFENCEAPPLEPROC)GetProcAddress("glSetFenceAPPLE");
    m_glFinishFenceAPPLE = (MYTH_GLFINISHFENCEAPPLEPROC)GetProcAddress("glFinishFenceAPPLE");
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
    m_default_texture_type = GL_TEXTURE_2D;

    m_viewport        = QRect();
    m_active_tex      = 0;
    m_active_tex_type = 0;
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
    m_map.clear();
}

void MythRenderOpenGL::ResetProcs(void)
{
    m_glMapBuffer = nullptr;
    m_glUnmapBuffer = nullptr;
    m_glGenFencesNV = nullptr;
    m_glDeleteFencesNV = nullptr;
    m_glSetFenceNV = nullptr;
    m_glFinishFenceNV = nullptr;
    m_glGenFencesAPPLE = nullptr;
    m_glDeleteFencesAPPLE = nullptr;
    m_glSetFenceAPPLE = nullptr;
    m_glFinishFenceAPPLE = nullptr;
}

uint MythRenderOpenGL::CreatePBO(uint tex)
{
    if (!(m_extraFeaturesUsed & kGLExtPBufObj))
        return 0;

    if (!m_textures.contains(tex))
        return 0;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    if (glCheck())
    {
        // looks like we dont support PBOs so dont bother doing the rest
        // and stop using it in the future
        LOG(VB_GENERAL, LOG_INFO, LOC + "Pixel Buffer Objects unusable, disabling");
        m_extraFeatures &= ~kGLExtPBufObj;
        m_extraFeaturesUsed &= ~kGLExtPBufObj;
        return 0;
    }
    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 m_textures[tex].m_size.width(),
                 m_textures[tex].m_size.height(), 0,
                 m_textures[tex].m_data_fmt, m_textures[tex].m_data_type, nullptr);

    GLuint tmp_pbo;
    glGenBuffers(1, &tmp_pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    Flush(true);
    return tmp_pbo;
}

uint MythRenderOpenGL::CreateVBO(void)
{
    GLuint tmp_vbo;
    glGenBuffers(1, &tmp_vbo);
    return tmp_vbo;
}

void MythRenderOpenGL::DeleteOpenGLResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting OpenGL Resources");
    DeleteFramebuffers();
    DeleteTextures();
    DeleteDefaultShaders();
    DeleteShaderPrograms();
    Flush(true);

    if (m_fence)
    {
        if (m_extraFeatures & kGLAppleFence)
            m_glDeleteFencesAPPLE(1, &m_fence);
        else if(m_extraFeatures & kGLNVFence)
            m_glDeleteFencesNV(1, &m_fence);
        m_fence = 0;
    }

    Flush(false);

    ExpireVertices();
    ExpireVBOS();

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

void MythRenderOpenGL::DeleteTextures(void)
{
    QHash<GLuint, MythGLTexture>::iterator it;
    for (it = m_textures.begin(); it !=m_textures.end(); ++it)
    {
        if (!it.value().m_external)
            glDeleteTextures(1, &(it.key()));
        if (it.value().m_data)
            delete it.value().m_data;
        if (it.value().m_pbo)
            glDeleteBuffers(1, &(it.value().m_pbo));
        if (it.value().m_vbo)
            glDeleteBuffers(1, &(it.value().m_vbo));
    }
    m_textures.clear();
    Flush(true);
}

void MythRenderOpenGL::DeleteFramebuffers(void)
{
    foreach (QOpenGLFramebufferObject *framebuffer, m_framebuffers)
    {
        DeleteTexture(framebuffer->texture());
        delete framebuffer;
    }
    m_framebuffers.clear();
    Flush(true);
}

bool MythRenderOpenGL::UpdateTextureVertices(uint tex, const QRect *src,
                                             const QRect *dst)
{
    if (!m_textures.contains(tex))
        return false;

    GLfloat *data = m_textures[tex].m_vertex_data;
    QSize    size = m_textures[tex].m_size;
    if (size.isEmpty())
        return false;

    int width  = min(src->width(),  size.width());
    int height = min(src->height(), size.height());

    data[0 + TEX_OFFSET] = src->left() / (float)size.width();
    data[1 + TEX_OFFSET] = (src->top() + height) / (float)size.height();
    data[6 + TEX_OFFSET] = (src->left() + width) / (float)size.width();
    data[7 + TEX_OFFSET] = src->top() / (float)size.height();

    data[2 + TEX_OFFSET] = data[0 + TEX_OFFSET];
    data[3 + TEX_OFFSET] = data[7 + TEX_OFFSET];
    data[4 + TEX_OFFSET] = data[6 + TEX_OFFSET];
    data[5 + TEX_OFFSET] = data[1 + TEX_OFFSET];

    data[2] = data[0] = dst->left();
    data[5] = data[1] = dst->top();
    data[4] = data[6] = dst->left() + min(width, dst->width());
    data[3] = data[7] = dst->top()  + min(height, dst->height());
    return true;
}

bool MythRenderOpenGL::UpdateTextureVertices(uint tex, const QRectF *src,
                                             const QRectF *dst)
{
    if (!m_textures.contains(tex))
        return false;

    GLfloat *data = m_textures[tex].m_vertex_data;
    QSize    size = m_textures[tex].m_size;
    if (size.isEmpty())
        return false;

    data[0 + TEX_OFFSET] = src->left() / (qreal)size.width();
    data[1 + TEX_OFFSET] = (src->top() + src->height()) / (qreal)size.height();
    data[6 + TEX_OFFSET] = (src->left() + src->width()) / (qreal)size.width();
    data[7 + TEX_OFFSET] = src->top() / (qreal)size.height();

    data[2 + TEX_OFFSET] = data[0 + TEX_OFFSET];
    data[3 + TEX_OFFSET] = data[7 + TEX_OFFSET];
    data[4 + TEX_OFFSET] = data[6 + TEX_OFFSET];
    data[5 + TEX_OFFSET] = data[1 + TEX_OFFSET];

    data[2] = data[0] = dst->left();
    data[5] = data[1] = dst->top();
    data[4] = data[6] = dst->left() + dst->width();
    data[3] = data[7] = dst->top() + dst->height();
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
    }
    else
    {
        GLfloat *vertices = GetCachedVertices(type, area);
        GLuint vbo = CreateVBO();
        m_cachedVBOS.insert(ref, vbo);
        m_vboExpiry.append(ref);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        if (m_extraFeaturesUsed & kGLBufferMap)
        {
            glBufferData(GL_ARRAY_BUFFER, kTextureOffset, nullptr, GL_STREAM_DRAW);
            void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            if (target)
                memcpy(target, vertices, kTextureOffset);
            m_glUnmapBuffer(GL_ARRAY_BUFFER);
        }
        else
        {
            glBufferData(GL_ARRAY_BUFFER, kTextureOffset, vertices, GL_STREAM_DRAW);
        }
        ExpireVBOS(MAX_VERTEX_CACHE);
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_cachedVBOS.value(ref));
}

void MythRenderOpenGL::ExpireVBOS(uint max)
{
    while ((uint)m_vboExpiry.size() > max)
    {
        uint64_t ref = m_vboExpiry.first();
        m_vboExpiry.removeFirst();
        if (m_cachedVBOS.contains(ref))
        {
            GLuint vbo = m_cachedVBOS.value(ref);
            glDeleteBuffers(1, &vbo);
            m_cachedVBOS.remove(ref);
        }
    }
}

bool MythRenderOpenGL::ClearTexture(uint tex)
{
    if (!m_textures.contains(tex))
        return false;

    QSize size = m_textures[tex].m_size;
    uint tmp_size = GetBufferSize(size, m_textures[tex].m_data_fmt,
                                  m_textures[tex].m_data_type);

    if (!tmp_size)
        return false;

    unsigned char *scratch = new unsigned char[tmp_size];

    if (!scratch)
        return false;

    memset(scratch, 0, tmp_size);

    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 size.width(), size.height(), 0, m_textures[tex].m_data_fmt,
                 m_textures[tex].m_data_type, scratch);
    delete [] scratch;

    if (glCheck())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("glTexImage size %1 failed")
            .arg(tmp_size));
        return false;
    }

    return true;
}

uint MythRenderOpenGL::GetBufferSize(QSize size, uint fmt, uint type)
{
    uint bytes;
    uint bpp;

    if (fmt ==GL_RGBA)
    {
        bpp = 4;
    }
    else if (fmt == GL_RGB)
    {
        bpp = 3;
    }
    else if (fmt == GL_YCBCR_MESA || fmt == GL_YCBCR_422_APPLE)
    {
        bpp = 2;
    }
    else if (fmt == GL_LUMINANCE || fmt == GL_ALPHA)
    {
        bpp = 1;
    }
    else
    {
        bpp =0;
    }

    switch (type)
    {
        case GL_UNSIGNED_BYTE:
            bytes = sizeof(GLubyte);
            break;
#ifdef GL_UNSIGNED_SHORT_8_8_MESA
        case GL_UNSIGNED_SHORT_8_8_MESA:
            bytes = sizeof(GLushort);
            break;
#endif
        case GL_FLOAT:
            bytes = sizeof(GLfloat);
            break;
        default:
            bytes = 0;
    }

    if (!bpp || !bytes || size.width() < 1 || size.height() < 1)
        return 0;

    return size.width() * size.height() * bpp * bytes;
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
    m_programs.append(program);
    return program;
}

void MythRenderOpenGL::DeleteShaderProgram(QOpenGLShaderProgram *Program)
{
    if (!m_programs.contains(Program))
        return;
    {
        OpenGLLocker locker(this);
        m_programs.removeAll(Program);
        delete Program;
        m_map.clear();
    }
}

void MythRenderOpenGL::DeleteShaderPrograms(void)
{
    OpenGLLocker locker(this);
    foreach (QOpenGLShaderProgram *program, m_programs)
        delete program;
    m_programs.clear();
    m_map.clear();
}

bool MythRenderOpenGL::EnableShaderProgram(QOpenGLShaderProgram* Program)
{
    if (!Program || !m_programs.contains(Program))
        return false;
    if (m_activeProgram == Program)
        return true;
    {
        OpenGLLocker locker(this);
        Program->bind();
        m_activeProgram = Program;
        return true;
    }
}

void MythRenderOpenGL::SetShaderProgramParams(QOpenGLShaderProgram *Program, const QMatrix4x4 &Value, const char *Uniform)
{
    OpenGLLocker locker(this);
    if (!Program || !Uniform || !EnableShaderProgram(Program))
        return;

    // Uniform cacheing
    QString tag = QStringLiteral("%1-%2").arg(Program->programId()).arg(Uniform);
    map_t::iterator it = m_map.find(tag);
    if (it == m_map.end())
        m_map.insert(tag, Value);
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

/*! \brief GLMatrix4x4 is a helper class to convert between QT and GT 4x4 matrices.
 *
 * Actually the conversion is a noop according to the QT 5 documentation.
 * From http://doc.qt.io/qt-5/qmatrix4x4.html#details :
 *
 * Internally the data is stored as column-major format, so as to be
 * optimal for passing to OpenGL functions, which expect column-major
 * data.
 *
 * When using these functions be aware that they return data in
 * column-major format: data(), constData()
 */
GLMatrix4x4::GLMatrix4x4(const QMatrix4x4 &m)
{
    // Convert from Qt's row-major to GL's column-major order
    for (int c = 0, i = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            m_v[i++] = m(r, c);
}

GLMatrix4x4::GLMatrix4x4(const GLfloat v[16])
{
    for (int i = 0; i < 16; ++i)
        m_v[i] = v[i];
}

GLMatrix4x4::operator const GLfloat *() const
{
    return m_v;
}

GLMatrix4x4::operator QMatrix4x4() const
{
    // Convert from GL's column-major order to Qt's row-major
    QMatrix4x4 m;
    for (int c = 0, i = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            m(r, c) = m_v[i++];
    return m;
}

