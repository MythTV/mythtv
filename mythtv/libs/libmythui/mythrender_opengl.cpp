#include "mythverbose.h"
#include "mythrender_opengl.h"
#include "mythxdisplay.h"

#define LOC QString("OpenGL: ")
#define LOC_ERR QString("OpenGL Error: ")

#if defined(Q_WS_X11)
#include <QX11Info>
#include <GL/glx.h>
#endif

MYTH_GLXGETVIDEOSYNCSGIPROC  MythRenderOpenGL::g_glXGetVideoSyncSGI  = NULL;
MYTH_GLXWAITVIDEOSYNCSGIPROC MythRenderOpenGL::g_glXWaitVideoSyncSGI = NULL;

#define VERTEX_INDEX  0
#define COLOR_INDEX   1
#define TEXTURE_INDEX 2
#define VERTEX_SIZE   2
#define TEXTURE_SIZE  2

static const GLuint kVertexOffset  = 0;
static const GLuint kTextureOffset = 8 * sizeof(GLfloat);
static const GLuint kVertexSize    = 16 * sizeof(GLfloat);

static const QString kDefaultVertexShader =
"GLSL_VERSION"
"GLSL_EXTENSIONS"
"attribute vec4 a_color;\n"
"attribute vec2 a_texcoord0;\n"
"varying   vec4 v_color;\n"
"varying   vec2 v_texcoord0;\n"
"void main() {\n"
"    gl_Position = ftransform();\n"
"    v_texcoord0 = a_texcoord0;\n"
"    v_color     = a_color;\n"
"}\n";

static const QString kDefaultFragmentShader =
"GLSL_VERSION"
"GLSL_EXTENSIONS"
"uniform sampler2D s_texture0;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = texture2D(s_texture0, v_texcoord0) * v_color;\n"
"}\n";

static const QString kSimpleVertexShader =
"GLSL_VERSION"
"GLSL_EXTENSIONS"
"attribute vec4 a_color;\n"
"varying   vec4 v_color;\n"
"void main() {\n"
"    gl_Position = ftransform();\n"
"    v_color     = a_color;\n"
"}\n";

static const QString kSimpleFragmentShader =
"GLSL_VERSION"
"GLSL_EXTENSIONS"
"varying vec4 v_color;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = v_color;\n"
"}\n";

static inline int __glCheck__(const QString &loc, const char* fileName, int n)
{
    int error = glGetError();
    if (error)
    { 
        VERBOSE(VB_IMPORTANT, QString("%1: %2 @ %3, %4")
            .arg(loc).arg((const char*)gluErrorString(error))
            .arg(fileName).arg(n));
    }
    return error;
}

#define MAX_VERTEX_CACHE 50
#define glCheck() __glCheck__(LOC, __FILE__, __LINE__)

class MythGLShaderObject
{
  public:
    MythGLShaderObject(uint vert, uint frag)
      : m_vertex_shader(vert), m_fragment_shader(frag) { }
    MythGLShaderObject()
      : m_vertex_shader(0), m_fragment_shader(0) { }

    GLuint m_vertex_shader;
    GLuint m_fragment_shader;
};

#define TEX_OFFSET 8
class MythGLTexture
{
  public:
    MythGLTexture() :
        m_type(GL_TEXTURE_2D), m_data(NULL), m_data_size(0),
        m_data_type(GL_UNSIGNED_BYTE), m_data_fmt(GL_BGRA),
        m_internal_fmt(GL_RGBA8), m_pbo(0), m_vbo(0),
        m_filter(GL_LINEAR), m_wrap(GL_CLAMP_TO_EDGE),
        m_size(0,0), m_act_size(0,0)
    {
        memset(&m_vertex_data, 0, sizeof(m_vertex_data));
    }

    ~MythGLTexture()
    {
    }

    GLuint  m_type;
    unsigned char *m_data;
    uint    m_data_size;
    GLuint  m_data_type;
    GLuint  m_data_fmt;
    GLuint  m_internal_fmt;
    GLuint  m_pbo;
    GLuint  m_vbo;
    GLuint  m_filter;
    GLuint  m_wrap;
    QSize   m_size;
    QSize   m_act_size;
    GLfloat m_vertex_data[16];
};

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

MythRenderOpenGL::MythRenderOpenGL(const QGLFormat& format, QPaintDevice* device)
  : QGLContext(format, device)
{
    Reset();
}

MythRenderOpenGL::MythRenderOpenGL(const QGLFormat& format)
  : QGLContext(format)
{
    Reset();
}

MythRenderOpenGL::~MythRenderOpenGL()
{
    makeCurrent();
    DeleteOpenGLResources();
    doneCurrent();
    if (m_lock)
        delete m_lock;
}

void MythRenderOpenGL::Init(void)
{
    OpenGLLocker locker(this);
    InitProcs();
    Init2DState();
    InitFeatures();

    VERBOSE(VB_GENERAL, LOC + "Initialised MythRenderOpenGL");
}

void MythRenderOpenGL::makeCurrent()
{
    m_lock->lock();
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Mis-matched calls to makeCurrent()");
    m_lock->unlock();
}

void MythRenderOpenGL::SetFeatures(uint features)
{
    m_exts_used = features;

    m_profile = kGLLegacyProfile;
    VERBOSE(VB_GENERAL, LOC + "Forcing legacy profile.");
    return;

    if ((m_exts_used & kGLExtVBO) && (m_exts_used & kGLSL) &&
        (m_exts_used & kGLExtFBufObj))
    {
        m_profile = kGLHighProfile;
        VERBOSE(VB_GENERAL, LOC + QString("Using high profile "
                                          "(GLSL + VBOs + FBOs)"));
    }
    else if ((m_exts_used & kGLVertexArray) && (m_exts_used & kGLMultiTex))
    {
        m_profile = kGLLegacyProfile;
        VERBOSE(VB_GENERAL, LOC + QString("Using legacy profile "
                                          " (Vertex arrays + multi-texturing)"));
        if (m_exts_used & kGLExtFragProg)
        {
            VERBOSE(VB_GENERAL, LOC +
                        QString("Fragment program support available"));
        }
    }
    else
    {
        m_profile = kGLNoProfile;
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Stoneage OpenGL installation?"));
    }

    if (m_exts_used & kGLExtPBufObj)
    {
        VERBOSE(VB_GENERAL, LOC +
                        QString("PixelBufferObject support available"));
    }

    DeleteDefaultShaders();
    if (kGLHighProfile == m_profile)
        CreateDefaultShaders();
}

