// -*- Mode: c++ -*-

#include "util-opengl.h"
#include "frame.h"

#ifdef MMX
extern "C" {
#include "libavcodec/i386/mmx.h"
}
#endif

PFNGLMAPBUFFERPROC                  gMythGLMapBufferARB      = NULL;
PFNGLBINDBUFFERARBPROC              gMythGLBindBufferARB     = NULL;
PFNGLGENBUFFERSARBPROC              gMythGLGenBuffersARB     = NULL;
PFNGLBUFFERDATAARBPROC              gMythGLBufferDataARB     = NULL;
PFNGLUNMAPBUFFERARBPROC             gMythGLUnmapBufferARB    = NULL;
PFNGLDELETEBUFFERSARBPROC           gMythGLDeleteBuffersARB  = NULL;

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

PFNGLGENFENCESNVPROC                gMythGLGenFencesNV      = NULL;
PFNGLDELETEFENCESNVPROC             gMythGLDeleteFencesNV   = NULL;
PFNGLSETFENCENVPROC                 gMythGLSetFenceNV       = NULL;
PFNGLFINISHFENCENVPROC              gMythGLFinishFenceNV    = NULL;

bool init_opengl(void)
{
    static bool is_initialized = false;
    static QMutex init_lock;

    QMutexLocker locker(&init_lock);
    if (is_initialized)
        return true;

    is_initialized = true;

    gMythGLMapBufferARB = (PFNGLMAPBUFFERPROC)
        get_gl_proc_address("glMapBufferARB");
    gMythGLBindBufferARB = (PFNGLBINDBUFFERARBPROC)
        get_gl_proc_address("glBindBufferARB");
    gMythGLGenBuffersARB = (PFNGLGENBUFFERSARBPROC)
        get_gl_proc_address("glGenBuffersARB");
    gMythGLBufferDataARB = (PFNGLBUFFERDATAARBPROC)
        get_gl_proc_address("glBufferDataARB");
    gMythGLUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)
        get_gl_proc_address("glUnmapBufferARB");
    gMythGLDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)
        get_gl_proc_address("glDeleteBuffersARB");

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

    gMythGLGenFencesNV = (PFNGLGENFENCESNVPROC)
        get_gl_proc_address("glGenFencesNV");
    gMythGLDeleteFencesNV = (PFNGLDELETEFENCESNVPROC)
        get_gl_proc_address("glDeleteFencesNV");
    gMythGLSetFenceNV = (PFNGLSETFENCENVPROC)
        get_gl_proc_address("glSetFenceNV");
    gMythGLFinishFenceNV = (PFNGLFINISHFENCENVPROC)
        get_gl_proc_address("glFinishFenceNV");

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

    /** HACK until nvidia fixes glx 1.3 support in the latest drivers */
    if (gl_minor > 2)
        gl_minor = 2;

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
                     const QRect &window_rect,
                     bool         map_window)
{
    X11L;

    XSetWindowAttributes attributes;
    attributes.colormap = XCreateColormap(
        XJ_disp, XJ_curwin, visInfo->visual, AllocNone);

    Window gl_window = XCreateWindow(
        XJ_disp, XJ_curwin, window_rect.x(), window_rect.y(), 
        window_rect.width(), window_rect.height(), 0,
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

    QByteArray tmp = procName.toAscii();
    const GLubyte *procedureName = (const GLubyte*) tmp.constData();

#if USING_GLX_PROC_ADDR_ARB
    X11S(ret = glXGetProcAddressARB(procedureName));
#elif GLX_VERSION_1_4
    X11S(ret = glXGetProcAddress(procedureName));
#elif GLX_ARB_get_proc_address
    X11S(ret = glXGetProcAddressARB(procedureName));
#elif GLX_EXT_get_proc_address
    X11S(ret = glXGetProcAddressEXT(procedureName));
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

bool has_gl_pixelbuffer_object_support(const QString &ext)
{
    init_opengl();

    if (!ext.contains("GL_ARB_pixel_buffer_object"))
        return false;

    return (gMythGLMapBufferARB     &&
            gMythGLBindBufferARB    &&
            gMythGLGenBuffersARB    &&
            gMythGLDeleteBuffersARB &&
            gMythGLBufferDataARB    &&
            gMythGLUnmapBufferARB);
}

bool has_gl_nvfence_support(const QString &ext)
{
    init_opengl();

    if (!ext.contains("GL_NV_fence"))
        return false;

    return (gMythGLGenFencesNV    &&
            gMythGLDeleteFencesNV &&
            gMythGLSetFenceNV     &&
            gMythGLFinishFenceNV);
}

#ifdef MMX
static inline void mmx_pack_alpha_high(uint8_t *a1, uint8_t *a2,
                                       uint8_t *y1, uint8_t *y2)
{
    movq_m2r (*a1, mm4);
    punpckhbw_m2r (*y1, mm4);
    movq_m2r (*a2, mm7);
    punpckhbw_m2r (*y2, mm7);
}

static inline void mmx_pack_alpha_low(uint8_t *a1, uint8_t *a2,
                                      uint8_t *y1, uint8_t *y2)
{
    movq_m2r (*a1, mm4);
    punpcklbw_m2r (*y1, mm4);
    movq_m2r (*a2, mm7);
    punpcklbw_m2r (*y2, mm7);
}

static mmx_t mmx_1s = {0xffffffffffffffffLL};

static inline void mmx_pack_alpha1s_high(uint8_t *y1, uint8_t *y2)
{
    movq_m2r (mmx_1s, mm4);
    punpckhbw_m2r (*y1, mm4);
    movq_m2r (mmx_1s, mm7);
    punpckhbw_m2r (*y2, mm7);
}

static inline void mmx_pack_alpha1s_low(uint8_t *y1, uint8_t *y2)
{
    movq_m2r (mmx_1s, mm4);
    punpcklbw_m2r (*y1, mm4);
    movq_m2r (mmx_1s, mm7);
    punpcklbw_m2r (*y2, mm7);
}

static inline void mmx_pack_middle(uint8_t *dest1, uint8_t *dest2)
{
    movq_r2r (mm3, mm5);
    punpcklbw_r2r (mm2, mm5);

    movq_r2r (mm5, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1));

    movq_r2r (mm5, mm6);
    punpckhbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1 + 8));

    movq_r2r (mm5, mm6);
    punpcklbw_r2r (mm7, mm6);
    movq_r2m (mm6, *(dest2));

    movq_r2r (mm5, mm6);
    punpckhbw_r2r (mm7, mm6);
    movq_r2m (mm6, *(dest2 + 8));
}

