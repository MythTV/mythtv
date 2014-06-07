#include <QLibrary>
#include <algorithm>
using namespace std;

#include "mythlogging.h"
#include "mythuitype.h"
#include "mythrender_opengl.h"
#include "mythxdisplay.h"

#define LOC QString("OpenGL: ")

#include "mythrender_opengl2.h"
#ifdef USING_OPENGLES
#include "mythrender_opengl2es.h"
#else
#include "mythrender_opengl1.h"
#endif

#ifdef USING_X11
#include "util-nvctrl.h"
#endif

static const GLuint kTextureOffset = 8 * sizeof(GLfloat);

static inline int __glCheck__(const QString &loc, const char* fileName, int n)
{
    int error = glGetError();
    if (error)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: %2 @ %3, %4")
            .arg(loc).arg(error).arg(fileName).arg(n));
    }
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

MythRenderOpenGL* MythRenderOpenGL::Create(const QString &painter,
                                           QPaintDevice* device)
{
    QGLFormat format;
    format.setDepth(false);

    bool setswapinterval = false;
    int synctovblank = -1;

#ifdef USING_X11
    synctovblank = CheckNVOpenGLSyncToVBlank();
#endif

    if (synctovblank < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Could not determine whether Sync "
                                           "to VBlank is enabled.");
    }
    else if (synctovblank == 0)
    {
        // currently only Linux NVidia is supported and there is no way of
        // forcing sync to vblank after the app has started. util-nvctrl will
        // warn the user and offer advice on settings.
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Sync to VBlank is enabled (good!)");
    }

#if defined(Q_OS_MAC)
    LOG(VB_GENERAL, LOG_INFO, LOC + "Forcing swap interval for OS X.");
    setswapinterval = true;
#endif

    if (setswapinterval)
        format.setSwapInterval(1);

    // Check OpenGL version supported
    QGLWidget *dummy = new QGLWidget;
    dummy->makeCurrent();
    QGLFormat qglFormat = dummy->format();
    delete dummy;

#ifdef USING_OPENGLES
    if (!(qglFormat.openGLVersionFlags() & QGLFormat::OpenGL_ES_Version_2_0))
    {
        LOG(VB_GENERAL, LOG_WARNING,
            "Using OpenGL ES 2.0 render, however OpenGL ES 2.0 "
            "version not supported");
    }
    if (device)
        return new MythRenderOpenGL2ES(format, device);
    return new MythRenderOpenGL2ES(format);
#else
    if ((qglFormat.openGLVersionFlags() & QGLFormat::OpenGL_Version_2_0) &&
        (painter.contains(OPENGL2_PAINTER) || painter.contains(AUTO_PAINTER) ||
         painter.isEmpty()))
    {
        LOG(VB_GENERAL, LOG_INFO, "Trying the OpenGL 2.0 render");
        format.setVersion(2,0);
        if (device)
            return new MythRenderOpenGL2(format, device);
        return new MythRenderOpenGL2(format);
    }

    if (!(qglFormat.openGLVersionFlags() & QGLFormat::OpenGL_Version_1_2))
    {
        LOG(VB_GENERAL, LOG_WARNING, "OpenGL 1.2 not supported, get new hardware!");
        return NULL;
    }

    LOG(VB_GENERAL, LOG_INFO, "Trying the OpenGL 1.2 render");
    format.setVersion(1,3);
    if (device)
        return new MythRenderOpenGL1(format, device);
    return new MythRenderOpenGL1(format);

#endif
}

MythRenderOpenGL::MythRenderOpenGL(const QGLFormat& format, QPaintDevice* device,
                                   RenderType type)
  : QGLContext(format, device), MythRender(type), m_lock(QMutex::Recursive)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL::MythRenderOpenGL(const QGLFormat& format, RenderType type)
  : QGLContext(format), MythRender(type), m_lock(QMutex::Recursive)
{
    ResetVars();
    ResetProcs();
}

MythRenderOpenGL::~MythRenderOpenGL()
{
}

void MythRenderOpenGL::Init(void)
{
    OpenGLLocker locker(this);
    InitProcs();
    Init2DState();
    InitFeatures();

    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialised MythRenderOpenGL");
}