int MythRenderOpenGL::SetPictureAttribute(int attribute, int newValue)
{
    int ret = -1;
    switch (attribute)
    {
        case kGLAttribBrightness:
            ret = newValue;
            m_attribs[attribute] = (newValue * 0.02f) - 0.5f;
            break;
        case kGLAttribContrast:
        case kGLAttribColour:
            ret = newValue;
            m_attribs[attribute] = (newValue * 0.02f);
            break;
        default:
            break;
    }
    return ret;
}

void MythRenderOpenGL::MoveResizeWindow(const QRect &rect)
{
    QWidget *parent = (QWidget*)this->device();
    if (parent)
        parent->setGeometry(rect);
}

void MythRenderOpenGL::SetViewPort(const QSize &size)
{
    if (size.width() == m_viewport.width() &&
        size.height() == m_viewport.height())
        return;

    makeCurrent();

    m_viewport = size;

    glViewport(0, 0, size.width(), size.height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, size.width(), size.height(), 0, 1, -1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

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

void MythRenderOpenGL::SetColor(int r, int g, int b, int a)
{
    uint32_t tmp = (r << 24) + (g << 16) + (b << 8) + a;
    if (tmp == m_color)
        return;

    m_color = tmp;
    makeCurrent();
    glColor4f(r / 255.0, g / 255.0, b / 255.0, a / 255.0);
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
            VERBOSE(VB_PLAYBACK, LOC + "Using GL_APPLE_fence");
    }
    else if (m_exts_used & kGLNVFence)
    {
        m_glGenFencesNV(1, &m_fence);
        if (m_fence)
            VERBOSE(VB_PLAYBACK, LOC + "Using GL_NV_fence");
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
        m_glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, m_textures[tex].m_pbo);
        m_glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                             m_textures[tex].m_data_size, NULL, GL_STREAM_DRAW);
        return m_glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
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
        m_glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
        glTexSubImage2D(m_textures[tex].m_type, 0, 0, 0, size.width(),
                        size.height(), m_textures[tex].m_data_fmt,
                        m_textures[tex].m_data_type, 0);
        m_glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
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
    int ret = GL_TEXTURE_2D;

    if (m_extensions.contains("GL_NV_texture_rectangle"))
        ret = GL_TEXTURE_RECTANGLE_NV;
    else if (m_extensions.contains("GL_ARB_texture_rectangle"))
        ret = GL_TEXTURE_RECTANGLE_ARB;
    else if (m_extensions.contains("GL_EXT_texture_rectangle"))
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
            if (kGLHighProfile == m_profile)
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

inline void store_bicubic_weights(float x, float *dst)
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

uint MythRenderOpenGL::CreateHelperTexture(void)
{
    makeCurrent();

    uint width = m_max_tex_size;
    uint tmp_tex = CreateTexture(QSize(width, 1), false,
                                 GL_TEXTURE_1D, GL_FLOAT, GL_RGBA,
                                 GL_RGBA16, GL_NEAREST, GL_REPEAT);

    if (!tmp_tex)
    {
        DeleteTexture(tmp_tex);
        return 0;
    }

    float *buf = NULL;
    buf = new float[m_textures[tmp_tex].m_data_size];
    float *ref = buf;

    for (uint i = 0; i < width; i++)
    {
        float x = (((float)i) + 0.5f) / (float)width;
        store_bicubic_weights(x, ref);
        ref += 4;
    }
    store_bicubic_weights(0, buf);
    store_bicubic_weights(1, &buf[(width - 1) << 2]);

    EnableTextures(tmp_tex);
    glBindTexture(m_textures[tmp_tex].m_type, tmp_tex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, width, 0, GL_RGBA, GL_FLOAT, buf);

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Created bicubic helper texture (%1 samples)")
            .arg(width));
    delete [] buf;
    doneCurrent();
    return tmp_tex;
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
        m_glDeleteBuffersARB(1, &(m_textures[tex].m_pbo));
    if (m_textures[tex].m_vbo)
        m_glDeleteBuffersARB(1, &(m_textures[tex].m_vbo));
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
    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0, 0, size.width(), size.height());
    m_glGenFramebuffersEXT(1, &glfb);
    m_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, glfb);
    glBindTexture(m_textures[tex].m_type, tex);
    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 (GLint) size.width(), (GLint) size.height(), 0,
                 m_textures[tex].m_data_fmt, m_textures[tex].m_data_type, NULL);
    m_glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        m_textures[tex].m_type, tex, 0);

    GLenum status;
    status = m_glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    m_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glPopAttrib();

    bool success = false;
    switch (status)
    {
        case GL_FRAMEBUFFER_COMPLETE_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Created frame buffer object (%1x%2).")
                    .arg(size.width()).arg(size.height()));
            success = true;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            VERBOSE(VB_PLAYBACK, LOC + "Frame buffer incomplete_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_MISSING_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_DUPLICATE_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_DIMENSIONS_EXT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_FORMATS_EXT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_DRAW_BUFFER_EXT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            VERBOSE(VB_PLAYBACK, LOC +
                    "Frame buffer incomplete_READ_BUFFER_EXT");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            VERBOSE(VB_PLAYBACK, LOC + "Frame buffer unsupported.");
            break;
        default:
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Unknown frame buffer error %1.").arg(status));
    }

    if (success)
        m_framebuffers.push_back(glfb);
    else
        m_glDeleteFramebuffersEXT(1, &glfb);

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
            m_glDeleteFramebuffersEXT(1, &(*it));
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
    m_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
    doneCurrent();
    m_active_fb = fb;
}

