#include "mythverbose.h"
#include "mythrender_opengl.h"
#include "mythxdisplay.h"

#define LOC QString("OpenGL: ")
#define LOC_ERR QString("OpenGL Error: ")

#if defined(Q_WS_X11)
#include <QX11Info>
#include <GL/glx.h>
#endif

MYTH_GLXGETVIDEOSYNCSGIPROC  MythRenderOpenGL::gMythGLXGetVideoSyncSGI  = NULL;
MYTH_GLXWAITVIDEOSYNCSGIPROC MythRenderOpenGL::gMythGLXWaitVideoSyncSGI = NULL;

static const QString kDefaultVertexShader =
"#version 140\n"
"uniform Transformation {\n"
"    mat4 projection_matrix;\n"
"    mat4 modelview_matrix;\n"
"};\n"
"in vec3 vertex;\n"
"void main() {\n"
"    gl_Position = projection_matrix * modelview_matrix * vec4(vertex, 1.0);\n"
"}\n";

static const QString kDefaultFragmentShader =
"void main(void)\n"
"{\n"
"    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
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

class MythGLTexture
{
  public:
    MythGLTexture() :
        m_type(GL_TEXTURE_2D), m_data(NULL), m_data_size(0),
        m_data_type(GL_UNSIGNED_BYTE), m_data_fmt(GL_BGRA),
        m_internal_fmt(GL_RGBA8), m_pbo(0),
        m_filter(GL_LINEAR), m_wrap(GL_CLAMP_TO_EDGE),
        m_size(0,0), m_act_size(0,0)
    {
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
    GLuint  m_filter;
    GLuint  m_wrap;
    QSize   m_size;
    QSize   m_act_size;
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
        gMythGLSetFenceAPPLE(m_fence);
        gMythGLFinishFenceAPPLE(m_fence);
    }
    else if ((m_exts_used & kGLNVFence) &&
             (m_fence && use_fence))
    {
        gMythGLSetFenceNV(m_fence, GL_ALL_COMPLETED_NV);
        gMythGLFinishFenceNV(m_fence);
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
        gMythGLGenFencesAPPLE(1, &m_fence);
        if (m_fence)
            VERBOSE(VB_PLAYBACK, LOC + "Using GL_APPLE_fence");
    }
    else if (m_exts_used & kGLNVFence)
    {
        gMythGLGenFencesNV(1, &m_fence);
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
        gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, m_textures[tex].m_pbo);
        gMythGLBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                             m_textures[tex].m_data_size, NULL, GL_STREAM_DRAW);
        return gMythGLMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
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
        gMythGLUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
        glTexSubImage2D(m_textures[tex].m_type, 0, 0, 0, size.width(),
                        size.height(), m_textures[tex].m_data_fmt,
                        m_textures[tex].m_data_type, 0);
        gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
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
        gMythGLActiveTexture(active_tex);
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
        gMythGLDeleteBuffersARB(1, &(m_textures[tex].m_pbo));
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
    gMythGLGenFramebuffersEXT(1, &glfb);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, glfb);
    glBindTexture(m_textures[tex].m_type, tex);
    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 (GLint) size.width(), (GLint) size.height(), 0,
                 m_textures[tex].m_data_fmt, m_textures[tex].m_data_type, NULL);
    gMythGLFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        m_textures[tex].m_type, tex, 0);

    GLenum status;
    status = gMythGLCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
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
        gMythGLDeleteFramebuffersEXT(1, &glfb);

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
            gMythGLDeleteFramebuffersEXT(1, &(*it));
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
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
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
    gMythGLGenProgramsARB(1, &glfp);
    gMythGLBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, glfp);
    gMythGLProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,
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
    gMythGLGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
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
        gMythGLDeleteProgramsARB(1, &glfp);

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
            gMythGLDeleteProgramsARB(1, &(*it));
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
        gMythGLBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
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

    result = gMythGLCreateProgramObject();
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
    gMythGLDetachObject(obj, vertex);
    gMythGLDetachObject(obj, fragment);
    gMythGLDeleteObject(vertex);
    gMythGLDeleteObject(fragment);
    gMythGLDeleteObject(obj);
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
        gMythGLUseProgram(0);
        m_active_obj = 0;
        doneCurrent();
        return;
    }

    if (!m_shader_objects.contains(obj))
        return;

    makeCurrent();
    gMythGLUseProgram(obj);
    m_active_obj = obj;
    doneCurrent();
}