bool MythRenderOpenGL::IsRecommendedRenderer(void)
{
    bool recommended = true;
    OpenGLLocker locker(this);
    QString renderer = (const char*) glGetString(GL_RENDERER);
    if (!(this->format().directRendering()))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "OpenGL is using software rendering.");
        recommended = false;
    }
    else if (renderer.contains("Software Rasterizer", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "OpenGL is using software rasterizer.");
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

void MythRenderOpenGL::makeCurrent()
{
    m_lock.lock();
    if (this != MythRenderOpenGL::currentContext())
        QGLContext::makeCurrent();
    m_lock_level++;
}

void MythRenderOpenGL::doneCurrent()
{
    m_lock_level--;
    if (m_lock_level == 0)
        QGLContext::doneCurrent();
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
    QWidget *parent = (QWidget*)this->device();
    if (parent)
        parent->setGeometry(rect);
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
    makeCurrent();

    if ((m_exts_used & kGLAppleFence) &&
        (m_fence && use_fence))
    {
        m_glSetFenceAPPLE(m_fence);
        m_glFinishFenceAPPLE(m_fence);
    }
    else if ((m_exts_used & kGLNVFence) &&
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
    makeCurrent();
    if (m_exts_used & kGLAppleFence)
    {
        m_glGenFencesAPPLE(1, &m_fence);
        if (m_fence)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Using GL_APPLE_fence");
    }
    else if (m_exts_used & kGLNVFence)
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
        return NULL;

    makeCurrent(); // associated doneCurrent() in UpdateTexture

    EnableTextures(tex);
    glBindTexture(m_textures[tex].m_type, tex);

    if (!create_buffer)
        return NULL;

    if (m_textures[tex].m_pbo)
    {
        m_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_textures[tex].m_pbo);
        m_glBufferData(GL_PIXEL_UNPACK_BUFFER,
                             m_textures[tex].m_data_size, NULL, GL_STREAM_DRAW);
        return m_glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
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
                        m_textures[tex].m_data_type, 0);
        m_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    else
    {
        glTexSubImage2D(m_textures[tex].m_type, 0, 0, 0, size.width(),
                        size.height(), m_textures[tex].m_data_fmt,
                        m_textures[tex].m_data_type, buf);
    }

    doneCurrent();
}

int MythRenderOpenGL::GetTextureType(bool &rect)
{
    static bool rects = true;
    static bool check = true;
    if (check)
    {
        check = false;
        rects = !getenv("OPENGL_NORECT");
        if (!rects)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling NPOT textures.");
    }

    int ret = GL_TEXTURE_2D;

    if (m_extensions.contains("GL_NV_texture_rectangle") && rects)
        ret = GL_TEXTURE_RECTANGLE_NV;
    else if (m_extensions.contains("GL_ARB_texture_rectangle") && rects)
        ret = GL_TEXTURE_RECTANGLE_ARB;
    else if (m_extensions.contains("GL_EXT_texture_rectangle") && rects)
        ret = GL_TEXTURE_RECTANGLE_EXT;

    rect = (ret != GL_TEXTURE_2D);
    return ret;
}

bool MythRenderOpenGL::IsRectTexture(uint type)
{
    if (type == GL_TEXTURE_RECTANGLE_NV || type == GL_TEXTURE_RECTANGLE_ARB ||
        type == GL_TEXTURE_RECTANGLE_EXT)
        return true;
    return false;
}

uint MythRenderOpenGL::CreateTexture(QSize act_size, bool use_pbo,
                                     uint type, uint data_type,
                                     uint data_fmt, uint internal_fmt,
                                     uint filter, uint wrap)
{
    if (!type)
        type = m_default_texture_type;

    QSize tot_size = GetTextureSize(type, act_size);

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
            if (use_pbo)
                m_textures[tex].m_pbo = CreatePBO(tex);
            if (m_exts_used & kGLExtVBO)
                m_textures[tex].m_vbo = CreateVBO();
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

QSize MythRenderOpenGL::GetTextureSize(uint type, const QSize &size)
{
    if (IsRectTexture(type))
        return size;

    int w = 64;
    int h = 64;

    while (w < size.width())
    {
        w *= 2;
    }

    while (h < size.height())
    {
        h *= 2;
    }

    return QSize(w, h);
}

QSize MythRenderOpenGL::GetTextureSize(uint tex)
{
    if (!m_textures.contains(tex))
        return QSize();
    return m_textures[tex].m_size;
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

    bool mipmaps = (m_exts_used & kGLMipMaps) &&
                   !IsRectTexture(m_textures[tex].m_type);
    if (filt == GL_LINEAR_MIPMAP_LINEAR && !mipmaps)
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
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
        glTexParameteri(type, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    }
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, mag_filt);
    glTexParameteri(type, GL_TEXTURE_WRAP_S, wrap);
    if (type != GL_TEXTURE_1D)
        glTexParameteri(type, GL_TEXTURE_WRAP_T, wrap);
    doneCurrent();
}

void MythRenderOpenGL::ActiveTexture(int active_tex)
{
    if (!(m_exts_used & kGLMultiTex))
        return;

    makeCurrent();
    if (m_active_tex != active_tex)
    {
        m_glActiveTexture(active_tex);
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

    GLuint gltex = tex;
    glDeleteTextures(1, &gltex);
    if (m_textures[tex].m_data)
        delete m_textures[tex].m_data;
    if (m_textures[tex].m_pbo)
        m_glDeleteBuffers(1, &(m_textures[tex].m_pbo));
    if (m_textures[tex].m_vbo)
        m_glDeleteBuffers(1, &(m_textures[tex].m_vbo));
    m_textures.remove(tex);

    Flush(true);
    doneCurrent();
}

bool MythRenderOpenGL::CreateFrameBuffer(uint &fb, uint tex)
{
    if (!(m_exts_used & kGLExtFBufObj))
        return false;

    if (!m_textures.contains(tex))
        return false;

    QSize size = m_textures[tex].m_size;
    GLuint glfb;

    makeCurrent();
    glCheck();

    EnableTextures(tex);
    QRect tmp_viewport = m_viewport;
    glViewport(0, 0, size.width(), size.height());
    m_glGenFramebuffers(1, &glfb);
    m_glBindFramebuffer(GL_FRAMEBUFFER, glfb);
    glBindTexture(m_textures[tex].m_type, tex);
    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 (GLint) size.width(), (GLint) size.height(), 0,
                 m_textures[tex].m_data_fmt, m_textures[tex].m_data_type, NULL);
    m_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             m_textures[tex].m_type, tex, 0);

    GLenum status;
    status = m_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    m_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(tmp_viewport.left(), tmp_viewport.top(),
               tmp_viewport.width(), tmp_viewport.height());

    bool success = false;
    switch (status)
    {
        case GL_FRAMEBUFFER_COMPLETE:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Created frame buffer object (%1x%2).")
                    .arg(size.width()).arg(size.height()));
            success = true;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Frame buffer incomplete_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Frame buffer incomplete_MISSING_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Frame buffer incomplete_DUPLICATE_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Frame buffer incomplete_DIMENSIONS");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Frame buffer incomplete_FORMATS");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Frame buffer incomplete_DRAW_BUFFER");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Frame buffer incomplete_READ_BUFFER");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Frame buffer unsupported.");
            break;
        default:
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Unknown frame buffer error %1.").arg(status));
    }

    if (success)
        m_framebuffers.push_back(glfb);
    else
        m_glDeleteFramebuffers(1, &glfb);

    Flush(true);
    glCheck();
    doneCurrent();
    fb = glfb;
    return success;
}

