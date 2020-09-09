#ifndef VIDEOVISUALMONOSCOPE_H
#define VIDEOVISUALMONOSCOPE_H

// MythTV
#include "videovisual.h"

#define NUM_SAMPLES 256
#define FADE_NAME   QString("FadeScope")
#define SIMPLE_NAME QString("SimpleScope")

class VideoVisualMonoScope : public VideoVisual
{
  public:
    VideoVisualMonoScope(AudioPlayer* Audio, MythRender* Render, bool Fade);
    QString  Name() override;

  protected:
    Q_DISABLE_COPY(VideoVisualMonoScope)
    void                  InitCommon      (const QRect& Area);
    void                  UpdateVertices  (float* Buffer);
    void                  UpdateTime      ();

    int64_t               m_lastTime      { 0     };
    float                 m_hue           { 0.0   };
    float                 m_rate          { 1.0   };
    float                 m_lineWidth     { 1.0   };
    float                 m_maxLineWidth  { 1.0   };
    bool                  m_fade          { false };
};

#endif
