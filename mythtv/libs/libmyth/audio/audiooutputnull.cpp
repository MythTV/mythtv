#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#else
#include "compat.h"
#endif
#include <cerrno>
#include <cstring>
#include <iostream>

#include "config.h"

using namespace std;

#include "mythlogging.h"
#include "audiooutputnull.h"

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

AudioOutputNULL::AudioOutputNULL(const AudioSettings &settings) :
    AudioOutputBase(settings),
    pcm_output_buffer_mutex(QMutex::NonRecursive),
    current_buffer_size(0)
{
    memset(pcm_output_buffer, 0, sizeof(char) * NULLAUDIO_OUTPUT_BUFFER_SIZE);
    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

AudioOutputNULL::~AudioOutputNULL()
{
    KillAudio();
}

bool AudioOutputNULL::OpenDevice()
{
    LOG(VB_GENERAL, LOG_INFO, "Opening NULL audio device, will fail.");

    fragment_size = NULLAUDIO_OUTPUT_BUFFER_SIZE / 2;
    soundcard_buffer_size = NULLAUDIO_OUTPUT_BUFFER_SIZE;

    return false;
}

void AudioOutputNULL::CloseDevice()
{
}

AudioOutputSettings* AudioOutputNULL::GetOutputSettings(bool /*digital*/)
{
    // Pretend that we support everything
    AudioFormat fmt;
    int rate;
    AudioOutputSettings *settings = new AudioOutputSettings();

    while ((rate = settings->GetNextRate()))
    {
        settings->AddSupportedRate(rate);
    }

    while ((fmt = settings->GetNextFormat()))
    {
        settings->AddSupportedFormat(fmt);
    }

    for (uint channels = CHANNELS_MIN; channels <= CHANNELS_MAX; channels++)
    {
        settings->AddSupportedChannels(channels);
    }

    settings->setPassthrough(-1);   // no passthrough

    return settings;
}


void AudioOutputNULL::WriteAudio(unsigned char* aubuf, int size)
{
    if (buffer_output_data_for_use)
    {
        if (size + current_buffer_size > NULLAUDIO_OUTPUT_BUFFER_SIZE)
        {
            LOG(VB_GENERAL, LOG_ERR, "null audio output should not have just "
                                     "had data written to it");
            return;
        }
        pcm_output_buffer_mutex.lock();
        memcpy(pcm_output_buffer + current_buffer_size, aubuf, size);
        current_buffer_size += size;
        pcm_output_buffer_mutex.unlock();
    }
}

int AudioOutputNULL::readOutputData(unsigned char *read_buffer, int max_length)
{
    int amount_to_read = max_length;
    if (amount_to_read > current_buffer_size)
    {
        amount_to_read = current_buffer_size;
    }

    pcm_output_buffer_mutex.lock();
    memcpy(read_buffer, pcm_output_buffer, amount_to_read);
    memmove(pcm_output_buffer, pcm_output_buffer + amount_to_read,
            current_buffer_size - amount_to_read);
    current_buffer_size -= amount_to_read;
    pcm_output_buffer_mutex.unlock();

    return amount_to_read;
}

void AudioOutputNULL::Reset()
{
    if (buffer_output_data_for_use)
    {
        pcm_output_buffer_mutex.lock();
            current_buffer_size = 0;
        pcm_output_buffer_mutex.unlock();
    }
    AudioOutputBase::Reset();
}

int AudioOutputNULL::GetBufferedOnSoundcard(void) const
{
    if (buffer_output_data_for_use)
    {
        return current_buffer_size;
    }

    return 0;
}