void MythRenderOpenGL::DrawBitmap(uint tex, uint target, const QRect *src,
                                  const QRect *dst, uint prog, int alpha,
                                  int red, int green, int blue)
{
    if (tex && !m_textures.contains(tex))
        tex = 0;

    if (target && !m_framebuffers.contains(target))
        target = 0;

    if (prog && !m_programs.contains(prog))
        prog = 0;

    double srcx1, srcx2, srcy1, srcy2;

    if (tex && !IsRectTexture(m_textures[tex].m_type))
    {
        srcx1 = src->x() / (double)m_textures[tex].m_size.width();
        srcx2 = srcx1 + src->width() / (double)m_textures[tex].m_size.width();
        srcy1 = src->y() / (double)m_textures[tex].m_size.height();
        srcy2 = srcy1 + src->height() / (double)m_textures[tex].m_size.height();
    }
    else
    {
        srcx1 = src->x();
        srcx2 = srcx1 + src->width();
        srcy1 = src->y();
        srcy2 = srcy1 + src->height();
    }

    int width = std::min(src->width(), dst->width());
    int height = std::min(src->height(), dst->height());

    makeCurrent();

    BindFramebuffer(target);
    EnableFragmentProgram(prog);
    SetBlend(true);
    SetColor(red, green, blue, alpha);
    if (tex)
    {
        EnableTextures(tex);
        glBindTexture(m_textures[tex].m_type, tex);
    }
    else
    {
        DisableTextures();
    }

    glBegin(GL_QUADS);
    glTexCoord2f(srcx1, srcy2); glVertex2f(dst->x(), dst->y());
    glTexCoord2f(srcx2, srcy2); glVertex2f(dst->x() + width, dst->y());
    glTexCoord2f(srcx2, srcy1); glVertex2f(dst->x() + width, dst->y() + height);
    glTexCoord2f(srcx1, srcy1); glVertex2f(dst->x(), dst->y()+height);
    glEnd();

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

    if (prog && !m_programs.contains(prog))
        prog = 0;

    float srcx1, srcx2, srcy1, srcy2;

    uint first = textures[0];
    if (!IsRectTexture(m_textures[first].m_type))
    {
        srcx1 = src->x() / m_textures[first].m_size.width();
        srcx2 = srcx1 + src->width() / m_textures[first].m_size.width();
        srcy1 = src->y() / m_textures[first].m_size.height();
        srcy2 = srcy1 + src->height() / m_textures[first].m_size.height();
    }
    else
    {
        srcx1 = src->x();
        srcx2 = srcx1 + src->width();
        srcy1 = src->y();
        srcy2 = srcy1 + src->height();
    }

    makeCurrent();

    BindFramebuffer(target);
    EnableFragmentProgram(prog);
    if (colour_control)
    {
        InitFragmentParams(0, m_attribs[kGLAttribBrightness],
                           m_attribs[kGLAttribContrast],
                           m_attribs[kGLAttribColour], 0.5f);
    }
    SetBlend(false);
    SetColor(255, 255, 255, 255);

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

    glBegin(GL_QUADS);
    glTexCoord2f(srcx1, srcy1);
    glVertex2f(dst->x(), dst->y());
    glTexCoord2f(srcx2, srcy1);
    glVertex2f(dst->x() + dst->width(), dst->y());
    glTexCoord2f(srcx2, srcy2);
    glVertex2f(dst->x() + dst->width(), dst->y() + dst->height());
    glTexCoord2f(srcx1, srcy2);
    glVertex2f(dst->x(), dst->y() + dst->height());
    glEnd();

    ActiveTexture(GL_TEXTURE0);
    doneCurrent();
}

