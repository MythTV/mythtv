#ifndef MYTHPLAYERCAPTIONSUI_H
#define MYTHPLAYERCAPTIONSUI_H

// MythTV
#include "mythcaptionsoverlay.h"
#include "mythplayeraudioui.h"

class MTV_PUBLIC MythPlayerCaptionsUI : public MythPlayerAudioUI
{
    Q_OBJECT

  signals:
    void CaptionsStateChanged(MythCaptionsState& CaptionsState);
    void ResizeForInteractiveTV(const QRect& Rect);
    void SetInteractiveStream(const QString& Stream);
    void SetInteractiveStreamPos(std::chrono::milliseconds Position);
    void PlayInteractiveStream(bool Play);
    void EnableSubtitles(bool Enable);

  public:
    MythPlayerCaptionsUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerCaptionsUI() override;

    // N.B. These methods handle audio tracks as well. Fix.
    QStringList GetTracks(uint Type);
    uint GetTrackCount(uint Type);
    int  GetTrack(uint Type);

    bool SetAudioByComponentTag(int Tag);
    bool SetVideoByComponentTag(int Tag);
    std::chrono::milliseconds GetStreamPos();
    std::chrono::milliseconds GetStreamMaxPos();
    InteractiveTV* GetInteractiveTV() override;

    void tracksChanged(uint TrackType) override;

  protected slots:
    void InitialiseState() override;
    void SetAllowForcedSubtitles(bool Allow);
    void ToggleCaptions();
    void ToggleCaptionsByType(uint Type);
    void SetCaptionsEnabled(bool Enable, bool UpdateOSD = true);
    virtual void DisableCaptions(uint Mode, bool UpdateOSD = true);
    virtual void EnableCaptions(uint Mode, bool UpdateOSD = true);
    virtual void SetTrack(uint Type, uint TrackNo);
    void ChangeCaptionTrack(int Direction);
    void ChangeTrack(uint Type, int Direction);
    void ResetCaptions();
    void EnableTeletext(int Page = 0x100);
    void ResetTeletext();
    void SetTeletextPage(uint Page);
    void HandleTeletextAction(const QString &Action, bool& Handled);
    void ITVHandleAction(const QString& Action, bool& Handled);
    void ITVRestart(uint Chanid, uint Cardid, bool IsLiveTV);
    void AdjustSubtitleZoom(int Delta);
    void AdjustSubtitleDelay(std::chrono::milliseconds Delta);

  private slots:
    void ExternalSubtitlesUpdated();
    void SetStream(const QString& Stream);
    void SetStreamPos(std::chrono::milliseconds Position);
    void StreamPlay(bool Playing = true);

  protected:
    double SafeFPS();
    void DoDisableForcedSubtitles();
    void DoEnableForcedSubtitles();
    void LoadExternalSubtitles();

    MythCaptionsOverlay m_captionsOverlay;
    MythCaptionsState m_captionsState { };
    bool m_textDesired                { false };
    uint m_lastTextDisplayMode        { kDisplayNone };
    uint m_lastValidTextDisplayMode   { kDisplayNone };
    InteractiveTV *m_interactiveTV    { nullptr };
    QMutex m_itvLock;
    bool m_itvEnabled   { false };
    bool m_itvVisible   { false };
    QString m_newStream;

  private:
    void DisableTeletext();
    bool HasCaptionTrack(uint Mode);
    uint NextCaptionTrack(uint Mode);
};

#endif
