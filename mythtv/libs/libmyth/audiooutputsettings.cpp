/* -*- Mode: c++ -*-
 *
 * Copyright (C) 2010 foobum@gmail.com and jyavenard@gmail.com
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#include <algorithm>
#include <vector>

using namespace std;

#include "audiooutputsettings.h"
#include "mythverbose.h"
#include "mythcorecontext.h"

extern "C" {
#include "libavutil/avutil.h"    // to check version of libavformat
}

#define LOC QString("AO: ")

AudioOutputSettings::AudioOutputSettings(bool invalid) :
    m_passthrough(-1),  m_features(FEATURE_NONE),
    m_invalid(invalid), m_has_eld(false)
{
    m_sr.assign(srs,  srs  +
                sizeof(srs)  / sizeof(int));
    m_sf.assign(fmts, fmts +
                sizeof(fmts) / sizeof(AudioFormat));
    m_sr_it = m_sr.begin();
    m_sf_it = m_sf.begin();
}

AudioOutputSettings::~AudioOutputSettings()
{
    m_sr.clear();
    m_rates.clear();
    m_sf.clear();
    m_formats.clear();
    m_channels.clear();
}

AudioOutputSettings& AudioOutputSettings::operator=(
    const AudioOutputSettings &rhs)
{
    if (this == &rhs)
        return *this;
    m_sr            = rhs.m_sr;
    m_rates         = rhs.m_rates;
    m_sf            = rhs.m_sf;
    m_formats       = rhs.m_formats;
    m_channels      = rhs.m_channels;
    m_passthrough   = rhs.m_passthrough;
    m_features      = rhs.m_features;
    m_has_eld       = rhs.m_has_eld;
    m_invalid       = rhs.m_invalid;
    m_sr_it         = m_sr.begin() + (rhs.m_sr_it - rhs.m_sr.begin());
    m_sf_it         = m_sf.begin() + (rhs.m_sf_it - rhs.m_sf.begin());
    return *this;
}

int AudioOutputSettings::GetNextRate()
{
    if (m_sr_it == m_sr.end())
    {
        m_sr_it = m_sr.begin();
        return 0;
    }

    return *m_sr_it++;
}

void AudioOutputSettings::AddSupportedRate(int rate)
{
    m_rates.push_back(rate);
    VERBOSE(VB_AUDIO, LOC + QString("Sample rate %1 is supported").arg(rate));
}

bool AudioOutputSettings::IsSupportedRate(int rate)
{
    if (m_rates.empty() && rate == 48000)
        return true;

    vector<int>::iterator it;

    for (it = m_rates.begin(); it != m_rates.end(); it++)
        if (*it == rate)
            return true;

    return false;
}

int AudioOutputSettings::BestSupportedRate()
{
    if (m_rates.empty())
        return 48000;
    return m_rates.back();
}

int AudioOutputSettings::NearestSupportedRate(int rate)
{
    if (m_rates.empty())
        return 48000;

    vector<int>::iterator it;

    // Assume rates vector is sorted
    for (it = m_rates.begin(); it != m_rates.end(); it++)
    {
        if (*it >= rate)
            return *it;
    }
    // Not found, so return highest available rate
    return m_rates.back();
}

AudioFormat AudioOutputSettings::GetNextFormat()
{
    if (m_sf_it == m_sf.end())
    {
        m_sf_it = m_sf.begin();
        return FORMAT_NONE;
    }

    return *m_sf_it++;
}

void AudioOutputSettings::AddSupportedFormat(AudioFormat format)
{
    m_formats.push_back(format);
}

bool AudioOutputSettings::IsSupportedFormat(AudioFormat format)
{
    if (m_formats.empty() && format == FORMAT_S16)
        return true;

    vector<AudioFormat>::iterator it;

    for (it = m_formats.begin(); it != m_formats.end(); it++)
        if (*it == format)
            return true;

    return false;
}

AudioFormat AudioOutputSettings::BestSupportedFormat()
{
    if (m_formats.empty())
        return FORMAT_S16;
    return m_formats.back();
}

int AudioOutputSettings::FormatToBits(AudioFormat format)
{
    switch (format)
    {
        case FORMAT_U8:     return 8;
        case FORMAT_S16:    return 16;
        case FORMAT_S24LSB:
        case FORMAT_S24:    return 24;
        case FORMAT_S32:
        case FORMAT_FLT:    return 32;
    }

    return -1;
}

const char* AudioOutputSettings::FormatToString(AudioFormat format)
{
    switch (format)
    {
        case FORMAT_U8:     return "unsigned 8 bit";
        case FORMAT_S16:    return "signed 16 bit";
        case FORMAT_S24:    return "signed 24 bit MSB";
        case FORMAT_S24LSB: return "signed 24 bit LSB";
        case FORMAT_S32:    return "signed 32 bit";
        case FORMAT_FLT:    return "32 bit floating point";
        default:            return "unknown";
    }
}

int AudioOutputSettings::SampleSize(AudioFormat format)
{
    switch (format)
    {
        case FORMAT_U8:     return 1;
        case FORMAT_S16:    return 2;
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
        case FORMAT_FLT:    return 4;
    }

    return 0;
}

void AudioOutputSettings::AddSupportedChannels(int channels)
{
    m_channels.push_back(channels);
    VERBOSE(VB_AUDIO, LOC + QString("%1 channel(s) are supported")
            .arg(channels));
}

bool AudioOutputSettings::IsSupportedChannels(int channels)
{
    if (m_channels.empty() && channels == 2)
        return true;

    vector<int>::iterator it;

    for (it = m_channels.begin(); it != m_channels.end(); it++)
        if (*it == channels)
            return true;

    return false;
}

int AudioOutputSettings::BestSupportedChannels()
{
    if (m_channels.empty())
        return 2;
    return m_channels.back();
}

void AudioOutputSettings::SortSupportedChannels()
{
    if (m_channels.empty())
        return;
    sort(m_channels.begin(), m_channels.end());
}

void AudioOutputSettings::SetBestSupportedChannels(int channels)
{
    if (m_channels.empty())
    {
        m_channels.push_back(channels);
        return;
    }

    vector<int>::reverse_iterator it;

    for (it = m_channels.rbegin();
         it != m_channels.rend() && *it >= channels;
         it++)
        m_channels.pop_back();
    m_channels.push_back(channels);
}

void AudioOutputSettings::setFeature(bool val, DigitalFeature arg)
{
    if (val)
        m_features |= arg;
    else
        m_features &= ~arg;
};

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3 and DTS)
 * Warning: do not call it twice in a row, will lead to invalid settings
 */
