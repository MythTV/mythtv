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
#include "libavutil/avutil.h"  // to check version of libavformat
}

#define LOC QString("AO: ")

AudioOutputSettings::AudioOutputSettings(bool invalid) :
    m_passthrough(-1), m_AC3(false),  m_DTS(false), m_LPCM(false),
    m_HD(false)      , m_HDLL(false), m_invalid(invalid)
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
    m_AC3           = rhs.m_AC3;
    m_DTS           = rhs.m_DTS;
    m_LPCM          = rhs.m_LPCM;
    m_HD            = rhs.m_HD;
    m_HDLL          = rhs.m_HDLL;
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

    aosettings->m_LPCM = (mchannels > 2);
        // E-AC3 is transferred as sterep PCM at 4 times the rates
        // assume all amplifier supporting E-AC3 also supports 7.1 LPCM
        // as it's required under the bluray standard
#if LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 52, 83, 0 )
    aosettings->m_HD = (mchannels == 8 && BestSupportedRate() == 192000);
    aosettings->m_HDLL = (mchannels == 8 && BestSupportedRate() == 192000);
#endif
    if (mchannels == 2 && m_passthrough >= 0)
    {
        VERBOSE(VB_AUDIO, LOC + QString("may be AC3 or DTS capable"));
        aosettings->AddSupportedChannels(6);
    }
    aosettings->m_DTS = aosettings->m_AC3 = (m_passthrough >= 0);

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
    bool bForceDigital = gCoreContext->GetNumSetting(
        "PassThruDeviceOverride", false);
    bool bAC3  = (aosettings->m_AC3 || bForceDigital) &&
        gCoreContext->GetNumSetting("AC3PassThru", false);
    bool bDTS  = (aosettings->m_DTS || bForceDigital) && 
        gCoreContext->GetNumSetting("DTSPassThru", false);
    bool bLPCM = aosettings->m_LPCM &&
        !gCoreContext->GetNumSetting("StereoPCM", false);
    bool bHD = bLPCM && aosettings->m_HD &&
        gCoreContext->GetNumSetting("EAC3PassThru", false) &&
        !gCoreContext->GetNumSetting("Audio48kOverride", false);
    bool bHDLL = bLPCM && aosettings->m_HD &&
        gCoreContext->GetNumSetting("TrueHDPassThru", false) &&
        !gCoreContext->GetNumSetting("Audio48kOverride", false);

    if (max_channels > 2 && !bLPCM)
        max_channels = 2;
    if (max_channels == 2 && (bAC3 || bDTS))
        max_channels = 6;

    if (cur_channels > max_channels)
        cur_channels = max_channels;

    aosettings->SetBestSupportedChannels(cur_channels);
    aosettings->m_AC3   = bAC3;
    aosettings->m_DTS   = bDTS;
    aosettings->m_HD    = bHD;
    aosettings->m_HDLL  = bHDLL;
        
    aosettings->m_LPCM = bLPCM;

    return aosettings;
}
