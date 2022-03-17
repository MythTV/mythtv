// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythui/mythmainwindow.h"

#include "audioplayer.h"
#include "mythplayeraudioui.h"
#include "tv_play.h"

#define LOC QString("PlayerAudio: ")

/*! \class MythPlayerAudioUI
 * \brief Acts as the interface between the UI and the underlying AudioPlayer object.
 *
 * Changes to the audio state (e.g. volume changes) are signalled from the parent
 * TV object. Any subsequent change in state is signalled back via the AudioStateChanged signal.
 *
 * As such, the public API is limited and is largely restricted to access to the
 * AudioGraph, as required by the OSD/Edit mode.
 *
 * Any other objects that are interested in the audio state should connect to the
 * AudioStateChanged signal and retrieve the initial state via GetAudioState.
 *
 * \note This could/should be extended to include track data and current
 * source/output metadata.
 * \note Upmixing status (MythAudioState::m_canUpmix and MythAudioState::m_isUpmixing)
 * appears to be working as expected but is not extensively tested. As far as I
 * can tell, upmixing state/support only changes when the underlying audio object is
 * reconfigured - which is restricted to ToggleUpmix and ReinitAudio - both of which
 * are captured.
*/
MythPlayerAudioUI::MythPlayerAudioUI(MythMainWindow* MainWindow, TV *Tv,
                                     PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerOverlayUI(MainWindow, Tv, Context, Flags)
{
    // Register our types for signal usage
    qRegisterMetaType<MythAudioPlayerState>();
    qRegisterMetaType<MythAudioState>();

    // Setup audio graph
    m_audioGraph.SetPainter(m_painter);

    // Connect incoming signals
    connect(m_tv, &TV::ReinitAudio,          this, &MythPlayerAudioUI::ReinitAudio);
    connect(m_tv, &TV::ResetAudio,           this, &MythPlayerAudioUI::ResetAudio);
    connect(m_tv, &TV::ChangeMuteState,      this, &MythPlayerAudioUI::ChangeMuteState);
    connect(m_tv, &TV::ChangeUpmix,          this, &MythPlayerAudioUI::EnableUpmix);
    connect(m_tv, &TV::ChangeVolume,         this, &MythPlayerAudioUI::ChangeVolume);
    connect(m_tv, &TV::ChangeAudioOffset,    this, &MythPlayerAudioUI::AdjustAudioTimecodeOffset);
    connect(m_tv, &TV::PauseAudioUntilReady, this, &MythPlayerAudioUI::PauseAudioUntilBuffered);

    // Connect outgoing signals
    connect(this, &MythPlayerAudioUI::AudioStateChanged, m_tv, &TV::AudioStateChanged);
    connect(&m_audio, &AudioPlayer::AudioPlayerStateChanged, m_tv, &TV::AudioPlayerStateChanged);

    // Setup
    MythPlayerAudioUI::SetupAudioOutput(Context->m_tsNormal);
    auto offset = gCoreContext->GetDurSetting<std::chrono::milliseconds>("AudioSyncOffset", 0ms);
    MythPlayerAudioUI::AdjustAudioTimecodeOffset(0ms, offset);
}

/// \brief Initialise audio and signal initial state
void MythPlayerAudioUI::InitialiseState()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialising audio");
    ReinitAudio();
    MythPlayerOverlayUI::InitialiseState();
}

void MythPlayerAudioUI::RefreshAudioState()
{
    emit AudioStateChanged({ &m_audio, m_tcWrap[TC_AUDIO] });
}

void MythPlayerAudioUI::ResetAudio()
{
    m_audio.Reset();
    emit AudioStateChanged({ &m_audio, m_tcWrap[TC_AUDIO] });
}

void MythPlayerAudioUI::ReinitAudio()
{
    (void)m_audio.ReinitAudio();
    emit AudioStateChanged({ &m_audio, m_tcWrap[TC_AUDIO] });
}

const AudioOutputGraph& MythPlayerAudioUI::GetAudioGraph() const
{
    return m_audioGraph;
}

void MythPlayerAudioUI::SetupAudioGraph(double VideoFrameRate)
{
    uint samplerate = static_cast<uint>(m_audio.GetSampleRate());
    m_audioGraph.SetSampleRate(samplerate);
    m_audioGraph.SetSampleCount(static_cast<unsigned>(samplerate / VideoFrameRate));
    m_audio.addVisual(&m_audioGraph);
}