static inline void mmx_pack_end(uint8_t *dest1, uint8_t *dest2)
{
    punpckhbw_r2r (mm2, mm3);

    movq_r2r (mm3, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1 + 16));

    movq_r2r (mm3, mm6);
    punpckhbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest1 + 24));

    movq_r2r (mm3, mm6);
    punpcklbw_r2r (mm7, mm6);
    movq_r2m (mm6, *(dest2 + 16));

    punpckhbw_r2r (mm7, mm3);
    movq_r2m (mm3, *(dest2 + 24));
}

static inline void mmx_pack_easy(uint8_t *dest, uint8_t *y)
{
    movq_m2r (mmx_1s, mm4);
    punpcklbw_m2r (*y, mm4);

    movq_r2r (mm3, mm5);
    punpcklbw_r2r (mm2, mm5);

    movq_r2r (mm5, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest));

    movq_r2r (mm5, mm6);
    punpckhbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest + 8));

    movq_m2r (mmx_1s, mm4);
    punpckhbw_m2r (*y, mm4);

    punpckhbw_r2r (mm2, mm3);

    movq_r2r (mm3, mm6);
    punpcklbw_r2r (mm4, mm6);
    movq_r2m (mm6, *(dest + 16));

    punpckhbw_r2r (mm4, mm3);
    movq_r2m (mm3, *(dest + 24));
}

