#ifndef __VOLUME_BASE__
#define __VOLUME_BASE__

#include "mythexp.h"

typedef enum {
    kMuteOff = 0,
    kMuteLeft,
    kMuteRight,
    kMuteAll,
} MuteState;

class MPUBLIC VolumeBase
{
  public:
    VolumeBase();
    virtual ~VolumeBase() {};

    void SWVolume(bool set);
    bool SWVolume(void) const;
    virtual uint GetCurrentVolume(void) const;
    virtual void SetCurrentVolume(int value);
    virtual void SaveCurrentVolume(void);
    virtual void AdjustCurrentVolume(int change);
    virtual void ToggleMute(void);

    virtual MuteState GetMuteState(void) const;
    virtual MuteState SetMuteState(MuteState);

    static MuteState NextMuteState(MuteState);

  protected:

    virtual int GetVolumeChannel(int channel) const = 0; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume) = 0; // range 0-100 for vol
    virtual void SetSWVolume(int new_volume, bool save) = 0;
    virtual int GetSWVolume(void) = 0;

    void UpdateVolume(void);
    void SyncVolume(void);
    void SetChannels(int new_channels);
    bool internal_vol;

 private:

    int volume;
    MuteState current_mute_state;
    bool swvol;
    bool swvol_setting;
    int channels;

};

#endif // __VOLUME_BASE__