void MythRenderOpenGL::DrawRect(const QRect &area, bool drawFill,
                                const QColor &fillColor,  bool drawLine,
                                int lineWidth, const QColor &lineColor,
                                int target, int prog)
{
    if (target && !m_framebuffers.contains(target))
        target = 0;

    if (prog && !m_programs.contains(prog))
        prog = 0;

    makeCurrent();

    BindFramebuffer(target);
    EnableFragmentProgram(prog);
    SetBlend(true);
    DisableTextures();

    if (drawFill)
    {
        SetColor(fillColor.red(), fillColor.green(),
                 fillColor.blue(), fillColor.alpha());
        glRectf(area.x(), area.y(), area.x() + area.width(),
                area.y() + area.height());
    }

    if (drawLine)
    {
        SetColor(lineColor.red(), lineColor.green(),
                 lineColor.blue(), lineColor.alpha());
        glLineWidth(lineWidth);

        glBegin(GL_LINES);
        glVertex2f(area.x(), area.y());
        glVertex2f(area.x() + area.width(), area.y());
        glVertex2f(area.x() + area.width(), area.y());
        glVertex2f(area.x() + area.width(), area.y() + area.height());
        glVertex2f(area.x() + area.width(), area.y() + area.height());
        glVertex2f(area.x(), area.y() + area.height());
        glVertex2f(area.x(), area.y() + area.height());
        glVertex2f(area.x(), area.y());
        glEnd();
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
            gMythGLXGetVideoSyncSGI = (MYTH_GLXGETVIDEOSYNCSGIPROC)
                getProcAddress("glXGetVideoSyncSGI");
            gMythGLXWaitVideoSyncSGI = (MYTH_GLXWAITVIDEOSYNCSGIPROC)
                getProcAddress("glXWaitVideoSyncSGI");
        }
#endif
        doneCurrent();
    }
    return gMythGLXGetVideoSyncSGI && gMythGLXWaitVideoSyncSGI;
}

unsigned int MythRenderOpenGL::GetVideoSyncCount(void)
{
    unsigned int count = 0;
    if (HasGLXWaitVideoSyncSGI())
    {
        makeCurrent();
        gMythGLXGetVideoSyncSGI(&count);
        doneCurrent();
    }
    return count;
}

