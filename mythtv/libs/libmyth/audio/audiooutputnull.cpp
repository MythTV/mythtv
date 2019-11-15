#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#else
#include "compat.h"
#endif

#include "config.h"

using namespace std;

#include "mythlogging.h"
#include "audiooutputnull.h"

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

AudioOutputNULL::AudioOutputNULL(const AudioSettings &settings) :
    AudioOutputBase(settings)
{
    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
}

AudioOutputNULL::~AudioOutputNULL()
{
    KillAudio();
}

bool AudioOutputNULL::OpenDevice()
{
    LOG(VB_GENERAL, LOG_INFO, "Opening NULL audio device, will fail.");

    m_fragment_size = NULLAUDIO_OUTPUT_BUFFER_SIZE / 2;
    m_soundcard_buffer_size = NULLAUDIO_OUTPUT_BUFFER_SIZE;

    return false;
}

void AudioOutputNULL::CloseDevice()
{
}

AudioOutputSettings* AudioOutputNULL::GetOutputSettings(bool /*digital*/)
{
    // Pretend that we support everything
    AudioOutputSettings *settings = new AudioOutputSettings();

    // NOLINTNEXTLINE(bugprone-infinite-loop)
    while (int rate = settings->GetNextRate())
    {
        settings->AddSupportedRate(rate);
    }

    // NOLINTNEXTLINE(bugprone-infinite-loop)
    while (AudioFormat fmt = settings->GetNextFormat())
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
    if (m_buffer_output_data_for_use)
    {
        if (size + m_current_buffer_size > NULLAUDIO_OUTPUT_BUFFER_SIZE)
        {
            LOG(VB_GENERAL, LOG_ERR, "null audio output should not have just "
                                     "had data written to it");
            return;
        }
        m_pcm_output_buffer_mutex.lock();
        memcpy(m_pcm_output_buffer + m_current_buffer_size, aubuf, size);
        m_current_buffer_size += size;
        m_pcm_output_buffer_mutex.unlock();
    }
}

int AudioOutputNULL::readOutputData(unsigned char *read_buffer, int max_length)
{
    int amount_to_read = max_length;
    if (amount_to_read > m_current_buffer_size)
    {
        amount_to_read = m_current_buffer_size;
    }

    m_pcm_output_buffer_mutex.lock();
    memcpy(read_buffer, m_pcm_output_buffer, amount_to_read);
    memmove(m_pcm_output_buffer, m_pcm_output_buffer + amount_to_read,
            m_current_buffer_size - amount_to_read);
    m_current_buffer_size -= amount_to_read;
    m_pcm_output_buffer_mutex.unlock();

    return amount_to_read;
}

void AudioOutputNULL::Reset()
{
    if (m_buffer_output_data_for_use)
    {
        m_pcm_output_buffer_mutex.lock();
        m_current_buffer_size = 0;
        m_pcm_output_buffer_mutex.unlock();
    }
    AudioOutputBase::Reset();
}

int AudioOutputNULL::GetBufferedOnSoundcard(void) const
{
    if (m_buffer_output_data_for_use)
    {
        return m_current_buffer_size;
    }

    return 0;
}
