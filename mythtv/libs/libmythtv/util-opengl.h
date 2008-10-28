// -*- Mode: c++ -*-

#ifndef _UTIL_OPENGL_H_
#define _UTIL_OPENGL_H_

#ifdef USING_OPENGL

// MythTV headers
#include "mythcontext.h"
#include "util-x11.h"
#include "frame.h"

// GLX headers
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glu.h>

// Qt headers
#include <qstring.h>

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
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

// Not all platforms with OpenGL that MythTV supports have the
// GL_EXT_framebuffer_object extension so we need to define these..
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

#ifndef GL_NV_fence
#define GL_ALL_COMPLETED_NV               0x84F2
#endif

#ifndef GLX_VERSION_1_3
typedef XID GLXPbuffer;
#endif // GLX_VERSION_1_3

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

bool get_glx_version(Display *XJ_disp, uint &major, uint &minor);

bool init_opengl(void);

typedef enum { kRenderRGBA, kSimpleRGBA } FrameBufferType;
int const *get_attr_cfg(FrameBufferType type);

// Requires GLX 1.3 or later
GLXFBConfig get_fbuffer_cfg(Display *XJ_disp, int XJ_screen_num,
                            const int*);

GLXPbuffer get_pbuffer(Display     *XJ_disp,
                       GLXFBConfig  glx_fbconfig,
                       const QSize &video_dim);

Window get_gl_window(Display     *XJ_disp,
                     Window       XJ_curwin,
                     XVisualInfo  *visinfo,
                     const QRect &window_rect,
                     bool         map_window = true);

GLXWindow get_glx_window(Display     *XJ_disp,
                         GLXFBConfig  glx_fbconfig,
                         Window       gl_window,
                         GLXContext   glx_context,
                         GLXPbuffer   glx_pbuffer,
                         const QSize &window_size);

void pack_yv12alpha(const unsigned char *source,
                 const unsigned char *dest,
                 const int *offsets,
                 const int *pitches,
                 const QSize size,
                 const unsigned char *alpha = NULL);

void pack_yv12interlaced(const unsigned char *source,
                 const unsigned char *dest,
                 const int *offsets,
                 const int *pitches,
                 const QSize size);

__GLXextFuncPtr get_gl_proc_address(const QString &procName);

int get_gl_texture_rect_type(const QString &extensions);
bool has_gl_fbuffer_object_support(const QString &extensions);
bool has_gl_fragment_program_support(const QString &extensions);
bool has_glx_video_sync_support(const QString &glx_extensions);
bool has_gl_pixelbuffer_object_support(const QString &extensions);
bool has_gl_nvfence_support(const QString &extensions);

extern QString                             gMythGLExtensions;
extern uint                                gMythGLExtSupported;

extern PFNGLGENPROGRAMSARBPROC             gMythGLGenProgramsARB;
extern PFNGLBINDPROGRAMARBPROC             gMythGLBindProgramARB;
extern PFNGLPROGRAMSTRINGARBPROC           gMythGLProgramStringARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC   gMythGLProgramEnvParameter4fARB;
extern PFNGLDELETEPROGRAMSARBPROC          gMythGLDeleteProgramsARB;
extern PFNGLGETPROGRAMIVARBPROC            gMythGLGetProgramivARB;

extern PFNGLMAPBUFFERPROC                  gMythGLMapBufferARB;
extern PFNGLBINDBUFFERARBPROC              gMythGLBindBufferARB;
extern PFNGLGENBUFFERSARBPROC              gMythGLGenBuffersARB;
extern PFNGLBUFFERDATAARBPROC              gMythGLBufferDataARB;
extern PFNGLUNMAPBUFFERARBPROC             gMythGLUnmapBufferARB;
extern PFNGLDELETEBUFFERSARBPROC           gMythGLDeleteBuffersARB;

// Not all platforms with OpenGL that MythTV supports have the
// GL_EXT_framebuffer_object extension so we need to define these..
typedef void (APIENTRYP MYTH_GLGENFRAMEBUFFERSEXTPROC)
    (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP MYTH_GLBINDFRAMEBUFFEREXTPROC)
    (GLenum target, GLuint framebuffer);
typedef void (APIENTRYP MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC)
    (GLenum target, GLenum attachment, GLenum textarget,
     GLuint texture, GLint level);
typedef GLenum (APIENTRYP MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC)
    (GLenum target);
typedef void (APIENTRYP MYTH_GLDELETEFRAMEBUFFERSEXTPROC)
    (GLsizei n, const GLuint *framebuffers);

extern MYTH_GLGENFRAMEBUFFERSEXTPROC         gMythGLGenFramebuffersEXT;
extern MYTH_GLBINDFRAMEBUFFEREXTPROC         gMythGLBindFramebufferEXT;
extern MYTH_GLFRAMEBUFFERTEXTURE2DEXTPROC    gMythGLFramebufferTexture2DEXT;
extern MYTH_GLCHECKFRAMEBUFFERSTATUSEXTPROC  gMythGLCheckFramebufferStatusEXT;
extern MYTH_GLDELETEFRAMEBUFFERSEXTPROC      gMythGLDeleteFramebuffersEXT;

extern PFNGLXGETVIDEOSYNCSGIPROC           gMythGLXGetVideoSyncSGI;
extern PFNGLXWAITVIDEOSYNCSGIPROC          gMythGLXWaitVideoSyncSGI;

extern PFNGLGENFENCESNVPROC                gMythGLGenFencesNV;
extern PFNGLDELETEFENCESNVPROC             gMythGLDeleteFencesNV;
extern PFNGLSETFENCENVPROC                 gMythGLSetFenceNV;
extern PFNGLFINISHFENCENVPROC              gMythGLFinishFenceNV;
#endif // USING_OPENGL

#endif // _UTIL_OPENGL_H_
