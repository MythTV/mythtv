#include "volumecontrol.h"

#ifndef _WIN32
#include <linux/soundcard.h>
#endif

#include <sys/ioctl.h>
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "mythcontext.h"

VolumeControl::VolumeControl(bool setstartingvolume)
{
    mixerfd = 0;
    volume = 0;

#ifndef _WIN32
    mute = false;
  
    QString device = gContext->GetSetting("MixerDevice", "/dev/mixer");
    mixerfd = open(device.ascii(), O_RDONLY);

    QString controlLabel = gContext->GetSetting("MixerControl", "PCM");

    if (controlLabel == "Master")
    {
        control = SOUND_MIXER_VOLUME;
    }
    else
    {
        control = SOUND_MIXER_PCM;
    }

    if (mixerfd < 0)
    {
        cerr << "Unable to open mixer: '" << device << "'\n";
        return;
    }

    int realvol;

    if (setstartingvolume)
    {
        volume = gContext->GetNumSetting("MasterMixerVolume", 80);
        realvol = (volume << 8) + volume;
        int ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_VOLUME), &realvol);
        if (ret < 0)
            perror("Setting master volume: ");

        volume = gContext->GetNumSetting("PCMMixerVolume", 80);
        realvol = (volume << 8) + volume;
        ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_PCM), &realvol);
        if (ret < 0)
            perror("Setting PCM volume: ");
    }

    internal_volume = GetCurrentVolume();
#endif
}

VolumeControl::~VolumeControl()
{
    if (mixerfd > 0)
        close(mixerfd);
}

int VolumeControl::GetCurrentVolume(void)
{
#ifndef _WIN32
    int realvol;
    
    if(mute)
    {
        return internal_volume;
    }
    else
    {
        int ret = ioctl(mixerfd, MIXER_READ(control), &realvol);
        if (ret < 0)
        {
            perror("Reading PCM volume: ");
        }
        volume = realvol & 0xff; // just use the left channel
        internal_volume = volume;
    }
#endif    

    return volume;
}

void VolumeControl::SetCurrentVolume(int value)
{
#ifndef _WIN32
    volume = value;

    if (volume > 100)
        volume = 100;
    if (volume < 0)
        volume = 0;

    internal_volume = volume;
    if (mixerfd > 0)
    {
        if(!mute)
        {
            int realvol = (volume << 8) + volume;
            int ret = ioctl(mixerfd, MIXER_WRITE(control), &realvol);
            if (ret < 0)
                perror("Setting volume: ");
        }
    }

    //mute = false;

    QString controlLabel = gContext->GetSetting("MixerControl", "PCM");
    controlLabel += "MixerVolume";
    gContext->SaveSetting(controlLabel, volume);
#endif
}

void VolumeControl::AdjustCurrentVolume(int change)
{
    int newvol = GetCurrentVolume() + change;

    SetCurrentVolume(newvol);
}

void VolumeControl::SetMute(bool on)
{
#ifndef _WIN32
    int realvol;

    if (on)
    {
        realvol = 0;
    }
    else
    {
        realvol = (internal_volume << 8) + internal_volume;
    }
    if (mixerfd > 0)
    {
        int ret = ioctl(mixerfd, MIXER_WRITE(control), &realvol);
        if (ret < 0)
            perror("Setting mute:");
    }

    mute = on;
#endif
}

void VolumeControl::ToggleMute(void)
{
    SetMute(!mute);
}

