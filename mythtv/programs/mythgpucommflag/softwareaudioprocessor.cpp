#include "mythlogging.h"
#include "resultslist.h"
#include "audioprocessor.h"

// Prototypes
FlagResults *SoftwareVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                                 int count, int64_t pts);

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

FlagResults *SoftwareVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                                 int count, int64_t pts)
{
    LOG(VB_GENERAL, LOG_INFO, "Software Volume Level");

    return NULL;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