void MythRenderOpenGL::ClearFramebuffer(void)
{
    makeCurrent();
    glClear(GL_COLOR_BUFFER_BIT);
    doneCurrent();
}

bool MythRenderOpenGL::CreateFragmentProgram(const QString &program, uint &prog)
{
    if (!(m_exts_used & kGLExtFragProg))
        return false;

    bool success = true;
    GLint error;

    makeCurrent();

    QByteArray tmp = program.toAscii();

    GLuint glfp;
    m_glGenProgramsARB(1, &glfp);
    m_glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, glfp);
    m_glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            tmp.length(), tmp.constData());

    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error);
    if (error != -1)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Fragment Program compile error: position %1:'%2'")
                .arg(error).arg(program.mid(error)));

        success = false;
    }
    m_glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
                           GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &error);
    if (error != 1)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                "Fragment program exceeds hardware capabilities.");

        success = false;
    }

    if (success)
        m_programs.push_back(glfp);
    else
        m_glDeleteProgramsARB(1, &glfp);

    Flush(true);
    doneCurrent();

    prog = glfp;
    return success;
}

void MythRenderOpenGL::DeleteFragmentProgram(uint fp)
{
    if (!m_programs.contains(fp))
        return;

    makeCurrent();
    QVector<GLuint>::iterator it;
    for (it = m_programs.begin(); it != m_programs.end(); ++it)
    {
        if (*it == fp)
        {
            m_glDeleteProgramsARB(1, &(*it));
            m_programs.erase(it);
            break;
        }
    }
    Flush(true);
    doneCurrent();
}

void MythRenderOpenGL::EnableFragmentProgram(int fp)
{
    if ((!fp && !m_active_prog) ||
        (fp && (fp == m_active_prog)))
        return;

    makeCurrent();

    if (!fp && m_active_prog)
    {
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        m_active_prog = 0;
        doneCurrent();
        return;
    }

    if (!m_programs.contains(fp))
        return;

    if (!m_active_prog)
        glEnable(GL_FRAGMENT_PROGRAM_ARB);

    if (fp != m_active_prog)
    {
        m_glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
        m_active_prog = fp;
    }

    doneCurrent();
}

uint MythRenderOpenGL::CreateShaderObject(const QString &vertex,
                                          const QString &fragment)
{
    if (!m_exts_supported & kGLSL)
        return 0;

    OpenGLLocker locker(this);

    uint result = 0;
    QString vert_shader = vertex.isEmpty() ? kDefaultVertexShader : vertex;
    QString frag_shader = fragment.isEmpty() ? kDefaultFragmentShader: fragment;
    vert_shader.detach();
    frag_shader.detach();

    OptimiseShaderSource(vert_shader);
    OptimiseShaderSource(frag_shader);

    result = m_glCreateProgramObject();
    if (!result)
        return 0;

    MythGLShaderObject object(CreateShader(GL_VERTEX_SHADER, vert_shader),
                              CreateShader(GL_FRAGMENT_SHADER, frag_shader));
    m_shader_objects.insert(result, object);

    if (!ValidateShaderObject(result))
    {
        DeleteShaderObject(result);
        return 0;
    }

    return result;
}

void MythRenderOpenGL::DeleteShaderObject(uint obj)
{
    if (!m_shader_objects.contains(obj))
        return;

    makeCurrent();

    GLuint vertex   = m_shader_objects[obj].m_vertex_shader;
    GLuint fragment = m_shader_objects[obj].m_fragment_shader;
    m_glDetachObject(obj, vertex);
    m_glDetachObject(obj, fragment);
    m_glDeleteObject(vertex);
    m_glDeleteObject(fragment);
    m_glDeleteObject(obj);
    m_shader_objects.remove(obj);

    Flush(true);
    doneCurrent();
}

