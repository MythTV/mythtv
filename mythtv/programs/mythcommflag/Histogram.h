#ifndef _HISTOGRAM_H_
#define _HISTOGRAM_H_

typedef struct VideoFrame_ VideoFrame;

class Histogram
{
public:
    Histogram();
    ~Histogram();

    void generateFromImage(VideoFrame* frame, unsigned int frameWidth,
             unsigned int frameHeight, unsigned int minScanX,
             unsigned int maxScanX, unsigned int minScanY,
             unsigned int maxScanY, unsigned int XSpacing,
             unsigned int YSpacing);
    float calculateSimilarityWith(const Histogram&) const;
    unsigned int getAverageIntensity() const;
    unsigned int getThresholdForPercentageOfPixels(float percentage) const;

    // do not override default copy constructor, as the default copy
    // constructor will do just fine.
private:
    int data[256];
    unsigned int numberOfSamples;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

