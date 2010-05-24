// -*- Mode: c++ -*-

#include "compat.h"
#include "util-opengl.h"
#include "frame.h"

#ifdef MMX
extern "C" {
#include "x86/mmx.h"
}
#endif

MYTH_GLACTIVETEXTUREPROC              gMythGLActiveTexture     = NULL;

#ifdef USING_MINGW
MYTH_WGLSWAPBUFFERSPROC               gMythWGLSwapBuffers    = NULL;
#endif // USING_MINGW

MYTH_GLXSWAPINTERVALSGIPROC           gMythGLXSwapIntervalSGI  = NULL;
MYTH_WGLSWAPINTERVALEXTPROC           gMythWGLSwapIntervalEXT  = NULL;

MYTH_GLMAPBUFFERARBPROC               gMythGLMapBufferARB      = NULL;
MYTH_GLBINDBUFFERARBPROC              gMythGLBindBufferARB     = NULL;
MYTH_GLGENBUFFERSARBPROC              gMythGLGenBuffersARB     = NULL;
MYTH_GLBUFFERDATAARBPROC              gMythGLBufferDataARB     = NULL;
MYTH_GLUNMAPBUFFERARBPROC             gMythGLUnmapBufferARB    = NULL;
MYTH_GLDELETEBUFFERSARBPROC           gMythGLDeleteBuffersARB  = NULL;

MYTH_GLGENPROGRAMSARBPROC             gMythGLGenProgramsARB            = NULL;
MYTH_GLBINDPROGRAMARBPROC             gMythGLBindProgramARB            = NULL;
MYTH_GLPROGRAMSTRINGARBPROC           gMythGLProgramStringARB          = NULL;
MYTH_GLPROGRAMENVPARAMETER4FARBPROC   gMythGLProgramEnvParameter4fARB  = NULL;
MYTH_GLDELETEPROGRAMSARBPROC          gMythGLDeleteProgramsARB         = NULL;
MYTH_GLGETPROGRAMIVARBPROC            gMythGLGetProgramivARB           = NULL;

MYTH_GLGENFRAMEBUFFERSEXTPROC         gMythGLGenFramebuffersEXT        = NULL;
MYTH_GLBINDFRAMEBUFFEREXTPROC         gMythGLBindFramebufferEXT        = NULL;
MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC    gMythGLFramebufferTexture2DEXT   = NULL;
MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC  gMythGLCheckFramebufferStatusEXT = NULL;
MYTH_GLDELETEFRAMEBUFFERSEXTPROC      gMythGLDeleteFramebuffersEXT     = NULL;

MYTH_GLXGETVIDEOSYNCSGIPROC           gMythGLXGetVideoSyncSGI          = NULL;
MYTH_GLXWAITVIDEOSYNCSGIPROC          gMythGLXWaitVideoSyncSGI         = NULL;

MYTH_GLGENFENCESNVPROC                gMythGLGenFencesNV      = NULL;
MYTH_GLDELETEFENCESNVPROC             gMythGLDeleteFencesNV   = NULL;
MYTH_GLSETFENCENVPROC                 gMythGLSetFenceNV       = NULL;
MYTH_GLFINISHFENCENVPROC              gMythGLFinishFenceNV    = NULL;

MYTH_GLGENFENCESAPPLEPROC             gMythGLGenFencesAPPLE    = NULL;
MYTH_GLDELETEFENCESAPPLEPROC          gMythGLDeleteFencesAPPLE = NULL;
MYTH_GLSETFENCEAPPLEPROC              gMythGLSetFenceAPPLE     = NULL;
MYTH_GLFINISHFENCEAPPLEPROC           gMythGLFinishFenceAPPLE  = NULL;