static mmx_t mmx_0s = {0x0000000000000000LL};
static mmx_t round  = {0x0002000200020002LL};

static inline void mmx_interp_start(uint8_t *left, uint8_t *right)
{
    movd_m2r  (*left, mm5);
    punpcklbw_m2r (mmx_0s, mm5);

    movq_r2r  (mm5, mm4);
    paddw_r2r (mm4, mm4);
    paddw_r2r (mm5, mm4);
    paddw_m2r (round, mm4);

    movd_m2r  (*right, mm5);
    punpcklbw_m2r (mmx_0s, mm5);
    paddw_r2r (mm5, mm4);

    psrlw_i2r (2, mm4);
}

static inline void mmx_interp_endu(void)
{
    movq_r2r  (mm4, mm2);
    psllw_i2r (8, mm2);
    paddb_r2r (mm4, mm2);
}
    
static inline void mmx_interp_endv(void)
{
    movq_r2r  (mm4, mm3);
    psllw_i2r (8, mm3);
    paddb_r2r (mm4, mm3);
}

static inline void mmx_pack_chroma(uint8_t *u, uint8_t *v)
{
    movd_m2r (*u,  mm2);
    movd_m2r (*v,  mm3);
    punpcklbw_r2r (mm2, mm2);
    punpcklbw_r2r (mm3, mm3);
}
#endif // MMX

static inline void c_interp(uint8_t *dest, uint8_t *a, uint8_t *b,
                            uint8_t *c, uint8_t *d)
{
    unsigned int tmp = (unsigned int) *a;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *c;
    dest[0] = (uint8_t) (tmp >> 2);

    tmp = (unsigned int) *b;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *d;
    dest[1] = (uint8_t) (tmp >> 2);

    tmp = (unsigned int) *c;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *a;
    dest[2] = (uint8_t) (tmp >> 2);

    tmp = (unsigned int) *d;
    tmp *= 3;
    tmp += 2;
    tmp += (unsigned int) *b;
    dest[3] = (uint8_t) (tmp >> 2);
}

