#include "volumecontrol.h"

#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "mythcontext.h"

VolumeControl::VolumeControl(bool setstartingvolume)
{
    mute = false;
    volume = 0;
  
    QString device = gContext->GetSetting("MixerDevice", "/dev/mixer");
    mixerfd = open(device.ascii(), O_RDONLY);

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
    else
    {
        int realvol;
        int ret = ioctl(mixerfd, MIXER_READ(SOUND_MIXER_PCM), &realvol);
        if (ret < 0)
            perror("Reading PCM volume: ");

        volume = realvol & 0xff; // just use the left channel
    }
}

VolumeControl::~VolumeControl()
{
    if (mixerfd > 0)
        close(mixerfd);
}

int VolumeControl::GetCurrentVolume(void)
{
    return volume;
}

void VolumeControl::SetCurrentVolume(int value)
{
    volume = value;

    if (volume > 100)
        volume = 100;
    if (volume < 0)
        volume = 0;

    if (mixerfd > 0)
    {
        int realvol = (volume << 8) + volume;
        int ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_PCM), &realvol);
        if (ret < 0)
            perror("Setting PCM volume: ");
    }

    mute = false;
}

void VolumeControl::AdjustCurrentVolume(int change)
{
    int newvol = volume + change;

    SetCurrentVolume(newvol);
}

void VolumeControl::SetMute(bool on)
{
    int realvol;

    if (on)
        realvol = 0;
    else
        realvol = (volume << 8) + volume;

    int ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_PCM), &realvol);
    if (ret < 0)
        perror("Setting PCM volume: ");

    mute = on;
}

void VolumeControl::ToggleMute(void)
{
    SetMute(!mute);
}