bool init_opengl(void)
{
    static bool is_initialized = false;
    static bool is_valid       = true;
    static QMutex init_lock;

    QMutexLocker locker(&init_lock);
    if (is_initialized)
        return is_valid;

#ifdef USING_X11
    LockMythXDisplays(true);
#endif

    is_initialized = true;

    gMythGLActiveTexture = (MYTH_GLACTIVETEXTUREPROC)
        get_gl_proc_address("glActiveTexture");

    if (!gMythGLActiveTexture)
    {
        VERBOSE(VB_IMPORTANT, "Multi-texturing not available.");
        is_valid = false;
    }

#ifdef USING_MINGW
    gMythWGLSwapBuffers = (MYTH_WGLSWAPBUFFERSPROC)
        get_gl_proc_address("wglSwapBuffers");

    if (!gMythWGLSwapBuffers)
    {
        VERBOSE(VB_IMPORTANT, "Failed to link to wglSwapBuffers.");
        is_valid = false;
    }
#endif // USING_MINGW

    gMythGLXSwapIntervalSGI  = (MYTH_GLXSWAPINTERVALSGIPROC)
        get_gl_proc_address("glXSwapIntervalSGI");
    gMythWGLSwapIntervalEXT  = (MYTH_WGLSWAPINTERVALEXTPROC)
        get_gl_proc_address("wglSwapIntervalEXT");

    gMythGLMapBufferARB = (MYTH_GLMAPBUFFERARBPROC)
        get_gl_proc_address("glMapBufferARB");
    gMythGLBindBufferARB = (MYTH_GLBINDBUFFERARBPROC)
        get_gl_proc_address("glBindBufferARB");
    gMythGLGenBuffersARB = (MYTH_GLGENBUFFERSARBPROC)
        get_gl_proc_address("glGenBuffersARB");
    gMythGLBufferDataARB = (MYTH_GLBUFFERDATAARBPROC)
        get_gl_proc_address("glBufferDataARB");
    gMythGLUnmapBufferARB = (MYTH_GLUNMAPBUFFERARBPROC)
        get_gl_proc_address("glUnmapBufferARB");
    gMythGLDeleteBuffersARB = (MYTH_GLDELETEBUFFERSARBPROC)
        get_gl_proc_address("glDeleteBuffersARB");

    gMythGLGenProgramsARB = (MYTH_GLGENPROGRAMSARBPROC)
        get_gl_proc_address("glGenProgramsARB");
    gMythGLBindProgramARB = (MYTH_GLBINDPROGRAMARBPROC)
        get_gl_proc_address("glBindProgramARB");
    gMythGLProgramStringARB = (MYTH_GLPROGRAMSTRINGARBPROC)
        get_gl_proc_address("glProgramStringARB");
    gMythGLProgramEnvParameter4fARB = (MYTH_GLPROGRAMENVPARAMETER4FARBPROC)
        get_gl_proc_address("glProgramEnvParameter4fARB");
    gMythGLDeleteProgramsARB = (MYTH_GLDELETEPROGRAMSARBPROC)
        get_gl_proc_address("glDeleteProgramsARB");
    gMythGLGetProgramivARB = (MYTH_GLGETPROGRAMIVARBPROC)
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

    gMythGLXGetVideoSyncSGI = (MYTH_GLXGETVIDEOSYNCSGIPROC)
        get_gl_proc_address("glXGetVideoSyncSGI");
    gMythGLXWaitVideoSyncSGI = (MYTH_GLXWAITVIDEOSYNCSGIPROC)
        get_gl_proc_address("glXWaitVideoSyncSGI");

    gMythGLGenFencesNV = (MYTH_GLGENFENCESNVPROC)
        get_gl_proc_address("glGenFencesNV");
    gMythGLDeleteFencesNV = (MYTH_GLDELETEFENCESNVPROC)
        get_gl_proc_address("glDeleteFencesNV");
    gMythGLSetFenceNV = (MYTH_GLSETFENCENVPROC)
        get_gl_proc_address("glSetFenceNV");
    gMythGLFinishFenceNV = (MYTH_GLFINISHFENCENVPROC)
        get_gl_proc_address("glFinishFenceNV");

    gMythGLGenFencesAPPLE = (MYTH_GLGENFENCESAPPLEPROC)
        get_gl_proc_address("glGenFencesAPPLE");
    gMythGLDeleteFencesAPPLE = (MYTH_GLDELETEFENCESAPPLEPROC)
        get_gl_proc_address("glDeleteFencesAPPLE");
    gMythGLSetFenceAPPLE = (MYTH_GLSETFENCEAPPLEPROC)
        get_gl_proc_address("glSetFenceAPPLE");
    gMythGLFinishFenceAPPLE = (MYTH_GLFINISHFENCEAPPLEPROC)
        get_gl_proc_address("glFinishFenceAPPLE");

#ifdef USING_X11
    LockMythXDisplays(false);
#endif
    return is_valid;
}