void MythRenderOpenGL::WaitForVideoSync(int div, int rem, unsigned int *count)
{
    if (HasGLXWaitVideoSyncSGI())
    {
        makeCurrent();
        gMythGLXWaitVideoSyncSGI(div, rem, count);
        doneCurrent();
    }
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

    gMythGLActiveTexture = (MYTH_GLACTIVETEXTUREPROC)
        getProcAddress("glActiveTexture");
    gMythGLMapBufferARB = (MYTH_GLMAPBUFFERARBPROC)
        getProcAddress("glMapBufferARB");
    gMythGLBindBufferARB = (MYTH_GLBINDBUFFERARBPROC)
        getProcAddress("glBindBufferARB");
    gMythGLGenBuffersARB = (MYTH_GLGENBUFFERSARBPROC)
        getProcAddress("glGenBuffersARB");
    gMythGLBufferDataARB = (MYTH_GLBUFFERDATAARBPROC)
        getProcAddress("glBufferDataARB");
    gMythGLUnmapBufferARB = (MYTH_GLUNMAPBUFFERARBPROC)
        getProcAddress("glUnmapBufferARB");
    gMythGLDeleteBuffersARB = (MYTH_GLDELETEBUFFERSARBPROC)
        getProcAddress("glDeleteBuffersARB");
    gMythGLGenProgramsARB = (MYTH_GLGENPROGRAMSARBPROC)
        getProcAddress("glGenProgramsARB");
    gMythGLBindProgramARB = (MYTH_GLBINDPROGRAMARBPROC)
        getProcAddress("glBindProgramARB");
    gMythGLProgramStringARB = (MYTH_GLPROGRAMSTRINGARBPROC)
        getProcAddress("glProgramStringARB");
    gMythGLProgramEnvParameter4fARB = (MYTH_GLPROGRAMENVPARAMETER4FARBPROC)
        getProcAddress("glProgramEnvParameter4fARB");
    gMythGLDeleteProgramsARB = (MYTH_GLDELETEPROGRAMSARBPROC)
        getProcAddress("glDeleteProgramsARB");
    gMythGLGetProgramivARB = (MYTH_GLGETPROGRAMIVARBPROC)
        getProcAddress("glGetProgramivARB");
    gMythGLGenFramebuffersEXT = (MYTH_GLGENFRAMEBUFFERSEXTPROC)
        getProcAddress("glGenFramebuffersEXT");
    gMythGLBindFramebufferEXT = (MYTH_GLBINDFRAMEBUFFEREXTPROC)
        getProcAddress("glBindFramebufferEXT");
    gMythGLFramebufferTexture2DEXT = (MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC)
        getProcAddress("glFramebufferTexture2DEXT");
    gMythGLCheckFramebufferStatusEXT = (MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC)
        getProcAddress("glCheckFramebufferStatusEXT");
    gMythGLDeleteFramebuffersEXT = (MYTH_GLDELETEFRAMEBUFFERSEXTPROC)
        getProcAddress("glDeleteFramebuffersEXT");
    gMythGLGenFencesNV = (MYTH_GLGENFENCESNVPROC)
        getProcAddress("glGenFencesNV");
    gMythGLDeleteFencesNV = (MYTH_GLDELETEFENCESNVPROC)
        getProcAddress("glDeleteFencesNV");
    gMythGLSetFenceNV = (MYTH_GLSETFENCENVPROC)
        getProcAddress("glSetFenceNV");
    gMythGLFinishFenceNV = (MYTH_GLFINISHFENCENVPROC)
        getProcAddress("glFinishFenceNV");
    gMythGLGenFencesAPPLE = (MYTH_GLGENFENCESAPPLEPROC)
        getProcAddress("glGenFencesAPPLE");
    gMythGLDeleteFencesAPPLE = (MYTH_GLDELETEFENCESAPPLEPROC)
        getProcAddress("glDeleteFencesAPPLE");
    gMythGLSetFenceAPPLE = (MYTH_GLSETFENCEAPPLEPROC)
        getProcAddress("glSetFenceAPPLE");
    gMythGLFinishFenceAPPLE = (MYTH_GLFINISHFENCEAPPLEPROC)
        getProcAddress("glFinishFenceAPPLE");
    gMythGLCreateShaderObject = (MYTH_GLCREATESHADEROBJECT)
        getProcAddress("glCreateShaderObjectARB");
    gMythGLShaderSource = (MYTH_GLSHADERSOURCE)
        getProcAddress("glShaderSourceARB");
    gMythGLCompileShader = (MYTH_GLCOMPILESHADER)
        getProcAddress("glCompileShaderARB");
    gMythGLCreateProgramObject = (MYTH_GLCREATEPROGRAMOBJECT)
        getProcAddress("glCreateProgramObjectARB");
    gMythGLAttachObject = (MYTH_GLATTACHOBJECT)
        getProcAddress("glAttachObjectARB");
    gMythGLLinkProgram = (MYTH_GLLINKPROGRAM)
        getProcAddress("glLinkProgramARB");
    gMythGLUseProgram = (MYTH_GLUSEPROGRAM)
        getProcAddress("glUseProgramObjectARB");
    gMythGLGetInfoLog = (MYTH_GLGETINFOLOG)
        getProcAddress("glGetInfoLogARB");
    gMythGLGetObjectParameteriv = (MYTH_GLGETOBJECTPARAMETERIV)
        getProcAddress("glGetObjectParameterivARB");
    gMythGLDetachObject = (MYTH_GLDETACHOBJECT)
        getProcAddress("glDetachObjectARB");
    gMythGLDeleteObject = (MYTH_GLDELETEOBJECT)
        getProcAddress("glDeleteObjectARB");
    gMythGLGetUniformLocation = (MYTH_GLGETUNIFORMLOCATION)
        getProcAddress("glGetUniformLocationARB");
    gMythGLUniform4f = (MYTH_GLUNIFORM4F)
        getProcAddress("glUniform4fARB");
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
        gMythGLActiveTexture)
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

    if (m_extensions.contains("GL_ARB_fragment_program") &&
        gMythGLGenProgramsARB   && gMythGLBindProgramARB &&
        gMythGLProgramStringARB && gMythGLDeleteProgramsARB &&
        gMythGLGetProgramivARB  && gMythGLProgramEnvParameter4fARB)
        m_exts_supported += kGLExtFragProg;

    if (m_extensions.contains("GL_ARB_shader_objects") &&
        m_extensions.contains("GL_ARB_vertex_shader") &&
        m_extensions.contains("GL_ARB_fragment_shader") &&
        gMythGLShaderSource  && gMythGLCreateShaderObject &&
        gMythGLCompileShader && gMythGLCreateProgramObject &&
        gMythGLAttachObject  && gMythGLLinkProgram &&
        gMythGLUseProgram    && gMythGLGetInfoLog &&
        gMythGLDetachObject  && gMythGLGetObjectParameteriv &&
        gMythGLDeleteObject  && gMythGLGetUniformLocation &&
        gMythGLUniform4f)
        m_exts_supported += kGLSL;

    if (m_extensions.contains("GL_EXT_framebuffer_object") &&
        gMythGLGenFramebuffersEXT      && gMythGLBindFramebufferEXT &&
        gMythGLFramebufferTexture2DEXT && gMythGLDeleteFramebuffersEXT &&
        gMythGLCheckFramebufferStatusEXT)
        m_exts_supported += kGLExtFBufObj;

    if(m_extensions.contains("GL_ARB_pixel_buffer_object") &&
        gMythGLMapBufferARB  && gMythGLBindBufferARB &&
        gMythGLGenBuffersARB && gMythGLDeleteBuffersARB &&
        gMythGLBufferDataARB && gMythGLUnmapBufferARB)
        m_exts_supported += kGLExtPBufObj;

    if(m_extensions.contains("GL_NV_fence") &&
        gMythGLGenFencesNV && gMythGLDeleteFencesNV &&
        gMythGLSetFenceNV  && gMythGLFinishFenceNV)
        m_exts_supported += kGLNVFence;

    if(m_extensions.contains("GL_APPLE_fence") &&
        gMythGLGenFencesAPPLE && gMythGLDeleteFencesAPPLE &&
        gMythGLSetFenceAPPLE  && gMythGLFinishFenceAPPLE)
        m_exts_supported += kGLAppleFence;

    if (m_extensions.contains("GL_MESA_ycbcr_texture"))
        m_exts_supported += kGLMesaYCbCr;

    if (m_extensions.contains("GL_APPLE_rgb_422"))
        m_exts_supported += kGLAppleRGB422;

    if (m_extensions.contains("GL_APPLE_ycbcr_422"))
        m_exts_supported += kGLAppleYCbCr;

    if (m_extensions.contains("GL_SGIS_generate_mipmap"))
        m_exts_supported += kGLMipMaps;

    m_exts_used = m_exts_supported;

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

    m_extensions      = QString();
    m_exts_supported  = kGLFeatNone;
    m_exts_used       = kGLFeatNone;
    m_max_tex_size    = 0;
    m_max_units       = 0;
    m_default_texture_type = GL_TEXTURE_2D;

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

    gMythGLActiveTexture = NULL;
    gMythGLGenProgramsARB = NULL;
    gMythGLBindProgramARB = NULL;
    gMythGLProgramStringARB = NULL;
    gMythGLProgramEnvParameter4fARB = NULL;
    gMythGLDeleteProgramsARB = NULL;
    gMythGLGetProgramivARB = NULL;
    gMythGLMapBufferARB = NULL;
    gMythGLBindBufferARB = NULL;
    gMythGLGenBuffersARB = NULL;
    gMythGLBufferDataARB = NULL;
    gMythGLUnmapBufferARB = NULL;
    gMythGLDeleteBuffersARB = NULL;
    gMythGLGenFramebuffersEXT = NULL;
    gMythGLBindFramebufferEXT = NULL;
    gMythGLFramebufferTexture2DEXT = NULL;
    gMythGLCheckFramebufferStatusEXT = NULL;
    gMythGLDeleteFramebuffersEXT = NULL;
    gMythGLGenFencesNV = NULL;
    gMythGLDeleteFencesNV = NULL;
    gMythGLSetFenceNV = NULL;
    gMythGLFinishFenceNV = NULL;
    gMythGLGenFencesAPPLE = NULL;
    gMythGLDeleteFencesAPPLE = NULL;
    gMythGLSetFenceAPPLE = NULL;
    gMythGLFinishFenceAPPLE = NULL;
    gMythGLCreateShaderObject = NULL;
    gMythGLShaderSource = NULL;
    gMythGLCompileShader = NULL;
    gMythGLCreateProgramObject = NULL;
    gMythGLAttachObject = NULL;
    gMythGLLinkProgram = NULL;
    gMythGLUseProgram = NULL;
    gMythGLGetInfoLog = NULL;
    gMythGLGetObjectParameteriv = NULL;
    gMythGLDetachObject = NULL;
    gMythGLDeleteObject = NULL;
    gMythGLGetUniformLocation = NULL;
    gMythGLUniform4f = NULL;
}

