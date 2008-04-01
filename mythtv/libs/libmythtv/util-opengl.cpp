// -*- Mode: c++ -*-

#include "util-opengl.h"
#include "frame.h"

PFNGLGENPROGRAMSARBPROC             gMythGLGenProgramsARB            = NULL;
PFNGLBINDPROGRAMARBPROC             gMythGLBindProgramARB            = NULL;
PFNGLPROGRAMSTRINGARBPROC           gMythGLProgramStringARB          = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC   gMythGLProgramEnvParameter4fARB  = NULL;
PFNGLDELETEPROGRAMSARBPROC          gMythGLDeleteProgramsARB         = NULL;
PFNGLGETPROGRAMIVARBPROC            gMythGLGetProgramivARB           = NULL;

MYTH_GLGENFRAMEBUFFERSEXTPROC         gMythGLGenFramebuffersEXT        = NULL;
MYTH_GLBINDFRAMEBUFFEREXTPROC         gMythGLBindFramebufferEXT        = NULL;
MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC    gMythGLFramebufferTexture2DEXT   = NULL;
MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC  gMythGLCheckFramebufferStatusEXT = NULL;
MYTH_GLDELETEFRAMEBUFFERSEXTPROC      gMythGLDeleteFramebuffersEXT     = NULL;

PFNGLXGETVIDEOSYNCSGIPROC           gMythGLXGetVideoSyncSGI          = NULL;
PFNGLXWAITVIDEOSYNCSGIPROC          gMythGLXWaitVideoSyncSGI         = NULL;

bool init_opengl(void)
{
    static bool is_initialized = false;
    static QMutex init_lock;

    QMutexLocker locker(&init_lock);
    if (is_initialized)
        return true;

    is_initialized = true;

    gMythGLGenProgramsARB = (PFNGLGENPROGRAMSARBPROC)
        get_gl_proc_address("glGenProgramsARB");
    gMythGLBindProgramARB = (PFNGLBINDPROGRAMARBPROC)
        get_gl_proc_address("glBindProgramARB");
    gMythGLProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC)
        get_gl_proc_address("glProgramStringARB");
    gMythGLProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC)
        get_gl_proc_address("glProgramEnvParameter4fARB");
    gMythGLDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)
        get_gl_proc_address("glDeleteProgramsARB");
    gMythGLGetProgramivARB = (PFNGLGETPROGRAMIVARBPROC)
        get_gl_proc_address("glGetProgramivARB");

    gMythGLGenFramebuffersEXT = (MYTH_GLGENFRAMEBUFFERSEXTPROC)
        get_gl_proc_address("glGenFramebuffersEXT");
    gMythGLBindFramebufferEXT = (MYTH_GLBINDFRAMEBUFFEREXTPROC)
        get_gl_proc_address("glBindFramebufferEXT");
    gMythGLFramebufferTexture2DEXT = (MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC)
        get_gl_proc_address("glFramebufferTexture2DEXT");
    gMythGLCheckFramebufferStatusEXT =
        (MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC)
        get_gl_proc_address("glCheckFramebufferStatusEXT");
    gMythGLDeleteFramebuffersEXT = (MYTH_GLDELETEFRAMEBUFFERSEXTPROC)
        get_gl_proc_address("glDeleteFramebuffersEXT");

    gMythGLXGetVideoSyncSGI = (PFNGLXGETVIDEOSYNCSGIPROC)
        get_gl_proc_address("glXGetVideoSyncSGI");
    gMythGLXWaitVideoSyncSGI = (PFNGLXWAITVIDEOSYNCSGIPROC)
        get_gl_proc_address("glXWaitVideoSyncSGI");

    return true;
}

