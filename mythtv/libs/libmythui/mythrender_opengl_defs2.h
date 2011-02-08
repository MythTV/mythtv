#ifndef MYTHRENDER_OPENGL_DEFS2_H
#define MYTHRENDER_OPENGL_DEFS2_H

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef GL_VERSION_2_0
typedef char GLchar;
#endif
#ifndef GL_ARB_shader_objects
typedef char GLcharARB;
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

#endif // MYTHRENDER_OPENGL_DEFS2_H