void MythRenderOpenGL::DeleteFrameBuffer(uint fb)
{
    if (!m_framebuffers.contains(fb))
        return;

    makeCurrent();
    QVector<GLuint>::iterator it;
    for (it = m_framebuffers.begin(); it != m_framebuffers.end(); ++it)
    {
        if (*it == fb)
        {
            m_glDeleteFramebuffers(1, &(*it));
            m_framebuffers.erase(it);
            break;
        }
    }

    Flush(true);
    doneCurrent();
}

void MythRenderOpenGL::BindFramebuffer(uint fb)
{
    if (fb && !m_framebuffers.contains(fb))
        return;

    if (fb == (uint)m_active_fb)
        return;

    makeCurrent();
    m_glBindFramebuffer(GL_FRAMEBUFFER, fb);
    doneCurrent();
    m_active_fb = fb;
}

void MythRenderOpenGL::ClearFramebuffer(void)
{
    makeCurrent();
    glClear(GL_COLOR_BUFFER_BIT);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(uint tex, uint target, const QRect *src,
                                  const QRect *dst, uint prog, int alpha,
                                  int red, int green, int blue)
{
    if (!tex || !m_textures.contains(tex))
        return;

    if (target && !m_framebuffers.contains(target))
        target = 0;

    makeCurrent();
    BindFramebuffer(target);
    DrawBitmapPriv(tex, src, dst, prog, alpha, red, green, blue);
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(uint *textures, uint texture_count,
                                  uint target, const QRectF *src,
                                  const QRectF *dst, uint prog)
{
    if (!textures || !texture_count)
        return;

    if (target && !m_framebuffers.contains(target))
        target = 0;

    makeCurrent();
    BindFramebuffer(target);
    DrawBitmapPriv(textures, texture_count, src, dst, prog);
    doneCurrent();
}

void MythRenderOpenGL::DrawRect(const QRect &area, const QBrush &fillBrush,
                                const QPen &linePen, int alpha)
{
    makeCurrent();
    BindFramebuffer(0);
    DrawRectPriv(area, fillBrush, linePen, alpha);
    doneCurrent();
}

void MythRenderOpenGL::DrawRoundRect(const QRect &area, int cornerRadius,
                                     const QBrush &fillBrush,
                                     const QPen &linePen, int alpha)
{
    makeCurrent();
    BindFramebuffer(0);
    DrawRoundRectPriv(area, cornerRadius, fillBrush, linePen, alpha);
    doneCurrent();
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
    m_extensions = (reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));

    m_glTexImage1D = (MYTH_GLTEXIMAGE1DPROC)
        GetProcAddress("glTexImage1D");
    m_glActiveTexture = (MYTH_GLACTIVETEXTUREPROC)
        GetProcAddress("glActiveTexture");
    m_glMapBuffer = (MYTH_GLMAPBUFFERPROC)
        GetProcAddress("glMapBuffer");
    m_glBindBuffer = (MYTH_GLBINDBUFFERPROC)
        GetProcAddress("glBindBuffer");
    m_glGenBuffers = (MYTH_GLGENBUFFERSPROC)
        GetProcAddress("glGenBuffers");
    m_glBufferData = (MYTH_GLBUFFERDATAPROC)
        GetProcAddress("glBufferData");
    m_glUnmapBuffer = (MYTH_GLUNMAPBUFFERPROC)
        GetProcAddress("glUnmapBuffer");
    m_glDeleteBuffers = (MYTH_GLDELETEBUFFERSPROC)
        GetProcAddress("glDeleteBuffers");
    m_glGenFramebuffers = (MYTH_GLGENFRAMEBUFFERSPROC)
        GetProcAddress("glGenFramebuffers");
    m_glBindFramebuffer = (MYTH_GLBINDFRAMEBUFFERPROC)
        GetProcAddress("glBindFramebuffer");
    m_glFramebufferTexture2D = (MYTH_GLFRAMEBUFFERTEXTURE2DPROC)
        GetProcAddress("glFramebufferTexture2D");
    m_glCheckFramebufferStatus = (MYTH_GLCHECKFRAMEBUFFERSTATUSPROC)
        GetProcAddress("glCheckFramebufferStatus");
    m_glDeleteFramebuffers = (MYTH_GLDELETEFRAMEBUFFERSPROC)
        GetProcAddress("glDeleteFramebuffers");
    m_glGenFencesNV = (MYTH_GLGENFENCESNVPROC)
        GetProcAddress("glGenFencesNV");
    m_glDeleteFencesNV = (MYTH_GLDELETEFENCESNVPROC)
        GetProcAddress("glDeleteFencesNV");
    m_glSetFenceNV = (MYTH_GLSETFENCENVPROC)
        GetProcAddress("glSetFenceNV");
    m_glFinishFenceNV = (MYTH_GLFINISHFENCENVPROC)
        GetProcAddress("glFinishFenceNV");
    m_glGenFencesAPPLE = (MYTH_GLGENFENCESAPPLEPROC)
        GetProcAddress("glGenFencesAPPLE");
    m_glDeleteFencesAPPLE = (MYTH_GLDELETEFENCESAPPLEPROC)
        GetProcAddress("glDeleteFencesAPPLE");
    m_glSetFenceAPPLE = (MYTH_GLSETFENCEAPPLEPROC)
        GetProcAddress("glSetFenceAPPLE");
    m_glFinishFenceAPPLE = (MYTH_GLFINISHFENCEAPPLEPROC)
        GetProcAddress("glFinishFenceAPPLE");
}

void* MythRenderOpenGL::GetProcAddress(const QString &proc) const
{
    // TODO FIXME - this should really return a void(*) not void*
    static const QString exts[4] = { "", "ARB", "EXT", "OES" };
    void *result;
    for (int i = 0; i < 4; i++)
    {
#ifdef USING_OPENGLES
        result = reinterpret_cast<void*>(
            QLibrary::resolve("libGLESv2", (proc + exts[i]).toLatin1().data()));
        if (result)
            break;
#endif
        result = reinterpret_cast<void*>(getProcAddress(proc + exts[i]));
        if (result)
            break;
    }
    if (result == NULL)
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Extension not found: %1").arg(proc));

    return result;
}

bool MythRenderOpenGL::InitFeatures(void)
{
    static bool multitexture  = true;
    static bool vertexarrays  = true;
    static bool framebuffers  = true;
    static bool pixelbuffers  = true;
    static bool vertexbuffers = true;
    static bool fences        = true;
    static bool ycbcrtextures = true;
    static bool mipmapping    = true;
    static bool check         = true;

    if (check)
    {
        check = false;
        multitexture  = !getenv("OPENGL_NOMULTITEX");
        vertexarrays  = !getenv("OPENGL_NOVERTARRAY");
        framebuffers  = !getenv("OPENGL_NOFBO");
        pixelbuffers  = !getenv("OPENGL_NOPBO");
        vertexbuffers = !getenv("OPENGL_NOVBO");
        fences        = !getenv("OPENGL_NOFENCE");
        ycbcrtextures = !getenv("OPENGL_NOYCBCR");
        mipmapping    = !getenv("OPENGL_NOMIPMAP");
        if (!multitexture)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling multi-texturing.");
        if (!vertexarrays)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling Vertex Arrays.");
        if (!framebuffers)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling Framebuffer Objects.");
        if (!pixelbuffers)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling Pixel Buffer Objects.");
        if (!vertexbuffers)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling Vertex Buffer Objects.");
        if (!fences)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling fences.");
        if (!ycbcrtextures)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling YCbCr textures.");
        if (!mipmapping)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling mipmapping.");
    }

    GLint maxtexsz = 0;
    GLint maxunits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxunits);
    m_max_units = maxunits;
    m_max_tex_size = (maxtexsz) ? maxtexsz : 512;

    m_extensions = (const char*) glGetString(GL_EXTENSIONS);
    bool rects;
    m_default_texture_type = GetTextureType(rects);
    if (rects)
        m_exts_supported += kGLExtRect;

    if (m_extensions.contains("GL_ARB_multitexture") &&
        m_glActiveTexture && multitexture)
    {
        m_exts_supported += kGLMultiTex;
        if (m_max_units < 3)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Insufficient texture units for advanced OpenGL features.");
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Multi-texturing not supported. Certain "
                                       "OpenGL features will not work");
    }

    if (m_extensions.contains("GL_EXT_vertex_array") && vertexarrays)
    {
        m_exts_supported += kGLVertexArray;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "GL_EXT_vertex_array extension not supported. This may not work");
    }

    if (m_extensions.contains("GL_EXT_framebuffer_object") &&
        m_glGenFramebuffers        && m_glBindFramebuffer &&
        m_glFramebufferTexture2D   && m_glDeleteFramebuffers &&
        m_glCheckFramebufferStatus && framebuffers)
        m_exts_supported += kGLExtFBufObj;

    bool buffer_procs = m_glMapBuffer  && m_glBindBuffer &&
                        m_glGenBuffers && m_glDeleteBuffers &&
                        m_glBufferData && m_glUnmapBuffer;

    if(m_extensions.contains("GL_ARB_pixel_buffer_object")
       && buffer_procs && pixelbuffers)
        m_exts_supported += kGLExtPBufObj;

    if (m_extensions.contains("GL_ARB_vertex_buffer_object")
        && buffer_procs && vertexbuffers)
        m_exts_supported += kGLExtVBO;

    if(m_extensions.contains("GL_NV_fence") &&
        m_glGenFencesNV && m_glDeleteFencesNV &&
        m_glSetFenceNV  && m_glFinishFenceNV && fences)
        m_exts_supported += kGLNVFence;

    if(m_extensions.contains("GL_APPLE_fence") &&
        m_glGenFencesAPPLE && m_glDeleteFencesAPPLE &&
        m_glSetFenceAPPLE  && m_glFinishFenceAPPLE && fences)
        m_exts_supported += kGLAppleFence;

    if (m_extensions.contains("GL_MESA_ycbcr_texture") && ycbcrtextures)
        m_exts_supported += kGLMesaYCbCr;

    if (m_extensions.contains("GL_APPLE_ycbcr_422") && ycbcrtextures)
        m_exts_supported += kGLAppleYCbCr;

    if (m_extensions.contains("GL_SGIS_generate_mipmap") && mipmapping)
        m_exts_supported += kGLMipMaps;

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL vendor  : %1")
                .arg((const char*) glGetString(GL_VENDOR)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL renderer: %1")
                .arg((const char*) glGetString(GL_RENDERER)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("OpenGL version : %1")
                .arg((const char*) glGetString(GL_VERSION)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture size: %1 x %2")
                .arg(m_max_tex_size).arg(m_max_tex_size));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max texture units: %1")
                .arg(m_max_units));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Direct rendering: %1")
                .arg((this->format().directRendering()) ? "Yes" : "No"));
    }

    m_exts_used = m_exts_supported;

    if (m_exts_used & kGLExtPBufObj)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "PixelBufferObject support available");
    }

    return true;
}

