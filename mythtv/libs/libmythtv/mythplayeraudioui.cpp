// MythTV
#include "mythcorecontext.h"
#include "tv_play.h"
#include "audioplayer.h"
#include "mythmainwindow.h"
#include "mythplayeraudioui.h"

MythPlayerAudioUI::MythPlayerAudioUI(MythMainWindow* MainWindow, TV *Tv,
                                     PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerOverlayUI(MainWindow, Tv, Context, Flags)
{
    m_audioGraph.SetPainter(m_painter);
    connect(m_tv, &TV::ChangeMuteState, this, &MythPlayerAudioUI::ChangeMuteState);
    connect(m_tv, QOverload<bool,int,bool>::of(&TV::ChangeVolume), this, &MythPlayerAudioUI::ChangeVolume);
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

void MythPlayerAudioUI::ChangeVolume(bool Direction, int Volume, bool UpdateOSD)
{
    if (Volume < 0)
        m_audio.AdjustVolume(Direction ? 2 : -2);
    else
        m_audio.SetVolume(Volume);

    if (UpdateOSD)
    {
        uint volume = m_audio.GetVolume();
        UpdateOSDStatus(tr("Adjust Volume"), tr("Volume"), QString::number(volume),
                        kOSDFunctionalType_PictureAdjust, "%", static_cast<int>(volume * 10),
                        kOSDTimeout_Med);
        ChangeOSDPositionUpdates(false);
    }
}

uint MythPlayerAudioUI::AdjustVolume(int Change)
{
    return m_audio.AdjustVolume(Change);
}

uint MythPlayerAudioUI::SetVolume(int Volume)
{
    return m_audio.SetVolume(Volume);
}

void MythPlayerAudioUI::ChangeMuteState(bool CycleChannels)
{
    if (!(m_audio.HasAudioOut() && m_audio.ControlsVolume()))
        return;

    if (CycleChannels)
        m_audio.IncrMuteState();
    else
        m_audio.SetMuted(!m_audio.IsMuted());

    QString text;
    switch (m_audio.GetMuteState())
    {
        case kMuteOff:   text = tr("Mute Off"); break;
        case kMuteAll:   text = tr("Mute On"); break;
        case kMuteLeft:  text = tr("Left Channel Muted"); break;
        case kMuteRight: text = tr("Right Channel Muted"); break;
    }

    UpdateOSDMessage(text);
}

MuteState MythPlayerAudioUI::GetMuteState()
{
    return m_audio.GetMuteState();
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

int64_t MythPlayerAudioUI::AdjustAudioTimecodeOffset(int64_t Delta, int Value)
{
    if ((Value >= -1000) && (Value <= 1000))
        m_tcWrap[TC_AUDIO] = Value;
    else
        m_tcWrap[TC_AUDIO] += Delta;
    return m_tcWrap[TC_AUDIO];
}

int64_t MythPlayerAudioUI::GetAudioTimecodeOffset() const
{
    return m_tcWrap[TC_AUDIO];
}