void MythRenderOpenGL::EnableShaderObject(uint obj)
{
    if (obj == m_active_obj)
        return;

    if (!obj && m_active_obj)
    {
        makeCurrent();
        m_glUseProgram(0);
        m_active_obj = 0;
        doneCurrent();
        return;
    }

    if (!m_shader_objects.contains(obj))
        return;

    makeCurrent();
    m_glUseProgram(obj);
    m_active_obj = obj;
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

    if (kGLLegacyProfile == m_profile)
    {
        DrawBitmapLegacy(tex, src, dst, prog, alpha, red, green, blue);
    }
    else if (kGLHighProfile == m_profile)
    {
        DrawBitmapHigh(tex, src, dst, prog, alpha, red, green, blue);
    }

    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(uint *textures, uint texture_count,
                                  uint target, const QRectF *src,
                                  const QRectF *dst, uint prog,
                                  bool colour_control)
{
    if (!textures || !texture_count)
        return;

    if (target && !m_framebuffers.contains(target))
        target = 0;

    makeCurrent();
    BindFramebuffer(target);

    if (kGLLegacyProfile == m_profile)
    {
        DrawBitmapLegacy(textures, texture_count, src, dst, prog, colour_control);
    }
    else if (kGLHighProfile == m_profile)
    {
        DrawBitmapHigh(textures, texture_count, src, dst, prog, colour_control);
    }

    doneCurrent();
}

void MythRenderOpenGL::DrawRect(const QRect &area, bool drawFill,
                                const QColor &fillColor,  bool drawLine,
                                int lineWidth, const QColor &lineColor,
                                int target, int prog)
{
    if (target && !m_framebuffers.contains(target))
        target = 0;

    makeCurrent();
    BindFramebuffer(target);

    if (kGLLegacyProfile == m_profile)
    {
        DrawRectLegacy(area, drawFill, fillColor, drawLine,
                       lineWidth, lineColor, prog);
    }
    else if (kGLHighProfile == m_profile)
    {
        DrawRectHigh(area, drawFill, fillColor, drawLine,
                       lineWidth, lineColor, prog);
    }

    doneCurrent();
}

bool MythRenderOpenGL::HasGLXWaitVideoSyncSGI(void)
{
    static bool initialised = false;
    if (!initialised)
    {
        makeCurrent();
        initialised = true;
#if defined(Q_WS_X11)
        int screen = DefaultScreen(QX11Info::display());
        QString glxExt =
            QString(QLatin1String(glXQueryExtensionsString(QX11Info::display(),
                                                           screen)));
        if (glxExt.contains(QLatin1String("GLX_SGI_video_sync")))
        {
            g_glXGetVideoSyncSGI = (MYTH_GLXGETVIDEOSYNCSGIPROC)
                GetProcAddress("glXGetVideoSyncSGI");
            g_glXWaitVideoSyncSGI = (MYTH_GLXWAITVIDEOSYNCSGIPROC)
                GetProcAddress("glXWaitVideoSyncSGI");
        }
#endif
        doneCurrent();
    }
    return g_glXGetVideoSyncSGI && g_glXWaitVideoSyncSGI;
}

unsigned int MythRenderOpenGL::GetVideoSyncCount(void)
{
    unsigned int count = 0;
    if (HasGLXWaitVideoSyncSGI())
    {
        makeCurrent();
        g_glXGetVideoSyncSGI(&count);
        doneCurrent();
    }
    return count;
}

void MythRenderOpenGL::WaitForVideoSync(int div, int rem, unsigned int *count)
{
    if (HasGLXWaitVideoSyncSGI())
    {
        makeCurrent();
        g_glXWaitVideoSyncSGI(div, rem, count);
        doneCurrent();
    }
}

void MythRenderOpenGL::DrawBitmapLegacy(uint tex, const QRect *src,
                                        const QRect *dst, uint prog, int alpha,
                                        int red, int green, int blue)
{
    if (prog && !m_programs.contains(prog))
        prog = 0;

    EnableFragmentProgram(prog);
    SetBlend(true);
    SetColor(red, green, blue, alpha);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    EnableTextures(tex);
    glBindTexture(m_textures[tex].m_type, tex);
    UpdateTextureVertices(tex, src, dst);
    glVertexPointer(2, GL_FLOAT, 0, m_textures[tex].m_vertex_data);
    glTexCoordPointer(2, GL_FLOAT, 0, m_textures[tex].m_vertex_data + TEX_OFFSET);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void MythRenderOpenGL::DrawBitmapHigh(uint tex, const QRect *src,
                                      const QRect *dst, uint prog, int alpha,
                                      int red, int green, int blue)
{
    if (prog && !m_shader_objects.contains(prog))
        prog = 0;
    if (prog == 0)
        prog = m_shaders[kShaderDefault];

    EnableShaderObject(prog);
    SetBlend(true);

    EnableTextures(tex);
    glBindTexture(m_textures[tex].m_type, tex);

    m_glBindBufferARB(GL_ARRAY_BUFFER, m_textures[tex].m_vbo);
    UpdateTextureVertices(tex, src, dst);
    m_glBufferDataARB(GL_ARRAY_BUFFER, kVertexSize, NULL, GL_STREAM_DRAW);
    void* target = m_glMapBufferARB(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (target)
        memcpy(target, m_textures[tex].m_vertex_data, kVertexSize);
    m_glUnmapBufferARB(GL_ARRAY_BUFFER);

    m_glEnableVertexAttribArray(VERTEX_INDEX);
    m_glEnableVertexAttribArray(TEXTURE_INDEX);

    m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            (const void *) kVertexOffset);
    m_glVertexAttrib4f(COLOR_INDEX, red / 255.0, green / 255.0, blue / 255.0, alpha / 255.0);
    m_glVertexAttribPointer(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            (const void *) kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_glDisableVertexAttribArray(TEXTURE_INDEX);
    m_glDisableVertexAttribArray(VERTEX_INDEX);
    m_glBindBufferARB(GL_ARRAY_BUFFER, 0);
}

void MythRenderOpenGL::DrawBitmapLegacy(uint *textures, uint texture_count,
                                        const QRectF *src, const QRectF *dst,
                                        uint prog, bool colour_control)
{
    if (prog && !m_programs.contains(prog))
        prog = 0;

    uint first = textures[0];

    EnableFragmentProgram(prog);
    if (colour_control)
    {
        InitFragmentParams(0, m_attribs[kGLAttribBrightness],
                           m_attribs[kGLAttribContrast],
                           m_attribs[kGLAttribColour], 0.5f);
    }
    SetBlend(false);
    SetColor(255, 255, 255, 255);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    EnableTextures(first);
    uint active_tex = 0;
    for (uint i = 0; i < texture_count; i++)
    {
        if (m_textures.contains(textures[i]))
        {
            ActiveTexture(GL_TEXTURE0 + active_tex++);
            glBindTexture(m_textures[textures[i]].m_type, textures[i]);
        }
    }

    UpdateTextureVertices(first, src, dst);
    glVertexPointer(2, GL_FLOAT, 0, m_textures[first].m_vertex_data);
    glTexCoordPointer(2, GL_FLOAT, 0, m_textures[first].m_vertex_data + TEX_OFFSET);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    ActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void MythRenderOpenGL::DrawBitmapHigh(uint *textures, uint texture_count,
                                      const QRectF *src, const QRectF *dst,
                                      uint prog, bool colour_control)
{

}

void MythRenderOpenGL::DrawRectLegacy(const QRect &area, bool drawFill,
                                      const QColor &fillColor,  bool drawLine,
                                      int lineWidth, const QColor &lineColor,
                                      int prog)
{
    if (prog && !m_programs.contains(prog))
        prog = 0;

    EnableFragmentProgram(prog);
    SetBlend(true);
    DisableTextures();
    glEnableClientState(GL_VERTEX_ARRAY);

    if (drawFill)
    {
        SetColor(fillColor.red(), fillColor.green(),
                 fillColor.blue(), fillColor.alpha());
        GLfloat *vertices = GetCachedVertices(GL_TRIANGLE_STRIP, area);
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    if (drawLine)
    {
        SetColor(lineColor.red(), lineColor.green(),
                 lineColor.blue(), lineColor.alpha());
        glLineWidth(lineWidth);
        GLfloat *vertices = GetCachedVertices(GL_LINE_LOOP, area);
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
}

void MythRenderOpenGL::DrawRectHigh(const QRect &area, bool drawFill,
                                    const QColor &fillColor,  bool drawLine,
                                    int lineWidth, const QColor &lineColor,
                                    int prog)
{
    if (prog && !m_shader_objects.contains(prog))
        prog = 0;
    if (prog == 0)
        prog = m_shaders[kShaderSimple];

    EnableShaderObject(prog);
    SetBlend(true);
    DisableTextures();

    m_glEnableVertexAttribArray(VERTEX_INDEX);

    if (drawFill)
    {
        m_glVertexAttrib4f(COLOR_INDEX,
                           fillColor.red() / 255.0,
                           fillColor.green() / 255.0,
                           fillColor.blue() / 255.0,
                           fillColor.alpha() / 255.0);
        GetCachedVBO(GL_TRIANGLE_STRIP, area);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_glBindBufferARB(GL_ARRAY_BUFFER, 0);
    }

    if (drawLine)
    {
        glLineWidth(lineWidth);
        m_glVertexAttrib4f(COLOR_INDEX,
                           lineColor.red() / 255.0,
                           lineColor.green() / 255.0,
                           lineColor.blue() / 255.0,
                           lineColor.alpha() / 255.0);
        GetCachedVBO(GL_LINE_LOOP, area);
        m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                                VERTEX_SIZE * sizeof(GLfloat),
                               (const void *) kVertexOffset);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        m_glBindBufferARB(GL_ARRAY_BUFFER, 0);
    }

    m_glDisableVertexAttribArray(VERTEX_INDEX);
}

void MythRenderOpenGL::Init2DState(void)
{
    SetBlend(false);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_FLAT);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Flush(true);
}

void MythRenderOpenGL::InitProcs(void)
{
    m_extensions = (reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));

    m_glActiveTexture = (MYTH_GLACTIVETEXTUREPROC)
        GetProcAddress("glActiveTexture");
    m_glMapBufferARB = (MYTH_GLMAPBUFFERARBPROC)
        GetProcAddress("glMapBufferARB");
    m_glBindBufferARB = (MYTH_GLBINDBUFFERARBPROC)
        GetProcAddress("glBindBufferARB");
    m_glGenBuffersARB = (MYTH_GLGENBUFFERSARBPROC)
        GetProcAddress("glGenBuffersARB");
    m_glBufferDataARB = (MYTH_GLBUFFERDATAARBPROC)
        GetProcAddress("glBufferDataARB");
    m_glUnmapBufferARB = (MYTH_GLUNMAPBUFFERARBPROC)
        GetProcAddress("glUnmapBufferARB");
    m_glDeleteBuffersARB = (MYTH_GLDELETEBUFFERSARBPROC)
        GetProcAddress("glDeleteBuffersARB");
    m_glGenProgramsARB = (MYTH_GLGENPROGRAMSARBPROC)
        GetProcAddress("glGenProgramsARB");
    m_glBindProgramARB = (MYTH_GLBINDPROGRAMARBPROC)
        GetProcAddress("glBindProgramARB");
    m_glProgramStringARB = (MYTH_GLPROGRAMSTRINGARBPROC)
        GetProcAddress("glProgramStringARB");
    m_glProgramEnvParameter4fARB = (MYTH_GLPROGRAMENVPARAMETER4FARBPROC)
        GetProcAddress("glProgramEnvParameter4fARB");
    m_glDeleteProgramsARB = (MYTH_GLDELETEPROGRAMSARBPROC)
        GetProcAddress("glDeleteProgramsARB");
    m_glGetProgramivARB = (MYTH_GLGETPROGRAMIVARBPROC)
        GetProcAddress("glGetProgramivARB");
    m_glGenFramebuffersEXT = (MYTH_GLGENFRAMEBUFFERSEXTPROC)
        GetProcAddress("glGenFramebuffersEXT");
    m_glBindFramebufferEXT = (MYTH_GLBINDFRAMEBUFFEREXTPROC)
        GetProcAddress("glBindFramebufferEXT");
    m_glFramebufferTexture2DEXT = (MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC)
        GetProcAddress("glFramebufferTexture2DEXT");
    m_glCheckFramebufferStatusEXT = (MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC)
        GetProcAddress("glCheckFramebufferStatusEXT");
    m_glDeleteFramebuffersEXT = (MYTH_GLDELETEFRAMEBUFFERSEXTPROC)
        GetProcAddress("glDeleteFramebuffersEXT");
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
    m_glCreateShaderObject = (MYTH_GLCREATESHADEROBJECT)
        GetProcAddress("glCreateShaderObjectARB");
    m_glShaderSource = (MYTH_GLSHADERSOURCE)
        GetProcAddress("glShaderSourceARB");
    m_glCompileShader = (MYTH_GLCOMPILESHADER)
        GetProcAddress("glCompileShaderARB");
    m_glGetShader = (MYTH_GLGETSHADER)
        GetProcAddress("glGetShaderiv");
    m_glGetShaderInfoLog = (MYTH_GLGETSHADERINFOLOG)
        GetProcAddress("glGetShaderInfoLog");
    m_glDeleteShader = (MYTH_GLDELETESHADER)
        GetProcAddress("glDeleteShader");
    m_glCreateProgramObject = (MYTH_GLCREATEPROGRAMOBJECT)
        GetProcAddress("glCreateProgramObjectARB");
    m_glAttachObject = (MYTH_GLATTACHOBJECT)
        GetProcAddress("glAttachObjectARB");
    m_glLinkProgram = (MYTH_GLLINKPROGRAM)
        GetProcAddress("glLinkProgramARB");
    m_glUseProgram = (MYTH_GLUSEPROGRAM)
        GetProcAddress("glUseProgramObjectARB");
    m_glGetInfoLog = (MYTH_GLGETINFOLOG)
        GetProcAddress("glGetInfoLogARB");
    m_glGetObjectParameteriv = (MYTH_GLGETOBJECTPARAMETERIV)
        GetProcAddress("glGetObjectParameterivARB");
    m_glDetachObject = (MYTH_GLDETACHOBJECT)
        GetProcAddress("glDetachObjectARB");
    m_glDeleteObject = (MYTH_GLDELETEOBJECT)
        GetProcAddress("glDeleteObjectARB");
    m_glGetUniformLocation = (MYTH_GLGETUNIFORMLOCATION)
        GetProcAddress("glGetUniformLocationARB");
    m_glUniform4f = (MYTH_GLUNIFORM4F)
        GetProcAddress("glUniform4fARB");
    m_glVertexAttribPointer = (MYTH_GLVERTEXATTRIBPOINTER)
        GetProcAddress("glVertexAttribPointer");
    m_glEnableVertexAttribArray = (MYTH_GLENABLEVERTEXATTRIBARRAY)
        GetProcAddress("glEnableVertexAttribArray");
    m_glDisableVertexAttribArray = (MYTH_GLDISABLEVERTEXATTRIBARRAY)
        GetProcAddress("glDisableVertexAttribArray");
    m_glBindAttribLocation = (MYTH_GLBINDATTRIBLOCATION)
        GetProcAddress("glBindAttribLocation");
    m_glVertexAttrib4f = (MYTH_GLVERTEXATTRIB4F)
        GetProcAddress("glVertexAttrib4f");
}

void* MythRenderOpenGL::GetProcAddress(const QString &proc) const
{
    void *result = getProcAddress(proc);
    if (result == NULL)
        VERBOSE(VB_EXTRA, LOC + QString("Extension not found: %1").arg(proc));
    return result;
}

void MythRenderOpenGL::InitFeatures(void)
{
    m_exts_supported = kGLFeatNone;

    GLint maxtexsz = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &m_max_units);
    m_max_tex_size = (maxtexsz) ? maxtexsz : 512;

    m_extensions = (const char*) glGetString(GL_EXTENSIONS);
    bool rects;
    m_default_texture_type = GetTextureType(rects);
    if (rects)
        m_exts_supported += kGLExtRect;

    if (m_extensions.contains("GL_ARB_multitexture") &&
        m_glActiveTexture)
    {
        m_exts_supported += kGLMultiTex;
        if (m_max_units < 3)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Insufficient texture units for "
                        "advanced OpenGL features."));
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Multi-texturing not supported. "
                    "Certain OpenGL features will not work"));
    }

    if (m_extensions.contains("GL_EXT_vertex_array"))
    {
        m_exts_supported += kGLVertexArray;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("GL_EXT_vertex_array extension not supported. "
                    "This may not work"));
    }

    if (m_extensions.contains("GL_ARB_fragment_program") &&
        m_glGenProgramsARB   && m_glBindProgramARB &&
        m_glProgramStringARB && m_glDeleteProgramsARB &&
        m_glGetProgramivARB  && m_glProgramEnvParameter4fARB)
        m_exts_supported += kGLExtFragProg;

    if (m_extensions.contains("GL_ARB_shader_objects") &&
        m_extensions.contains("GL_ARB_vertex_shader") &&
        m_extensions.contains("GL_ARB_fragment_shader") &&
        m_glShaderSource  && m_glCreateShaderObject &&
        m_glCompileShader && m_glGetShader &&
        m_glGetShaderInfoLog && m_glDeleteShader &&
        m_glCreateProgramObject &&
        m_glAttachObject  && m_glLinkProgram &&
        m_glUseProgram    && m_glGetInfoLog &&
        m_glDetachObject  && m_glGetObjectParameteriv &&
        m_glDeleteObject  && m_glGetUniformLocation &&
        m_glUniform4f     && m_glVertexAttribPointer &&
        m_glEnableVertexAttribArray &&
        m_glDisableVertexAttribArray &&
        m_glBindAttribLocation &&
        m_glVertexAttrib4f)
    {
        m_exts_supported += kGLSL;
    }

    if (m_extensions.contains("GL_EXT_framebuffer_object") &&
        m_glGenFramebuffersEXT      && m_glBindFramebufferEXT &&
        m_glFramebufferTexture2DEXT && m_glDeleteFramebuffersEXT &&
        m_glCheckFramebufferStatusEXT)
        m_exts_supported += kGLExtFBufObj;

    bool buffer_procs = m_glMapBufferARB  && m_glBindBufferARB &&
                        m_glGenBuffersARB && m_glDeleteBuffersARB &&
                        m_glBufferDataARB && m_glUnmapBufferARB;

    if(m_extensions.contains("GL_ARB_pixel_buffer_object") && buffer_procs)
        m_exts_supported += kGLExtPBufObj;

    if (m_extensions.contains("GL_ARB_vertex_buffer_object") && buffer_procs)
        m_exts_supported += kGLExtVBO;

    if(m_extensions.contains("GL_NV_fence") &&
        m_glGenFencesNV && m_glDeleteFencesNV &&
        m_glSetFenceNV  && m_glFinishFenceNV)
        m_exts_supported += kGLNVFence;

    if(m_extensions.contains("GL_APPLE_fence") &&
        m_glGenFencesAPPLE && m_glDeleteFencesAPPLE &&
        m_glSetFenceAPPLE  && m_glFinishFenceAPPLE)
        m_exts_supported += kGLAppleFence;

    if (m_extensions.contains("GL_MESA_ycbcr_texture"))
        m_exts_supported += kGLMesaYCbCr;

    if (m_extensions.contains("GL_APPLE_rgb_422"))
        m_exts_supported += kGLAppleRGB422;

    if (m_extensions.contains("GL_APPLE_ycbcr_422"))
        m_exts_supported += kGLAppleYCbCr;

    if (m_extensions.contains("GL_SGIS_generate_mipmap"))
        m_exts_supported += kGLMipMaps;

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;
        VERBOSE(VB_GENERAL, LOC + QString("OpenGL vendor  : %1")
                .arg((const char*) glGetString(GL_VENDOR)));
        VERBOSE(VB_GENERAL, LOC + QString("OpenGL renderer: %1")
                .arg((const char*) glGetString(GL_RENDERER)));
        VERBOSE(VB_GENERAL, LOC + QString("OpenGL version : %1")
                .arg((const char*) glGetString(GL_VERSION)));
        VERBOSE(VB_GENERAL, LOC + QString("Max texture size: %1 x %2")
                .arg(m_max_tex_size).arg(m_max_tex_size));
        VERBOSE(VB_GENERAL, LOC + QString("Max texture units: %1")
                .arg(m_max_units));
        VERBOSE(VB_GENERAL, LOC + QString("Direct rendering: %1")
                .arg((this->format().directRendering()) ? "Yes" : "No"));
    }

    SetFeatures(m_exts_supported);
}

