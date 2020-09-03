#ifndef VIDEOVISUALMONOSCOPE_H
#define VIDEOVISUALMONOSCOPE_H

// MythTV
#include "opengl/mythrenderopengl.h"
#include "videovisual.h"

#define NUM_SAMPLES 256
#define FADE_NAME   QStringLiteral("FadeScope")
#define SIMPLE_NAME QStringLiteral("SimpleScope")

using Vertices = std::array<GLfloat, NUM_SAMPLES * 2>;
using FrameBuffers = std::array<QOpenGLFramebufferObject*, 2>;
using Textures = std::array<MythGLTexture*, 2>;

class VideoVisualMonoScope : public VideoVisual
{
  public:
    VideoVisualMonoScope(AudioPlayer* Audio, MythRender* Render, bool Fade);
    ~VideoVisualMonoScope() override;
    void     Draw(const QRect& Area, MythPainter* Painter, QPaintDevice* /*Device*/) override;
    QString  Name() override;

  private:
    MythRenderOpenGL* Initialise(const QRect& Area);

    bool                  m_fade       { false };
    Vertices              m_vertices   { 0.0 };
    QOpenGLShaderProgram* m_shader     { nullptr };
    QOpenGLBuffer*        m_vbo        { nullptr };
    bool                  m_currentFBO { false };
    FrameBuffers          m_fbos       { };
    Textures              m_textures   { };
    int64_t               m_lastTime   { 0 };
    qreal                 m_hue        { 0.0 };
};

#endif
