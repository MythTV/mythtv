#ifndef MYTHPLAYERSTATE_H
#define MYTHPLAYERSTATE_H

// MythTV
#include "mythtvexp.h"
#include "volumebase.h"
#include "videoouttypes.h"

class AudioPlayer;

class MTV_PUBLIC MythAudioState
{
  public:
    MythAudioState() = default;
    MythAudioState(AudioPlayer* Player, int64_t Offset);

    bool m_hasAudioOut    { true  };
    bool m_volumeControl  { true  };
    uint m_volume         { 0     };
    MuteState m_muteState { kMuteOff };
    bool m_canUpmix       { false };
    bool m_isUpmixing     { false };
    bool m_paused         { false };
    int64_t m_audioOffset { 0     };
};

#endif
