#include <algorithm>
using namespace std;

#include "ClassicSceneChangeDetector.h"
#include "Histogram.h"

ClassicSceneChangeDetector::ClassicSceneChangeDetector(unsigned int width,
        unsigned int height, unsigned int commdetectborder_in,
        unsigned int xspacing_in, unsigned int yspacing_in):
    SceneChangeDetectorBase(width,height),
    m_xspacing(xspacing_in),
    m_yspacing(yspacing_in),
    m_commdetectborder(commdetectborder_in)
{
    m_histogram = new Histogram;
    m_previousHistogram = new Histogram;
}

ClassicSceneChangeDetector::~ClassicSceneChangeDetector()
{
    delete m_histogram;
    delete m_previousHistogram;
}

void ClassicSceneChangeDetector::deleteLater(void)
{
    SceneChangeDetectorBase::deleteLater();
}

void ClassicSceneChangeDetector::processFrame(VideoFrame* frame)
{
    m_histogram->generateFromImage(frame, m_width, m_height, m_commdetectborder,
                                 m_width-m_commdetectborder, m_commdetectborder,
                                 m_height-m_commdetectborder, m_xspacing, m_yspacing);
    float similar = m_histogram->calculateSimilarityWith(*m_previousHistogram);

    bool isSceneChange = (similar < .85F && !m_previousFrameWasSceneChange);

    emit(haveNewInformation(m_frameNumber,isSceneChange,similar));
    m_previousFrameWasSceneChange = isSceneChange;

    std::swap(m_histogram,m_previousHistogram);
    m_frameNumber++;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

