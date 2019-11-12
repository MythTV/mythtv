/* -*- Mode: c++ -*-
 *
 * Copyright (C) Daniel Kristjansson 2008
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#include "audiosettings.h"

// startup_upmixer
AudioSettings::AudioSettings(const AudioSettings &other) :
    m_main_device(other.m_main_device),
    m_passthru_device(other.m_passthru_device),
    m_format(other.m_format),
    m_channels(other.m_channels),
    m_codec(other.m_codec),
    m_codec_profile(other.m_codec_profile),
    m_samplerate(other.m_samplerate),
    m_set_initial_vol(other.m_set_initial_vol),
    m_use_passthru(other.m_use_passthru),
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
        m_custom = nullptr;
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
    AudioOutputSettings        *custom) :
    m_main_device(std::move(main_device)),
    m_passthru_device(std::move(passthru_device)),
    m_format(format),
    m_channels(channels),
    m_codec(codec),
    m_samplerate(samplerate),
    m_set_initial_vol(set_initial_vol),
    m_use_passthru(use_passthru),
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
        this->m_custom = nullptr;
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
    m_codec_profile(codec_profile),
    m_samplerate(samplerate),
    m_use_passthru(use_passthru),
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
    if (m_passthru_device.isEmpty())
        m_passthru_device = "auto";
}

void AudioSettings::TrimDeviceType(void)
{
    m_main_device.remove(0, 5);
    if (m_passthru_device != "auto" && m_passthru_device.toLower() != "default")
        m_passthru_device.remove(0, 5);
}
