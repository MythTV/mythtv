#ifndef MYTHVISUALMONOSCOPEOPENGL_H
#define MYTHVISUALMONOSCOPEOPENGL_H

// MythTV
#include "libmythui/opengl/mythrenderopengl.h"
#include "visualisations/videovisualmonoscope.h"

// Vertex buffer + Hue,Alpha,Zoom
using VertexStateGL  = std::pair<QOpenGLBuffer*, std::array<float,3>>;
using VertexStatesGL = QVector<VertexStateGL>;
using Vertices = std::vector<float>;

class MythVisualMonoScopeOpenGL : public VideoVisualMonoScope
{
  public:
    MythVisualMonoScopeOpenGL(AudioPlayer* Audio, MythRender* Render, bool Fade);
   ~MythVisualMonoScopeOpenGL() override;

    void Draw(QRect Area, MythPainter* /*Painter*/, QPaintDevice* /*Device*/) override;

  private:
    MythRenderOpenGL* Initialise (QRect Area);
    void              TearDown   ();

    bool                  m_bufferMaps   { false };
    QOpenGLShaderProgram* m_openglShader { nullptr };
    VertexStatesGL        m_vbos;
    Vertices              m_vertices;
};

#endif
