#ifndef MYTHRENDER_OPENGL_DEFS_H_
#define MYTHRENDER_OPENGL_DEFS_H_

// OpenGL ES 2.0 workarounds
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 GL_RGBA
#endif
#ifndef GL_RGBA16
#ifdef GL_EXT_texture_norm16
#define GL_RGBA16 GL_RGBA16_EXT
#else
#define GL_RGBA16 0x805B
#endif
#endif
// end workarounds

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
#ifndef GL_UNSIGNED_SHORT_8_8_MESA
#define GL_UNSIGNED_SHORT_8_8_MESA        0x85BA
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

typedef ptrdiff_t MYTH_GLsizeiptr;
typedef GLvoid* (APIENTRY * MYTH_GLMAPBUFFERPROC)
    (GLenum target, GLenum access);
typedef GLboolean (APIENTRY * MYTH_GLUNMAPBUFFERPROC)
    (GLenum target);
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
