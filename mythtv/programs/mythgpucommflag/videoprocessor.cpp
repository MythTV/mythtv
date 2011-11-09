#include "videoprocessor.h"

void InitVideoProcessors(void)
{
    InitOpenCLVideoProcessors();
    InitSoftwareVideoProcessors();
}

VideoProcessorList::VideoProcessorList(VideoProcessorInit initList[])
{
    VideoProcessorInit *init;
    for (init = initList; init && init->func; init++)
    {
        VideoProcessor *proc = new VideoProcessor(init->name, init->func);
        append(proc);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
