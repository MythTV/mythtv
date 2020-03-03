#ifndef _CLASSICSCENECHANGEDETECTOR_H_
#define _CLASSICSCENECHANGEDETECTOR_H_

#include "SceneChangeDetectorBase.h"

class Histogram;

class ClassicSceneChangeDetector : public SceneChangeDetectorBase
{
  public:
    ClassicSceneChangeDetector(unsigned int width, unsigned int height,
        unsigned int commdetectborder, unsigned int xspacing,
        unsigned int yspacing);
    virtual void deleteLater(void);

    void processFrame(VideoFrame* frame) override; // SceneChangeDetectorBase

  private:
    ~ClassicSceneChangeDetector() override;

  private:
    Histogram    *m_histogram                   {nullptr};
    Histogram    *m_previousHistogram           {nullptr};
    unsigned int  m_frameNumber                 {0};
    bool          m_previousFrameWasSceneChange {false};
    unsigned int  m_xspacing, m_yspacing;
    unsigned int  m_commdetectborder;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