void MythRenderOpenGL::Reset(void)
{
    ResetVars();
    ResetProcs();
}

void MythRenderOpenGL::ResetVars(void)
{
    m_fence           = 0;

    m_lock            = new QMutex(QMutex::Recursive);
    m_lock_level      = 0;

    m_profile         = kGLNoProfile;
    m_extensions      = QString();
    m_exts_supported  = kGLFeatNone;
    m_exts_used       = kGLFeatNone;
    m_max_tex_size    = 0;
    m_max_units       = 0;
    m_default_texture_type = GL_TEXTURE_2D;
    memset(m_shaders, 0, sizeof(m_shaders));

    m_viewport        = QSize();
    m_active_tex      = 0;
    m_active_tex_type = 0;
    m_active_prog     = 0;
    m_active_obj      = 0;
    m_active_fb       = 0;
    m_blend           = false;
    m_color           = 0x00000000;
    m_background      = 0x00000000;
}

void MythRenderOpenGL::ResetProcs(void)
{
    m_extensions = QString();

    m_glActiveTexture = NULL;
    m_glGenProgramsARB = NULL;
    m_glBindProgramARB = NULL;
    m_glProgramStringARB = NULL;
    m_glProgramEnvParameter4fARB = NULL;
    m_glDeleteProgramsARB = NULL;
    m_glGetProgramivARB = NULL;
    m_glMapBufferARB = NULL;
    m_glBindBufferARB = NULL;
    m_glGenBuffersARB = NULL;
    m_glBufferDataARB = NULL;
    m_glUnmapBufferARB = NULL;
    m_glDeleteBuffersARB = NULL;
    m_glGenFramebuffersEXT = NULL;
    m_glBindFramebufferEXT = NULL;
    m_glFramebufferTexture2DEXT = NULL;
    m_glCheckFramebufferStatusEXT = NULL;
    m_glDeleteFramebuffersEXT = NULL;
    m_glGenFencesNV = NULL;
    m_glDeleteFencesNV = NULL;
    m_glSetFenceNV = NULL;
    m_glFinishFenceNV = NULL;
    m_glGenFencesAPPLE = NULL;
    m_glDeleteFencesAPPLE = NULL;
    m_glSetFenceAPPLE = NULL;
    m_glFinishFenceAPPLE = NULL;
    m_glCreateShaderObject = NULL;
    m_glShaderSource = NULL;
    m_glCompileShader = NULL;
    m_glGetShader = NULL;
    m_glGetShaderInfoLog = NULL;
    m_glDeleteShader = NULL;
    m_glCreateProgramObject = NULL;
    m_glAttachObject = NULL;
    m_glLinkProgram = NULL;
    m_glUseProgram = NULL;
    m_glGetInfoLog = NULL;
    m_glGetObjectParameteriv = NULL;
    m_glDetachObject = NULL;
    m_glDeleteObject = NULL;
    m_glGetUniformLocation = NULL;
    m_glUniform4f = NULL;
    m_glVertexAttribPointer = NULL;
    m_glEnableVertexAttribArray = NULL;
    m_glDisableVertexAttribArray = NULL;
    m_glBindAttribLocation = NULL;
    m_glVertexAttrib4f = NULL;
}

