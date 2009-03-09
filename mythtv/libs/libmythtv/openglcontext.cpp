// MythTV headers
#include "mythcontext.h"
#include "openglcontext.h"

#include "util-opengl.h"

#define LOC QString("GLCtx: ")
#define LOC_ERR QString("GLCtx, Error: ")

OpenGLContextLocker::OpenGLContextLocker(OpenGLContext *ctx) : m_ctx(ctx)
{
    if (m_ctx)
        m_ctx->MakeCurrent(true);
}
OpenGLContextLocker::~OpenGLContextLocker()
{
    if (m_ctx)
        m_ctx->MakeCurrent(false);
}

class MythGLTexture
{
  public:
    MythGLTexture() :
        m_type(GL_TEXTURE_2D), m_data(NULL), m_data_size(0),
        m_data_type(GL_UNSIGNED_BYTE), m_data_fmt(GL_BGRA),
        m_internal_fmt(GL_RGBA8), m_pbo(0),
        m_filter(GL_LINEAR), m_wrap(GL_CLAMP_TO_EDGE),
        m_size(0,0), m_vid_size(0,0)
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
    QSize   m_vid_size;
};

class PrivateContext
{
  public:
    PrivateContext() :
        m_texture_type(0), m_fence(0),
        m_active_tex(0), m_active_prog(0)
    {
    }

    ~PrivateContext()
    {
    }

    int            m_texture_type;
    map<GLuint, MythGLTexture> m_textures;
    vector<GLuint> m_programs;
    vector<GLuint> m_framebuffers;
    GLuint         m_fence;
    GLuint         m_active_tex;
    GLuint         m_active_prog;
};

OpenGLContext *OpenGLContext::Create(QMutex *lock)
{
    OpenGLContext *ctx = NULL;
#ifdef USING_X11
    ctx = new OpenGLContextGLX(lock);
#endif

#ifdef USING_MINGW
    ctx = new OpenGLContextWGL(lock);
#endif

#ifdef Q_WS_MACX
    ctx = new OpenGLContextAGL(lock);
#endif
    return ctx;
}

OpenGLContext::OpenGLContext(QMutex *lock) :
    m_priv(new PrivateContext()), m_extensions(QString::null),
    m_ext_supported(0), m_ext_used(0),
    m_max_tex_size(0), m_viewport(0,0),
    m_lock(lock), m_lock_level(0),
    m_colour_control(false)
{
}

OpenGLContext::~OpenGLContext()
{
    if (m_priv)
    {
        delete m_priv;
        m_priv = NULL;
    }
}

bool OpenGLContext::CreateCommon(bool colour_control, QRect display_visible)
{
    static bool debugged = false;

    m_colour_control = colour_control;
    m_window_rect = display_visible;

    MakeCurrent(true);

    if (!init_opengl())
    {
        MakeCurrent(false);
        return false;
    }

    GLint maxtexsz = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
    m_max_tex_size = (maxtexsz) ? maxtexsz : 512;
    m_extensions = (const char*) glGetString(GL_EXTENSIONS);

    if (!debugged)
    {
        debugged = true;
      
        VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL vendor  : %1")
                .arg((const char*) glGetString(GL_VENDOR)));
        VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL renderer: %1")
                .arg((const char*) glGetString(GL_RENDERER)));
        VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL version : %1")
                .arg((const char*) glGetString(GL_VERSION)));
        VERBOSE(VB_PLAYBACK, LOC + QString("Max texture size: %1 x %2")
                .arg(m_max_tex_size).arg(m_max_tex_size));
    }

    m_ext_supported |=
        ((get_gl_texture_rect_type(m_extensions)) ? kGLExtRect : 0) |
        ((has_gl_fragment_program_support(m_extensions)) ?
         kGLExtFragProg : 0) |
        ((has_gl_pixelbuffer_object_support(m_extensions)) ?
         kGLExtPBufObj : 0) |
        ((has_gl_fbuffer_object_support(m_extensions)) ? kGLExtFBufObj : 0) |
        ((has_gl_applefence_support(m_extensions)) ? kGLAppleFence : 0) |
        ((has_wgl_swapinterval_support(m_extensions)) ? kGLWGLSwap : 0) |
        ((has_gl_nvfence_support(m_extensions)) ? kGLNVFence : 0) | kGLFinish;

    m_ext_used = m_ext_supported;

    Init2DState();
    MakeCurrent(false);

    return true;
}

void OpenGLContext::DeleteOpenGLResources(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Deleting OpenGL Resources");

    if (!m_priv)
        return;

    MakeCurrent(true);

    DeletePrograms();
    DeleteTextures();
    DeleteFrameBuffers();

    Flush(true);

    if (m_priv->m_fence)
    {
        if (m_ext_supported & kGLAppleFence)
        {
            gMythGLDeleteFencesAPPLE(1, &(m_priv->m_fence));
        }
        else if(m_ext_supported & kGLNVFence)
        {
            gMythGLDeleteFencesNV(1, &(m_priv->m_fence));
        }
    }

    Flush(false);

    MakeCurrent(false);
}

