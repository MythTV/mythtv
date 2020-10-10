// MythTV
#include "mythcorecontext.h"
#include "audioplayer.h"
#include "mythmainwindow.h"
#include "mythplayeraudiointerface.h"

MythPlayerAudioInterface::MythPlayerAudioInterface(MythMainWindow* MainWindow, TV *Tv,
                                                   PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerVisualiser(MainWindow, Tv, Context, Flags)
{
    m_audioGraph.SetPainter(m_painter);
}

void MythPlayerAudioInterface::ResetAudio()
{
    m_audio.Reset();
}

void MythPlayerAudioInterface::ReinitAudio()
{
    (void)m_audio.ReinitAudio();
}

const AudioOutputGraph& MythPlayerAudioInterface::GetAudioGraph() const
{
    return m_audioGraph;
}

void MythPlayerAudioInterface::SetupAudioGraph(double VideoFrameRate)
{
    uint samplerate = static_cast<uint>(m_audio.GetSampleRate());
    m_audioGraph.SetSampleRate(samplerate);
    m_audioGraph.SetSampleCount(static_cast<unsigned>(samplerate / VideoFrameRate));
    m_audio.addVisual(&m_audioGraph);
}

void MythPlayerAudioInterface::ClearAudioGraph()
{
    m_audio.removeVisual(&m_audioGraph);
    m_audioGraph.Reset();
}

uint MythPlayerAudioInterface::GetVolume()
{
    return m_audio.GetVolume();
}

uint MythPlayerAudioInterface::AdjustVolume(int Change)
{
    return m_audio.AdjustVolume(Change);
}

uint MythPlayerAudioInterface::SetVolume(int Volume)
{
    return m_audio.SetVolume(Volume);
}

bool MythPlayerAudioInterface::SetMuted(bool Mute)
{
    return m_audio.SetMuted(Mute);
}

MuteState MythPlayerAudioInterface::GetMuteState()
{
    return m_audio.GetMuteState();
}

MuteState MythPlayerAudioInterface::SetMuteState(MuteState State)
{
    return m_audio.SetMuteState(State);
}

MuteState MythPlayerAudioInterface::IncrMuteState()
{
    return m_audio.IncrMuteState();
}

bool MythPlayerAudioInterface::HasAudioOut() const
{
    return m_audio.HasAudioOut();
}

bool MythPlayerAudioInterface::IsMuted()
{
    return m_audio.IsMuted();
}

bool MythPlayerAudioInterface::CanUpmix()
{
    return m_audio.CanUpmix();
}

bool MythPlayerAudioInterface::IsUpmixing()
{
    return m_audio.IsUpmixing();
}

bool MythPlayerAudioInterface::PlayerControlsVolume() const
{
    return m_audio.ControlsVolume();
}

bool MythPlayerAudioInterface::EnableUpmix(bool Enable, bool Toggle)
{
    return m_audio.EnableUpmix(Enable, Toggle);
}

void MythPlayerAudioInterface::PauseAudioUntilBuffered()
{
    m_audio.PauseAudioUntilBuffered();
}

void MythPlayerAudioInterface::SetupAudioOutput(float TimeStretch)
{
    QString passthru = gCoreContext->GetBoolSetting("PassThruDeviceOverride", false) ?
                       gCoreContext->GetSetting("PassThruOutputDevice") : QString();
    m_audio.SetAudioInfo(gCoreContext->GetSetting("AudioOutputDevice"), passthru,
                             static_cast<uint>(gCoreContext->GetNumSetting("AudioSampleRate", 44100)));
    m_audio.SetStretchFactor(TimeStretch);
}

