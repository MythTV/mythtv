#include "mythlogging.h"
#include "resultslist.h"
#include "openclinterface.h"
#include "audioprocessor.h"

// Prototypes
FlagResults *OpenCLVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                               int count, int64_t pts, int rate);

AudioProcessorList *openCLAudioProcessorList;

AudioProcessorInit openCLAudioProcessorInit[] = {
    { "Volume Level", OpenCLVolumeLevel },
    { "", NULL }
};

void InitOpenCLAudioProcessors(void)
{
    openCLAudioProcessorList =
        new AudioProcessorList(openCLAudioProcessorInit);
}

FlagResults *OpenCLVolumeLevel(OpenCLDevice *dev, int16_t *samples, int size,
                               int count, int64_t pts, int rate)
{
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Volume Level");

    return NULL;
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
