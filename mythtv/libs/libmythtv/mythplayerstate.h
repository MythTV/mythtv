#ifndef MYTHPLAYERSTATE_H
#define MYTHPLAYERSTATE_H

// MythTV
#include "mythtvexp.h"
#include "volumebase.h"
#include "videoouttypes.h"
#include "audiooutputsettings.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
}

class OSD;
class AudioPlayer;

class MTV_PUBLIC MythAudioPlayerState
{
  public:
    MythAudioPlayerState() = default;

    int          m_channels     { -1 };
    int          m_origChannels { -1 };
    AVCodecID    m_codec        { AV_CODEC_ID_NONE };
    AudioFormat  m_format       { FORMAT_NONE };
    int          m_sampleRate   { 44100 };
    int          m_codecProfile { 0 };
    bool         m_passthru     { false };
};

class MTV_PUBLIC MythAudioState
{
  public:
    MythAudioState() = default;
    MythAudioState(AudioPlayer* Player, int64_t Offset);

    bool m_hasAudioOut    { true  };
    bool m_volumeControl  { true  };
    uint m_volume         { 0     };
    MuteState m_muteState { kMuteOff };
    bool m_canUpmix       { false };
    bool m_isUpmixing     { false };
    bool m_paused         { false };
    int64_t m_audioOffset { 0     };
};

class MTV_PUBLIC MythVideoBoundsState
{
  public:
    MythVideoBoundsState() = default;
    MythVideoBoundsState(AdjustFillMode AdjustFill, AspectOverrideMode AspectOverride,
                         float HorizScale, float VertScale, const QPoint& Move);

    AdjustFillMode     m_adjustFillMode     { kAdjustFill_Off };
    AspectOverrideMode m_aspectOverrideMode { kAspect_Off     };
    float              m_manualHorizScale   { 1.0F };
    float              m_manualVertScale    { 1.0F };
    QPoint             m_manualMove         { 0, 0 };
};

class MTV_PUBLIC MythOverlayState
{
  public:
    MythOverlayState() = default;
    MythOverlayState(bool Browsing, bool Editing);

    bool m_browsing { false };
    bool m_editing  { false };
};

class MTV_PUBLIC MythVisualiserState
{
  public:
    MythVisualiserState() = default;
    MythVisualiserState(bool Embedding, bool Visualising,
                        QString Name, QStringList Visualisers);

    bool        m_canVisualise   { false };
    bool        m_embedding      { false };
    bool        m_visualising    { false };
    QString     m_visualiserName { };
    QStringList m_visualiserList { };
};

class MTV_PUBLIC MythCaptionsState
{
  public:
    MythCaptionsState() = default;
    MythCaptionsState(bool ITV);

    bool m_haveITV { false };
};

#endif
