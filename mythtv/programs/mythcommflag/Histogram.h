#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "libmythtv/mythframe.h"

class Histogram
{
public:
    Histogram() = default;
    ~Histogram() = default;

    void generateFromImage(MythVideoFrame* frame, unsigned int frameWidth,
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
    std::array<int,256> m_data {0};

    // prevent division by 0 in case a virgin histogram gets used.
    unsigned int m_numberOfSamples {1};
};

#endif // HISTOGRAM_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
