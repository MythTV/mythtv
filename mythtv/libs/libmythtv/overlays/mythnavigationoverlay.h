#ifndef MYTHNAVIGATIONOVERLAY_H
#define MYTHNAVIGATIONOVERLAY_H

// MythTV
#include "libmythui/mythscreentype.h"
#include "mythplayerstate.h"

class MythMainWindow;
class TV;
class MythPlayerUI;

class MythNavigationOverlay : public MythScreenType
{
    Q_OBJECT

  public:
    MythNavigationOverlay(MythMainWindow* MainWindow, TV* Tv,
                          MythPlayerUI* Player, const QString& Name, OSD* Osd);
    bool Create() override;
    bool keyPressEvent(QKeyEvent* Event) override;
    void ShowMenu() override;

  public slots:
    void AudioStateChanged(const MythAudioState& AudioState);
    void PauseChanged(bool Paused);
    void GeneralAction();
    void More();

  protected:
    void SendResult(int Result, const QString& Action);

    MythMainWindow* m_mainWindow      { nullptr };
    TV*             m_tv              { nullptr };
    MythPlayerUI*   m_player          { nullptr };
    OSD*            m_parentOverlay   { nullptr };
    MythUIButton*   m_playButton      { nullptr };
    MythUIButton*   m_pauseButton     { nullptr };
    MythUIButton*   m_muteButton      { nullptr };
    MythUIButton*   m_unMuteButton    { nullptr };
    bool            m_paused          { false   };
    MythAudioState  m_audioState;
    int             m_visibleGroup    { 0 };
    int             m_maxGroupNum     { -1 };
};

#endif