uint MythRenderOpenGL::CreatePBO(uint tex)
{
    if (!(m_exts_used & kGLExtPBufObj))
        return 0;

    if (!m_textures.contains(tex))
        return 0;

    gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    glTexImage2D(m_textures[tex].m_type, 0, m_textures[tex].m_internal_fmt,
                 m_textures[tex].m_size.width(),
                 m_textures[tex].m_size.height(), 0,
                 m_textures[tex].m_data_fmt, m_textures[tex].m_data_type, NULL);

    GLuint tmp_pbo;
    gMythGLGenBuffersARB(1, &tmp_pbo);
    gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

    Flush(true);
    return tmp_pbo;
}

void MythRenderOpenGL::DeleteOpenGLResources(void)
{
    VERBOSE(VB_GENERAL, LOC + "Deleting OpenGL Resources");

    DeletePrograms();
    DeleteTextures();
    DeleteFrameBuffers();
    DeleteShaderObjects();
    Flush(true);

    if (m_fence)
    {
        if (m_exts_supported & kGLAppleFence)
            gMythGLDeleteFencesAPPLE(1, &m_fence);
        else if(m_exts_supported & kGLNVFence)
            gMythGLDeleteFencesNV(1, &m_fence);
        m_fence = 0;
    }

    Flush(false);
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
            gMythGLDeleteBuffersARB(1, &(it.value().m_pbo));
    }
    m_textures.clear();
    Flush(true);
}

