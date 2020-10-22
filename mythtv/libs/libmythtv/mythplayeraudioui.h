#ifndef MYTHPLAYERAUDIOUI_H
#define MYTHPLAYERAUDIOUI_H

// MythTV
#include "volumebase.h"
#include "audiooutputgraph.h"
#include "mythplayeroverlayui.h"

class MTV_PUBLIC MythPlayerAudioUI : public MythPlayerOverlayUI
{
    Q_OBJECT

  signals:
    void      MuteChanged(MuteState Mute);

  protected slots:
    void      ChangeMuteState(bool CycleChannels);
    void      ChangeVolume(bool Direction, int Volume, bool UpdateOSD);

  public:
    MythPlayerAudioUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    void      ResetAudio();
    void      ReinitAudio();
    const AudioOutputGraph& GetAudioGraph() const;
    void      SetupAudioGraph (double VideoFrameRate);
    void      ClearAudioGraph ();
    uint      GetVolume();
    uint      AdjustVolume(int Change);
    uint      SetVolume(int Volume);
    bool      HasAudioOut() const;
    bool      IsMuted();
    bool      PlayerControlsVolume() const;
    MuteState GetMuteState();

    bool      CanUpmix();
    bool      IsUpmixing();
    bool      EnableUpmix(bool Enable, bool Toggle = false);
    void      PauseAudioUntilBuffered();
    void      SetupAudioOutput(float TimeStretch);
    int64_t   AdjustAudioTimecodeOffset(int64_t Delta, int Value = -9999);
    int64_t   GetAudioTimecodeOffset() const;

  private:
    Q_DISABLE_COPY(MythPlayerAudioUI)
    AudioOutputGraph m_audioGraph;
};

#endif
