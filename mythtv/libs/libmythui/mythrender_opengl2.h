#ifndef MYTHRENDEROPENGL2_H
#define MYTHRENDEROPENGL2_H

#include <QStack>
#include <QHash>
#include <QMatrix4x4>

#include "mythrender_opengl.h"
#include "mythrender_opengl_defs2.h"

typedef enum
{
    kShaderSimple  = 0,
    kShaderDefault,
    kShaderCircle,
    kShaderCircleEdge,
    kShaderVertLine,
    kShaderHorizLine,
    kShaderCount,
} DefaultShaders;

class MythGLShaderObject;

class MUI_PUBLIC MythRenderOpenGL2 : public MythRenderOpenGL
{
  public:
    MythRenderOpenGL2(const MythRenderFormat& format, QPaintDevice* device,
                      RenderType type = kRenderOpenGL2);
    MythRenderOpenGL2(const MythRenderFormat& format, RenderType type = kRenderOpenGL2);

    uint CreateShaderObject(const QString &vert, const QString &frag) override; // MythRenderOpenGL
    void DeleteShaderObject(uint obj) override; // MythRenderOpenGL
    void EnableShaderObject(uint obj) override; // MythRenderOpenGL
    void SetShaderParams(uint obj, const QMatrix4x4 &m, const char* uniform) override; // MythRenderOpenGL

    bool RectanglesAreAccelerated(void) override // MythRenderOpenGL
        { return true; }

    void  PushTransformation(const UIEffects &fx, QPointF &center) override; // MythRenderOpenGL
    void  PopTransformation(void) override; // MythRenderOpenGL

  protected:
    virtual ~MythRenderOpenGL2();
    void DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                        uint prog, int alpha,
                        int red, int green, int blue) override; // MythRenderOpenGL
    void DrawBitmapPriv(uint *textures, uint texture_count,
                        const QRectF *src, const QRectF *dst,
                        uint prog) override; // MythRenderOpenGL
    void DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                      const QPen &linePen, int alpha) override; // MythRenderOpenGL
    void DrawRoundRectPriv(const QRect &area, int cornerRadius,
                           const QBrush &fillBrush, const QPen &linePen,
                           int alpha) override; // MythRenderOpenGL

    void Init2DState(void) override; // MythRenderOpenGL
    void InitProcs(void) override; // MythRenderOpenGL
    void DeleteShaders(void) override; // MythRenderOpenGL
    bool InitFeatures(void) override; // MythRenderOpenGL
    void ResetVars(void) override; // MythRenderOpenGL
    void ResetProcs(void) override; // MythRenderOpenGL
    void DeleteOpenGLResources(void) override; // MythRenderOpenGL
    void SetMatrixView(void) override; // MythRenderOpenGL

    void CreateDefaultShaders(void);
    void DeleteDefaultShaders(void);
    uint CreateShader(int type, const QString &source);
    bool ValidateShaderObject(uint obj);
    bool CheckObjectStatus(uint obj);
    void OptimiseShaderSource(QString &source);

    // Resources
    QHash<GLuint, MythGLShaderObject> m_shader_objects;
    uint     m_shaders[kShaderCount];

    // State
    uint  m_active_obj;
    QMatrix4x4 m_projection;
    QStack<QMatrix4x4> m_transforms;
    QMatrix4x4 m_parameters;
    QString m_qualifiers;
    QString m_GLSLVersion;

    typedef QHash<QString,QMatrix4x4> map_t;
    map_t m_map;

    // Procs
    MYTH_GLGETSHADERIVPROC               m_glGetShaderiv;
    MYTH_GLCREATESHADERPROC              m_glCreateShader;
    MYTH_GLSHADERSOURCEPROC              m_glShaderSource;
    MYTH_GLCOMPILESHADERPROC             m_glCompileShader;
    MYTH_GLATTACHSHADERPROC              m_glAttachShader;
    MYTH_GLGETSHADERINFOLOGPROC          m_glGetShaderInfoLog;
    MYTH_GLDETACHSHADERPROC              m_glDetachShader;
    MYTH_GLDELETESHADERPROC              m_glDeleteShader;

    MYTH_GLCREATEPROGRAMPROC             m_glCreateProgram;
    MYTH_GLLINKPROGRAMPROC               m_glLinkProgram;
    MYTH_GLUSEPROGRAMPROC                m_glUseProgram;
    MYTH_GLDELETEPROGRAMPROC             m_glDeleteProgram;
    MYTH_GLGETPROGRAMINFOLOGPROC         m_glGetProgramInfoLog;
    MYTH_GLGETPROGRAMIVPROC              m_glGetProgramiv;

    MYTH_GLGETUNIFORMLOCATIONPROC        m_glGetUniformLocation;
    MYTH_GLUNIFORM1IPROC                 m_glUniform1i;
    MYTH_GLUNIFORMMATRIX4FVPROC          m_glUniformMatrix4fv;
    MYTH_GLVERTEXATTRIBPOINTERPROC       m_glVertexAttribPointer;
    MYTH_GLENABLEVERTEXATTRIBARRAYPROC   m_glEnableVertexAttribArray;
    MYTH_GLDISABLEVERTEXATTRIBARRAYPROC  m_glDisableVertexAttribArray;
    MYTH_GLBINDATTRIBLOCATIONPROC        m_glBindAttribLocation;
    MYTH_GLVERTEXATTRIB4FPROC            m_glVertexAttrib4f;

    // Prevent compiler complaints about using 0 as a null pointer.
    inline void m_glVertexAttribPointerI(GLuint index, GLint size, GLenum type,
                                  GLboolean normalize, GLsizei stride,
                                  const GLuint value);
};

#endif // MYTHRENDEROPENGL2_H
