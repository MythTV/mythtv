#include <math.h>

#include "mythlogging.h"
#include "flagresults.h"
#include "audioprocessor.h"

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

// Prototypes
FlagFindings *SoftwareVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                                  int count, int64_t pts, int rate);

AudioProcessorList *softwareAudioProcessorList;

AudioProcessorInit softwareAudioProcessorInit[] = {
    { "Volume Level", SoftwareVolumeLevel },
    { "", NULL }
};

void InitSoftwareAudioProcessors(void)
{
    softwareAudioProcessorList =
        new AudioProcessorList(softwareAudioProcessorInit);
}

#define MAX_ACCUM (((uint64_t)(~1)) - (0xFFFFLL * 0xFFFFLL))
FlagFindings *SoftwareVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                                  int count, int64_t pts, int rate)
{
    static uint64_t accumSRMS = 0;
    static int accumSRMSShift = 0;
    static int64_t accumDC = 0;
    static uint64_t accumSample = 0;

    int16_t maxsample = 0;
    uint64_t accum = 0;
    int accumShift = 0;
    int64_t accumWindowDC = 0;
    int channels = size / count / sizeof(int16_t);
    int sampleCount = count * channels;

#if 0
    LOG(VB_GENERAL, LOG_INFO, "Software Volume Level");
#endif

    // Accumulate this frame's partial squared RMS
    for (int i = 0; i < sampleCount; i++)
    {
        int16_t sample = samples[i];

        // Check accumulator for potential overflow (shouldn't happen)
        if (accum >= MAX_ACCUM)
        {
            accum >>= 2;
            accumShift += 2;
        }
        accumWindowDC += sample;
        accum += (uint64_t)(sample*sample) >> accumShift;
        if (sample < 0)
            sample = -sample;
        maxsample = MAX(maxsample, sample);
    }

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, 
        QString("accum: %1, accumShift: %2, sampleCount: %3")
        .arg(accum) .arg(accumShift) .arg(sampleCount));
#endif

    // Calculate RMS level of this window
    uint16_t windowRMS = sqrt((double)accum * exp2((double)accumShift) /
                              (double)sampleCount);
    if (!windowRMS)
        windowRMS = 1;

    float windowRMSdB = 20.0 * log10((double)windowRMS / 32767.0);
    int64_t windowDC = accumWindowDC / sampleCount;
    LOG(VB_GENERAL, LOG_INFO, QString("Window DC: %1").arg(windowDC));

    // Check overall SRMS for potential overflow
    if (accumSRMS >= MAX_ACCUM)
    {
        accumSRMS >>= 2;
        accumSRMSShift += 2;
    }

    // Normalize the window and overall SRMS to have the same number of shifts
    if (accumShift > accumSRMSShift)
    {
        accumSRMS >>= (accumShift - accumSRMSShift);
        accumSRMSShift = accumShift;
    }
    else if (accumShift < accumSRMSShift)
    {
        accum >>= (accumSRMSShift - accumShift);
        accumShift = accumSRMSShift;
    }

    // Accumulate the overall SRMS
    accumSRMS += accum;
    accumSample += sampleCount;
    accumDC += accumWindowDC;

    int64_t overallDC = accumDC / (int64_t)accumSample;

#if 0
    LOG(VB_GENERAL, LOG_INFO,
        QString("accumDC: %1  accumSample: %2  overallDC: %3")
        .arg(accumDC) .arg(accumSample) .arg(overallDC));
#endif

    maxsample -= overallDC;
    if (maxsample == 0)
        maxsample = 1;

    float DNRdB = 20.0 * log10((double)maxsample / (double) windowRMS);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("accumSRMS: %1, accumSRMSShift: %2, accumSample: %3")
        .arg(accumSRMS) .arg(accumSRMSShift) .arg(accumSample));
#endif

    // Calculate RMS level of the recording so far
    uint16_t overallRMS = sqrt((double)accumSRMS *
                               exp2((double)accumSRMSShift) /
                               (double)accumSample);
    if (!overallRMS)
        overallRMS = 1;

    float overallRMSdB = 20.0 * log10((double)overallRMS / 32767.0);
    float deltaRMSdB = windowRMSdB - overallRMSdB;

#if 1
    LOG(VB_GENERAL, LOG_INFO,
        QString("Window RMS: %1 (%2 dB), Overall RMS: %3 (%4 dB), Delta: %5 dB")
        .arg(windowRMS) .arg(windowRMSdB) .arg(overallRMS) .arg(overallRMSdB)
        .arg(deltaRMSdB));
    LOG(VB_GENERAL, LOG_INFO, QString("Peak: %1  DNR: %2 dB  (DC: %3)")
        .arg(maxsample) .arg(DNRdB) .arg(overallDC));
#endif

    FlagFindings *findings = NULL;

    if (deltaRMSdB >= 6.0)
        findings = new FlagFindings(kFindingAudioHigh, true);
    else if (deltaRMSdB <= -12.0)
        findings = new FlagFindings(kFindingAudioLow, true);

    return findings;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