AudioOutputSettings* AudioOutputSettings::GetCleaned(bool newcopy)
{
    AudioOutputSettings* aosettings;

    if (newcopy)
    {
        aosettings = new AudioOutputSettings;
        *aosettings = *this;
    }
    else
        aosettings = this;

    if (m_invalid)
        return aosettings;

    int mchannels = BestSupportedChannels();

    if (mchannels > 2)
    {
        aosettings->setFeature(FEATURE_LPCM);
    }

    if (IsSupportedFormat(FORMAT_S16))
    {
        // E-AC3 is transferred as stereo PCM at 4 times the rates
        // assume all amplifier supporting E-AC3 also supports 7.1 LPCM
        // as it's mandatory under the bluray standard
//#if LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 52, 83, 0 )
        if (IsSupportedChannels(8) && IsSupportedRate(192000))
            aosettings->setFeature(FEATURE_TRUEHD | FEATURE_DTSHD | FEATURE_EAC3);
//#endif
        if (m_passthrough >= 0)
        {
            if (mchannels == 2)
            {
                VERBOSE(VB_AUDIO, LOC + QString("may be AC3 or DTS capable"));
                aosettings->AddSupportedChannels(6);
            }
            aosettings->setFeature(FEATURE_AC3 | FEATURE_DTS);
        }
    }

    return aosettings;
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3 and DTS) as well as the user settings
 * If newcopy = false, assume GetCleaned was called before hand
 */
