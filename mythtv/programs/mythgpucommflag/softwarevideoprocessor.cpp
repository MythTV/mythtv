#include <math.h>

#include "mythlogging.h"
#include "resultslist.h"
#include "videoprocessor.h"

// Prototypes

VideoProcessorList *softwareVideoProcessorList;

VideoProcessorInit softwareVideoProcessorInit[] = {
    { "", NULL }
};

void InitSoftwareVideoProcessors(void)
{
    softwareVideoProcessorList =
        new VideoProcessorList(softwareVideoProcessorInit);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
