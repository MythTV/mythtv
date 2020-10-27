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
    // Audio
    void ReinitAudio();
    void ResetAudio();
    void PauseAudioUntilReady();
    void ChangeMuteState(bool CycleChannels);
    void ChangeVolume(bool Direction, int Volume);
    void ChangeUpmix(bool Enable, bool Toggle = false);
    void ChangeAudioOffset(int64_t Delta, int Value);

    // Video
    void RequestEmbedding(bool Embed, const QRect& Rect = {}, const QStringList& Data = {});
    void EmbedPlayback(bool Embed, const QRect& Rect = {});
    void WindowResized(const QSize& Size);
    void ChangeStereoOverride(StereoscopicMode Mode);
    void ChangePictureAttribute(PictureAttribute Attribute, bool Direction, int Value);
    void ToggleDetectLetterBox();
    void ChangeAdjustFill(AdjustFillMode FillMode = kAdjustFill_Toggle);
    void ChangeAspectOverride(AspectOverrideMode AspectMode = kAspect_Toggle);

    // Overlays
    void ChangeOSDPositionUpdates(bool Enable);
    void ChangeOSDDebug();
    void ChangeOSDText(const QString &Window, const InfoMap &Map, OSDTimeout Timeout);
    void ChangeOSDDialog(const MythOSDDialogData& Data);

    // Visualiser
    void EnableVisualiser(bool Enable, bool Toggle = false, const QString& Name = QString());

  public slots:
    void AudioPlayerStateChanged(MythAudioPlayerState AudioPlayerState);
    void AudioStateChanged(MythAudioState AudioState);
    void VideoBoundsStateChanged(MythVideoBoundsState VideoBoundsState);
    void OverlayStateChanged(MythOverlayState OverlayState);
    void VisualiserStateChanged(MythVisualiserState VisualiserState);

  protected:
    MythAudioPlayerState m_audioPlayerState { };
    MythAudioState       m_audioState       { };
    MythVideoBoundsState m_videoBoundsState { };
    MythOverlayState     m_overlayState     { };
    MythVisualiserState  m_visualiserState  { };
};

#endif
