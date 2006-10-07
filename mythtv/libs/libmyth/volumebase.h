#ifndef VOLUMEBASE
#define VOLUMEBASE

#include <iostream>
using namespace std;

#include <qstring.h>
#include "mythcontext.h"

typedef enum {
    MUTE_OFF = 0,
    MUTE_LEFT,
    MUTE_RIGHT,
    MUTE_BOTH
} kMuteState;

class MPUBLIC VolumeBase
{
 public:
    VolumeBase();    
    virtual ~VolumeBase() {};

    virtual int GetCurrentVolume(void);
    virtual void SetCurrentVolume(int value);
    virtual void AdjustCurrentVolume(int change);
    virtual void SetMute(bool on);
    virtual void ToggleMute(void);
    virtual kMuteState GetMute(void);
    virtual kMuteState IterateMutedChannels(void);

 protected:

    virtual int GetVolumeChannel(int channel) = 0; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume) = 0; // range 0-100 for vol

    void UpdateVolume(void);
    void SyncVolume(void);

    bool internal_vol;

 private:
    
    int volume;
    kMuteState current_mute_state;

};

#endif

