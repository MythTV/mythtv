#ifndef VIDEOVISUALCIRCLES_H
#define VIDEOVISUALCIRCLES_H

#include "videovisualspectrum.h"

class VideoVisualCircles : public VideoVisualSpectrum
{
  public:
    VideoVisualCircles(AudioPlayer *audio, MythRender *render);
    QString Name(void) override // VideoVisualSpectrum
        { return "Circles"; }

  protected:
    bool InitialisePriv(void) override; // VideoVisualSpectrum
    void DrawPriv(MythPainter *painter, QPaintDevice* device) override; // VideoVisualSpectrum
};

#endif // VIDEOVISUALCIRCLES_H
