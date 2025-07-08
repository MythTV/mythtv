#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#else
#include "libmythbase/compat.h"
#endif

#include "libmythbase/mythlogging.h"
#include "audiooutputnull.h"

static constexpr uint8_t CHANNELS_MIN { 1 };
static constexpr uint8_t CHANNELS_MAX { 8 };

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

    m_fragmentSize = NULLAUDIO_OUTPUT_BUFFER_SIZE / 2;
    m_soundcardBufferSize = NULLAUDIO_OUTPUT_BUFFER_SIZE;

    return false;
}

void AudioOutputNULL::CloseDevice()
{
}

AudioOutputSettings* AudioOutputNULL::GetOutputSettings(bool /*digital*/)
{
    // Pretend that we support everything
    auto *settings = new AudioOutputSettings();

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
    if (m_bufferOutputDataForUse)
    {
        if (size + m_pcmOutputBuffer.size() > NULLAUDIO_OUTPUT_BUFFER_SIZE)
        {
            LOG(VB_GENERAL, LOG_ERR, "null audio output should not have just "
                                     "had data written to it");
            return;
        }
        m_pcmOutputBufferMutex.lock();
        m_pcmOutputBuffer.insert(m_pcmOutputBuffer.end(), aubuf, aubuf+size);
        m_pcmOutputBufferMutex.unlock();
    }
}

int AudioOutputNULL::readOutputData(unsigned char *read_buffer, size_t max_length)
{
    size_t amount_to_read = std::min(max_length, m_pcmOutputBuffer.size());

    m_pcmOutputBufferMutex.lock();
    std::copy(m_pcmOutputBuffer.cbegin(),
              m_pcmOutputBuffer.cbegin() + amount_to_read,
              read_buffer);
    m_pcmOutputBuffer.erase(m_pcmOutputBuffer.cbegin(),
              m_pcmOutputBuffer.cbegin() + amount_to_read);
    m_pcmOutputBufferMutex.unlock();

    return amount_to_read;
}

void AudioOutputNULL::Reset()
{
    if (m_bufferOutputDataForUse)
    {
        m_pcmOutputBufferMutex.lock();
        m_pcmOutputBuffer.clear();
        m_pcmOutputBufferMutex.unlock();
    }
    AudioOutputBase::Reset();
}

int AudioOutputNULL::GetBufferedOnSoundcard(void) const
{
    if (m_bufferOutputDataForUse)
        return m_pcmOutputBuffer.size();

    return 0;
}
