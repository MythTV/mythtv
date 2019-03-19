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

    void SetColor(int r, int g, int b, int a) override; // MythRenderOpenGL

    uint CreateShaderObject(const QString &vert, const QString &frag) override; // MythRenderOpenGL
    void DeleteShaderObject(uint fp) override; // MythRenderOpenGL
    void EnableShaderObject(uint obj) override; // MythRenderOpenGL
    void SetShaderParams(uint obj, const QMatrix4x4 &m, const char* uniform) override; // MythRenderOpenGL
    void PushTransformation(const UIEffects &fx, QPointF &center) override; // MythRenderOpenGL
    void PopTransformation(void) override; // MythRenderOpenGL


  protected:
    virtual ~MythRenderOpenGL1();
    void DrawBitmapPriv(uint tex, const QRect *src, const QRect *dst,
                        uint prog, int alpha,
                        int red, int green, int blue) override; // MythRenderOpenGL
    void DrawBitmapPriv(uint *textures, uint texture_count,
                        const QRectF *src, const QRectF *dst,
                        uint prog) override; // MythRenderOpenGL
    void DrawRectPriv(const QRect &area, const QBrush &fillBrush,
                      const QPen &linePen, int alpha) override; // MythRenderOpenGL
    void DrawRoundRectPriv(const QRect &/*area*/, int /*cornerRadius*/,
                           const QBrush &/*fillBrush*/, const QPen &/*linePen*/,
                           int /*alpha*/) override { } // MythRenderOpenGL

    void Init2DState(void) override; // MythRenderOpenGL
    void InitProcs(void) override; // MythRenderOpenGL
    bool InitFeatures(void) override; // MythRenderOpenGL
    void DeleteShaders(void) override; // MythRenderOpenGL
    void ResetVars(void) override; // MythRenderOpenGL
    void ResetProcs(void) override; // MythRenderOpenGL
    void DeleteOpenGLResources(void) override; // MythRenderOpenGL
    void SetMatrixView(void) override; // MythRenderOpenGL
    // Resources
    QVector<GLuint> m_programs;

    // State
    uint     m_active_prog {0};
    uint32_t m_color       {0x00000000};

    // Fragment programs
    MYTH_GLGENPROGRAMSARBPROC             m_glGenProgramsARB {nullptr};
    MYTH_GLBINDPROGRAMARBPROC             m_glBindProgramARB {nullptr};
    MYTH_GLPROGRAMSTRINGARBPROC           m_glProgramStringARB {nullptr};
    MYTH_GLPROGRAMLOCALPARAMETER4FARBPROC m_glProgramLocalParameter4fARB {nullptr};
    MYTH_GLDELETEPROGRAMSARBPROC          m_glDeleteProgramsARB {nullptr};
    MYTH_GLGETPROGRAMIVARBPROC            m_glGetProgramivARB {nullptr};
};

#endif // MYTHRENDEROPENGL1_H
