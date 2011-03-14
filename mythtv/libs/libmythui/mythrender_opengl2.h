#ifndef MYTHRENDEROPENGL2_H
#define MYTHRENDEROPENGL2_H

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
    MythRenderOpenGL2(const QGLFormat& format, QPaintDevice* device);
    MythRenderOpenGL2(const QGLFormat& format);
    virtual ~MythRenderOpenGL2();

    virtual uint CreateShaderObject(const QString &vert, const QString &frag);
    virtual void DeleteShaderObject(uint obj);
    virtual void EnableShaderObject(uint obj);
    virtual void SetShaderParams(uint obj, void* vals, const char* uniform);

    virtual bool RectanglesAreAccelerated(void) { return true; }

  protected:
    virtual void DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                                uint prog, int alpha,
                                int red, int green, int blue);
    virtual void DrawBitmapPriv(uint *textures, uint texture_count,
                                const QRectF *src, const QRectF *dst,
                                uint prog);
    virtual void DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                              const QPen &linePen, int alpha);
    virtual void DrawRoundRectPriv(const QRect &area, int cornerRadius,
                                   const QBrush &fillBrush, const QPen &linePen,
                                   int alpha);

    virtual void Init2DState(void);
    virtual void InitProcs(void);
    virtual void DeleteShaders(void);
    virtual bool InitFeatures(void);
    virtual void ResetVars(void);
    virtual void ResetProcs(void);
    virtual void DeleteOpenGLResources(void);
    virtual void SetMatrixView(void);

    void SetScaling(int horizontal, int vertical);
    void SetRotation(int degrees);
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
    float m_projection[4][4];
    float m_scale[4][4];
    float m_rotate[4][4];
    float m_parameters[4][4];
    QString m_qualifiers;

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
    MYTH_GLUNIFORM4FPROC                 m_glUniform4f;
    MYTH_GLUNIFORMMATRIX4FVPROC          m_glUniformMatrix4fv;
    MYTH_GLVERTEXATTRIBPOINTERPROC       m_glVertexAttribPointer;
    MYTH_GLENABLEVERTEXATTRIBARRAYPROC   m_glEnableVertexAttribArray;
    MYTH_GLDISABLEVERTEXATTRIBARRAYPROC  m_glDisableVertexAttribArray;
    MYTH_GLBINDATTRIBLOCATIONPROC        m_glBindAttribLocation;
    MYTH_GLVERTEXATTRIB4FPROC            m_glVertexAttrib4f;
};

#endif // MYTHRENDEROPENGL2_H
