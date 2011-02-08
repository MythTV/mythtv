#ifndef MYTHRENDEROPENGL2_H
#define MYTHRENDEROPENGL2_H

#include "mythrender_opengl.h"
#include "mythrender_opengl_defs2.h"

typedef enum
{
    kShaderSimple  = 0,
    kShaderDefault = 1,
    kShaderCount   = 2,
} DefaultShaders;

class MythGLShaderObject;

class MPUBLIC MythRenderOpenGL2 : public MythRenderOpenGL
{
  public:
    MythRenderOpenGL2(const QGLFormat& format, QPaintDevice* device);
    MythRenderOpenGL2(const QGLFormat& format);
    virtual ~MythRenderOpenGL2();

    virtual uint CreateShaderObject(const QString &vert, const QString &frag);
    virtual void DeleteShaderObject(uint obj);
    virtual void EnableShaderObject(uint obj);
    virtual void SetShaderParams(uint obj, void* vals, const char* uniform);

  protected:
    virtual void DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                                uint prog, int alpha,
                                int red, int green, int blue);
    virtual void DrawBitmapPriv(uint *textures, uint texture_count,
                                const QRectF *src, const QRectF *dst,
                                uint prog);
    virtual void DrawRectPriv(const QRect &area, bool drawFill,
                              const QColor &fillColor,  bool drawLine,
                              int lineWidth, const QColor &lineColor,
                              int prog);
    virtual void Init2DState(void);
    virtual void InitProcs(void);
    virtual void DeleteShaders(void);
    virtual bool InitFeatures(void);
    virtual void ResetVars(void);
    virtual void ResetProcs(void);
    virtual void DeleteOpenGLResources(void);
    virtual void SetMatrixView(void);

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
    uint       m_active_obj;

    // Procs
    MYTH_GLCREATESHADEROBJECTPROC        m_glCreateShaderObject;
    MYTH_GLSHADERSOURCEPROC              m_glShaderSource;
    MYTH_GLCOMPILESHADERPROC             m_glCompileShader;
    MYTH_GLGETSHADERPROC                 m_glGetShader;
    MYTH_GLGETSHADERINFOLOGPROC          m_glGetShaderInfoLog;
    MYTH_GLDELETESHADERPROC              m_glDeleteShader;
    MYTH_GLCREATEPROGRAMOBJECTPROC       m_glCreateProgramObject;
    MYTH_GLATTACHOBJECTPROC              m_glAttachObject;
    MYTH_GLLINKPROGRAMPROC               m_glLinkProgram;
    MYTH_GLUSEPROGRAMPROC                m_glUseProgram;
    MYTH_GLGETINFOLOGPROC                m_glGetInfoLog;
    MYTH_GLGETOBJECTPARAMETERIVPROC      m_glGetObjectParameteriv;
    MYTH_GLDETACHOBJECTPROC              m_glDetachObject;
    MYTH_GLDELETEOBJECTPROC              m_glDeleteObject;
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