AudioOutputSettings* AudioOutputSettings::GetUsers(bool newcopy)
{
    AudioOutputSettings* aosettings;

    if (newcopy)
        aosettings = GetCleaned(newcopy);
    else
        aosettings = this;

    if (aosettings->m_invalid)
        return aosettings;

    int cur_channels = gCoreContext->GetNumSetting("MaxChannels", 2);
    int max_channels = aosettings->BestSupportedChannels();

    bool bAC3  = aosettings->canFeature(FEATURE_AC3) &&
        gCoreContext->GetNumSetting("AC3PassThru", false);

    bool bDTS  = aosettings->canFeature(FEATURE_DTS) && 
        gCoreContext->GetNumSetting("DTSPassThru", false);

    bool bLPCM = aosettings->canFeature(FEATURE_LPCM) &&
        !gCoreContext->GetNumSetting("StereoPCM", false);

    bool bEAC3 = aosettings->canFeature(FEATURE_EAC3) &&
        gCoreContext->GetNumSetting("EAC3PassThru", false) &&
        !gCoreContext->GetNumSetting("Audio48kOverride", false);

        // TrueHD requires HBR support.
    bool bTRUEHD = aosettings->canFeature(FEATURE_TRUEHD) &&
        gCoreContext->GetNumSetting("TrueHDPassThru", false) &&
        !gCoreContext->GetNumSetting("Audio48kOverride", false) &&
        gCoreContext->GetNumSetting("HBRPassthru", true);

    bool bDTSHD = aosettings->canFeature(FEATURE_DTSHD) &&
        gCoreContext->GetNumSetting("DTSHDPassThru", false) &&
        !gCoreContext->GetNumSetting("Audio48kOverride", false);

    if (max_channels > 2 && !bLPCM)
        max_channels = 2;
    if (max_channels == 2 && (bAC3 || bDTS))
        max_channels = 6;

    if (cur_channels > max_channels)
        cur_channels = max_channels;

    aosettings->SetBestSupportedChannels(cur_channels);
    aosettings->setFeature(bAC3, FEATURE_AC3);
    aosettings->setFeature(bDTS, FEATURE_DTS);
    aosettings->setFeature(bLPCM, FEATURE_LPCM);
    aosettings->setFeature(bEAC3, FEATURE_EAC3);
    aosettings->setFeature(bTRUEHD, FEATURE_TRUEHD);
    aosettings->setFeature(bDTSHD, FEATURE_DTSHD);

    return aosettings;
}

int AudioOutputSettings::GetMaxBitrate(int codec)
{
    if (codec == CODEC_ID_DTS)
    {
        if (!canFeature(FEATURE_DTSHD))
        {   // only supports DTS core
            return 48000 * 16 * 2;   // DTS Core: 48kHz, 16 bits, 2 ch
        }
            // If no HBR or no LPCM, limit bitrate to 6.144Mbit/s
        if (!gCoreContext->GetNumSetting("HBRPassthru", true) ||
            !canFeature(FEATURE_LPCM))
        {
            return 192000 * 16 * 2;  // E-AC3/DTS-HD High Res: 192k, 16 bits, 2 ch
        }
        return 192000 * 16 * 8;      // TrueHD or DTS-HD MA: 192k, 16 bits, 8 ch
    }
    return 48000 * 16 * 2;
}

#define ARG(x) ((tmp.isEmpty() ? "" : ",") + QString(x))
    
QString AudioOutputSettings::FeaturesToString(DigitalFeature arg)
{
    QString tmp;
    DigitalFeature feature[] = {
        FEATURE_AC3,
        FEATURE_DTS,
        FEATURE_LPCM,
        FEATURE_EAC3,
        FEATURE_TRUEHD,
        FEATURE_DTSHD,
        FEATURE_AAC,
        (DigitalFeature)-1
    };
    const char *feature_str[] = {
        "AC3",
        "DTS",
        "LPCM",
        "EAC3",
        "TRUEHD",
        "DTSHD",
        "AAC",
        NULL
    };
    
    for (unsigned int i = 0; feature[i] != (DigitalFeature)-1; i++)
    {
        if (arg & feature[i])
            tmp += ARG(feature_str[i]);
    }
    return tmp;
}
