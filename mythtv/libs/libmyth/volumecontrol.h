#ifndef VOLUMECONTROL_H_
#define VOLUMECONTROL_H_

typedef enum { MUTE_OFF=0, MUTE_LEFT, MUTE_RIGHT, MUTE_BOTH } kMuteState;

class VolumeControl
{
  public:
    VolumeControl(bool setstartingvolume = true);
   ~VolumeControl();

    int GetCurrentVolume(void);
    void SetCurrentVolume(int value);
    void AdjustCurrentVolume(int change);

    void SetMute(bool on);
    void ToggleMute(void);
    bool GetMute(void) { return mute; }
    kMuteState IterateMutedChannels(void);

  private:
    int mixerfd;
    int volume;
    int internal_volume;
    int control;

    bool mute;
    kMuteState current_mute_state;
};

#endif
