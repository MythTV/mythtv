// MythTV headers
#include "mythcontext.h"
#include "openglcontext.h"

#include "util-opengl.h"

#define LOC QString("GLCtx: ")
#define LOC_ERR QString("GLCtx, Error: ")

class PrivateContext
{
  public:
    PrivateContext() :
        m_glx_fbconfig(0), m_gl_window(0), m_glx_window(0),
        m_glx_context(NULL),
        m_texture_type(GL_TEXTURE_2D), m_textures_enabled(false),
        m_vis_info(NULL), m_attr_list(NULL)
    {
    }

    ~PrivateContext()
    {
    }

    GLXFBConfig  m_glx_fbconfig;
    Window       m_gl_window;
    GLXWindow    m_glx_window;
    GLXContext   m_glx_context;
    int          m_texture_type;
    bool         m_textures_enabled;
    XVisualInfo *m_vis_info;
    int const   *m_attr_list;

    vector<GLuint> m_textures;
    vector<GLuint> m_programs;
    vector<GLuint> m_framebuffers;
};

OpenGLContext::OpenGLContext() :
    m_priv(new PrivateContext()),
    m_display(NULL), m_screen_num(0),
    m_major_ver(1), m_minor_ver(2),
    m_extensions(QString::null), m_ext_supported(0),
    m_visible(true), m_max_tex_size(0)
{
    if (!init_opengl())
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to initialize OpenGL support.");
}

OpenGLContext::~OpenGLContext()
{
    MakeCurrent(true);

    if (m_priv->m_glx_context)
    {
        DeletePrograms();
        DeleteTextures();
        DeleteFrameBuffers();
    }

    glFlush();

    MakeCurrent(false);

    if (m_priv->m_glx_window)
    {
        X11S(glXDestroyWindow(m_display, m_priv->m_glx_window));
        m_priv->m_glx_window = 0;
    }

    if (m_priv->m_gl_window)
    {
        X11S(XDestroyWindow(m_display, m_priv->m_gl_window));
        m_priv->m_gl_window = 0;
    }

    if (m_priv->m_glx_context)
    {
        X11S(glXDestroyContext(m_display, m_priv->m_glx_context));
        m_priv->m_glx_context = 0;
    }

    if (m_priv)
    {
        delete m_priv;
        m_priv = NULL;
    }
}

void OpenGLContext::Hide(void)
{
    X11S(XUnmapWindow(m_display, m_priv->m_gl_window));
}

void OpenGLContext::Show(void)
{
    X11S(XMapWindow(m_display, m_priv->m_gl_window));
}