bool OpenGLContext::MakeCurrent(bool current)
{
    bool ok = true;

    if (current)
    {
        m_lock->lock();
        if (m_lock_level == 0)
        {
            MakeContextCurrent(true);
        }
        m_lock_level++;
    }
    else
    {
        m_lock_level--;
        if (m_lock_level == 0)
        {
            MakeContextCurrent(false);
        }
        else if (m_lock_level < 0)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + "Mis-matched calls to MakeCurrent");
        }
        m_lock->unlock();
    }

    if (!ok)
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Could not make context current.");

    return ok;
}

void OpenGLContext::Flush(bool use_fence)
{
    MakeCurrent(true);

    if ((m_ext_used & kGLAppleFence) &&
        m_priv->m_fence && use_fence)
    {
        gMythGLSetFenceAPPLE(m_priv->m_fence);
        gMythGLFinishFenceAPPLE(m_priv->m_fence);
    }
    else if ((m_ext_used & kGLNVFence) &&
             m_priv->m_fence && use_fence)
    {
        gMythGLSetFenceNV(m_priv->m_fence, GL_ALL_COMPLETED_NV);
        gMythGLFinishFenceNV(m_priv->m_fence);
    }
    else
    {
        glFlush();
    }
    
    MakeCurrent(false);
}

void OpenGLContext::ActiveTexture(uint active_tex)
{
    MakeCurrent(true);

    if (m_priv->m_active_tex != active_tex)
    {
        gMythGLActiveTexture(active_tex);
        m_priv->m_active_tex = active_tex;
    }

    MakeCurrent(false);
}

void OpenGLContext::EnableTextures(uint tex, uint tex_type)
{
    MakeCurrent(true);

    int type = tex ? m_priv->m_textures[tex].m_type : tex_type;

    if (type != m_priv->m_texture_type)
    {
        if (m_priv->m_texture_type)
        {
            glDisable(m_priv->m_texture_type);
        }
        glEnable(type);
        m_priv->m_texture_type = type;
    }

    MakeCurrent(false);
}

void OpenGLContext::DisableTextures(void)
{
    MakeCurrent(true);

    glDisable(m_priv->m_texture_type);
    m_priv->m_texture_type = 0;

    MakeCurrent(false);
}

void OpenGLContext::UpdateTexture(uint tex,
                                  const unsigned char *buf,
                                  const int *offsets,
                                  const int *pitches,
                                  VideoFrameType fmt,
                                  bool interlaced,
                                  const unsigned char* alpha)
{
    MakeCurrent(true);

    MythGLTexture *tmp_tex = &m_priv->m_textures[tex];
    QSize size = tmp_tex->m_vid_size;

    EnableTextures(tex);
    glBindTexture(tmp_tex->m_type, tex);

    if (tmp_tex->m_pbo)
    {
        void *pboMemory;

        gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, tmp_tex->m_pbo);
        gMythGLBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                             tmp_tex->m_data_size, NULL, GL_STREAM_DRAW);

        pboMemory = gMythGLMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB,
                                        GL_WRITE_ONLY);

        if (FMT_BGRA == fmt)
        {
            memcpy(pboMemory, buf, tmp_tex->m_data_size);
        }
        else if (interlaced)
        {
            pack_yv12interlaced(buf, (unsigned char *)pboMemory,
                                offsets, pitches, size);
        }
        else
        {
            pack_yv12alpha(buf, (unsigned char *)pboMemory,
                           offsets, pitches, size, alpha);
        }

        gMythGLUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);

        glTexSubImage2D(tmp_tex->m_type, 0, 0, 0, size.width(), size.height(),
                        tmp_tex->m_data_fmt, tmp_tex->m_data_type, 0);

        gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    }
    else
    {
        if (!tmp_tex->m_data)
        {
            unsigned char *scratch = new unsigned char[tmp_tex->m_data_size];
            if (scratch)
            {
                bzero(scratch, tmp_tex->m_data_size);
                tmp_tex->m_data = scratch;
            }
        }

        if (tmp_tex->m_data)
        {
            const unsigned char *tmp = tmp_tex->m_data;

            if (FMT_BGRA == fmt)
            {
                tmp = buf;
            }
            else if (interlaced)
            {
                pack_yv12interlaced(buf, tmp,
                                    offsets, pitches, size);
            }
            else
            {
                pack_yv12alpha(buf, tmp, offsets,
                               pitches, size, alpha);
            }

            glTexSubImage2D(tmp_tex->m_type, 0, 0, 0,
                            size.width(), size.height(),
                            tmp_tex->m_data_fmt, tmp_tex->m_data_type,
                            tmp);
        }
    }

    MakeCurrent(false);
}