bool get_glx_version(Display *XJ_disp, uint &major, uint &minor)
{
    // Crashes Unichrome-based system if it is run more than once. -- Tegue
    static bool has_run = false;
    static int static_major = 0;
    static int static_minor = 0;
    static int static_ret = false;
    static QMutex get_glx_version_lock;
    QMutexLocker locker(&get_glx_version_lock);

    int ret, errbase, eventbase, gl_major, gl_minor;

    if (has_run)
    {
        major = static_major;
        minor = static_minor;
        return static_ret;
    }

    major = minor = 0;
    has_run = true;

    X11S(ret = glXQueryExtension(XJ_disp, &errbase, &eventbase));
    if (!ret)
        return false;

    // Unfortunately, nVidia drivers up into the 9xxx series have
    // a bug that causes them to SEGFAULT MythTV when we call
    // XCloseDisplay later on, if we query the GLX version of a
    // display pointer and then use that display pointer for
    // something other than OpenGL. So we open a separate
    // connection to the X server here just to query the GLX version.
    Display *tmp_disp = MythXOpenDisplay();
    X11S(ret = glXQueryVersion(tmp_disp, &gl_major, &gl_minor));
    XCloseDisplay(tmp_disp);

    if (!ret)
        return false;

    static_major = major = gl_major;
    static_minor = minor = gl_minor;
    static_ret = true;

    return true;
}

int const *get_attr_cfg(FrameBufferType type)
{
    static const int render_rgba_config[] =
    {
        GLX_CONFIG_CAVEAT, GLX_NONE,
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_RED_SIZE,      5,
        GLX_GREEN_SIZE,    5,
        GLX_BLUE_SIZE,     5,
        GLX_DOUBLEBUFFER,  1,
        GLX_STEREO,        0,
        GLX_LEVEL,         0,
        GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT | GLX_WINDOW_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        None
    };

    static const int simple_rgba_config[] =
    {
        GLX_RGBA,
        GLX_RED_SIZE,     5,
        GLX_GREEN_SIZE,   5,
        GLX_BLUE_SIZE,    5,
        GLX_DOUBLEBUFFER, 1,
        None
    };
 
    int const * attr_config = simple_rgba_config;

    if (kRenderRGBA == type)
        attr_config = render_rgba_config;

    if (kSimpleRGBA == type)
        attr_config = simple_rgba_config;
    
    return attr_config;   
}


GLXFBConfig get_fbuffer_cfg(Display *XJ_disp, int XJ_screen_num,
                            const int *attr_fbconfig)
{


    GLXFBConfig  cfg  = 0;
    GLXFBConfig *cfgs = NULL;
    int          num  = 0;

    X11L;

    cfgs = glXChooseFBConfig(XJ_disp, XJ_screen_num, attr_fbconfig, &num);

    if (num)
    {
        cfg = cfgs[0];

        // Try to waste less memory by getting a frame buffer without depth
        for (int i = 0; i < num; i++)
        {
            int value;
            glXGetFBConfigAttrib(XJ_disp, cfgs[i], GLX_DEPTH_SIZE, &value);
            if (!value)
            {
                cfg = cfgs[i];
                break;
            }
        }
    }

    XFree(cfgs);

    X11U;

    return cfg;
}

GLXPbuffer get_pbuffer(Display     *XJ_disp,
                       GLXFBConfig  glx_fbconfig,
                       const QSize &video_dim)
{
    int attrib_pbuffer[16];
    bzero(attrib_pbuffer, sizeof(int) * 16);
    attrib_pbuffer[0] = GLX_PBUFFER_WIDTH;
    attrib_pbuffer[1] = video_dim.width();
    attrib_pbuffer[2] = GLX_PBUFFER_HEIGHT;
    attrib_pbuffer[3] = video_dim.height();
    attrib_pbuffer[4] = GLX_PRESERVED_CONTENTS;
    attrib_pbuffer[5] = 0;

    GLXPbuffer tmp = 0;

#ifdef GLX_VERSION_1_3
    X11S(tmp = glXCreatePbuffer(XJ_disp, glx_fbconfig, attrib_pbuffer));
#endif // GLX_VERSION_1_3

    return tmp;
}

Window get_gl_window(Display     *XJ_disp,
                     Window       XJ_curwin,
                     XVisualInfo  *visInfo,
                     const QSize &window_size,
                     bool         map_window)
{
    X11L;

    XSetWindowAttributes attributes;
    attributes.colormap = XCreateColormap(
        XJ_disp, XJ_curwin, visInfo->visual, AllocNone);

    Window gl_window = XCreateWindow(
        XJ_disp, XJ_curwin, 0, 0, window_size.width(), window_size.height(), 0,
        visInfo->depth, InputOutput, visInfo->visual, CWColormap, &attributes);

    if (map_window)
        XMapWindow(XJ_disp, gl_window);

    XFree(visInfo);

    X11U;

    return gl_window;
}

