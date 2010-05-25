/* -*- Mode: c++ -*-
 *
 * Copyright (C) foobum@gmail.com 2010
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

using namespace std;

#include "audiooutputsettings.h"
#include "mythverbose.h"

#define LOC QString("AO: ")

AudioOutputSettings::AudioOutputSettings()
{
    sr.assign(srs,  srs  + sizeof(srs)  / sizeof(int));
    sf.assign(fmts, fmts + sizeof(fmts) / sizeof(AudioFormat));
    sr_it = sr.begin();
    sf_it = sf.begin();
}

AudioOutputSettings::~AudioOutputSettings()
{
    sr.clear();
    rates.clear();
    sf.clear();
    formats.clear();
    channels.clear();
}

int AudioOutputSettings::GetNextRate()
{
    if (sr_it == sr.end())
    {
        sr_it = sr.begin();
        return 0;
    }

    return *sr_it++;
}

void AudioOutputSettings::AddSupportedRate(int rate)
{
    rates.push_back(rate);
    VERBOSE(VB_AUDIO, LOC + QString("Sample rate %1 is supported").arg(rate));
}

bool AudioOutputSettings::IsSupportedRate(int rate)
{
    if (rates.empty() && rate == 48000)
        return true;

    vector<int>::iterator it;

    for (it = rates.begin(); it < rates.end(); it++)
        if (*it == rate)
            return true;

    return false;
}

int AudioOutputSettings::BestSupportedRate()
{
    if (rates.empty())
        return 48000;
    return rates.back();
}

int AudioOutputSettings::NearestSupportedRate(int rate)
{
    if (rates.empty())
        return 48000;

    vector<int>::iterator it;

    // Assume rates vector is sorted
    for (it = rates.begin(); it != rates.end(); it++)
    {
        if (*it >= rate)
            return *it;
    }
    // Not found, so return highest available rate
    return rates.back();
}

AudioFormat AudioOutputSettings::GetNextFormat()
{
    if (sf_it == sf.end())
    {
        sf_it = sf.begin();
        return FORMAT_NONE;
    }

    return *sf_it++;
}

void AudioOutputSettings::AddSupportedFormat(AudioFormat format)
{
    formats.push_back(format);
}

bool AudioOutputSettings::IsSupportedFormat(AudioFormat format)
{
    if (formats.empty() && format == FORMAT_S16)
        return true;

    vector<AudioFormat>::iterator it;

    for (it = formats.begin(); it < formats.end(); it++)
        if (*it == format)
            return true;

    return false;
}

AudioFormat AudioOutputSettings::BestSupportedFormat()
{
    if (formats.empty())
        return FORMAT_S16;
    return formats.back();
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
    this->channels.push_back(channels);
}

bool AudioOutputSettings::IsSupportedChannels(int channels)
{
    if (this->channels.empty() && channels == 2)
        return true;

    vector<int>::iterator it;

    for (it = this->channels.begin(); it < this->channels.end(); it++)
        if (*it == channels)
            return true;

    return false;
}

int AudioOutputSettings::BestSupportedChannels()
{
    if (channels.empty())
        return 2;
    return channels.back();
}
