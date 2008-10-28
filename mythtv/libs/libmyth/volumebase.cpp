#include <qstring.h>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
using namespace std;
#include "volumebase.h"

VolumeBase::VolumeBase() :
    internal_vol(false), volume(80), 
    current_mute_state(kMuteOff)
{
}

uint VolumeBase::GetCurrentVolume(void) const
{
    return volume;
}

void VolumeBase::SetCurrentVolume(int value)
{
    volume = max(min(value, 100), 0);
    UpdateVolume();

    QString controlLabel = gContext->GetSetting("MixerControl", "PCM");
    controlLabel += "MixerVolume";
    gContext->SaveSetting(controlLabel, volume);    
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
    if (current_mute_state == kMuteAll)
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
    volume = GetVolumeChannel(0);
}

