#ifndef __VOLUME_BASE__
#define __VOLUME_BASE__

#include "mythexp.h"

enum MuteState {
    kMuteOff = 0,
    kMuteLeft,
    kMuteRight,
    kMuteAll,
};

class MPUBLIC VolumeBase
{
  public:
    VolumeBase();
    virtual ~VolumeBase() = default;

    void SWVolume(bool set);
    bool SWVolume(void) const;
    virtual uint GetCurrentVolume(void) const;
    virtual void SetCurrentVolume(int value);
    virtual void AdjustCurrentVolume(int change);
    virtual void ToggleMute(void);

    virtual MuteState GetMuteState(void) const;
    virtual MuteState SetMuteState(MuteState /*mstate*/);

    static MuteState NextMuteState(MuteState /*cur*/);

  protected:

    virtual int GetVolumeChannel(int channel) const = 0; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume) = 0; // range 0-100 for vol
    virtual void SetSWVolume(int new_volume, bool save) = 0;
    virtual int GetSWVolume(void) = 0;

    void UpdateVolume(void);
    void SyncVolume(void);
    void SetChannels(int new_channels);
    bool      m_internalVol        {false};

 private:

    int       m_volume             {80};
    MuteState m_currentMuteState   {kMuteOff};
    bool      m_swvol              {false};
    bool      m_swvolSetting       {false};
    int       m_channels           {0};

};

#endif // __VOLUME_BASE__
