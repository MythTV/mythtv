#include "ClassicSceneChangeDetector.h"
#include <algorithm>
#include "Histogram.h"

#include "libmyth/mythcontext.h"  //To be able to use VERBOSE()

ClassicSceneChangeDetector::ClassicSceneChangeDetector(unsigned int width,
        unsigned int height, unsigned int commdetectborder_in,
        unsigned int xspacing_in, unsigned int yspacing_in):
    SceneChangeDetectorBase(width,height),
    frameNumber(0),
    previousFrameWasSceneChange(false),
    xspacing(xspacing_in),
    yspacing(yspacing_in),
    commdetectborder(commdetectborder_in)
{
    histogram = new Histogram;
    previousHistogram = new Histogram;
}

ClassicSceneChangeDetector::~ClassicSceneChangeDetector()
{
    delete histogram;
    delete previousHistogram;
}

void ClassicSceneChangeDetector::processFrame(unsigned char* frame)
{
    histogram->generateFromImage(frame, width, height, commdetectborder,
                                 width-commdetectborder, commdetectborder,
                                 height-commdetectborder, xspacing, yspacing);
    float similar = histogram->calculateSimilarityWith(*previousHistogram);

    bool isSceneChange = (similar < .85 && !previousFrameWasSceneChange);

    emit(haveNewInformation(frameNumber,isSceneChange,similar));
    previousFrameWasSceneChange = isSceneChange;

    std::swap(histogram,previousHistogram);
    frameNumber++;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

