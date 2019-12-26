#ifndef _HISTOGRAM_H_
#define _HISTOGRAM_H_

#include "mythframe.h"

class Histogram
{
public:
    Histogram() = default;
    ~Histogram() = default;

    void generateFromImage(VideoFrame* frame, unsigned int frameWidth,
             unsigned int frameHeight, unsigned int minScanX,
             unsigned int maxScanX, unsigned int minScanY,
             unsigned int maxScanY, unsigned int XSpacing,
             unsigned int YSpacing);
    float calculateSimilarityWith(const Histogram &other) const;
    unsigned int getAverageIntensity() const;
    unsigned int getThresholdForPercentageOfPixels(float percentage) const;

    // do not override default copy constructor, as the default copy
    // constructor will do just fine.
private:
    int m_data[256] {0};

    // prevent division by 0 in case a virgin histogram gets used.
    unsigned int m_numberOfSamples {1};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

