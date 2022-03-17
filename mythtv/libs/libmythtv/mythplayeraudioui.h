#ifndef MYTHPLAYERAUDIOUI_H
#define MYTHPLAYERAUDIOUI_H

// MythTV
#include "libmyth/audio/volumebase.h"
#include "libmyth/audio/audiooutputgraph.h"
#include "mythplayeroverlayui.h"

class MTV_PUBLIC MythPlayerAudioUI : public MythPlayerOverlayUI
{
    Q_OBJECT

  signals:
    void      AudioStateChanged(MythAudioState State);

  public slots:
    void      RefreshAudioState();

  protected slots:
    void      InitialiseState() override;
    void      ChangeMuteState(bool CycleChannels);
    void      ChangeVolume(bool Direction, int Volume);
    void      ResetAudio();
    void      ReinitAudio();
    void      EnableUpmix(bool Enable, bool Toggle = false);
    void      PauseAudioUntilBuffered();
    void      AdjustAudioTimecodeOffset(std::chrono::milliseconds Delta, std::chrono::milliseconds Value);

  public:
    MythPlayerAudioUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    const AudioOutputGraph& GetAudioGraph() const;
    void      SetupAudioGraph (double VideoFrameRate);
    void      ClearAudioGraph ();

  private:
    Q_DISABLE_COPY(MythPlayerAudioUI)
    void      SetupAudioOutput(float TimeStretch);

    AudioOutputGraph m_audioGraph;
};

#endif
