// -*- Mode: c++ -*-

#ifndef _UTIL_OPENGL_H_
#define _UTIL_OPENGL_H_

#ifdef USING_OPENGL

// MythTV headers
#include "mythconfig.h"
#include "mythcontext.h"
#include "frame.h"

// GLX headers
#define GLX_GLXEXT_PROTOTYPES
#ifdef USING_X11
#include "mythxdisplay.h"
#include <GL/glx.h>
#endif // USING_X11

#ifdef Q_WS_MACX
#import <OpenGL/glu.h>
#import <mach-o/dyld.h>
#else
#include <GL/glu.h>
#endif

// Qt headers
#include <qstring.h>

#ifndef GL_TEXTTURE0
#define GL_TEXTURE0 0x84C0
#endif

#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#endif

#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT 0x8CD8
#endif

#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB           0x8804
#endif

#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT                0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE_EXT
#define GL_FRAMEBUFFER_COMPLETE_EXT       0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT 0x8CD6
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT 0x8CD7
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT 0x8CD9
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT 0x8CDA
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT 0x8CDB
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT 0x8CDC
#endif
#ifndef GL_FRAMEBUFFER_UNSUPPORTED_EXT
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT    0x8CDD
#endif

#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif

#ifndef GL_PIXEL_UNPACK_BUFFER_ARB
#define GL_PIXEL_UNPACK_BUFFER_ARB        0x88EC
#endif

#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW                    0x88E0
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY                     0x88B9
#endif

#ifndef GL_PROGRAM_FORMAT_ASCII_ARB
#define GL_PROGRAM_FORMAT_ASCII_ARB       0x8875
#endif

#ifndef GL_PROGRAM_ERROR_POSITION_ARB
#define GL_PROGRAM_ERROR_POSITION_ARB     0x864B
#endif

#ifndef GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB
#define GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB 0x88B6
#endif

#ifndef GL_NV_fence
#define GL_ALL_COMPLETED_NV               0x84F2
#endif

#ifndef GL_YCBCR_MESA
#define GL_YCBCR_MESA                     0x8757
#endif
#ifndef GL_UNSIGNED_SHORT_8_8_MESA
#define GL_UNSIGNED_SHORT_8_8_MESA        0x85BA
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

#ifdef USING_X11
#ifndef GLX_VERSION_1_3
typedef XID GLXPbuffer;
#endif // GLX_VERSION_1_3

bool get_glx_version(MythXDisplay *disp, uint &major, uint &minor);

typedef enum { kRenderRGBA, kSimpleRGBA } FrameBufferType;
int const *get_attr_cfg(FrameBufferType type);

// Requires GLX 1.3 or later
GLXFBConfig get_fbuffer_cfg(MythXDisplay *disp, const int*);
Window get_gl_window(MythXDisplay *disp,
                     Window        XJ_curwin,
                     XVisualInfo  *visinfo,
                     const QRect  &window_rect,
                     bool          map_window = true);
#endif // USING_X11

static inline int __glCheck__(const QString &loc, const char* fileName, int n)
{
    int error = glGetError();
    if (error)
    { 
        VERBOSE(VB_IMPORTANT, loc << gluErrorString(error) << " @ "
                << fileName << ", #" << n);
    }
    return error;
}

#define glCheck() __glCheck__(LOC, __FILE__, __LINE__)

bool init_opengl(void);

void pack_yv12alpha(const unsigned char *source, const unsigned char *dest,
                    const int *offsets, const int *pitches,
                    const QSize size, const unsigned char *alpha = NULL);

void pack_yv12interlaced(const unsigned char *source, const unsigned char *dest,
                         const int *offsets, const int *pitches,
                         const QSize size);

void store_bicubic_weights(float x, float *dst);

void *get_gl_proc_address(const QString &procName);

