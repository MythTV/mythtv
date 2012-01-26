#ifndef VIDEOVISUALGOOM_H
#define VIDEOVISUALGOOM_H

#include "videovisual.h"

class VideoVisualGoom : public VideoVisual
{
  public:
    VideoVisualGoom(AudioPlayer *audio, MythRender *render, bool hd);
    virtual ~VideoVisualGoom();

    virtual void Draw(const QRect &area, MythPainter *painter,
                      QPaintDevice* device);
    virtual QString Name(void) { return m_hd ? "Goom HD" : "Goom"; }

  private:
    unsigned int* m_buffer;
    uint          m_surface;
    bool          m_hd;
};

#endif // VIDEOVISUALGOOM_H
