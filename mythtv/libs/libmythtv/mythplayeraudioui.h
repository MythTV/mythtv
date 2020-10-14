#ifndef MYTHPLAYERAUDIOUI_H
#define MYTHPLAYERAUDIOUI_H

// MythTV
#include "volumebase.h"
#include "audiooutputgraph.h"
#include "mythplayeroverlayui.h"

class MythPlayerAudioUI : public MythPlayerOverlayUI
{
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
    bool      SetMuted(bool Mute);
    bool      HasAudioOut() const;
    bool      IsMuted();
    bool      PlayerControlsVolume() const;
    MuteState GetMuteState();
    MuteState SetMuteState(MuteState State);
    MuteState IncrMuteState();
    bool      CanUpmix();
    bool      IsUpmixing();
    bool      EnableUpmix(bool Enable, bool Toggle = false);
    void      PauseAudioUntilBuffered();
    void      SetupAudioOutput(float TimeStretch);

  private:
    Q_DISABLE_COPY(MythPlayerAudioUI)
    AudioOutputGraph m_audioGraph;
};

#endif
