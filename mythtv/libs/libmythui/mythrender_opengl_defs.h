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

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER          0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0    0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT 0x8CD8
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_FORMATS
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS 0x8CDA
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#endif
#ifndef GL_FRAMEBUFFER_UNSUPPORTED
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD
#endif

#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER               0x8892
#endif

#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER        0x88EC
#endif

#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW                0x88E0
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY                 0x88B9
#endif

#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB           0x8804
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
#ifndef GL_OBJECT_LINK_STATUS
#define GL_OBJECT_LINK_STATUS             0x8B82
#endif
#ifndef GL_OBJECT_INFO_LOG_LENGTH
#define GL_OBJECT_INFO_LOG_LENGTH         0x8B84
#endif

#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS                 0x8B81
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH                0x8B84
#endif

#ifndef GL_VERSION_2_0
typedef char GLchar;
#endif
#ifndef GL_ARB_shader_objects
typedef char GLcharARB;
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
typedef void (APIENTRY * MYTH_GLPROGRAMLOCALPARAMETER4FARBPROC)
    (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * MYTH_GLGETPROGRAMIVARBPROC)
    (GLenum target, GLenum pname, GLint *params);
typedef ptrdiff_t MYTH_GLsizeiptr;
typedef GLvoid* (APIENTRY * MYTH_GLMAPBUFFERPROC)
    (GLenum target, GLenum access);
typedef void (APIENTRY * MYTH_GLBINDBUFFERPROC)
    (GLenum target, GLuint buffer);
typedef void (APIENTRY * MYTH_GLGENBUFFERSPROC)
    (GLsizei n, GLuint *buffers);
typedef void (APIENTRY * MYTH_GLBUFFERDATAPROC)
    (GLenum target, MYTH_GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLboolean (APIENTRY * MYTH_GLUNMAPBUFFERPROC)
    (GLenum target);
typedef void (APIENTRY * MYTH_GLDELETEBUFFERSPROC)
    (GLsizei n, const GLuint *buffers);
typedef void (APIENTRY * MYTH_GLGENFRAMEBUFFERSPROC)
    (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRY * MYTH_GLBINDFRAMEBUFFERPROC)
    (GLenum target, GLuint framebuffer);
typedef void (APIENTRY * MYTH_GLFRAMEBUFFERTEXTURE2DPROC)
    (GLenum target, GLenum attachment,
     GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRY * MYTH_GLCHECKFRAMEBUFFERSTATUSPROC)
    (GLenum target);
typedef void (APIENTRY * MYTH_GLDELETEFRAMEBUFFERSPROC)
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
typedef GLuint ( * MYTH_GLCREATESHADEROBJECTPROC)
    (GLenum shaderType);
typedef void ( * MYTH_GLSHADERSOURCEPROC)
    (GLuint shader, int numOfStrings, const char **strings, int *lenOfStrings);
typedef void ( * MYTH_GLCOMPILESHADERPROC)
    (GLuint shader);
typedef void ( * MYTH_GLGETSHADERPROC)
    (GLuint shader, GLenum pname, GLint *params);
typedef void ( * MYTH_GLGETSHADERINFOLOGPROC)
    (GLuint shader, GLint maxlength, GLint length, GLchar *infolog);
typedef void ( * MYTH_GLDELETESHADERPROC)
    (GLuint shader);
typedef GLuint ( * MYTH_GLCREATEPROGRAMOBJECTPROC)
    (void);
typedef void ( * MYTH_GLATTACHOBJECTPROC)
    (GLuint program, GLuint shader);
typedef void ( * MYTH_GLLINKPROGRAMPROC)
    (GLuint program);
typedef void ( * MYTH_GLUSEPROGRAMPROC)
    (GLuint program);
typedef void ( * MYTH_GLGETINFOLOGPROC)
    (GLuint object, int maxLen, int *len, char *log);
typedef void ( * MYTH_GLGETOBJECTPARAMETERIVPROC)
    (GLuint object, GLenum type, int *param);
typedef void ( * MYTH_GLDETACHOBJECTPROC)
    (GLuint program, GLuint shader);
typedef void ( * MYTH_GLDELETEOBJECTPROC)
    (GLuint id);
typedef GLint ( * MYTH_GLGETUNIFORMLOCATIONPROC)
    (GLuint program, const char *name);
typedef void  ( * MYTH_GLUNIFORM4FPROC)
    (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void  ( * MYTH_GLUNIFORMMATRIX4FVPROC)
    (GLint location, GLint size, GLboolean transpose, const GLfloat *values);
typedef void ( * MYTH_GLVERTEXATTRIBPOINTERPROC)
    (GLuint index, GLint size, GLenum type, GLboolean normalize,
     GLsizei stride, const GLvoid *ptr);
typedef void ( * MYTH_GLENABLEVERTEXATTRIBARRAYPROC)
    (GLuint index);
typedef void ( * MYTH_GLDISABLEVERTEXATTRIBARRAYPROC)
    (GLuint index);
typedef void ( * MYTH_GLBINDATTRIBLOCATIONPROC)
    (GLuint program, GLuint index, const GLcharARB *name);
typedef void ( * MYTH_GLVERTEXATTRIB4FPROC)
    (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

#endif
