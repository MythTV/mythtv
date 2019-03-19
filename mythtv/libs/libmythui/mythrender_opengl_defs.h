#ifndef MYTHRENDER_OPENGL_DEFS_H_
#define MYTHRENDER_OPENGL_DEFS_H_

#ifndef GL_COLOR_EXT
#define GL_COLOR_EXT 0x1800
#endif

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

#ifndef GL_NV_fence
#define GL_ALL_COMPLETED_NV               0x84F2
#endif

#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
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
typedef void (APIENTRY * MYTH_GLDISCARDFRAMEBUFFER)
    (GLenum target, GLsizei numAttachments, const GLenum *attachments);
#endif