uint OpenGLContext::CreateTexture(QSize tot_size, QSize vid_size,
                                  bool use_pbo,
                                  uint type, uint data_type,
                                  uint data_fmt, uint internal_fmt,
                                  uint filter, uint wrap)
{
    if ((uint)tot_size.width() > m_max_tex_size ||
        (uint)tot_size.height() > m_max_tex_size)
        return 0;

    MakeCurrent(true);

    EnableTextures(0, type);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(type, tex);

    if (tex)
    {
        MythGLTexture *texture = new MythGLTexture();
        texture->m_type = type;
        texture->m_data_type = data_type;
        texture->m_data_fmt = data_fmt;
        texture->m_internal_fmt = internal_fmt;
        texture->m_size = tot_size;
        texture->m_vid_size = vid_size;
        texture->m_data_size = GetBufferSize(vid_size, data_fmt, data_type);
        m_priv->m_textures[tex] = *texture;

        if (ClearTexture(tex) && m_priv->m_textures[tex].m_data_size)
        {
            SetTextureFilters(tex, filter, wrap);
            if (use_pbo)
                m_priv->m_textures[tex].m_pbo = CreatePBO(tex);
        }
        else
        {
            DeleteTexture(tex);
            tex = 0;
        }

        delete texture;
    }

    Flush(true);

    MakeCurrent(false);

    return tex;
}

uint OpenGLContext::GetBufferSize(QSize size, uint fmt, uint type)
{
    uint bytes;
    uint bpp;

    switch (fmt)
    {
        case GL_BGRA:
        case GL_RGBA:
            bpp = 4;
            break;
        default:
            bpp =0;
    }

    switch (type)
    {
        case GL_UNSIGNED_BYTE:
            bytes = sizeof(GLubyte);
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

bool OpenGLContext::ClearTexture(uint tex)
{
    MythGLTexture *tmp = &m_priv->m_textures[tex];
    QSize size = tmp->m_size;

    uint tmp_size = GetBufferSize(size, tmp->m_data_fmt, tmp->m_data_type);

    if (!tmp_size)
        return false;

    unsigned char *scratch = new unsigned char[tmp_size];

    if (!scratch)
        return false;

    bzero(scratch, tmp_size);

    GLint check;
    if (tmp->m_type == GL_TEXTURE_1D)
    {
        glTexImage1D(tmp->m_type, 0, tmp->m_internal_fmt,
                     size.width(), 0,
                     tmp->m_data_fmt , tmp->m_data_type, scratch);
    }
    else
    {
        glTexImage2D(tmp->m_type, 0, tmp->m_internal_fmt,
                     size.width(), size.height(), 0,
                     tmp->m_data_fmt , tmp->m_data_type, scratch);
    }
    glGetTexLevelParameteriv(tmp->m_type, 0, GL_TEXTURE_WIDTH, &check);

    delete [] scratch;

    return (check == size.width());
}

void OpenGLContext::SetTextureFilters(uint tex, uint filt, uint wrap)
{
    if (!m_priv->m_textures.count(tex))
        return;

    MakeCurrent(true);

    EnableTextures(tex);

    m_priv->m_textures[tex].m_filter = filt;
    m_priv->m_textures[tex].m_wrap = wrap;

    uint type = m_priv->m_textures[tex].m_type;

    glBindTexture(type, tex);
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, filt);
    glTexParameteri(type, GL_TEXTURE_WRAP_S, wrap);
    if (type != GL_TEXTURE_1D)
        glTexParameteri(type, GL_TEXTURE_WRAP_T, wrap);

    MakeCurrent(false);
}

void OpenGLContext::DeleteTexture(uint tex)
{
    if (!m_priv->m_textures.count(tex))
        return;

    MakeCurrent(true);

    GLuint gltex = tex;
    glDeleteTextures(1, &gltex);

    if (m_priv->m_textures[tex].m_data)
    {
        delete m_priv->m_textures[tex].m_data;
    }

    if (m_priv->m_textures[tex].m_pbo)
    {
        gMythGLDeleteBuffersARB(1, &(m_priv->m_textures[tex].m_pbo));
    }

    m_priv->m_textures.erase(tex);

    Flush(true);

    MakeCurrent(false);
}

void OpenGLContext::DeleteTextures(void)
{
    map<GLuint, MythGLTexture>::iterator it;
    for (it = m_priv->m_textures.begin(); it !=m_priv->m_textures.end(); it++)
    {
        GLuint gltex = it->first;
        glDeleteTextures(1, &gltex);

        if (it->second.m_data)
        {
            delete it->second.m_data;
        }

        if (it->second.m_pbo)
        {
            gltex = it->second.m_pbo;
            gMythGLDeleteBuffersARB(1, &gltex);
        }
    }
    m_priv->m_textures.clear();

    Flush(true);
}

void OpenGLContext::GetTextureType(uint &current, bool &rect)
{
    uint type = get_gl_texture_rect_type(m_extensions);
    if (type)
    {
        rect = true;
        current = type;
        return;
    }

    rect = false;
    return;
}

bool OpenGLContext::CreateFragmentProgram(const QString &program, uint &fp)
{
    bool success = true;

    if (!(m_ext_used & kGLExtFragProg))
        return false;

    GLint error;

    MakeCurrent(true);

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
    {
        m_priv->m_programs.push_back(glfp);
    }
    else
    {
        gMythGLDeleteProgramsARB(1, &glfp);
    }

    Flush(true);

    MakeCurrent(false);

    fp = glfp;

    return success;
}

void OpenGLContext::DeleteFragmentProgram(uint fp)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_programs.begin(); it != m_priv->m_programs.end(); it++)
    {
        if (*(it) == fp)
        {
            GLuint glfp = fp;
            gMythGLDeleteProgramsARB(1, &glfp);
            m_priv->m_programs.erase(it);
            break;
        }
    }

    Flush(true);

    MakeCurrent(false);
}