void MythRenderOpenGL::DeletePrograms(void)
{
    QVector<GLuint>::iterator it;
    for (it = m_programs.begin(); it != m_programs.end(); ++it)
        gMythGLDeleteProgramsARB(1, &(*(it)));
    m_programs.clear();
    Flush(true);
}

void MythRenderOpenGL::DeleteFrameBuffers(void)
{
    QVector<GLuint>::iterator it;
    for (it = m_framebuffers.begin(); it != m_framebuffers.end(); ++it)
        gMythGLDeleteFramebuffersEXT(1, &(*(it)));
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
        gMythGLDetachObject(object, vertex);
        gMythGLDetachObject(object, fragment);
        gMythGLDeleteObject(vertex);
        gMythGLDeleteObject(fragment);
        gMythGLDeleteObject(object);
    }
    m_shader_objects.clear();
    Flush(true);
}

uint MythRenderOpenGL::CreateShader(int type, const QString source)
{
    uint result = gMythGLCreateShaderObject(type);
    QByteArray src = source.toAscii();
    const char* tmp[1] = { src.constData() };
    gMythGLShaderSource(result, 1, tmp, NULL);
    gMythGLCompileShader(result);
    return result;
}

bool MythRenderOpenGL::ValidateShaderObject(uint obj)
{
    if (!m_shader_objects.contains(obj))
        return false;
    if (!m_shader_objects[obj].m_fragment_shader ||
        !m_shader_objects[obj].m_vertex_shader)
        return false;

    gMythGLAttachObject(obj, m_shader_objects[obj].m_fragment_shader);
    gMythGLAttachObject(obj, m_shader_objects[obj].m_vertex_shader);
    gMythGLLinkProgram(obj);
    return CheckObjectStatus(obj);
}

bool MythRenderOpenGL::CheckObjectStatus(uint obj)
{
    int ok;
    gMythGLGetObjectParameteriv(obj, GL_OBJECT_LINK_STATUS_ARB, &ok);
    if (ok > 0)
        return true;

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to link shader object."));
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    gMythGLGetObjectParameteriv(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB,
                                &infologLength);
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        gMythGLGetInfoLog(obj, infologLength, &charsWritten, infoLog);
        VERBOSE(VB_IMPORTANT, QString("\n\n%1").arg(infoLog));
        free(infoLog);
    }
    return false;
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
    gMythGLProgramEnvParameter4fARB(
        GL_FRAGMENT_PROGRAM_ARB, fp, a, b, c, d);
    doneCurrent();
}