uint MythRenderOpenGL::CreatePBO(uint tex)
{
    if (!(m_exts_used & kGLExtPBufObj))
        return 0;

    if (!m_textures.contains(tex))
        return 0;

    m_glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 m_textures[tex].m_size.width(),
                 m_textures[tex].m_size.height(), 0,
                 m_textures[tex].m_data_fmt, m_textures[tex].m_data_type, NULL);

    GLuint tmp_pbo;
    m_glGenBuffersARB(1, &tmp_pbo);
    m_glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

    Flush(true);
    return tmp_pbo;
}

uint MythRenderOpenGL::CreateVBO(void)
{
    if (!(m_exts_used & kGLExtVBO))
        return 0;

    GLuint tmp_vbo;
    m_glGenBuffersARB(1, &tmp_vbo);
    return tmp_vbo;
}

void MythRenderOpenGL::DeleteOpenGLResources(void)
{
    VERBOSE(VB_GENERAL, LOC + "Deleting OpenGL Resources");

    DeleteDefaultShaders();
    DeletePrograms();
    DeleteTextures();
    DeleteFrameBuffers();
    DeleteShaderObjects();
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(" %1 unexpired vertices")
            .arg(m_cachedVertices.size()));
    }

    if (m_cachedVBOS.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(" %1 unexpired VBOs")
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
            m_glDeleteBuffersARB(1, &(it.value().m_pbo));
    }
    m_textures.clear();
    Flush(true);
}