void MythPlayerAudioUI::ClearAudioGraph()
{
    m_audio.removeVisual(&m_audioGraph);
    m_audioGraph.Reset();
}

void MythPlayerAudioUI::ChangeVolume(bool Direction, int Volume)
{
    uint oldvolume = m_audio.GetVolume();

    if (Volume < 0)
        m_audio.AdjustVolume(Direction ? 2 : -2);
    else
        m_audio.SetVolume(Volume);

    if (!(m_browsing || m_editing))
    {
        uint volume = m_audio.GetVolume();
        UpdateOSDStatus(tr("Adjust Volume"), tr("Volume"), QString::number(volume),
                        kOSDFunctionalType_PictureAdjust, "%", static_cast<int>(volume * 10),
                        kOSDTimeout_Med);
        ChangeOSDPositionUpdates(false);
    }

    if (m_audio.GetVolume() != oldvolume)
        emit AudioStateChanged({ &m_audio, m_tcWrap[TC_AUDIO] });
}

void MythPlayerAudioUI::ChangeMuteState(bool CycleChannels)
{
    if (!(m_audio.HasAudioOut() && m_audio.ControlsVolume()))
        return;

    MuteState oldstate = m_audio.GetMuteState();

    if (CycleChannels)
        m_audio.IncrMuteState();
    else
        m_audio.SetMuted(!m_audio.IsMuted());

    QString text;
    MuteState mute = m_audio.GetMuteState();
    switch (mute)
    {
        case kMuteOff:   text = tr("Mute Off"); break;
        case kMuteAll:   text = tr("Mute On"); break;
        case kMuteLeft:  text = tr("Left Channel Muted"); break;
        case kMuteRight: text = tr("Right Channel Muted"); break;
    }

    UpdateOSDMessage(text);

    if (m_audio.GetMuteState() != oldstate)
        emit AudioStateChanged({ &m_audio, m_tcWrap[TC_AUDIO] });
}

void MythPlayerAudioUI::EnableUpmix(bool Enable, bool Toggle)
{
    if (!m_audio.HasAudioOut())
        return;

    bool oldupmixing = m_audio.IsUpmixing();
    m_audio.EnableUpmix(Enable, Toggle);
    ForceSetupAudioStream();
    bool newupmixing = m_audio.IsUpmixing();

    UpdateOSDMessage(newupmixing ? tr("Upmixer On") : tr("Upmixer Off"));

    if (newupmixing != oldupmixing)
        emit AudioStateChanged({ &m_audio, m_tcWrap[TC_AUDIO] });
}

void MythPlayerAudioUI::PauseAudioUntilBuffered()
{
    m_audio.PauseAudioUntilBuffered();
}

void MythPlayerAudioUI::SetupAudioOutput(float TimeStretch)
{
    QString passthru = gCoreContext->GetBoolSetting("PassThruDeviceOverride", false) ?
                       gCoreContext->GetSetting("PassThruOutputDevice") : QString();
    m_audio.SetAudioInfo(gCoreContext->GetSetting("AudioOutputDevice"), passthru,
                             static_cast<uint>(gCoreContext->GetNumSetting("AudioSampleRate", 44100)));
    m_audio.SetStretchFactor(TimeStretch);
}

void MythPlayerAudioUI::AdjustAudioTimecodeOffset(std::chrono::milliseconds Delta, std::chrono::milliseconds Value)
{
    std::chrono::milliseconds oldwrap = m_tcWrap[TC_AUDIO];

    if ((Value >= -1000ms) && (Value <= 1000ms))
        m_tcWrap[TC_AUDIO] = Value;
    else
        m_tcWrap[TC_AUDIO] += Delta;

    std::chrono::milliseconds newwrap = m_tcWrap[TC_AUDIO];
    if (!(m_browsing || m_editing))
    {
        UpdateOSDStatus(tr("Adjust Audio Sync"), tr("Audio Sync"),
                        QString::number(newwrap.count()), kOSDFunctionalType_AudioSyncAdjust,
                        "ms", (newwrap / 2 + 500ms).count(), kOSDTimeout_None);
        ChangeOSDPositionUpdates(false);
    }

    if (newwrap != oldwrap)
        emit AudioStateChanged({ &m_audio, newwrap });
}