// locking ok
bool OpenGLContext::Create(
    Display *XJ_disp, Window XJ_curwin, uint screen_num,
    const QSize &display_visible_size, bool visible)
{
    static bool debugged = false;

    m_visible = visible;
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
        m_priv->m_attr_list = get_attr_cfg(kRenderRGBA);
        m_priv->m_vis_info = glXChooseVisual(
            XJ_disp, m_screen_num, (int*) m_priv->m_attr_list);
        if (!m_priv->m_vis_info)
        {
            m_priv->m_attr_list = get_attr_cfg(kSimpleRGBA);
            m_priv->m_vis_info = glXChooseVisual(
                XJ_disp, m_screen_num, (int*) m_priv->m_attr_list);
        }
        if (!m_priv->m_vis_info)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "No appropriate visual found");
            return false;
        }
        X11S(m_priv->m_glx_context = glXCreateContext(
                 XJ_disp, m_priv->m_vis_info, None, GL_TRUE));
    }
    else
    {
        m_priv->m_attr_list = get_attr_cfg(kRenderRGBA);
        m_priv->m_glx_fbconfig = get_fbuffer_cfg(
            XJ_disp, m_screen_num, m_priv->m_attr_list);

        if (!m_priv->m_glx_fbconfig)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "No framebuffer "
                    "with needed OpenGL attributes.");

            return false;
        }


        X11S(m_priv->m_glx_context = glXCreateNewContext(
                 m_display, m_priv->m_glx_fbconfig,
                 GLX_RGBA_TYPE, NULL, GL_TRUE));
    }

    if (!m_priv->m_glx_context)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to create GLX context");

        return false;
    }

    if ((1 == major) && (minor > 2))
    {
        m_priv->m_vis_info = 
            glXGetVisualFromFBConfig(XJ_disp, m_priv->m_glx_fbconfig);
    }

    m_priv->m_gl_window = get_gl_window(
        XJ_disp, XJ_curwin, m_priv->m_vis_info, display_visible_size, visible);
    
    if (!m_priv->m_gl_window)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't create OpenGL Window");

        return false;
    }

    if ((1 == major) && (minor > 2))
    {
        X11S(m_priv->m_glx_window = glXCreateWindow(
                XJ_disp, m_priv->m_glx_fbconfig, m_priv->m_gl_window, NULL));

        if (!m_priv->m_glx_window)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't create GLX Window");

            return false;
        }
    }
   
    VERBOSE(VB_PLAYBACK, LOC + QString("Created window%1 and context.")
            .arg(m_visible ? "" : " (Offscreen)"));

    {
        MakeCurrent(true);

        GLint maxtexsz = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsz);
        m_max_tex_size = (maxtexsz) ? maxtexsz : 512;
        m_extensions = (const char*) glGetString(GL_EXTENSIONS);

        if (!debugged)
        {
            debugged = true;

            bool direct = false; 
            X11S(direct = glXIsDirect(XJ_disp, m_priv->m_glx_context));
       
            VERBOSE(VB_PLAYBACK, LOC + QString("GLX Version: %1.%2")
                    .arg(major).arg(minor));
            VERBOSE(VB_PLAYBACK, LOC + QString("Direct rendering: %1")
                   .arg(direct ? "Yes" : "No"));
            VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL vendor  : %1")
                   .arg((const char*) glGetString(GL_VENDOR)));
            VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL renderer: %1")
                   .arg((const char*) glGetString(GL_RENDERER)));
            VERBOSE(VB_PLAYBACK, LOC + QString("OpenGL version : %1")
                   .arg((const char*) glGetString(GL_VERSION)));
            VERBOSE(VB_PLAYBACK, LOC + QString("Max texture size: %1 x %2")
                   .arg(m_max_tex_size).arg(m_max_tex_size));
        }

        MakeCurrent(false);
    }

    int tex_type = get_gl_texture_rect_type(m_extensions);
    m_priv->m_texture_type = (tex_type) ? tex_type : m_priv->m_texture_type;

    m_ext_supported =
        ((tex_type) ? kGLExtRect : 0) |
        ((has_gl_fragment_program_support(m_extensions)) ?
         kGLExtFragProg : 0) |
        ((has_gl_fbuffer_object_support(m_extensions)) ? kGLExtFBufObj : 0) |
        ((minor >= 3) ? kGLXPBuffer : 0);

    return true;
}

// locking ok
bool OpenGLContext::MakeCurrent(bool current)
{
    bool ok;

    if (current)
    {
        if (IsGLXSupported(1,3))
        {
            X11S(ok = glXMakeCurrent(m_display,
                                     m_priv->m_glx_window,
                                     m_priv->m_glx_context));
        }
        else
        {
            X11S(ok = glXMakeCurrent(m_display,
                                     m_priv->m_gl_window,
                                     m_priv->m_glx_context));
        }
    }
    else
    {
        X11S(ok = glXMakeCurrent(m_display, None, NULL));
    }

    if (!ok)
        VERBOSE(VB_PLAYBACK, LOC + "Could not make context current.");

    return ok;
}

// locking ok
void OpenGLContext::SwapBuffers(void)
{
    if (m_visible)
    {
        MakeCurrent(true);

        glFinish();
        if (IsGLXSupported(1,3))
            X11S(glXSwapBuffers(m_display, m_priv->m_glx_window));
        else
            X11S(glXSwapBuffers(m_display, m_priv->m_gl_window));

        MakeCurrent(false);
    }
}

// locking ok
void OpenGLContext::Flush(void)
{
    glFlush();
}

// locking ok
void OpenGLContext::EnableTextures(void)
{
    if (!m_priv->m_textures_enabled)
    {
        m_priv->m_textures_enabled = true;

        MakeCurrent(true);
        glEnable(GetTextureType());
        MakeCurrent(false);
    }
}

// locking ok
uint OpenGLContext::CreateTexture(void)
{
    MakeCurrent(true);

    GLuint tex;
    glGenTextures(1, &tex);
    SetupTextureFilters(tex, GL_LINEAR);
    m_priv->m_textures.push_back(tex);

    MakeCurrent(false);

    return tex;
}

