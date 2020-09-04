#ifndef VIDEOVISUALCIRCLES_H
#define VIDEOVISUALCIRCLES_H

#include "videovisualspectrum.h"

#define CIRCLES_NAME QString("Circles")

class VideoVisualCircles : public VideoVisualSpectrum
{
  public:
    VideoVisualCircles(AudioPlayer* Audio, MythRender* Render);
    QString Name() override { return CIRCLES_NAME; }

  protected:
    bool InitialisePriv() override;
    void DrawPriv(MythPainter* Painter, QPaintDevice* Device) override;
};

#endif
