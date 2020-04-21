#ifndef VIDEOVISUALMONOSCOPE_H
#define VIDEOVISUALMONOSCOPE_H

// MythTV
#include "opengl/mythrenderopengl.h"
#include "videovisual.h"

#define NUM_SAMPLES 256

class VideoVisualMonoScope : public VideoVisual
{
  public:
    VideoVisualMonoScope(AudioPlayer *Audio, MythRender *Render, bool Fade);
    ~VideoVisualMonoScope() override;
    void     Draw(const QRect &Area, MythPainter *Painter, QPaintDevice* /*device*/) override;
    QString  Name(void) override;

  private:
    MythRenderOpenGL* Initialise(const QRect &Area);

    bool                  m_fade       { false };
    std::array<GLfloat,NUM_SAMPLES * 2>     m_vertices   { 0.0 };
    QOpenGLShaderProgram *m_shader     { nullptr };
    QOpenGLBuffer        *m_vbo        { nullptr };
    bool                  m_currentFBO { false };
    std::array<QOpenGLFramebufferObject*,2> m_fbo        { };
    std::array<MythGLTexture*,2>            m_texture    { };
    int64_t               m_lastTime   { 0 };
    qreal                 m_hue        { 0.0 };
};

#endif // VIDEOVISUALMONOSCOPE_H