// locking ok
bool OpenGLContext::SetupTexture(const QSize &size, uint tex, int filt)
{
    unsigned char *scratch =
        new unsigned char[(size.width() * size.height() * 4) + 128];

    bzero(scratch, size.width() * size.height() * 4);

    GLint check;

    MakeCurrent(true);
    SetupTextureFilters(tex, filt);
    glTexImage2D(GetTextureType(), 0, GL_RGBA8, size.width(), size.height(),
                 0, GL_RGB , GL_UNSIGNED_BYTE, scratch);
    glGetTexLevelParameteriv(GetTextureType(), 0, GL_TEXTURE_WIDTH, &check);
    MakeCurrent(false);

    if (scratch)
    {
        delete scratch;
        scratch = NULL;
    }

    return (check == size.width());
}

// locking ok
void OpenGLContext::SetupTextureFilters(uint tex, int filt)
{
    glBindTexture(GetTextureType(), tex);
    glTexParameteri(GetTextureType(), GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GetTextureType(), GL_TEXTURE_MAG_FILTER, filt);
    glTexParameteri(GetTextureType(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GetTextureType(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// locking ok
void OpenGLContext::DeleteTexture(uint tex)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_textures.begin(); it !=m_priv->m_textures.end(); it++)
    {
        if (*(it) == tex)
        {
            GLuint gltex = tex;
            glDeleteTextures(1, &gltex);
            m_priv->m_textures.erase(it);
            break;
        }
    }

    MakeCurrent(false);
}

// locking ok
void OpenGLContext::DeleteTextures(void)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_textures.begin(); it !=m_priv->m_textures.end(); it++)
        glDeleteTextures(1, &(*(it)));
    m_priv->m_textures.clear();

    MakeCurrent(false);
}

int OpenGLContext::GetTextureType(void) const
{
    return m_priv->m_texture_type;
}

// locking ok
bool OpenGLContext::CreateFragmentProgram(const QString &program, uint &fp)
{
    bool success = true;
    GLint error;

    MakeCurrent(true);

    GLuint glfp;
    gMythGLGenProgramsARB(1, &glfp);
    gMythGLBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, glfp);
    gMythGLProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            program.length(), program.latin1());

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

    MakeCurrent(false);

    fp = glfp;

    return success;
}

// locking ok
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

    MakeCurrent(false);
}

void OpenGLContext::BindFragmentProgram(uint fp)
{
    gMythGLBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
}

void OpenGLContext::InitFragmentParams(
    uint fp, float a, float b, float c, float d)
{
    gMythGLProgramEnvParameter4fARB(
        GL_FRAGMENT_PROGRAM_ARB, fp, a, b, c, d);
}

void OpenGLContext::DeletePrograms(void)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_programs.begin(); it != m_priv->m_programs.end(); it++)
        gMythGLDeleteProgramsARB(1, &(*(it)));
    m_priv->m_programs.clear();

    MakeCurrent(false);
}

// locking ok
bool OpenGLContext::CreateFrameBuffer(uint &fb, uint tex, const QSize &size)
{
    GLuint glfb;

    MakeCurrent(true);

    SetupTextureFilters(tex, GL_LINEAR);

    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0, 0, size.width(), size.height());
    gMythGLGenFramebuffersEXT(1, &glfb);
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, glfb);
    glBindTexture(GetTextureType(), tex);
    glTexImage2D(GetTextureType(), 0, GL_RGBA8,
                 (GLint) size.width(), (GLint) size.height(), 0,
                 GL_RGB, GL_UNSIGNED_BYTE, NULL);
    gMythGLFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GetTextureType(), tex, 0);

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

    MakeCurrent(false);

    fb = glfb;

    return success;
}

// locking ok
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

    MakeCurrent(false);
}

void OpenGLContext::DeleteFrameBuffers(void)
{
    MakeCurrent(true);

    vector<GLuint>::iterator it;
    for (it = m_priv->m_framebuffers.begin();
         it != m_priv->m_framebuffers.end(); it++)
    {
        gMythGLDeleteFramebuffersEXT(1, &(*(it)));
    }
    m_priv->m_framebuffers.clear();

    MakeCurrent(false);
}

// locking ok
void OpenGLContext::BindFramebuffer(uint fb)
{
    gMythGLBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
}

bool OpenGLContext::IsGLXSupported(
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