void OpenGLContext::EnableFragmentProgram(uint fp)
{
    if ((!fp && !m_priv->m_active_prog) ||
        (fp && (fp == m_priv->m_active_prog)))
        return;

    MakeCurrent(true);

    if (!fp && m_priv->m_active_prog)
    {
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        m_priv->m_active_prog = 0;
        MakeCurrent(false);
        return;
    }

    if (fp && !m_priv->m_active_prog)
    {
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    }

    if (fp != m_priv->m_active_prog)
    {
        gMythGLBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
        m_priv->m_active_prog = fp;
    }
    MakeCurrent(false);
}

void OpenGLContext::InitFragmentParams(
    uint fp, float a, float b, float c, float d)
{
    MakeCurrent(true);
    gMythGLProgramEnvParameter4fARB(
        GL_FRAGMENT_PROGRAM_ARB, fp, a, b, c, d);
    MakeCurrent(false);
}

void OpenGLContext::DeletePrograms(void)
{
    vector<GLuint>::iterator it;
    for (it = m_priv->m_programs.begin(); it != m_priv->m_programs.end(); it++)
        gMythGLDeleteProgramsARB(1, &(*(it)));
    m_priv->m_programs.clear();

    Flush(true);
}

bool OpenGLContext::CreateFrameBuffer(uint &fb, uint tex)
{
    if (!(m_ext_used & kGLExtFBufObj))
        return false;

    if (!m_priv->m_textures.count(tex))
        return false;

    MythGLTexture *tmp = &m_priv->m_textures[tex];
    QSize size = tmp->m_size;
    GLuint glfb;

    MakeCurrent(true);
    glCheck();

    EnableTextures(tex);

    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0, 0, size.width(), size.height());
    gMythGLGenFramebuffersEXT(1, &glfb);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, glfb);
    glBindTexture(tmp->m_type, tex);
    glTexImage2D(tmp->m_type, 0, tmp->m_internal_fmt,
                 (GLint) size.width(), (GLint) size.height(), 0,
                 tmp->m_data_fmt, tmp->m_data_type, NULL);
    gMythGLFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        tmp->m_type, tex, 0);

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
        m_priv->m_framebuffers.push_back(glfb);
    else
        gMythGLDeleteFramebuffersEXT(1, &glfb);

    Flush(true);

    glCheck();
    MakeCurrent(false);

    fb = glfb;

    return success;
}

void OpenGLContext::DeleteFrameBuffer(uint fb)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_framebuffers.begin();
         it != m_priv->m_framebuffers.end(); it++)
    {
        if (*(it) == fb)
        {
            GLuint glfb = fb;
            gMythGLDeleteFramebuffersEXT(1, &glfb);
            m_priv->m_framebuffers.erase(it);
            break;
        }
    }

    Flush(true);

    MakeCurrent(false);
}

void OpenGLContext::DeleteFrameBuffers(void)
{
    vector<GLuint>::iterator it;
    for (it = m_priv->m_framebuffers.begin();
         it != m_priv->m_framebuffers.end(); it++)
    {
        gMythGLDeleteFramebuffersEXT(1, &(*(it)));
    }
    m_priv->m_framebuffers.clear();

    Flush(true);
}

void OpenGLContext::BindFramebuffer(uint fb)
{
    MakeCurrent(true);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
    MakeCurrent(false);
}

void OpenGLContext::Init2DState(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // for gl osd
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_FLAT);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Flush(true);
}

void OpenGLContext::SetViewPort(const QSize &size)
{
    if (size.width() == m_viewport.width() &&
        size.height() == m_viewport.height())
        return;

    MakeCurrent(true);

    m_viewport = size;

    glViewport(0, 0, size.width(), size.height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, size.width() - 1,
            0, size.height() - 1, 1, -1); // aargh...
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    MakeCurrent(false);
}

uint OpenGLContext::CreatePBO(uint tex)
{
    if (!(m_ext_used & kGLExtPBufObj))
        return 0;

    if (!m_priv->m_textures.count(tex))
        return 0;

    MythGLTexture *tmp = &m_priv->m_textures[tex];

    gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    glTexImage2D(tmp->m_type, 0, tmp->m_internal_fmt,
                 tmp->m_size.width(), tmp->m_size.height(), 0,
                 tmp->m_data_fmt, tmp->m_data_type, NULL);

    GLuint tmp_pbo;
    gMythGLGenBuffersARB(1, &tmp_pbo);

    gMythGLBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

    Flush(true);

    return tmp_pbo;
}

uint OpenGLContext::CreateHelperTexture(void)
{
    MakeCurrent(true);

    uint width = m_max_tex_size;

    uint tmp_tex = CreateTexture(QSize(width, 1), QSize(width, 1),
                                 false,
                                 GL_TEXTURE_1D, GL_FLOAT,
                                 GL_RGBA, GL_RGBA16,
                                 GL_NEAREST, GL_REPEAT);

    if (!tmp_tex)
    {
        DeleteTexture(tmp_tex);
        return 0;
    }

    float *buf = NULL;
    buf = new float[m_priv->m_textures[tmp_tex].m_data_size];
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
    glBindTexture(m_priv->m_textures[tmp_tex].m_type, tmp_tex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, width, 0, GL_RGBA, GL_FLOAT, buf);

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Created bicubic helper texture (%1 samples)")
            .arg(width));

    delete [] buf;

    MakeCurrent(false);

    return tmp_tex;
}