int get_gl_texture_rect_type(const QString &extensions);
bool has_gl_fbuffer_object_support(const QString &extensions);
bool has_gl_fragment_program_support(const QString &extensions);
bool has_glx_video_sync_support(const QString &glx_extensions);
bool has_gl_pixelbuffer_object_support(const QString &extensions);
bool has_gl_nvfence_support(const QString &extensions);
bool has_gl_applefence_support(const QString & extensions);
bool has_glx_swapinterval_support(const QString &glx_extensions);
bool has_wgl_swapinterval_support(const QString &extensions);
bool has_gl_ycbcrmesa_support(const QString &ext);

extern QString                             gMythGLExtensions;
extern uint                                gMythGLExtSupported;

// Multi-texturing
typedef void (APIENTRY * MYTH_GLACTIVETEXTUREPROC)
    (GLenum texture);
extern MYTH_GLACTIVETEXTUREPROC            gMythGLActiveTexture;

// Fragment programs
typedef void (APIENTRY * MYTH_GLPROGRAMSTRINGARBPROC)
    (GLenum target, GLenum format, GLsizei len, const GLvoid *string);
typedef void (APIENTRY * MYTH_GLBINDPROGRAMARBPROC)
    (GLenum target, GLuint program);
typedef void (APIENTRY * MYTH_GLDELETEPROGRAMSARBPROC)
    (GLsizei n, const GLuint *programs);
typedef void (APIENTRY * MYTH_GLGENPROGRAMSARBPROC)
    (GLsizei n, GLuint *programs);
typedef void (APIENTRY * MYTH_GLPROGRAMENVPARAMETER4FARBPROC)
    (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * MYTH_GLGETPROGRAMIVARBPROC)
    (GLenum target, GLenum pname, GLint *params);
extern MYTH_GLGENPROGRAMSARBPROC             gMythGLGenProgramsARB;
extern MYTH_GLBINDPROGRAMARBPROC             gMythGLBindProgramARB;
extern MYTH_GLPROGRAMSTRINGARBPROC           gMythGLProgramStringARB;
extern MYTH_GLPROGRAMENVPARAMETER4FARBPROC   gMythGLProgramEnvParameter4fARB;
extern MYTH_GLDELETEPROGRAMSARBPROC          gMythGLDeleteProgramsARB;
extern MYTH_GLGETPROGRAMIVARBPROC            gMythGLGetProgramivARB;

// PixelBuffer Objects
typedef ptrdiff_t MYTH_GLsizeiptr;
typedef GLvoid* (APIENTRY * MYTH_GLMAPBUFFERARBPROC)
    (GLenum target, GLenum access);
typedef void (APIENTRY * MYTH_GLBINDBUFFERARBPROC)
    (GLenum target, GLuint buffer);
typedef void (APIENTRY * MYTH_GLGENBUFFERSARBPROC)
    (GLsizei n, GLuint *buffers);