void MythRenderOpenGL::ResetVars(void)
{
    m_fence           = 0;

    m_lock_level      = 0;

    m_extensions      = QString();
    m_exts_supported  = kGLFeatNone;
    m_exts_used       = kGLFeatNone;
    m_max_tex_size    = 0;
    m_max_units       = 0;
    m_default_texture_type = GL_TEXTURE_2D;

    m_viewport        = QRect();
    m_active_tex      = 0;
    m_active_tex_type = 0;
    m_active_fb       = 0;
    m_blend           = false;
    m_background      = 0x00000000;
}

void MythRenderOpenGL::ResetProcs(void)
{
    m_extensions = QString();

    m_glTexImage1D = NULL;
    m_glActiveTexture = NULL;
    m_glMapBuffer = NULL;
    m_glBindBuffer = NULL;
    m_glGenBuffers = NULL;
    m_glBufferData = NULL;
    m_glUnmapBuffer = NULL;
    m_glDeleteBuffers = NULL;
    m_glGenFramebuffers = NULL;
    m_glBindFramebuffer = NULL;
    m_glFramebufferTexture2D = NULL;
    m_glCheckFramebufferStatus = NULL;
    m_glDeleteFramebuffers = NULL;
    m_glGenFencesNV = NULL;
    m_glDeleteFencesNV = NULL;
    m_glSetFenceNV = NULL;
    m_glFinishFenceNV = NULL;
    m_glGenFencesAPPLE = NULL;
    m_glDeleteFencesAPPLE = NULL;
    m_glSetFenceAPPLE = NULL;
    m_glFinishFenceAPPLE = NULL;
}