void pack_yv12alpha(const unsigned char *source,
                    const unsigned char *dest,
                    const int *offsets,
                    const int *pitches,
                    const QSize size,
                    const unsigned char *alpha)
{
    const int width = size.width();
    const int height = size.height();

    if (height % 2)
        return;

    uint bgra_width  = width << 2;
    uint chroma_width = width >> 1;
    uint y_extra     = (pitches[0] << 1) - width;
    uint u_extra     = pitches[1] - chroma_width;
    uint v_extra     = pitches[2] - chroma_width;

    uint8_t *ypt_1   = (uint8_t *)source + offsets[0];
    uint8_t *ypt_2   = ypt_1 + pitches[0];
    uint8_t *upt     = (uint8_t *)source + offsets[1];
    uint8_t *vpt     = (uint8_t *)source + offsets[2];
    uint8_t *dst_1   = (uint8_t *) dest;
    uint8_t *dst_2   = dst_1 + bgra_width;

    if (alpha)
    {
        uint8_t *alpha_1 = (uint8_t *) alpha;
        uint8_t *alpha_2 = alpha_1 + width;

#ifdef MMX
        if (!(width % 8))
        {
            for (int row = 0; row < height; row += 2)
            {
                for (int col = 0; col < width; col += 8)
                {
                    mmx_pack_chroma(upt,  vpt);
                    mmx_pack_alpha_low(alpha_1, alpha_2, ypt_1, ypt_2);
                    mmx_pack_middle(dst_1, dst_2);
                    mmx_pack_alpha_high(alpha_1, alpha_2, ypt_1, ypt_2);
                    mmx_pack_end(dst_1, dst_2);

                    dst_1 += 32; dst_2 += 32;
                    alpha_1 += 8; alpha_2 += 8;
                    ypt_1 += 8; ypt_2 += 8;
                    upt   += 4; vpt   += 4;
                }

                ypt_1 += y_extra; ypt_2 += y_extra;
                upt   += u_extra; vpt   += v_extra;
                dst_1 += bgra_width; dst_2 += bgra_width;
                alpha_1 += width; alpha_2 += width;
            }

            emms();

            return;
        }
#endif //MMX

        for (int row = 0; row < height; row += 2)
        {
            for (int col = 0; col < width; col += 2)
            {
                *(dst_1++) = *vpt; *(dst_2++) = *vpt;
                *(dst_1++) = *(alpha_1++);
                *(dst_2++) = *(alpha_2++);
                *(dst_1++) = *upt; *(dst_2++) = *upt;
                *(dst_1++) = *(ypt_1++);
                *(dst_2++) = *(ypt_2++);

                *(dst_1++) = *vpt; *(dst_2++) = *(vpt++);
                *(dst_1++) = *(alpha_1++);
                *(dst_2++) = *(alpha_2++);
                *(dst_1++) = *upt; *(dst_2++) = *(upt++);
                *(dst_1++) = *(ypt_1++);
                *(dst_2++) = *(ypt_2++);
            }

            ypt_1   += y_extra; ypt_2   += y_extra;
            upt     += u_extra; vpt     += v_extra;
            alpha_1 += width;   alpha_2 += width;
            dst_1   += bgra_width; dst_2   += bgra_width;
        }
    }
    else
    {

#ifdef MMX
       if (!(width % 8))
        {
            for (int row = 0; row < height; row += 2)
            {
                for (int col = 0; col < width; col += 8)
                {
                    mmx_pack_chroma(upt,  vpt);
                    mmx_pack_alpha1s_low(ypt_1, ypt_2);
                    mmx_pack_middle(dst_1, dst_2);
                    mmx_pack_alpha1s_high(ypt_1, ypt_2);
                    mmx_pack_end(dst_1, dst_2);

                    dst_1 += 32; dst_2 += 32;
                    ypt_1 += 8;  ypt_2 += 8;
                    upt   += 4;  vpt   += 4;

                }
                ypt_1 += y_extra; ypt_2 += y_extra;
                upt   += u_extra; vpt   += v_extra;
                dst_1 += bgra_width; dst_2 += bgra_width;
            }

            emms();

            return;
        }
#endif //MMX

        for (int row = 0; row < height; row += 2)
        {
            for (int col = 0; col < width; col += 2)
            {
                *(dst_1++) = *vpt; *(dst_2++) = *vpt;
                *(dst_1++) = 255;  *(dst_2++) = 255;
                *(dst_1++) = *upt; *(dst_2++) = *upt;
                *(dst_1++) = *(ypt_1++);
                *(dst_2++) = *(ypt_2++);

                *(dst_1++) = *vpt; *(dst_2++) = *(vpt++);
                *(dst_1++) = 255;  *(dst_2++) = 255;
                *(dst_1++) = *upt; *(dst_2++) = *(upt++);
                *(dst_1++) = *(ypt_1++);
                *(dst_2++) = *(ypt_2++);
            }
            ypt_1   += y_extra; ypt_2   += y_extra;
            upt     += u_extra; vpt     += v_extra;
            dst_1   += bgra_width; dst_2   += bgra_width;
        }
    }
}

