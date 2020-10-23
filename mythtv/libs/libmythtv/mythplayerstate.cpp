// MythTV
#include "audioplayer.h"
#include "mythplayerstate.h"

/*! \class MythAudioState
 * \brief A simple wrapper around audio state used to signal changes
 * in the current state.
*/
MythAudioState::MythAudioState(AudioPlayer* Player, int64_t Offset)
  : m_hasAudioOut(Player->HasAudioOut()),
    m_volumeControl(Player->ControlsVolume()),
    m_volume(Player->GetVolume()),
    m_muteState(Player->GetMuteState()),
    m_canUpmix(Player->CanUpmix()),
    m_isUpmixing(Player->IsUpmixing()),
    m_paused(Player->IsPaused()),
    m_audioOffset(Offset)
{
}