int OpenGLContext::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    if (!m_colour_control)
        return -1;

    MakeCurrent(true);

    int ret = -1;
    switch (attribute)
    {
        case kPictureAttribute_Brightness:
            ret = newValue;
            pictureAttribs[attribute] = (newValue * 0.02f) - 0.5f;
            break;
        case kPictureAttribute_Contrast:
        case kPictureAttribute_Colour:
            ret = newValue;
            pictureAttribs[attribute] = (newValue * 0.02f);
            break;
        case kPictureAttribute_Hue: // not supported yet...
            break;
        default:
            break;
    }

    MakeCurrent(false);

    return ret;
}

PictureAttributeSupported 
OpenGLContext::GetSupportedPictureAttributes(void) const
{
    return (!m_colour_control) ?
        kPictureAttributeSupported_None :
        (PictureAttributeSupported) 
        (kPictureAttributeSupported_Brightness |
         kPictureAttributeSupported_Contrast |
         kPictureAttributeSupported_Colour);
}

void OpenGLContext::SetColourParams(void)
{
    if (!m_colour_control)
        return;

    MakeCurrent(true);

    InitFragmentParams(0,
                       pictureAttribs[kPictureAttribute_Brightness],
                       pictureAttribs[kPictureAttribute_Contrast],
                       pictureAttribs[kPictureAttribute_Colour],
                       0.5f);

    MakeCurrent(false);    
}

void OpenGLContext::SetFence(void)
{
    MakeCurrent(true);
    if (m_ext_used & kGLAppleFence)
    {
        gMythGLGenFencesAPPLE(1, &(m_priv->m_fence));
        if (m_priv->m_fence)
            VERBOSE(VB_PLAYBACK, LOC + "Using GL_APPLE_fence");
    }
    else if (m_ext_used & kGLNVFence)
    {
        gMythGLGenFencesNV(1, &(m_priv->m_fence));
        if (m_priv->m_fence)
            VERBOSE(VB_PLAYBACK, LOC + "Using GL_NV_fence");
    }
    MakeCurrent(false);
}

#ifdef USING_X11

OpenGLContextGLX::OpenGLContextGLX(QMutex *lock)
    : OpenGLContext(lock),
      m_display(NULL), m_created_display(false), m_screen_num(0),
      m_major_ver(1), m_minor_ver(1),
      m_glx_fbconfig(0), m_gl_window(0), m_glx_window(0),
      m_glx_context(NULL), m_vis_info(NULL), m_attr_list(NULL)
{
}

OpenGLContextGLX::~OpenGLContextGLX()
{
    DeleteOpenGLResources();
    DeleteWindowResources();
}

void OpenGLContextGLX::Show(void)
{
    MakeCurrent(true);
    MapWindow();
    MakeCurrent(false);
}

void OpenGLContextGLX::MapWindow(void)
{
    if (m_display && m_priv && m_gl_window)
        X11S(XMapWindow(m_display, m_gl_window));
}

void OpenGLContextGLX::Hide(void)
{
    MakeCurrent(true);
    UnmapWindow();
    MakeCurrent(false);
}

void OpenGLContextGLX::UnmapWindow(void)
{
    if (m_display && m_priv && m_gl_window)
        X11S(XUnmapWindow(m_display, m_gl_window));
}

bool OpenGLContextGLX::Create(WId window, const QRect &display_visible,
                              bool colour_control)
{
    m_display = MythXOpenDisplay();
    if (!m_display)
        return false;
    m_created_display = true;
    X11S(m_screen_num = DefaultScreen(m_display));

    bool show_window = true;

    if (!window)
    {
        X11S(window = DefaultRootWindow(m_display));
        show_window = false;
    }

    if (!window)
        return false;

    return Create(m_display, window, m_screen_num,
                  display_visible, colour_control, show_window);
}

