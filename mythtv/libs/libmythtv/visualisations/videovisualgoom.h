#ifndef VIDEOVISUALGOOM_H
#define VIDEOVISUALGOOM_H

#include "videovisual.h"

class MythGLTexture;
class VideoVisualGoom : public VideoVisual
{
  public:
    VideoVisualGoom(AudioPlayer *audio, MythRender *render, bool hd);
    virtual ~VideoVisualGoom() override;

    // VideoVisual
    void Draw(const QRect &area, MythPainter *painter, QPaintDevice* device) override;
    QString Name(void) override { return m_hd ? "Goom HD" : "Goom"; }

  private:
    unsigned int  *m_buffer;
    MythGLTexture *m_glSurface;
    bool           m_hd;
};

#endif // VIDEOVISUALGOOM_H
