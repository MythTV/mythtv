#include "mythlogging.h"
#include "flagresults.h"
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


FlagFindings *CommonChannelCount(OpenCLDevice *dev, int16_t *samples, int size,
                                 int count, int64_t pts, int rate)
{
    LOG(VB_GPUAUDIO, LOG_INFO, "Audio Channel Count");

    static int prevChannels = -1;
    FlagFindings *findings = NULL;

    int channels = size / count / sizeof(int16_t);
    if (prevChannels != channels && prevChannels != -1)
    {
        findings = new FlagFindings(kFindingAudioChannelCount, channels);
    }

    prevChannels = channels;

    LOG(VB_GPUAUDIO, LOG_INFO, "Done Audio Channel Count");
    return findings;
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
