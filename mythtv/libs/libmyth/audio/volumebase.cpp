#include <cstdio>
#include <cstdlib>

#include <algorithm>
using namespace std;

#include <QString>

#include "volumebase.h"
#include "mythcorecontext.h"

VolumeBase::VolumeBase() :
    volume(80), current_mute_state(kMuteOff), channels(0)
{
    internal_vol = gCoreContext->GetNumSetting("MythControlsVolume", 1);
    swvol = swvol_setting =
        (gCoreContext->GetSetting("MixerDevice", "default").toLower() == "software");
}

bool VolumeBase::SWVolume(void) const
{
    return swvol;
}

void VolumeBase::SWVolume(bool set)
{
    if (swvol_setting)
        return;
    swvol = set;
}

uint VolumeBase::GetCurrentVolume(void) const
{
    return volume;
}

void VolumeBase::SetCurrentVolume(int value)
{
    volume = max(min(value, 100), 0);
    UpdateVolume();

    QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");
    controlLabel += "MixerVolume";
    gCoreContext->SaveSetting(controlLabel, volume);    
}

void VolumeBase::AdjustCurrentVolume(int change)
{
    SetCurrentVolume(volume + change);
}

MuteState VolumeBase::SetMuteState(MuteState mstate)
{
    current_mute_state = mstate;
    UpdateVolume();
    return current_mute_state;
}

void VolumeBase::ToggleMute(void)
{
    bool is_muted = GetMuteState() == kMuteAll;
    SetMuteState((is_muted) ? kMuteOff : kMuteAll);
}

MuteState VolumeBase::GetMuteState(void) const
{
    return current_mute_state;
}

MuteState VolumeBase::NextMuteState(MuteState cur)
{
    MuteState next = cur;

    switch (cur)
    {
       case kMuteOff:
           next = kMuteLeft;
           break;
       case kMuteLeft:
           next = kMuteRight;
           break;
       case kMuteRight:
           next = kMuteAll;
           break;
       case kMuteAll:
           next = kMuteOff;
           break;
    }

    return (next);
}

void VolumeBase::UpdateVolume(void)
{
    int new_volume = volume;
    bool save = true;
    if (current_mute_state == kMuteAll)
    {
        new_volume = 0;
        save = false;
    }

    if (swvol)
    {
        SetSWVolume(new_volume, save);
        return;
    }
    
    for (int i = 0; i < channels; i++)
    {
        SetVolumeChannel(i, new_volume);
    }
    
    // Individual channel muting is handled in GetAudioData,
    // this code demonstrates the old method.
    // if (current_mute_state == kMuteLeft)
    // {
    //     SetVolumeChannel(0, 0);
    // }
    // else if (current_mute_state == kMuteRight)
    // {
    //     SetVolumeChannel(1, 0);
    // }
}

void VolumeBase::SyncVolume(void)
{
    // Read the volume from the audio driver and setup our internal state to match
    if (swvol)
        volume = GetSWVolume();
    else
        volume = GetVolumeChannel(0);
}

void VolumeBase::SetChannels(int new_channels)
{
    channels = new_channels;
}
