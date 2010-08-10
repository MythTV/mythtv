#ifndef MYTHRENDER_OPENGL_DEFS_H_
#define MYTHRENDER_OPENGL_DEFS_H_

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
#ifndef GL_YCBCR_422_APPLE
#define GL_YCBCR_422_APPLE                0x85B9
#endif
#ifndef GL_RGB_422_APPLE
#define GL_RGB_422_APPLE                  0x8A1F
#endif
#ifndef GL_UNSIGNED_SHORT_8_8_MESA
#define GL_UNSIGNED_SHORT_8_8_MESA        0x85BA
#endif

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER                0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER                  0x8B31
#endif
#ifndef GL_OBJECT_LINK_STATUS_ARB
#define GL_OBJECT_LINK_STATUS_ARB         0x8B82
#endif
#ifndef GL_OBJECT_INFO_LOG_LENGTH_ARB
#define GL_OBJECT_INFO_LOG_LENGTH_ARB     0x8B84
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

typedef void (APIENTRY * MYTH_GLACTIVETEXTUREPROC)
    (GLenum texture);
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
typedef void (APIENTRY * MYTH_GLDELETEFENCESNVPROC)
    (GLsizei n, const GLuint *fences);
typedef void (APIENTRY * MYTH_GLGENFENCESNVPROC)
    (GLsizei n, GLuint *fences);
typedef void (APIENTRY * MYTH_GLFINISHFENCENVPROC)
    (GLuint fence);
typedef void (APIENTRY * MYTH_GLSETFENCENVPROC)
    (GLuint fence, GLenum condition);
typedef void ( * MYTH_GLGENFENCESAPPLEPROC)
    (GLsizei n, GLuint *fences);
typedef void ( * MYTH_GLDELETEFENCESAPPLEPROC)
    (GLsizei n, const GLuint *fences);
typedef void ( * MYTH_GLSETFENCEAPPLEPROC)
    (GLuint fence);
typedef void ( * MYTH_GLFINISHFENCEAPPLEPROC)
    (GLuint fence);
typedef int ( * MYTH_GLXGETVIDEOSYNCSGIPROC)
    (unsigned int *count);
typedef int ( * MYTH_GLXWAITVIDEOSYNCSGIPROC)
    (int divisor, int remainder, unsigned int *count);
typedef GLuint ( * MYTH_GLCREATESHADEROBJECT)
    (GLenum shaderType);
typedef void ( * MYTH_GLSHADERSOURCE)
    (GLuint shader, int numOfStrings, const char **strings, int *lenOfStrings);
typedef void ( * MYTH_GLCOMPILESHADER)
    (GLuint shader);
typedef GLuint ( * MYTH_GLCREATEPROGRAMOBJECT)
    (void);
typedef void ( * MYTH_GLATTACHOBJECT)
    (GLuint program, GLuint shader);
typedef void ( * MYTH_GLLINKPROGRAM)
    (GLuint program);
typedef void ( * MYTH_GLUSEPROGRAM)
    (GLuint program);
typedef void ( * MYTH_GLGETINFOLOG)
    (GLuint object, int maxLen, int *len, char *log);
typedef void ( * MYTH_GLGETOBJECTPARAMETERIV)
    (GLuint object, GLenum type, int *param);
typedef void ( * MYTH_GLDETACHOBJECT)
    (GLuint program, GLuint shader);
typedef void ( * MYTH_GLDELETEOBJECT)
    (GLuint id);
typedef GLint ( * MYTH_GLGETUNIFORMLOCATION)
    (GLuint program, const char *name);
typedef void  ( * MYTH_GLUNIFORM4F)
    (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

#endif
