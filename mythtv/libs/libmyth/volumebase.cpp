#include <qstring.h>
#include <cstdio>
#include <cstdlib>

using namespace std;
#include "volumebase.h"

VolumeBase::VolumeBase() 
{
    volume = 80;
    current_mute_state=MUTE_OFF;
    internal_vol = false;
};

int VolumeBase::GetCurrentVolume(void)
{
    return volume;
}

void VolumeBase::SetCurrentVolume(int value)
{
    volume = value;
    if (volume < 0)
    {
        volume = 0;
    }
    else if (volume > 100)
    {
        volume = 100;
    }
    UpdateVolume();

    QString controlLabel = gContext->GetSetting("MixerControl", "PCM");
    controlLabel += "MixerVolume";
    gContext->SaveSetting(controlLabel, volume);    
}

void VolumeBase::AdjustCurrentVolume(int change)
{
    SetCurrentVolume(volume + change);
}

void VolumeBase::SetMute(bool on)
{
    if (on)
    {
        current_mute_state = MUTE_BOTH;
    }
    else
    {
        current_mute_state = MUTE_OFF;
    }
    UpdateVolume();
}

void VolumeBase::ToggleMute(void)
{
    SetMute(current_mute_state == MUTE_OFF);
}

kMuteState VolumeBase::GetMute(void)
{
    return current_mute_state;
}

kMuteState VolumeBase::IterateMutedChannels(void)
{
    // mute states are checked in GetAudioData
    switch (current_mute_state)
    {
       case MUTE_OFF:
           current_mute_state = MUTE_LEFT;
           break;
       case MUTE_LEFT:
           current_mute_state = MUTE_RIGHT;
           break;
       case MUTE_RIGHT:
           current_mute_state = MUTE_BOTH;
           break;
       case MUTE_BOTH:
           current_mute_state = MUTE_OFF;
           break;
    }
    UpdateVolume();
    return (current_mute_state);
}

void VolumeBase::UpdateVolume(void)
{
    int new_volume = volume;
    if (current_mute_state == MUTE_BOTH)
    {
        new_volume = 0;
    }
    
    // TODO: Avoid assumption that there are 2 channels!
    for (int i = 0; i < 2; i++)
    {
        SetVolumeChannel(i, new_volume);
    }
    
    // Individual channel muting is handled in GetAudioData,
    // this code demonstrates the old method.
    // if (current_mute_state == MUTE_LEFT)
    // {
    //     SetVolumeChannel(0, 0);
    // }
    // else if (current_mute_state == MUTE_RIGHT)
    // {
    //     SetVolumeChannel(1, 0);
    // }
}

void VolumeBase::SyncVolume(void)
{
    // Read the volume from the audio driver and setup our internal state to match
    volume = GetVolumeChannel(0);
}

