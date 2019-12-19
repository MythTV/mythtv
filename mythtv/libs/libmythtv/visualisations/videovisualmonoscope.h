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
    virtual ~VideoVisualMonoScope() override;
    void     Draw(const QRect &Area, MythPainter *Painter, QPaintDevice*) override;
    QString  Name(void) override;

  private:
    MythRenderOpenGL* Initialise(const QRect &Area);

    bool                  m_fade       { false };
    GLfloat               m_vertices[(NUM_SAMPLES * 2) + 16] { 0.0 };
    QOpenGLShaderProgram *m_shader     { nullptr };
    QOpenGLBuffer        *m_vbo        { nullptr };
    bool                  m_currentFBO { false };
    QOpenGLFramebufferObject *m_fbo[2] { nullptr };
    MythGLTexture        *m_texture[2] { nullptr };
    int64_t               m_lastTime   { 0 };
    qreal                 m_hue        { 0.0 };
};

#endif // VIDEOVISUALMONOSCOPE_H
