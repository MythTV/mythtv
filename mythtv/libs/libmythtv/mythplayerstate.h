#ifndef MYTHPLAYERSTATE_H
#define MYTHPLAYERSTATE_H

// MythTV
#include "mythtvexp.h"
#include "volumebase.h"
#include "videoouttypes.h"

class OSD;
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

class MTV_PUBLIC MythOverlayState
{
  public:
    MythOverlayState() = default;
    MythOverlayState(bool Browsing, bool Editing);

    bool m_browsing { false };
    bool m_editing  { false };
};

#endif
