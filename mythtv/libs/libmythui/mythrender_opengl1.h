#ifndef MYTHRENDEROPENGL1_H
#define MYTHRENDEROPENGL1_H

#include "mythrender_opengl.h"
#include "mythrender_opengl_defs1.h"
#if !defined(Q_OS_MAC)
#include <GL/gl.h>
#endif

class MUI_PUBLIC MythRenderOpenGL1 : public MythRenderOpenGL
{
  public:
    MythRenderOpenGL1(const MythRenderFormat& format, QPaintDevice* device);
    explicit MythRenderOpenGL1(const MythRenderFormat& format);

    virtual void SetColor(int r, int g, int b, int a);

    virtual uint CreateShaderObject(const QString &vert, const QString &frag);
    virtual void DeleteShaderObject(uint obj);
    virtual void EnableShaderObject(uint obj);
    virtual void SetShaderParams(uint obj, const QMatrix4x4 &m, const char* uniform);

    virtual uint CreateHelperTexture(void);

    virtual void  PushTransformation(const UIEffects &fx, QPointF &center);
    virtual void  PopTransformation(void);


  protected:
    virtual ~MythRenderOpenGL1();
    virtual void DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                                uint prog, int alpha,
                                int red, int green, int blue);
    virtual void DrawBitmapPriv(uint *textures, uint texture_count,
                                const QRectF *src, const QRectF *dst,
                                uint prog);
    virtual void DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                              const QPen &linePen, int alpha);
    virtual void DrawRoundRectPriv(const QRect &/*area*/, int /*cornerRadius*/,
                                   const QBrush &/*fillBrush*/, const QPen &/*linePen*/,
                                   int /*alpha*/) { }

    virtual void Init2DState(void);
    virtual void InitProcs(void);
    virtual bool InitFeatures(void);
    virtual void DeleteShaders(void);
    virtual void ResetVars(void);
    virtual void ResetProcs(void);
    virtual void DeleteOpenGLResources(void);
    virtual void SetMatrixView(void);
    // Resources
    QVector<GLuint> m_programs;

    // State
    uint     m_active_prog;
    uint32_t m_color;

    // Fragment programs
    MYTH_GLGENPROGRAMSARBPROC             m_glGenProgramsARB;
    MYTH_GLBINDPROGRAMARBPROC             m_glBindProgramARB;
    MYTH_GLPROGRAMSTRINGARBPROC           m_glProgramStringARB;
    MYTH_GLPROGRAMLOCALPARAMETER4FARBPROC m_glProgramLocalParameter4fARB;
    MYTH_GLDELETEPROGRAMSARBPROC          m_glDeleteProgramsARB;
    MYTH_GLGETPROGRAMIVARBPROC            m_glGetProgramivARB;
};

#endif // MYTHRENDEROPENGL1_H
