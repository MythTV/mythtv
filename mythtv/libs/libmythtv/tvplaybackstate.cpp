// MythTV
#include "tvplaybackstate.h"

/*! \class TVPlaybackState
 * \brief Tracks current playback state, as signalled by the player, and requests
 * changes to the current state.
 *
 * Requesting player state changes is, by necessity, granular. Corresponding feedback
 * of new state is however signalled in small, functionally similar/associated groups
 * of state. This is to minimise the complexity of the API.
 *
 * \note The TV and player objects both operate within the main thread. As such
 * all signalling should be synchronous and hence the state should always be current.
 * \note Do not trigger additional code when state updates are received - as this
 * may lead to a feedback loop and/or recursion.
*/

void TVPlaybackState::OverlayStateChanged(const MythOverlayState OverlayState)
{
    m_overlayState = OverlayState;
}

void TVPlaybackState::AudioPlayerStateChanged(const MythAudioPlayerState& AudioPlayerState)
{
    m_audioPlayerState = AudioPlayerState;
}

void TVPlaybackState::AudioStateChanged(const MythAudioState& AudioState)
{
    m_audioState = AudioState;
}

void TVPlaybackState::CaptionsStateChanged(const MythCaptionsState CaptionsState)
{
    m_captionsState = CaptionsState;
}

void TVPlaybackState::VideoBoundsStateChanged(const MythVideoBoundsState& VideoBoundsState)
{
    m_videoBoundsState = VideoBoundsState;
}

void TVPlaybackState::VideoColourStateChanged(const MythVideoColourState& ColourState)
{
    m_videoColourState = ColourState;
}

void TVPlaybackState::VisualiserStateChanged(const MythVisualiserState& VisualiserState)
{
    m_visualiserState = VisualiserState;
}

void TVPlaybackState::EditorStateChanged(const MythEditorState& EditorState)
{
    m_editorState = EditorState;
}