bool OpenGLContextGLX::Create(
    Display *XJ_disp, Window XJ_curwin, uint screen_num,
    const QRect &display_visible, bool colour_control,
    bool map_window)
{
    m_display = XJ_disp;
    m_screen_num = screen_num;
    uint major, minor;

    if (!get_glx_version(XJ_disp, major, minor))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "GLX extension not present.");
        return false;
    }

    m_major_ver = major;
    m_minor_ver = minor;

    if ((1 == major) && (minor < 2))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString(
                    "Need GLX 1.2 or better, have %1.%2")
                .arg(major).arg(minor));

        return false;
    } 
    else if ((1 == major) && (minor == 2))
    {
        m_attr_list = get_attr_cfg(kRenderRGBA);
        X11S(m_vis_info = glXChooseVisual(
                 m_display, m_screen_num, (int*) m_attr_list));
        if (!m_vis_info)
        {
            m_attr_list = get_attr_cfg(kSimpleRGBA);
            X11S(m_vis_info = glXChooseVisual(
                     m_display, m_screen_num, (int*) m_attr_list));
        }
        if (!m_vis_info)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "No appropriate visual found");
            return false;
        }
        X11S(m_glx_context = glXCreateContext(
                 XJ_disp, m_vis_info, None, GL_TRUE));
    }
    else
    {
        m_attr_list = get_attr_cfg(kRenderRGBA);
        m_glx_fbconfig = get_fbuffer_cfg(
            XJ_disp, m_screen_num, m_attr_list);

        if (!m_glx_fbconfig)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "No framebuffer "
                    "with needed OpenGL attributes.");

            return false;
        }


        X11S(m_glx_context = glXCreateNewContext(
                 m_display, m_glx_fbconfig,
                 GLX_RGBA_TYPE, NULL, GL_TRUE));
    }

    if (!m_glx_context)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to create GLX context");

        return false;
    }

    if ((1 == major) && (minor > 2))
    {
        X11S(m_vis_info = glXGetVisualFromFBConfig(XJ_disp, m_glx_fbconfig));
    }

    m_gl_window = get_gl_window(XJ_disp, XJ_curwin,
                                m_vis_info, display_visible, map_window);

    if (!m_gl_window)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't create OpenGL Window");

        return false;
    }

    if ((1 == major) && (minor > 2))
    {
        X11S(m_glx_window = glXCreateWindow(
                 m_display, m_glx_fbconfig, m_gl_window, NULL));

        if (!m_glx_window)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't create GLX Window");

            return false;
        }
    }
   
    VERBOSE(VB_PLAYBACK, LOC + QString("Created window and GLX context."));

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;

        bool direct = false; 
        X11S(direct = glXIsDirect(m_display, m_glx_context));

        VERBOSE(VB_PLAYBACK, LOC + QString("GLX Version: %1.%2")
                .arg(major).arg(minor));
        VERBOSE(VB_PLAYBACK, LOC + QString("Direct rendering: %1")
                .arg(direct ? "Yes" : "No"));
    }

    const char *xt = NULL;
    X11S(xt = glXQueryExtensionsString(m_display, m_screen_num));
    const QString glx_ext = xt;
    m_ext_supported = ((minor >= 3) ? kGLXPBuffer : 0) |
        (has_glx_swapinterval_support(glx_ext) ? kGLGLXSwap : 0);

    if (map_window)
        Show();

    return CreateCommon(colour_control, display_visible);
}

bool OpenGLContextGLX::MakeContextCurrent(bool current)
{
    bool ok = true;
    if (current)
    {
        if (IsGLXSupported(1,3))
        {
            X11S(ok = glXMakeContextCurrent(m_display,
                                            m_glx_window,
                                            m_glx_window,
                                            m_glx_context));
        }
        else
        {
            X11S(ok = glXMakeCurrent(m_display,
                                     m_gl_window,
                                     m_glx_context));
        }
    }
    else
    {
        if (IsGLXSupported(1,3))
        {
            X11S(ok = glXMakeContextCurrent(m_display, None, None, NULL));
        }
        else
        {
            X11S(ok = glXMakeCurrent(m_display, None, NULL));
        }
    }

    return ok;
}

void OpenGLContextGLX::SwapBuffers(void)
{
    MakeCurrent(true);

    if (m_ext_used & kGLFinish)
        glFinish();

    if (IsGLXSupported(1,3))
        X11S(glXSwapBuffers(m_display, m_glx_window));
    else
        X11S(glXSwapBuffers(m_display, m_gl_window));

    MakeCurrent(false);
}

void OpenGLContextGLX::DeleteWindowResources(void)
{
    MakeCurrent(true);

    if (m_glx_window)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Destroying glx window");
        X11S(glXDestroyWindow(m_display, m_glx_window));
        m_glx_window = 0;
    }

    if (m_gl_window)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Unmapping gl window");
        UnmapWindow(); // needed for nvidia driver bug
        VERBOSE(VB_PLAYBACK, LOC + "Destroying gl window");
        X11S(XDestroyWindow(m_display, m_gl_window));
        m_gl_window = 0;
    }

    MakeCurrent(false);
    X11L;

    if (m_glx_context)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Destroying glx context");
        glXDestroyContext(m_display, m_glx_context);
        m_glx_context = 0;
    }

    XSync(m_display, false);

    if (m_created_display && m_display)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Closing display");
        XCloseDisplay(m_display);
        m_display = NULL;
        m_created_display = false;
    }

    X11U;
}

bool OpenGLContextGLX::IsGLXSupported(
    Display *display, uint min_major, uint min_minor)
{
    uint major, minor;
    if (init_opengl() && get_glx_version(display, major, minor))
    {
        return ((major > min_major) ||
                (major == min_major && (minor >= min_minor)));
    }

    return false;
}

int OpenGLContextGLX::GetRefreshRate(void)
{
    return MythXGetRefreshRate(m_display, m_screen_num);
}

void OpenGLContextGLX::SetSwapInterval(int interval)
{
    if (!(m_ext_used & kGLGLXSwap))
        return;

    MakeCurrent(true);

    X11S(gMythGLXSwapIntervalSGI(interval));

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Swap interval set to %1.").arg(interval));

    MakeCurrent(false);
}