typedef void (APIENTRY * MYTH_GLBUFFERDATAARBPROC)
    (GLenum target, MYTH_GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLboolean (APIENTRY * MYTH_GLUNMAPBUFFERARBPROC)
    (GLenum target);
typedef void (APIENTRY * MYTH_GLDELETEBUFFERSARBPROC)
    (GLsizei n, const GLuint *buffers);
extern MYTH_GLMAPBUFFERARBPROC               gMythGLMapBufferARB;
extern MYTH_GLBINDBUFFERARBPROC              gMythGLBindBufferARB;
extern MYTH_GLGENBUFFERSARBPROC              gMythGLGenBuffersARB;
extern MYTH_GLBUFFERDATAARBPROC              gMythGLBufferDataARB;
extern MYTH_GLUNMAPBUFFERARBPROC             gMythGLUnmapBufferARB;
extern MYTH_GLDELETEBUFFERSARBPROC           gMythGLDeleteBuffersARB;

// FrameBuffer Objects
typedef void (APIENTRY * MYTH_GLGENFRAMEBUFFERSEXTPROC)
    (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRY * MYTH_GLBINDFRAMEBUFFEREXTPROC)
    (GLenum target, GLuint framebuffer);
typedef void (APIENTRY * MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC)
    (GLenum target, GLenum attachment,
     GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRY * MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC)
    (GLenum target);
typedef void (APIENTRY * MYTH_GLDELETEFRAMEBUFFERSEXTPROC)
    (GLsizei n, const GLuint *framebuffers);

extern MYTH_GLGENFRAMEBUFFERSEXTPROC         gMythGLGenFramebuffersEXT;
extern MYTH_GLBINDFRAMEBUFFEREXTPROC         gMythGLBindFramebufferEXT;
extern MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC    gMythGLFramebufferTexture2DEXT;
extern MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC  gMythGLCheckFramebufferStatusEXT;
extern MYTH_GLDELETEFRAMEBUFFERSEXTPROC      gMythGLDeleteFramebuffersEXT;

// GLX Video sync
typedef int ( * MYTH_GLXGETVIDEOSYNCSGIPROC)
    (unsigned int *count);
typedef int ( * MYTH_GLXWAITVIDEOSYNCSGIPROC)
    (int divisor, int remainder, unsigned int *count);
extern MYTH_GLXGETVIDEOSYNCSGIPROC           gMythGLXGetVideoSyncSGI;
extern MYTH_GLXWAITVIDEOSYNCSGIPROC          gMythGLXWaitVideoSyncSGI;

// NV_fence
typedef void (APIENTRY * MYTH_GLDELETEFENCESNVPROC)
    (GLsizei n, const GLuint *fences);
typedef void (APIENTRY * MYTH_GLGENFENCESNVPROC)
    (GLsizei n, GLuint *fences);
typedef void (APIENTRY * MYTH_GLFINISHFENCENVPROC)
    (GLuint fence);
typedef void (APIENTRY * MYTH_GLSETFENCENVPROC)
    (GLuint fence, GLenum condition);
extern MYTH_GLGENFENCESNVPROC                gMythGLGenFencesNV;
extern MYTH_GLDELETEFENCESNVPROC             gMythGLDeleteFencesNV;
extern MYTH_GLSETFENCENVPROC                 gMythGLSetFenceNV;
extern MYTH_GLFINISHFENCENVPROC              gMythGLFinishFenceNV;

// APPLE_fence
typedef void ( * MYTH_GLGENFENCESAPPLEPROC)
    (GLsizei n, GLuint *fences);
typedef void ( * MYTH_GLDELETEFENCESAPPLEPROC)
    (GLsizei n, const GLuint *fences);
typedef void ( * MYTH_GLSETFENCEAPPLEPROC)
    (GLuint fence);
typedef void ( * MYTH_GLFINISHFENCEAPPLEPROC)
    (GLuint fence);
extern MYTH_GLGENFENCESAPPLEPROC    gMythGLGenFencesAPPLE;
extern MYTH_GLDELETEFENCESAPPLEPROC gMythGLDeleteFencesAPPLE;
extern MYTH_GLSETFENCEAPPLEPROC     gMythGLSetFenceAPPLE;
extern MYTH_GLFINISHFENCEAPPLEPROC  gMythGLFinishFenceAPPLE;

// win32 SwapBuffers
#ifdef USING_MINGW
typedef void (APIENTRY * MYTH_WGLSWAPBUFFERSPROC) (HDC hDC); 
extern MYTH_WGLSWAPBUFFERSPROC gMythWGLSwapBuffers;
#endif // USING_MINGW

//GLX Swap Control
typedef int ( * MYTH_GLXSWAPINTERVALSGIPROC)
    (int interval);
extern MYTH_GLXSWAPINTERVALSGIPROC gMythGLXSwapIntervalSGI;

//WGL Swap Control
typedef bool (APIENTRY * MYTH_WGLSWAPINTERVALEXTPROC)
    (int interval);
extern MYTH_WGLSWAPINTERVALEXTPROC gMythWGLSwapIntervalEXT;
#endif // USING_OPENGL

#endif // _UTIL_OPENGL_H_
