/* -*- Mode: c++ -*-
 *
 * Copyright (C) Daniel Kristjansson 2008
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#include "audiosettings.h"

AudioSettings::AudioSettings() :
    main_device(QString::null),
    passthru_device(QString::null),
    bits(-1),
    channels(-1),
    samplerate(-1),
    set_initial_vol(false),
    use_passthru(false),
    codec(NULL),
    source(AUDIOOUTPUT_UNKNOWN)
{
}

AudioSettings::AudioSettings(const AudioSettings &other) :
    main_device(other.main_device),
    passthru_device(other.passthru_device),
    bits(other.bits),
    channels(other.channels),
    samplerate(other.samplerate),
    set_initial_vol(other.set_initial_vol),
    use_passthru(other.use_passthru),
    codec(other.codec),
    source(other.source)
{
}

AudioSettings::AudioSettings(
    const QString &audio_main_device,
    const QString &audio_passthru_device,
    int audio_bits,
    int audio_channels,
    int audio_samplerate,
    AudioOutputSource audio_source,
    bool audio_set_initial_vol,
    bool audio_use_passthru,
    void *audio_codec) :
    main_device(audio_main_device),
    passthru_device(audio_passthru_device),
    bits(audio_bits),
    channels(audio_channels),
    samplerate(audio_samplerate),
    set_initial_vol(audio_set_initial_vol),
    use_passthru(audio_use_passthru),
    codec(audio_codec),
    source(audio_source)
{
}

AudioSettings::AudioSettings(
    int   audio_bits, 
    int   audio_channels, 
    int   audio_samplerate,
    bool  audio_use_passthru,
    void *audio_codec) :
    main_device(QString::null),
    passthru_device(QString::null),
    bits(audio_bits),
    channels(audio_channels),
    samplerate(audio_samplerate),
    set_initial_vol(false),
    use_passthru(audio_use_passthru),
    codec(audio_codec),
    source(AUDIOOUTPUT_UNKNOWN)
{
}

void AudioSettings::FixPassThrough(void)
{
    if (passthru_device.isEmpty() || passthru_device.toLower() == "default")
        passthru_device = GetMainDevice();
}

void AudioSettings::TrimDeviceType(void)
{
    main_device.remove(0, 5);
    passthru_device.remove(0, 5);
}

QString AudioSettings::GetMainDevice(void) const
{
    QString ret = main_device;
    ret.detach();
    return ret;
}

QString AudioSettings::GetPassthruDevice(void) const
{
    QString ret = passthru_device;
    ret.detach();
    return ret;
}