void MythRenderOpenGL::DeletePrograms(void)
{
    QVector<GLuint>::iterator it;
    for (it = m_programs.begin(); it != m_programs.end(); ++it)
        m_glDeleteProgramsARB(1, &(*(it)));
    m_programs.clear();
    Flush(true);
}

void MythRenderOpenGL::DeleteFrameBuffers(void)
{
    QVector<GLuint>::iterator it;
    for (it = m_framebuffers.begin(); it != m_framebuffers.end(); ++it)
        m_glDeleteFramebuffersEXT(1, &(*(it)));
    m_framebuffers.clear();
    Flush(true);
}

void MythRenderOpenGL::DeleteShaderObjects(void)
{
    QHash<GLuint, MythGLShaderObject>::iterator it;
    for (it = m_shader_objects.begin(); it != m_shader_objects.end(); ++it)
    {
        GLuint object   = it.key();
        GLuint vertex   = it.value().m_vertex_shader;
        GLuint fragment = it.value().m_fragment_shader;
        m_glDetachObject(object, vertex);
        m_glDetachObject(object, fragment);
        m_glDeleteShader(vertex);
        m_glDeleteShader(fragment);
        m_glDeleteObject(object);
    }
    m_shader_objects.clear();
    Flush(true);
}

void MythRenderOpenGL::CreateDefaultShaders(void)
{
    m_shaders[kShaderSimple]  = CreateShaderObject(kSimpleVertexShader,
                                                   kSimpleFragmentShader);
    m_shaders[kShaderDefault] = CreateShaderObject(kDefaultVertexShader,
                                                   kDefaultFragmentShader);
}

