#ifndef VOLUMECONTROL_H_
#define VOLUMECONTROL_H_

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

  private:
    int mixerfd;
    int volume;

    int control;

    bool mute;
};

#endif
