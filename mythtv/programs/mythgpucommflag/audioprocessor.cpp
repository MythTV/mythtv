#include "audioprocessor.h"

void InitAudioProcessors(void)
{
    InitOpenCLAudioProcessors();
    InitSoftwareAudioProcessors();
}

AudioProcessorList::AudioProcessorList(AudioProcessorInit initList[])
{
    AudioProcessorInit *init;
    for (init = initList; init && init->func; init++)
    {
        AudioProcessor *proc = new AudioProcessor(init->name, init->func);
        append(proc);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