void MythRenderOpenGL::DeleteDefaultShaders(void)
{
    for (int i = 0; i < kShaderCount; i++)
    {
        DeleteShaderObject(m_shaders[i]);
        m_shaders[i] = 0;
    }
}

uint MythRenderOpenGL::CreateShader(int type, const QString source)
{
    uint result = m_glCreateShaderObject(type);
    QByteArray src = source.toAscii();
    const char* tmp[1] = { src.constData() };
    m_glShaderSource(result, 1, tmp, NULL);
    m_glCompileShader(result);
    GLint compiled;
    m_glGetShader(result, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint length = 0;
        m_glGetShader(result, GL_INFO_LOG_LENGTH, &length);
        if (length > 1)
        {
            char *log = (char*)malloc(sizeof(char) * length);
            m_glGetShaderInfoLog(result, length, NULL, log);
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to compile shader.");
            VERBOSE(VB_IMPORTANT, log);
            free(log);
        }
        m_glDeleteShader(result);
        result = 0;
    }
    return result;
}

bool MythRenderOpenGL::ValidateShaderObject(uint obj)
{
    if (!m_shader_objects.contains(obj))
        return false;
    if (!m_shader_objects[obj].m_fragment_shader ||
        !m_shader_objects[obj].m_vertex_shader)
        return false;

    m_glAttachObject(obj, m_shader_objects[obj].m_fragment_shader);
    m_glAttachObject(obj, m_shader_objects[obj].m_vertex_shader);
    m_glBindAttribLocation(obj, TEXTURE_INDEX, "a_texcoord0");
    m_glBindAttribLocation(obj, COLOR_INDEX, "a_color");
    m_glLinkProgram(obj);
    return CheckObjectStatus(obj);
}

bool MythRenderOpenGL::CheckObjectStatus(uint obj)
{
    int ok;
    m_glGetObjectParameteriv(obj, GL_OBJECT_LINK_STATUS_ARB, &ok);
    if (ok > 0)
        return true;

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to link shader object."));
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    m_glGetObjectParameteriv(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB,
                                &infologLength);
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        m_glGetInfoLog(obj, infologLength, &charsWritten, infoLog);
        VERBOSE(VB_IMPORTANT, QString("\n\n%1").arg(infoLog));
        free(infoLog);
    }
    return false;
}

void MythRenderOpenGL::OptimiseShaderSource(QString &source)
{
    QString version = "#version 120\n";
    QString extensions = "";

    if (m_exts_used & kGLExtRect)
    {
        extensions += "#extension GL_ARB_texture_rectangle : enable\n";
        source.replace("sampler2D", "sampler2DRect");
        source.replace("texture2D", "texture2DRect");
    }

    source.replace("GLSL_VERSION", version);
    source.replace("GLSL_EXTENSIONS", extensions);

    VERBOSE(VB_EXTRA, "\n" + source);
}

bool MythRenderOpenGL::UpdateTextureVertices(uint tex, const QRect *src,
                                             const QRect *dst)
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
    data[4] = data[6] = dst->left() + std::min(src->width(), dst->width());
    data[3] = data[7] = dst->top() + std::min(src->height(), dst->height());

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

        m_glBindBufferARB(GL_ARRAY_BUFFER, vbo);
        m_glBufferDataARB(GL_ARRAY_BUFFER, kTextureOffset, NULL, GL_STREAM_DRAW);
        void* target = m_glMapBufferARB(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        if (target)
            memcpy(target, vertices, kTextureOffset);
        m_glUnmapBufferARB(GL_ARRAY_BUFFER);

        ExpireVBOS(MAX_VERTEX_CACHE);
        return;
    }

    m_glBindBufferARB(GL_ARRAY_BUFFER, m_cachedVBOS.value(ref));
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
            m_glDeleteBuffersARB(1, &vbo);
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

    GLint check;
    if (m_textures[tex].m_type == GL_TEXTURE_1D)
    {
        glTexImage1D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                     size.width(), 0, m_textures[tex].m_data_fmt ,
                     m_textures[tex].m_data_type, scratch);
    }
    else
    {
        glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                     size.width(), size.height(), 0, m_textures[tex].m_data_fmt,
                     m_textures[tex].m_data_type, scratch);
    }
    glGetTexLevelParameteriv(m_textures[tex].m_type, 0, GL_TEXTURE_WIDTH, &check);

    delete [] scratch;

    return (check == size.width());
}

uint MythRenderOpenGL::GetBufferSize(QSize size, uint fmt, uint type)
{
    uint bytes;
    uint bpp;

    switch (fmt)
    {
        case GL_BGRA:
        case GL_RGBA:
            bpp = 4;
            break;
        case GL_YCBCR_MESA:
            bpp = 2;
            break;
        default:
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

void MythRenderOpenGL::InitFragmentParams(uint fp, float a, float b,
                                          float c, float d)
{
    makeCurrent();
    m_glProgramEnvParameter4fARB(
        GL_FRAGMENT_PROGRAM_ARB, fp, a, b, c, d);
    doneCurrent();
}
