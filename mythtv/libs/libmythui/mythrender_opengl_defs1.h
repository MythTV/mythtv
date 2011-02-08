#ifndef MYTHRENDER_OPENGL_DEFS1_H
#define MYTHRENDER_OPENGL_DEFS1_H

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

#endif // MYTHRENDER_OPENGL_DEFS1_H