void OpenGLContextGLX::GetDisplayDimensions(QSize &dimensions)
{
    dimensions = MythXGetDisplayDimensions(m_display, m_screen_num);
}

void OpenGLContextGLX::GetDisplaySize(QSize &size)
{
    size = MythXGetDisplaySize(m_display, m_screen_num);
}

void OpenGLContextGLX::MoveResizeWindow(QRect rect)
{
    X11S(XMoveResizeWindow(m_display, m_gl_window,
                           rect.left(), rect.top(),
                           rect.width(), rect.height()));
    m_window_rect = rect;
}

bool OpenGLContextGLX::OverrideDisplayDim(QSize &disp_dim, float pixel_aspect)
{
    bool ret = (GetNumberOfXineramaScreens() > 1);
    if (ret)
    {
        float displayAspect = gContext->GetFloatSettingOnHost(
            "XineramaMonitorAspectRatio",
            gContext->GetHostName(), pixel_aspect);
        if (disp_dim.height() <= 0)
            disp_dim.setHeight(300);
        disp_dim.setWidth((int)((float)disp_dim.height() * displayAspect));
    }

    return ret;
}
#endif // USING_X11

#ifdef USING_MINGW
OpenGLContextWGL::OpenGLContextWGL(QMutex *lock)
    : OpenGLContext(lock),
      hDC(NULL), hRC(NULL), hWnd(NULL)
{
}

OpenGLContextWGL::~OpenGLContextWGL()
{
    DeleteOpenGLResources();
    DeleteWindowResources();
}

bool OpenGLContextWGL::Create(WId window, const QRect &display_visible,
                              bool colour_control)
{
    hWnd = window;
    if (!hWnd)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to get opengl window");
        return false;
    }

    hDC = GetDC(hWnd);
    if (!hDC)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Couldn't get device context");
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;

    int pf = ChoosePixelFormat(hDC, &pfd);
    if (!pf)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to chose pixel format");
        return false;
    } 
 
    if (!SetPixelFormat(hDC, pf, &pfd))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to set pixel format");
        return false;
    } 

    DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    hRC = wglCreateContext(hDC);
    if (!hRC)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to create opengl context.");
        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC + "Created window and WGL context");

    return CreateCommon(colour_control, display_visible);
}

bool OpenGLContextWGL::MakeContextCurrent(bool current)
{
    bool ok = true;
    if (current)
    {
        ok = wglMakeCurrent(hDC, hRC);
    }
    else
    {
        ok = wglMakeCurrent(NULL, NULL);
    }
    return ok;
}

void OpenGLContextWGL::SwapBuffers(void)
{
    MakeCurrent(true);
    if (m_ext_used & kGLFinish)
        glFinish();

    gMythWGLSwapBuffers(hDC);
    MakeCurrent(false);
}

void OpenGLContextWGL::DeleteWindowResources(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Deleting WGL window resources.");

    wglMakeCurrent(NULL, NULL);
    if (hRC)
    {
        wglDeleteContext(hRC);
        hRC = NULL;
    }
    if (hDC)
    {
        ReleaseDC(hWnd, hDC);
        hDC = NULL;
    }
}

int OpenGLContextWGL::GetRefreshRate(void)
{
    int result = GetDeviceCaps(hDC, VREFRESH);

    if (result > 20 && result < 200)
        return 1000000 / result;

    return -1;
}

void OpenGLContextWGL::SetSwapInterval(int interval)
{
    if (!(m_ext_used & kGLWGLSwap))
        return;

    MakeCurrent(true);
    gMythWGLSwapIntervalEXT(interval);
    VERBOSE(VB_PLAYBACK, LOC + QString("Swap interval set to %1.").arg(interval));
    MakeCurrent(false);
}

void OpenGLContextWGL::GetDisplayDimensions(QSize &dimensions)
{
    if (!hDC)
        return;

    int width  = GetDeviceCaps(hDC, HORZSIZE);
    int height = GetDeviceCaps(hDC, VERTSIZE);
    dimensions = QSize((uint)width, (uint)height);
}

void OpenGLContextWGL::GetDisplaySize(QSize &size)
{
    if (!hDC)
        return;

    int width  = GetDeviceCaps(hDC, HORZRES);
    int height = GetDeviceCaps(hDC, VERTRES);
    size = QSize((uint)width, (uint)height);
}
#endif // USING_MINGW

#ifdef Q_WS_MACX
OpenGLContextAGL::OpenGLContextAGL(QMutex *lock)
    : OpenGLContext(lock),
      m_window(NULL), m_context(NULL),
      m_screen(NULL), m_port(NULL)
{
}

OpenGLContextAGL::~OpenGLContextAGL()
{
    DeleteOpenGLResources();
    DeleteWindowResources();
}

