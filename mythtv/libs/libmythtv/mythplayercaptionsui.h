#ifndef MYTHPLAYERCAPTIONSUI_H
#define MYTHPLAYERCAPTIONSUI_H

// MythTV
#include "mythcaptionsoverlay.h"
#include "mythplayeraudioui.h"

class MTV_PUBLIC MythPlayerCaptionsUI : public MythPlayerAudioUI
{
    Q_OBJECT

  signals:
    void CaptionsStateChanged(MythCaptionsState CaptionsState);
    void ResizeForInteractiveTV(const QRect& Rect);
    void SetInteractiveStream(const QString& Stream);
    void SetInteractiveStreamPos(long Position);
    void PlayInteractiveStream(bool Play);

  public:
    MythPlayerCaptionsUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerCaptionsUI() override;

    bool HasTextSubtitles();
    bool GetCaptionsEnabled() const;

    // N.B. These methods handle audio tracks as well. Fix.
    QStringList GetTracks(uint Type);
    uint GetTrackCount(uint Type);
    virtual int SetTrack(uint Type, uint TrackNo);
    int  GetTrack(uint Type);
    int  ChangeTrack(uint Type, int Direction);

    bool SetAudioByComponentTag(int Tag);
    bool SetVideoByComponentTag(int Tag);
    long GetStreamPos();
    long GetStreamMaxPos();
    InteractiveTV* GetInteractiveTV() override;

  protected slots:
    void ToggleCaptions();
    void ToggleCaptionsByType(uint Type);
    void SetCaptionsEnabled(bool Enable, bool UpdateOSD = true);
    virtual void DisableCaptions(uint Mode, bool UpdateOSD = true);
    virtual void EnableCaptions(uint Mode, bool UpdateOSD = true);
    void ChangeCaptionTrack(int Direction);
    void ResetCaptions();
    void EnableTeletext(int Page = 0x100);
    void ResetTeletext();
    void SetTeletextPage(uint Page);
    void HandleTeletextAction(const QString &Action, bool& Handled);
    void ITVHandleAction(const QString& Action, bool& Handled);
    void ITVRestart(uint Chanid, uint Cardid, bool IsLiveTV);
    void AdjustSubtitleZoom(int Delta);
    void AdjustSubtitleDelay(int Delta);

  private slots:
    void SetStream(const QString& Stream);
    long SetStreamPos(long Position);
    void StreamPlay(bool Playing = true);

  protected:
    double SafeFPS();
    void DoDisableForcedSubtitles();
    void DoEnableForcedSubtitles();

    MythCaptionsOverlay m_captionsOverlay;
    MythCaptionsState m_captionsState { };
    InteractiveTV *m_interactiveTV { nullptr };
    QMutex m_itvLock    { };
    bool m_itvEnabled   { false };
    bool m_itvVisible   { false };
    QString m_newStream { };

  private:
    void DisableTeletext();
    bool HasCaptionTrack(uint Mode);
    uint NextCaptionTrack(uint Mode);
};

#endif