uint MythRenderOpenGL::CreatePBO(uint tex)
{
    if (!(m_exts_used & kGLExtPBufObj))
        return 0;

    if (!m_textures.contains(tex))
        return 0;

    m_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 m_textures[tex].m_size.width(),
                 m_textures[tex].m_size.height(), 0,
                 m_textures[tex].m_data_fmt, m_textures[tex].m_data_type, NULL);

    GLuint tmp_pbo;
    m_glGenBuffers(1, &tmp_pbo);
    m_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    Flush(true);
    return tmp_pbo;
}

uint MythRenderOpenGL::CreateVBO(void)
{
    if (!(m_exts_used & kGLExtVBO))
        return 0;

    GLuint tmp_vbo;
    m_glGenBuffers(1, &tmp_vbo);
    return tmp_vbo;
}

void MythRenderOpenGL::DeleteOpenGLResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting OpenGL Resources");
    DeleteTextures();
    DeleteFrameBuffers();
    Flush(true);

    if (m_fence)
    {
        if (m_exts_supported & kGLAppleFence)
            m_glDeleteFencesAPPLE(1, &m_fence);
        else if(m_exts_supported & kGLNVFence)
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
        glDeleteTextures(1, &(it.key()));
        if (it.value().m_data)
            delete it.value().m_data;
        if (it.value().m_pbo)
            m_glDeleteBuffers(1, &(it.value().m_pbo));
    }
    m_textures.clear();
    Flush(true);
}

