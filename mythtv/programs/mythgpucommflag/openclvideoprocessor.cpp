#include <QMap>
#include <strings.h>

#include "mythlogging.h"
#include "resultslist.h"
#include "openclinterface.h"
#include "videoprocessor.h"
#include "videopacket.h"

// Prototypes

VideoProcessorList *openCLVideoProcessorList;

VideoProcessorInit openCLVideoProcessorInit[] = {
    { "", NULL }
};

void InitOpenCLVideoProcessors(void)
{
    openCLVideoProcessorList =
        new VideoProcessorList(openCLVideoProcessorInit);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
