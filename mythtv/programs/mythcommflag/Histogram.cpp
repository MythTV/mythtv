#include "Histogram.h"
#include <string>
#include <cmath>
#include <cstring>

#include "frame.h"

Histogram::Histogram()
{
    memset(data,0,sizeof(data));

    // prevent division by 0 in case a virgin histogram gets used.
    numberOfSamples =1;
}

Histogram::~Histogram()
{
}

void Histogram::generateFromImage(VideoFrame* frame, unsigned int frameWidth,
         unsigned int frameHeight, unsigned int minScanX, unsigned int maxScanX,
         unsigned int minScanY, unsigned int maxScanY, unsigned int XSpacing,
         unsigned int YSpacing)
{
    memset(data,0,sizeof(data));
    numberOfSamples = 0;

    if (maxScanX > frameWidth-1)
        maxScanX = frameWidth-1;

    if (maxScanY > frameHeight-1)
        maxScanY = frameHeight-1;

    unsigned char* framePtr = frame->buf;
    int bytesPerLine = frame->pitches[0];
    for(unsigned int y = minScanY; y < maxScanY; y += YSpacing)
        for(unsigned int x = minScanX; x < maxScanX; x += XSpacing)
        {
            data[framePtr[y * bytesPerLine + x]]++;
            numberOfSamples++;
        }
}

unsigned int Histogram::getAverageIntensity(void) const
{
    if (!numberOfSamples)
       return 0;

    long value = 0;

    for(int i = 0; i < 256; i++)
    {
        value += data[i]*i;
    }

    return value / numberOfSamples;
}

unsigned int Histogram::getThresholdForPercentageOfPixels(float percentage)
    const
{
    long value = 0;

    for(int i = 255; i !=0; i--)
    {
        if (value > percentage*numberOfSamples)
            return i;

        value += data[i];
    }

    return 0;
}

float Histogram::calculateSimilarityWith(const Histogram& other) const
{
    long similar = 0;

    for(unsigned int i = 0; i < 256; i++)
    {
        if (data[i] < other.data[i])
            similar += data[i];
        else
            similar += other.data[i];
    }

    //Using c style cast for old gcc compatibility.
    return static_cast<float>(similar) / static_cast<float>(numberOfSamples);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
