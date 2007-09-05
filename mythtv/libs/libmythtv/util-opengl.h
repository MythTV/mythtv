// -*- Mode: c++ -*-

#ifndef _UTIL_OPENGL_H_
#define _UTIL_OPENGL_H_

#ifdef USING_OPENGL

// MythTV headers
#include "mythcontext.h"
#include "util-x11.h"

// GLX headers
#include <GL/glx.h>
#include <GL/glu.h>

// Qt headers
#include <qstring.h>

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
GLXFBConfig get_fbuffer_cfg(Display *XJ_disp, int XJ_screen_num,
                            FrameBufferType);

// Requires GLX 1.3 or later
GLXPbuffer get_pbuffer(Display     *XJ_disp,
                       GLXFBConfig  glx_fbconfig,
                       const QSize &video_dim);

Window get_gl_window(Display     *XJ_disp,
                     Window       XJ_curwin,
                     GLXFBConfig  glx_fbconfig,
                     const QSize &window_size,
                     bool         map_window);

GLXWindow get_glx_window(Display     *XJ_disp,
                         GLXFBConfig  glx_fbconfig,
                         Window       gl_window,
                         GLXContext   glx_context,
                         GLXPbuffer   glx_pbuffer,
                         const QSize &window_size);

void copy_pixels_to_texture(const unsigned char *buf,
                            int          buffer_format,
                            const QSize &buffer_size,
                            int          texture,
                            int          texture_type);

__GLXextFuncPtr get_gl_proc_address(const QString &procName);

int get_gl_texture_rect_type(const QString &extensions);
bool has_gl_fbuffer_object_support(const QString &extensions);
bool has_gl_fragment_program_support(const QString &extensions);
bool has_glx_video_sync_support(const QString &glx_extensions);

extern QString                             gMythGLExtensions;
extern uint                                gMythGLExtSupported;

extern PFNGLGENPROGRAMSARBPROC             gMythGLGenProgramsARB;
extern PFNGLBINDPROGRAMARBPROC             gMythGLBindProgramARB;
extern PFNGLPROGRAMSTRINGARBPROC           gMythGLProgramStringARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC   gMythGLProgramEnvParameter4fARB;
extern PFNGLDELETEPROGRAMSARBPROC          gMythGLDeleteProgramsARB;
extern PFNGLGETPROGRAMIVARBPROC            gMythGLGetProgramivARB;

extern PFNGLGENFRAMEBUFFERSEXTPROC         gMythGLGenFramebuffersEXT;
extern PFNGLBINDFRAMEBUFFEREXTPROC         gMythGLBindFramebufferEXT;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    gMythGLFramebufferTexture2DEXT;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  gMythGLCheckFramebufferStatusEXT;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC      gMythGLDeleteFramebuffersEXT;

extern PFNGLXGETVIDEOSYNCSGIPROC           gMythGLXGetVideoSyncSGI;
extern PFNGLXWAITVIDEOSYNCSGIPROC          gMythGLXWaitVideoSyncSGI;

#endif // USING_OPENGL

#endif // _UTIL_OPENGL_H_
