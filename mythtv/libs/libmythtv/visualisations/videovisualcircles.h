#ifndef VIDEOVISUALCIRCLES_H
#define VIDEOVISUALCIRCLES_H

#include "videovisualspectrum.h"

class VideoVisualCircles : public VideoVisualSpectrum
{
  public:
    VideoVisualCircles(AudioPlayer *audio, MythRender *render);

  protected:
    virtual bool InitialisePriv(void);
    virtual void DrawPriv(MythPainter *painter, QPaintDevice* device);
};

#endif // VIDEOVISUALCIRCLES_H
