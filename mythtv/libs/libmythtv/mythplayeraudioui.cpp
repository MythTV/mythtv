// MythTV
#include "mythcorecontext.h"
#include "audioplayer.h"
#include "mythmainwindow.h"
#include "mythplayeraudioui.h"

MythPlayerAudioUI::MythPlayerAudioUI(MythMainWindow* MainWindow, TV *Tv,
                                                   PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerVisualiserUI(MainWindow, Tv, Context, Flags)
{
    m_audioGraph.SetPainter(m_painter);
}

void MythPlayerAudioUI::ResetAudio()
{
    m_audio.Reset();
}

void MythPlayerAudioUI::ReinitAudio()
{
    (void)m_audio.ReinitAudio();
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

uint MythPlayerAudioUI::GetVolume()
{
    return m_audio.GetVolume();
}

uint MythPlayerAudioUI::AdjustVolume(int Change)
{
    return m_audio.AdjustVolume(Change);
}

uint MythPlayerAudioUI::SetVolume(int Volume)
{
    return m_audio.SetVolume(Volume);
}

bool MythPlayerAudioUI::SetMuted(bool Mute)
{
    return m_audio.SetMuted(Mute);
}

MuteState MythPlayerAudioUI::GetMuteState()
{
    return m_audio.GetMuteState();
}

MuteState MythPlayerAudioUI::SetMuteState(MuteState State)
{
    return m_audio.SetMuteState(State);
}

MuteState MythPlayerAudioUI::IncrMuteState()
{
    return m_audio.IncrMuteState();
}

bool MythPlayerAudioUI::HasAudioOut() const
{
    return m_audio.HasAudioOut();
}

bool MythPlayerAudioUI::IsMuted()
{
    return m_audio.IsMuted();
}

bool MythPlayerAudioUI::CanUpmix()
{
    return m_audio.CanUpmix();
}

bool MythPlayerAudioUI::IsUpmixing()
{
    return m_audio.IsUpmixing();
}

bool MythPlayerAudioUI::PlayerControlsVolume() const
{
    return m_audio.ControlsVolume();
}

bool MythPlayerAudioUI::EnableUpmix(bool Enable, bool Toggle)
{
    return m_audio.EnableUpmix(Enable, Toggle);
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

