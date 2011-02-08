#ifndef MYTHRENDER_OPENGL_DEFS_H_
#define MYTHRENDER_OPENGL_DEFS_H_

#ifndef GL_GENERATE_MIPMAP_SGIS
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#endif

#ifndef GL_GENERATE_MIPMAP_HINT_SGIS
#define GL_GENERATE_MIPMAP_HINT_SGIS 0x8192
#endif

#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif

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

#ifndef APIENTRY
#define APIENTRY
#endif

typedef void (APIENTRY * MYTH_GLACTIVETEXTUREPROC)
    (GLenum texture);

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
#endif
