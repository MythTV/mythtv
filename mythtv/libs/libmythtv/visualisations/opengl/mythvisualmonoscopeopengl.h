#ifndef MYTHVISUALMONOSCOPEOPENGL_H
#define MYTHVISUALMONOSCOPEOPENGL_H

// MythTV
#include "opengl/mythrenderopengl.h"
#include "visualisations/videovisualmonoscope.h"

using FrameBuffers = std::array<QOpenGLFramebufferObject*, 2>;
using Textures = std::array<MythGLTexture*, 2>;

class MythVisualMonoScopeOpenGL : public VideoVisualMonoScope
{
  public:
    MythVisualMonoScopeOpenGL(AudioPlayer* Audio, MythRender* Render, bool Fade);
   ~MythVisualMonoScopeOpenGL() override;

    void Draw(const QRect& Area, MythPainter* /*Painter*/, QPaintDevice* /*Device*/) override;

  private:
    MythRenderOpenGL* Initialise (const QRect& Area);
    void              TearDown   ();

    QOpenGLShaderProgram* m_openglShader { nullptr };
    QOpenGLBuffer*        m_vbo          { nullptr };
    bool                  m_currentFBO   { false };
    FrameBuffers          m_fbos         { };
    Textures              m_textures     { };
    Vertices              m_vertices     { 0.0 };
};

#endif
