// MythTV
#include "mythcorecontext.h"
#include "audioplayer.h"
#include "mythmainwindow.h"
#include "mythplayeraudiointerface.h"

MythPlayerAudioInterface::MythPlayerAudioInterface(MythMainWindow* MainWindow, AudioPlayer* Audio)
  : m_audioOut(Audio)
{
    m_audioGraph.SetPainter(MainWindow->GetPainter());
}

void MythPlayerAudioInterface::ResetAudio()
{
    m_audioOut->Reset();
}

void MythPlayerAudioInterface::ReinitAudio()
{
    (void)m_audioOut->ReinitAudio();
}

const AudioOutputGraph& MythPlayerAudioInterface::GetAudioGraph() const
{
    return m_audioGraph;
}

void MythPlayerAudioInterface::SetupAudioGraph(double VideoFrameRate)
{
    uint samplerate = static_cast<uint>(m_audioOut->GetSampleRate());
    m_audioGraph.SetSampleRate(samplerate);
    m_audioGraph.SetSampleCount(static_cast<unsigned>(samplerate / VideoFrameRate));
    m_audioOut->addVisual(&m_audioGraph);
}

void MythPlayerAudioInterface::ClearAudioGraph()
{
    m_audioOut->removeVisual(&m_audioGraph);
    m_audioGraph.Reset();
}

uint MythPlayerAudioInterface::GetVolume()
{
    return m_audioOut->GetVolume();
}

uint MythPlayerAudioInterface::AdjustVolume(int Change)
{
    return m_audioOut->AdjustVolume(Change);
}

uint MythPlayerAudioInterface::SetVolume(int Volume)
{
    return m_audioOut->SetVolume(Volume);
}

bool MythPlayerAudioInterface::SetMuted(bool Mute)
{
    return m_audioOut->SetMuted(Mute);
}

MuteState MythPlayerAudioInterface::GetMuteState()
{
    return m_audioOut->GetMuteState();
}

MuteState MythPlayerAudioInterface::SetMuteState(MuteState State)
{
    return m_audioOut->SetMuteState(State);
}

MuteState MythPlayerAudioInterface::IncrMuteState()
{
    return m_audioOut->IncrMuteState();
}

bool MythPlayerAudioInterface::HasAudioOut() const
{
    return m_audioOut->HasAudioOut();
}

bool MythPlayerAudioInterface::IsMuted()
{
    return m_audioOut->IsMuted();
}

bool MythPlayerAudioInterface::CanUpmix()
{
    return m_audioOut->CanUpmix();
}

bool MythPlayerAudioInterface::IsUpmixing()
{
    return m_audioOut->IsUpmixing();
}

bool MythPlayerAudioInterface::PlayerControlsVolume() const
{
    return m_audioOut->ControlsVolume();
}

bool MythPlayerAudioInterface::EnableUpmix(bool Enable, bool Toggle)
{
    return m_audioOut->EnableUpmix(Enable, Toggle);
}

void MythPlayerAudioInterface::PauseAudioUntilBuffered()
{
    m_audioOut->PauseAudioUntilBuffered();
}

void MythPlayerAudioInterface::SetupAudioOutput(float TimeStretch)
{
    QString passthru = gCoreContext->GetBoolSetting("PassThruDeviceOverride", false) ?
                       gCoreContext->GetSetting("PassThruOutputDevice") : QString();
    m_audioOut->SetAudioInfo(gCoreContext->GetSetting("AudioOutputDevice"), passthru,
                             static_cast<uint>(gCoreContext->GetNumSetting("AudioSampleRate", 44100)));
    m_audioOut->SetStretchFactor(TimeStretch);
}