void pack_yv12interlaced(const unsigned char *source,
                         const unsigned char *dest,
                         const int *offsets,
                         const int *pitches,
                         const QSize size)
{
    int width = size.width();
    int height = size.height();

    if (height % 4 || width % 2)
        return;

    uint bgra_width  = width << 2;
    uint dwrap  = (bgra_width << 2) - bgra_width;
    uint chroma_width = width >> 1;
    uint ywrap     = (pitches[0] << 1) - width;
    uint uwrap     = (pitches[1] << 1) - chroma_width;
    uint vwrap     = (pitches[2] << 1) - chroma_width;

    uint8_t *ypt_1   = (uint8_t *)source + offsets[0];
    uint8_t *ypt_2   = ypt_1 + pitches[0];
    uint8_t *ypt_3   = ypt_1 + (pitches[0] * (height - 2));
    uint8_t *ypt_4   = ypt_3 + pitches[0];

    uint8_t *u1     = (uint8_t *)source + offsets[1];
    uint8_t *v1     = (uint8_t *)source + offsets[2];
    uint8_t *u2     = u1 + pitches[1]; uint8_t *v2     = v1 + pitches[2];
    uint8_t *u3     = u1 + (pitches[1] * ((height - 4) >> 1));
    uint8_t *v3     = v1 + (pitches[2] * ((height - 4) >> 1));
    uint8_t *u4     = u3 + pitches[1]; uint8_t *v4     = v3 + pitches[2];

    uint8_t *dst_1   = (uint8_t *) dest;
    uint8_t *dst_2   = dst_1 + bgra_width;
    uint8_t *dst_3   = dst_1 + (bgra_width * (height - 2));
    uint8_t *dst_4   = dst_3 + bgra_width;

#ifdef MMX

    if (!(width % 8))
    {
        // pack first 2 and last 2 rows
        for (int col = 0; col < width; col += 8)
        {
            mmx_pack_chroma(u1, v1);
            mmx_pack_easy(dst_1, ypt_1);
            mmx_pack_chroma(u2, v2);
            mmx_pack_easy(dst_2, ypt_2);
            mmx_pack_chroma(u3, v3);
            mmx_pack_easy(dst_3, ypt_3);
            mmx_pack_chroma(u4, v4);
            mmx_pack_easy(dst_4, ypt_4);

            dst_1 += 32; dst_2 += 32; dst_3 += 32; dst_4 += 32;
            ypt_1 += 8; ypt_2 += 8; ypt_3 += 8; ypt_4 += 8;
            u1   += 4; v1   += 4; u2   += 4; v2   += 4;
            u3   += 4; v3   += 4; u4   += 4; v4   += 4;
        }

        ypt_1 += ywrap; ypt_2 += ywrap;
        dst_1 += bgra_width; dst_2 += bgra_width;

        ypt_3 = ypt_2 + pitches[0];
        ypt_4 = ypt_3 + pitches[0];
        dst_3 = dst_2 + bgra_width;
        dst_4 = dst_3 + bgra_width;

        ywrap = (pitches[0] << 2) - width;

        u1 = (uint8_t *)source + offsets[1];
        v1 = (uint8_t *)source + offsets[2];
        u2 = u1 + pitches[1]; v2 = v1 + pitches[2];
        u3 = u2 + pitches[1]; v3 = v2 + pitches[2];
        u4 = u3 + pitches[1]; v4 = v3 + pitches[2];

        height -= 4;

        // pack main body
        for (int row = 0 ; row < height; row += 4)
        {
            for (int col = 0; col < width; col += 8)
            {
                mmx_interp_start(u1, u3); mmx_interp_endu();
                mmx_interp_start(v1, v3); mmx_interp_endv();
                mmx_pack_easy(dst_1, ypt_1);

                mmx_interp_start(u2, u4); mmx_interp_endu();
                mmx_interp_start(v2, v4); mmx_interp_endv();
                mmx_pack_easy(dst_2, ypt_2);

                mmx_interp_start(u3, u1); mmx_interp_endu();
                mmx_interp_start(v3, v1); mmx_interp_endv();
                mmx_pack_easy(dst_3, ypt_3);

                mmx_interp_start(u4, u2); mmx_interp_endu();
                mmx_interp_start(v4, v2); mmx_interp_endv();
                mmx_pack_easy(dst_4, ypt_4);

                dst_1 += 32; dst_2 += 32; dst_3 += 32; dst_4 += 32;
                ypt_1 += 8; ypt_2 += 8; ypt_3 += 8; ypt_4 += 8;
                u1   += 4; u2   += 4; u3   += 4; u4   += 4;
                v1   += 4; v2   += 4; v3   += 4; v4   += 4;
            }

            ypt_1 += ywrap; ypt_2 += ywrap; ypt_3 += ywrap; ypt_4 += ywrap;
            dst_1 += dwrap; dst_2 += dwrap; dst_3 += dwrap; dst_4 += dwrap;
            u1 += uwrap; v1 += vwrap; u2 += uwrap; v2 += vwrap;
            u3 += uwrap; v3 += vwrap; u4 += uwrap;v4 += vwrap;
        }

        emms();
        
        return;
    }
#endif //MMX

    // pack first 2 and last 2 rows
    for (int col = 0; col < width; col += 2)
    {
        *(dst_1++) = *v1; *(dst_2++) = *v2; *(dst_3++) = *v3; *(dst_4++) = *v4;
        *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
        *(dst_1++) = *u1; *(dst_2++) = *u2; *(dst_3++) = *u3; *(dst_4++) = *u4;
        *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
        *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);

        *(dst_1++) = *(v1++); *(dst_2++) = *(v2++);
        *(dst_3++) = *(v3++); *(dst_4++) = *(v4++);
        *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
        *(dst_1++) = *(u1++); *(dst_2++) = *(u2++);
        *(dst_3++) = *(u3++); *(dst_4++) = *(u4++);
        *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
        *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);
    }

    ypt_1 += ywrap; ypt_2 += ywrap;
    dst_1 += bgra_width; dst_2 += bgra_width;

    ypt_3 = ypt_2 + pitches[0];
    ypt_4 = ypt_3 + pitches[0];
    dst_3 = dst_2 + bgra_width;
    dst_4 = dst_3 + bgra_width;

    ywrap = (pitches[0] << 2) - width;

    u1 = (uint8_t *)source + offsets[1];
    v1 = (uint8_t *)source + offsets[2];
    u2 = u1 + pitches[1]; v2 = v1 + pitches[2];
    u3 = u2 + pitches[1]; v3 = v2 + pitches[2];
    u4 = u3 + pitches[1]; v4 = v3 + pitches[2];

    height -= 4;

    uint8_t v[4], u[4];

    // pack main body
    for (int row = 0; row < height; row += 4)
    {
        for (int col = 0; col < width; col += 2)
        {
            c_interp(v, v1, v2, v3, v4);
            c_interp(u, u1, u2, u3, u4);

            *(dst_1++) = v[0]; *(dst_2++) = v[1];
            *(dst_3++) = v[2]; *(dst_4++) = v[3];
            *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
            *(dst_1++) = u[0]; *(dst_2++) = u[1];
            *(dst_3++) = u[2]; *(dst_4++) = u[3];
            *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
            *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);

            *(dst_1++) = v[0]; *(dst_2++) = v[1];
            *(dst_3++) = v[2]; *(dst_4++) = v[3];
            *(dst_1++) = 255; *(dst_2++) = 255; *(dst_3++) = 255; *(dst_4++) = 255;
            *(dst_1++) = u[0]; *(dst_2++) = u[1];
            *(dst_3++) = u[2]; *(dst_4++) = u[3];
            *(dst_1++) = *(ypt_1++); *(dst_2++) = *(ypt_2++);
            *(dst_3++) = *(ypt_3++); *(dst_4++) = *(ypt_4++);

            v1++; v2++; v3++; v4++;
            u1++; u2++; u3++; u4++;
        }
        ypt_1 += ywrap; ypt_2 += ywrap; ypt_3 += ywrap; ypt_4 += ywrap;
        u1 += uwrap; u2 += uwrap; u3 += uwrap; u4 += uwrap;
        v1 += vwrap; v2 += vwrap; v3 += vwrap; v4 += vwrap;
        dst_1 += dwrap; dst_2 += dwrap; dst_3 += dwrap; dst_4 += dwrap;
    }
}
