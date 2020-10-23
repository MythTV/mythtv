#ifndef TVPLAYBACKSTATE_H
#define TVPLAYBACKSTATE_H

// Qt
#include <QRect>
#include <QObject>

// MythTV
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
    void ChangeVolume(bool Direction, int Volume, bool UpdateOSD);
    void ChangeUpmix(bool Enable, bool Toggle = false);
    void ChangeAudioOffset(int64_t Delta, int Value, bool UpdateOSD);

    // Video
    void RequestEmbedding(bool Embed, const QRect& Rect = {}, const QStringList& Data = {});
    void EmbedPlayback(bool Embed, const QRect& Rect = {});
    void WindowResized(const QSize Size);
    void ChangeStereoOverride(StereoscopicMode Mode);
    void ChangePictureAttribute(PictureAttribute Attribute, bool Direction, int Value);

  public slots:
    void AudioStateChanged(MythAudioState AudioState);

  protected:
    MythAudioState m_audioState { };
};

#endif
