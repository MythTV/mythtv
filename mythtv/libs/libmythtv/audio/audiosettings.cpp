/* -*- Mode: c++ -*-
 *
 * Copyright (C) Daniel Kristjansson 2008
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#include "audiosettings.h"

// startup_upmixer
AudioSettings::AudioSettings(const AudioSettings &other) :
    m_mainDevice(other.m_mainDevice),
    m_passthruDevice(other.m_passthruDevice),
    m_format(other.m_format),
    m_channels(other.m_channels),
    m_codec(other.m_codec),
    m_codecProfile(other.m_codecProfile),
    m_sampleRate(other.m_sampleRate),
    m_setInitialVol(other.m_setInitialVol),
    m_usePassthru(other.m_usePassthru),
    m_source(other.m_source),
    m_upmixer(other.m_upmixer),
    m_init(true)
{
    if (other.m_custom)
    {
            // make a copy of it
        m_custom = new AudioOutputSettings;
        *m_custom = *other.m_custom;
    }
    else
    {
        m_custom = nullptr;
    }
}

AudioSettings::AudioSettings(
    QString                     main_device,
    QString                     passthru_device,
    AudioFormat                 format,
    int                         channels,
    AVCodecID                   codec,
    int                         samplerate,
    AudioOutputSource           source,
    bool                        set_initial_vol,
    bool                        use_passthru,
    int                         upmixer_startup,
    const AudioOutputSettings  *custom) :
    m_mainDevice(std::move(main_device)),
    m_passthruDevice(std::move(passthru_device)),
    m_format(format),
    m_channels(channels),
    m_codec(codec),
    m_sampleRate(samplerate),
    m_setInitialVol(set_initial_vol),
    m_usePassthru(use_passthru),
    m_source(source),
    m_upmixer(upmixer_startup),
    m_init(true)
{
    if (custom)
    {
            // make a copy of it
        this->m_custom = new AudioOutputSettings;
        *this->m_custom = *custom;
    }
    else
    {
        this->m_custom = nullptr;
    }
}

AudioSettings::AudioSettings(
    AudioFormat format,
    int         channels,
    AVCodecID   codec,
    int         samplerate,
    bool        use_passthru,
    int         upmixer_startup,
    int         codec_profile) :
    m_format(format),
    m_channels(channels),
    m_codec(codec),
    m_codecProfile(codec_profile),
    m_sampleRate(samplerate),
    m_usePassthru(use_passthru),
    m_upmixer(upmixer_startup),
    m_init(true)
{
}

AudioSettings::~AudioSettings()
{
    delete m_custom;
}

void AudioSettings::FixPassThrough(void)
{
    if (m_passthruDevice.isEmpty())
        m_passthruDevice = "auto";
}

void AudioSettings::TrimDeviceType(void)
{
    m_mainDevice.remove(0, 5);
    if (m_passthruDevice != "auto" && m_passthruDevice.toLower() != "default")
        m_passthruDevice.remove(0, 5);
}