GLXWindow get_glx_window(Display    *XJ_disp,     GLXFBConfig  glx_fbconfig,
                         Window      gl_window,   GLXContext   glx_context,
                         GLXPbuffer  glx_pbuffer, const QSize &window_size)
{
    X11L;

    GLXWindow glx_window = glXCreateWindow(
        XJ_disp, glx_fbconfig, gl_window, NULL);

    glXMakeContextCurrent(XJ_disp, glx_window, glx_pbuffer, glx_context);

    glDrawBuffer(GL_BACK_LEFT);
    glReadBuffer(GL_FRONT_LEFT);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glShadeModel(GL_FLAT);
    glEnable(GL_TEXTURE_RECTANGLE_NV);
    
    glViewport(0, 0, window_size.width(), window_size.height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // left, right, botton, top, near, far
    glOrtho(0, window_size.width(), 0, window_size.height(), -2, 2);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glFinish();

    glXMakeContextCurrent(XJ_disp, None, None, NULL);

    X11U;

    return glx_window;
}                       

void copy_pixels_to_texture(const unsigned char *buf,
                            int                  buffer_format,
                            const QSize         &buffer_size,
                            int                  texture,
                            int                  texture_type)
{
    glBindTexture(texture_type, texture);

    uint format;
    switch (buffer_format)
    {
        case FMT_YV12:
            format = GL_LUMINANCE;
            break;
        case FMT_RGB24:
            format = GL_RGB;
            break;
        case FMT_RGBA32:
            format = GL_RGBA;
            break;
        case FMT_ALPHA:
            format = GL_ALPHA;
            break;
        default:
            return;
    }

    glTexSubImage2D(
        texture_type,
        0, 0, 0,
        buffer_size.width(), buffer_size.height(),
        format, GL_UNSIGNED_BYTE,
        buf);
}

__GLXextFuncPtr get_gl_proc_address(const QString &procName)
{
    __GLXextFuncPtr ret = NULL;

#if USING_GLX_PROC_ADDR_ARB
    X11S(ret = glXGetProcAddressARB((const GLubyte*)procName.latin1()));
#elif GLX_VERSION_1_4
    X11S(ret = glXGetProcAddress((const GLubyte*)procName.latin1()));
#elif GLX_ARB_get_proc_address
    X11S(ret = glXGetProcAddressARB((const GLubyte*)procName.latin1()));
#elif GLX_EXT_get_proc_address
    X11S(ret = glXGetProcAddressEXT((const GLubyte*)procName.latin1()));
#endif

    return ret;
}

int get_gl_texture_rect_type(const QString &ext)
{
    init_opengl();

    if (ext.contains("GL_NV_texture_rectangle"))
        return GL_TEXTURE_RECTANGLE_NV;
    else if (ext.contains("GL_ARB_texture_rectangle"))
        return GL_TEXTURE_RECTANGLE_ARB;
    else if (ext.contains("GL_EXT_texture_rectangle"))
        return GL_TEXTURE_RECTANGLE_EXT;

    return 0;
}

bool has_gl_fbuffer_object_support(const QString &ext)
{
    init_opengl();

    if (!ext.contains("GL_EXT_framebuffer_object"))
        return false;

    return (gMythGLGenFramebuffersEXT      &&
            gMythGLBindFramebufferEXT      &&
            gMythGLFramebufferTexture2DEXT &&
            gMythGLDeleteFramebuffersEXT   &&
            gMythGLCheckFramebufferStatusEXT);
}

bool has_gl_fragment_program_support(const QString &ext)
{
    init_opengl();

    if (!ext.contains("GL_ARB_fragment_program"))
        return false;

    return (gMythGLGenProgramsARB    &&
            gMythGLBindProgramARB    &&
            gMythGLProgramStringARB  &&
            gMythGLDeleteProgramsARB &&
            gMythGLGetProgramivARB   &&
            gMythGLProgramEnvParameter4fARB);
}

bool has_glx_video_sync_support(const QString &glx_ext)
{
    init_opengl();

    if (!glx_ext.contains("GLX_SGI_video_sync"))
        return false;

    return gMythGLXGetVideoSyncSGI && gMythGLXWaitVideoSyncSGI;
}