void MythRenderOpenGL::DeleteFrameBuffers(void)
{
    QVector<GLuint>::iterator it;
    for (it = m_framebuffers.begin(); it != m_framebuffers.end(); ++it)
        m_glDeleteFramebuffers(1, &(*(it)));
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

    int width  = min(src->width(),  size.width());
    int height = min(src->height(), size.height());

    data[0 + TEX_OFFSET] = src->left();
    data[1 + TEX_OFFSET] = src->top() + height;

    data[6 + TEX_OFFSET] = src->left() + width;
    data[7 + TEX_OFFSET] = src->top();

    if (!IsRectTexture(m_textures[tex].m_type))
    {
        data[0 + TEX_OFFSET] /= (float)size.width();
        data[6 + TEX_OFFSET] /= (float)size.width();
        data[1 + TEX_OFFSET] /= (float)size.height();
        data[7 + TEX_OFFSET] /= (float)size.height();
    }

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

    data[0 + TEX_OFFSET] = src->left();
    data[1 + TEX_OFFSET] = src->top() + src->height();

    data[6 + TEX_OFFSET] = src->left() + src->width();
    data[7 + TEX_OFFSET] = src->top();

    if (!IsRectTexture(m_textures[tex].m_type))
    {
        data[0 + TEX_OFFSET] /= (float)m_textures[tex].m_size.width();
        data[6 + TEX_OFFSET] /= (float)m_textures[tex].m_size.width();
        data[1 + TEX_OFFSET] /= (float)m_textures[tex].m_size.height();
        data[7 + TEX_OFFSET] /= (float)m_textures[tex].m_size.height();
    }

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
        GLfloat *vertices = NULL;
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

        m_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        m_glBufferData(GL_ARRAY_BUFFER, kTextureOffset, NULL, GL_STREAM_DRAW);
        void* target = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        if (target)
            memcpy(target, vertices, kTextureOffset);
        m_glUnmapBuffer(GL_ARRAY_BUFFER);

        ExpireVBOS(MAX_VERTEX_CACHE);
        return;
    }

    m_glBindBuffer(GL_ARRAY_BUFFER, m_cachedVBOS.value(ref));
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
            m_glDeleteBuffers(1, &vbo);
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

    if ((m_textures[tex].m_type == GL_TEXTURE_1D) && m_glTexImage1D)
    {
        m_glTexImage1D(m_textures[tex].m_type, 0,
                       m_textures[tex].m_internal_fmt,
                       size.width(), 0, m_textures[tex].m_data_fmt,
                       m_textures[tex].m_data_type, scratch);
    }
    else
    {
        glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                     size.width(), size.height(), 0, m_textures[tex].m_data_fmt,
                     m_textures[tex].m_data_type, scratch);
    }
    delete [] scratch;

    return true;
}

uint MythRenderOpenGL::GetBufferSize(QSize size, uint fmt, uint type)
{
    uint bytes;
    uint bpp;

    if (fmt == GL_BGRA || fmt ==GL_RGBA)
    {
        bpp = 4;
    }
    else if (fmt == GL_YCBCR_MESA || fmt == GL_YCBCR_422_APPLE ||
             fmt == MYTHTV_UYVY)
    {
        bpp = 2;
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
        case GL_UNSIGNED_SHORT_8_8_MESA:
            bytes = sizeof(GLushort);
            break;
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
