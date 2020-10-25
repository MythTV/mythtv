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

    // Overlays
    void ChangeOSDPositionUpdates(bool Enable);
    void ChangeOSDDebug();
    void ChangeOSDText(const QString &Window, const InfoMap &Map, OSDTimeout Timeout);
    void ChangeOSDDialog(MythOSDDialogData Data);

  public slots:
    void AudioStateChanged(MythAudioState AudioState);
    void OverlayStateChanged(MythOverlayState OverlayState);

  protected:
    MythAudioState   m_audioState   { };
    MythOverlayState m_overlayState { };
};

#endif