bool OpenGLContextAGL::Create(WId window, const QRect &display_visible,
                              bool colour_control)
{
    WindowRef main_window = FrontNonFloatingWindow();
    if (main_window)
    {
        Rect bounds;
        if (!GetWindowBounds(main_window,
                             kWindowStructureRgn,
                             &bounds))
        {
            CGDisplayCount ct ;
            CGPoint pt;
            pt.x = bounds.left;
            pt.y = bounds.top;

            if (CGGetDisplaysWithPoint(pt, 1, &m_screen, &ct))
                m_screen = CGMainDisplayID();
        }
    }

    GLint pix_fmt [] = { AGL_RGBA,
                         AGL_DOUBLEBUFFER,
                         AGL_NO_RECOVERY,
                         AGL_MINIMUM_POLICY,
                         AGL_ACCELERATED,
                         AGL_RED_SIZE, 8,
                         AGL_GREEN_SIZE, 8,
                         AGL_BLUE_SIZE, 8,
                         AGL_ALPHA_SIZE, 8,
                         AGL_DEPTH_SIZE, 0,
                         AGL_NONE };
    AGLPixelFormat fmt = aglChoosePixelFormat(NULL, 0, pix_fmt);
    if (!fmt)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to get pixel format."));
        return false;
    }

    m_context = aglCreateContext(fmt, NULL);
    aglDestroyPixelFormat(fmt);

    if (!m_context)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to create AGL context."));
        return false;
    }

    Rect rect;
    rect.top = display_visible.top();
    rect.left = display_visible.left();
    rect.bottom = display_visible.bottom();
    rect.right = display_visible.right();
    OSStatus err = CreateNewWindow(kOverlayWindowClass,
                                   kWindowNoAttributes,
                                   &rect, &m_window);
    if (!m_window)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to create window for AGL (Error: %1)").arg(err));
        return false;
    }

    m_port = GetWindowPort (m_window);
    if (!m_port)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to get window port."));
        return false;
    }

    aglSetDrawable(m_context,  m_port);
    if (main_window)
        RepositionWindow(m_window, main_window, kWindowCenterOnParentWindow);
    ShowWindow(m_window);

    m_extensions = kGLAGLSwap;
    VERBOSE(VB_PLAYBACK, LOC + QString("Created window and AGL context."));

    return CreateCommon(colour_control, display_visible);
}

bool OpenGLContextAGL::MakeContextCurrent(bool current)
{
    bool ok = true;
    if (current)
    {
        ok = aglSetCurrentContext(m_context);
    }
    else
    {
        ok = aglSetCurrentContext(NULL);
    }
    return ok;
}

void OpenGLContextAGL::SwapBuffers(void)
{
    MakeCurrent(true);
    if (m_ext_used & kGLFinish)
        glFinish();

    aglSwapBuffers(m_context);
    MakeCurrent(false);
}

void OpenGLContextAGL::DeleteWindowResources(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Deleting AGL window resources.");

    if (m_context)
    {
        aglSetDrawable(m_context, NULL);
        aglSetCurrentContext(NULL);
        aglDestroyContext(m_context);
        m_context = NULL;
    }

    if (m_window)
    {
        DisposeWindow(m_window);
        m_window = NULL;
    }
}

int OpenGLContextAGL::GetRefreshRate(void)
{
    int ret = -1;

    if (m_screen)
    {
        CFDictionaryRef ref = CGDisplayCurrentMode(m_screen);
        if (ref)
        {
            int rate = get_float_CF(ref, kCGDisplayRefreshRate);
            if (rate > 20 && rate < 200)
                ret = 1000000 / rate;
        }
    }
    return ret;
}

void OpenGLContextAGL::SetSwapInterval(int interval)
{
    MakeCurrent(true);
    GLint swap = interval;
    aglSetInteger(m_context, AGL_SWAP_INTERVAL, &swap);
    VERBOSE(VB_PLAYBACK, LOC + QString("Swap interval set to %1.").arg(swap));
    MakeCurrent(false);
}

void OpenGLContextAGL::GetDisplayDimensions(QSize &dimensions)
{
    if (!m_screen)
        return;

    CGSize size_in_mm = CGDisplayScreenSize(m_screen);
    dimensions = QSize((uint) size_in_mm.width, (uint) size_in_mm.height);
}

void OpenGLContextAGL::GetDisplaySize(QSize &size)
{
    if (!m_screen)
        return;

    uint width  = (uint)CGDisplayPixelsWide(m_screen);
    uint height = (uint)CGDisplayPixelsHigh(m_screen);
    size = QSize(width, height);
}

void OpenGLContextAGL::MoveResizeWindow(QRect rect)
{
    if (!m_window)
        return;

    Rect bounds;
    bounds.top = rect.top();
    bounds.left = rect.left();
    bounds.bottom = rect.bottom();
    bounds.right = rect.right();
    SetWindowBounds(m_window, kWindowStructureRgn, &bounds);
    m_port = GetWindowPort (m_window);
    if (!m_port)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to get window port."));
        return;
    }

    aglSetDrawable(m_context,  m_port);
    m_window_rect = rect;
}

void OpenGLContextAGL::EmbedInWidget(int x, int y, int w, int h)
{
    if (m_context)
        aglSetDrawable(m_context, NULL);
}

void OpenGLContextAGL::StopEmbedding(void)
{
    if(m_context && m_port)
        aglSetDrawable(m_context, m_port);
}
#endif //Q_WS_MACX

