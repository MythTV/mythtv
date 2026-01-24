#ifndef VIDEOVISUALGOOM_H
#define VIDEOVISUALGOOM_H

#include "libmythbase/mythconfig.h"

#include "videovisual.h"

class MythGLTexture;

#define GOOM_NAME QString("Goom")
#define GOOMHD_NAME QString("Goom HD")

class VideoVisualGoom : public VideoVisual
{
  public:
    VideoVisualGoom(AudioPlayer* Audio, MythRender* Render, bool HD);
    ~VideoVisualGoom() override;

    void Draw(QRect Area, MythPainter* Painter, QPaintDevice* Device) override;
    QString Name(void) override { return m_hd ? GOOMHD_NAME : GOOM_NAME; }

  private:
    unsigned int*  m_buffer    { nullptr };
#if CONFIG_OPENGL
    MythGLTexture* m_glSurface { nullptr };
#endif
    bool           m_hd        { false   };
};

#endif
