#ifndef TVPLAYBACKSTATE_H
#define TVPLAYBACKSTATE_H

// Qt
#include <QRect>
#include <QObject>

// MythTV
#include "osd.h"
#include "mythplayerstate.h"

class MTV_PUBLIC TVPlaybackState : public QObject
{
    Q_OBJECT

  public:
    TVPlaybackState() = default;

  signals:
    // General
    void UpdateBookmark(bool Clear = false);

    // Overlays
    void IsOSDVisible(bool& Visible);
    void ChangeOSDPositionUpdates(bool Enable);
    void ChangeOSDDebug();
    void ChangeOSDText(const QString &Window, const InfoMap &Map, OSDTimeout Timeout);
    void ChangeOSDDialog(const MythOSDDialogData& Data);
    void ChangeOSDMessage(const QString& Message);

    // Audio
    void ReinitAudio();
    void ResetAudio();
    void PauseAudioUntilReady();
    void ChangeMuteState(bool CycleChannels = false);
    void ChangeVolume(bool Direction, int Volume);
    void ChangeUpmix(bool Enable, bool Toggle = false);
    void ChangeAudioOffset(int64_t Delta, int Value = -9999);

    // Audio and captions
    void SetTrack(uint Type, uint TrackNo);
    void ChangeTrack(uint Type, int Direction);

    // Captions/Interactive
    void ChangeAllowForcedSubtitles(bool Allow);
    void ToggleCaptions();
    void ToggleCaptionsByType(uint Type);
    void SetCaptionsEnabled(bool Enable, bool UpdateOSD = true);
    void DisableCaptions(uint Mode, bool UpdateOSD = true);
    void EnableCaptions(uint Mode, bool UpdateOSD = true);
    void ChangeCaptionTrack(int Direction);

    void ResetCaptions();
    void EnableTeletext(int Page = 0x100);
    void ResetTeletext();
    void SetTeletextPage(uint Page);
    void HandleTeletextAction(const QString &Action, bool& Handled);
    void RestartITV(uint Chanid, uint Cardid, bool IsLiveTV);
    void HandleITVAction(const QString& Action, bool& Handled);
    void AdjustSubtitleZoom(int Delta);
    void AdjustSubtitleDelay(int Delta);

    // Video
    void RequestEmbedding(bool Embed, const QRect& Rect = {}, const QStringList& Data = {});
    void EmbedPlayback(bool Embed, const QRect& Rect = {});
    void WindowResized(const QSize& Size);
    void ChangeStereoOverride(StereoscopicMode Mode);
    void ChangePictureAttribute(PictureAttribute Attribute, bool Direction, int Value);
    void ToggleDetectLetterBox();
    void ChangeAdjustFill(AdjustFillMode FillMode = kAdjustFill_Toggle);
    void ChangeAspectOverride(AspectOverrideMode AspectMode = kAspect_Toggle);
    void ChangeZoom(ZoomDirection Zoom);
    void ToggleMoveBottomLine();
    void SaveBottomLine();

    // Visualiser
    void EnableVisualiser(bool Enable, bool Toggle = false, const QString& Name = QString());

  public slots:
    void OverlayStateChanged(const MythOverlayState& OverlayState);
    void AudioPlayerStateChanged(const MythAudioPlayerState& AudioPlayerState);
    void AudioStateChanged(const MythAudioState& AudioState);
    void CaptionsStateChanged(const MythCaptionsState& CaptionsState);
    void VideoBoundsStateChanged(const MythVideoBoundsState& VideoBoundsState);
    void VisualiserStateChanged(const MythVisualiserState& VisualiserState);

  protected:
    MythOverlayState     m_overlayState     { };
    MythAudioPlayerState m_audioPlayerState { };
    MythAudioState       m_audioState       { };
    MythCaptionsState    m_captionsState    { };
    MythVideoBoundsState m_videoBoundsState { };
    MythVisualiserState  m_visualiserState  { };
};

#endif
