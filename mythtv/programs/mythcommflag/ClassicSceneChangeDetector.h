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
    ~ClassicSceneChangeDetector();

    void processFrame(unsigned char* frame);

private:
    Histogram* histogram;
    Histogram* previousHistogram;
    unsigned int frameNumber;
    bool previousFrameWasSceneChange;
    unsigned int xspacing, yspacing;
    unsigned int commdetectborder;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