#ifdef USING_X11
bool get_glx_version(MythXDisplay *disp, uint &major, uint &minor)
{
    // Crashes Unichrome-based system if it is run more than once. -- Tegue
    static bool has_run = false;
    static int static_major = 0;
    static int static_minor = 0;
    static int static_ret = false;
    static QMutex get_glx_version_lock;
    QMutexLocker locker(&get_glx_version_lock);

    int ret = 0, errbase, eventbase, gl_major = 0, gl_minor = 0;

    if (has_run)
    {
        major = static_major;
        minor = static_minor;
        return static_ret;
    }

    major = minor = 0;
    has_run = true;

    XLOCK(disp,
        ret = glXQueryExtension(disp->GetDisplay(), &errbase, &eventbase));

    if (!ret)
        return false;

    // Unfortunately, nVidia drivers up into the 9xxx series have
    // a bug that causes them to SEGFAULT MythTV when we call
    // XCloseDisplay later on, if we query the GLX version of a
    // display pointer and then use that display pointer for
    // something other than OpenGL. So we open a separate
    // connection to the X server here just to query the GLX version.
    MythXDisplay *tmp_disp = OpenMythXDisplay();
    if (tmp_disp)
    {
        ret = glXQueryVersion(tmp_disp->GetDisplay(), &gl_major, &gl_minor);
        delete tmp_disp;
    }

    if (!ret)
        return false;

    /** HACK until nvidia fixes glx 1.3 support in the latest drivers */
    if (gl_minor > 2)
    {
        VERBOSE(VB_PLAYBACK, QString("Forcing GLX version to %1.2 (orig %1.%2)")
                .arg(gl_major).arg(gl_minor));
        gl_minor = 2;
    }

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
        GLX_ALPHA_SIZE,    5,
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


GLXFBConfig get_fbuffer_cfg(MythXDisplay *disp, const int *attr_fbconfig)
{


    GLXFBConfig  cfg  = 0;
    GLXFBConfig *cfgs = NULL;
    int          num  = 0;

    MythXLocker lock(disp);

    cfgs = glXChooseFBConfig(disp->GetDisplay(), disp->GetScreen(),
                             attr_fbconfig, &num);

    if (num)
    {
        cfg = cfgs[0];

        // Try to waste less memory by getting a frame buffer without depth
        for (int i = 0; i < num; i++)
        {
            int value;
            glXGetFBConfigAttrib(disp->GetDisplay(), cfgs[i],
                                 GLX_DEPTH_SIZE, &value);
            if (!value)
            {
                cfg = cfgs[i];
                break;
            }
        }
    }

    XFree(cfgs);
    return cfg;
}

Window get_gl_window(MythXDisplay *disp,
                     Window        XJ_curwin,
                     XVisualInfo  *visInfo,
                     const QRect  &window_rect,
                     bool          map_window)
{
    MythXLocker lock(disp);

    XSetWindowAttributes attributes;
    attributes.colormap = XCreateColormap(
        disp->GetDisplay(), XJ_curwin, visInfo->visual, AllocNone);

    Window gl_window = XCreateWindow(
        disp->GetDisplay(), XJ_curwin, window_rect.x(), window_rect.y(),
        window_rect.width(), window_rect.height(), 0,
        visInfo->depth, InputOutput, visInfo->visual, CWColormap, &attributes);

    if (map_window)
        XMapWindow(disp->GetDisplay(), gl_window);

    XFree(visInfo);
    return gl_window;
}
#endif // USING_X11

void *get_gl_proc_address(const QString &procName)
{
    void *ret = NULL;

    QByteArray tmp = procName.toAscii();
    const GLubyte *procedureName = (const GLubyte*) tmp.constData();

#if USING_GLX_PROC_ADDR_ARB
    ret = (void*)glXGetProcAddressARB(procedureName);
#elif GLX_VERSION_1_4
    ret = (void*)glXGetProcAddress(procedureName);
#elif GLX_ARB_get_proc_address
    ret = (void*)glXGetProcAddressARB(procedureName);
#elif GLX_EXT_get_proc_address
    ret = (void*)glXGetProcAddressEXT(procedureName);
#elif USING_MINGW
    ret = (void*)wglGetProcAddress((const char*)procedureName);
    if (ret)
        return ret;
    ret = (void*)GetProcAddress(GetModuleHandle("opengl32.dll"),
                                (const char*) procedureName);
#endif
#ifdef Q_WS_MACX
    (void) procedureName;
    NSSymbol symbol = NULL;
    QByteArray symbol_name("_");
    symbol_name.append(tmp);
    if (NSIsSymbolNameDefined (symbol_name))
        symbol = NSLookupAndBindSymbol (symbol_name);
    ret = symbol ? NSAddressOfSymbol (symbol) : NULL;
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

bool has_gl_applefence_support(const QString &ext)
{
    init_opengl();

    if (!ext.contains("GL_APPLE_fence"))
        return false;

    return (gMythGLGenFencesAPPLE    &&
            gMythGLDeleteFencesAPPLE &&
            gMythGLSetFenceAPPLE     &&
            gMythGLFinishFenceAPPLE);
}

bool has_glx_swapinterval_support(const QString &glx_ext)
{
    init_opengl();

    if (!glx_ext.contains("GLX_SGI_swap_control"))
        return false;

    return gMythGLXSwapIntervalSGI;
}

bool has_wgl_swapinterval_support(const QString &ext)
{
    init_opengl();

    if (!ext.contains("WGL_EXT_swap_control"))
        return false;

    return gMythWGLSwapIntervalEXT;
}

bool has_gl_ycbcrmesa_support(const QString &ext)
{
    init_opengl();

    if (!ext.contains("GL_MESA_ycbcr_texture"))
        return false;

    return true;
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

    if (height % 2 || width % 2)
        return;

#ifdef MMX
    int residual  = width % 8;
    int mmx_width = width - residual;
    int c_start_w = mmx_width;
#else
    int residual  = 0;
    int mmx_width = width;
    int c_start_w = 0;
#endif

    uint bgra_width  = width << 2;
    uint chroma_width = width >> 1;

    uint y_extra     = (pitches[0] << 1) - width + residual;
    uint u_extra     = pitches[1] - chroma_width + (residual >> 1);
    uint v_extra     = pitches[2] - chroma_width + (residual >> 1);
    uint d_extra     = bgra_width + (residual << 2);

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
        uint a_extra  = width + residual;

#ifdef MMX
        for (int row = 0; row < height; row += 2)
        {
            for (int col = 0; col < mmx_width; col += 8)
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

            ypt_1   += y_extra; ypt_2   += y_extra;
            upt     += u_extra; vpt     += v_extra;
            dst_1   += d_extra; dst_2   += d_extra;
            alpha_1 += a_extra; alpha_2 += a_extra;
        }

        emms();

        if (residual)
        {
            y_extra     = (pitches[0] << 1) - width + mmx_width;
            u_extra     = pitches[1] - chroma_width + (mmx_width >> 1);
            v_extra     = pitches[2] - chroma_width + (mmx_width >> 1);
            d_extra     = bgra_width + (mmx_width << 2);

            ypt_1   = (uint8_t *)source + offsets[0] + mmx_width;
            ypt_2   = ypt_1 + pitches[0];
            upt     = (uint8_t *)source + offsets[1] + (mmx_width>>1);
            vpt     = (uint8_t *)source + offsets[2] + (mmx_width>>1);
            dst_1   = (uint8_t *) dest + (mmx_width << 2);
            dst_2   = dst_1 + bgra_width;

            alpha_1 = (uint8_t *) alpha + mmx_width;
            alpha_2 = alpha_1 + width;
            a_extra  = width + mmx_width;
        }
        else
        {
            return;
        }
#endif //MMX

        for (int row = 0; row < height; row += 2)
        {
            for (int col = c_start_w; col < width; col += 2)
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
            alpha_1 += a_extra; alpha_2 += a_extra;
            dst_1   += d_extra; dst_2   += d_extra;
        }
    }
    else
    {

#ifdef MMX
        for (int row = 0; row < height; row += 2)
        {
            for (int col = 0; col < mmx_width; col += 8)
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
            dst_1 += d_extra; dst_2 += d_extra;
        }

        emms();

        if (residual)
        {
            y_extra     = (pitches[0] << 1) - width + mmx_width;
            u_extra     = pitches[1] - chroma_width + (mmx_width >> 1);
            v_extra     = pitches[2] - chroma_width + (mmx_width >> 1);
            d_extra     = bgra_width + (mmx_width << 2);

            ypt_1   = (uint8_t *)source + offsets[0] + mmx_width;
            ypt_2   = ypt_1 + pitches[0];
            upt     = (uint8_t *)source + offsets[1] + (mmx_width>>1);
            vpt     = (uint8_t *)source + offsets[2] + (mmx_width>>1);
            dst_1   = (uint8_t *) dest + (mmx_width << 2);
            dst_2   = dst_1 + bgra_width;
        }
        else
        {
            return;
        }
#endif //MMX

        for (int row = 0; row < height; row += 2)
        {
            for (int col = c_start_w; col < width; col += 2)
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
            dst_1   += d_extra; dst_2   += d_extra;
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

void store_bicubic_weights(float x, float *dst)
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
