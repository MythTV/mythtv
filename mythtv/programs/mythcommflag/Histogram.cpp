#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "libmythtv/mythframe.h"

#include "Histogram.h"

void Histogram::generateFromImage(MythVideoFrame* frame, unsigned int frameWidth,
         unsigned int frameHeight, unsigned int minScanX, unsigned int maxScanX,
         unsigned int minScanY, unsigned int maxScanY, unsigned int XSpacing,
         unsigned int YSpacing)
{
    m_data.fill(0);
    m_numberOfSamples = 0;

    maxScanX = std::min(maxScanX, frameWidth-1);

    maxScanY = std::min(maxScanY, frameHeight-1);

    unsigned char* framePtr = frame->m_buffer;
    int bytesPerLine = frame->m_pitches[0];
    for(unsigned int y = minScanY; y < maxScanY; y += YSpacing)
    {
        for(unsigned int x = minScanX; x < maxScanX; x += XSpacing)
        {
            m_data[framePtr[(y * bytesPerLine) + x]]++;
            m_numberOfSamples++;
        }
    }
}

unsigned int Histogram::getAverageIntensity(void) const
{
    if (!m_numberOfSamples)
       return 0;

    long value = 0;

    for(long i = 0; i < 256; i++)
    {
        value += m_data[i]*i;
    }

    return value / m_numberOfSamples;
}

unsigned int Histogram::getThresholdForPercentageOfPixels(float percentage)
    const
{
    long value = 0;

    for(int i = 255; i !=0; i--)
    {
        if (value > percentage*m_numberOfSamples)
            return i;

        value += m_data[i];
    }

    return 0;
}

float Histogram::calculateSimilarityWith(const Histogram& other) const
{
    long similar = 0;

    for(unsigned int i = 0; i < 256; i++)
    {
        if (m_data[i] < other.m_data[i])
            similar += m_data[i];
        else
            similar += other.m_data[i];
    }

    //Using c style cast for old gcc compatibility.
    return static_cast<float>(similar) / static_cast<float>(m_numberOfSamples);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
