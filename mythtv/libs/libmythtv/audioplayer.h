#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "mythplayerstate.h"

#include <QCoreApplication>
#include <QRecursiveMutex>

#include <cstdint>
#include <vector>

class  MythPlayer;
class  AudioOutput;
struct AVCodecContext;
struct AVPacket;

namespace MythTV
{
    class Visual;
}

class MTV_PUBLIC AudioPlayer : public QObject
{
    Q_OBJECT

  signals:
    void AudioPlayerStateChanged(MythAudioPlayerState State);

  public:
    AudioPlayer(MythPlayer *parent, bool muted);
   ~AudioPlayer() override;

    void addVisual(MythTV::Visual *vis);
    void removeVisual(MythTV::Visual *vis);

    void  Reset(void);
    void  DeleteOutput(void);
    QString ReinitAudio(void);
    void  SetAudioOutput(AudioOutput *ao);
    void  SetAudioInfo(const QString &main_device,
                       const QString &passthru_device,
                       uint           samplerate,
                       int            codec_profile = -1);
    void  SetAudioParams(AudioFormat format, int orig_channels, int channels,
                         AVCodecID codec, int samplerate, bool passthru,
                         int codec_profile = -1);
    void  SetEffDsp(int dsprate);

    void  CheckFormat(void);
    void  SetNoAudio(void)           { m_noAudioOut = true;      }
    bool  HasAudioIn(void) const     { return !m_noAudioIn;      }
    bool  HasAudioOut(void) const    { return !m_noAudioOut;     }
    bool  ControlsVolume(void) const { return m_controlsVolume;  }

    bool  Pause(bool pause);
    bool  IsPaused(void);
    void  PauseAudioUntilBuffered(void);
    AVCodecID GetCodec(void)    const { return m_state.m_codec;        }
    int   GetOrigChannels(void) const { return m_state.m_origChannels; }
    int   GetSampleRate(void)   const { return m_state.m_sampleRate;   }
    uint  GetVolume(void);
    uint  AdjustVolume(int change);
    uint  SetVolume(int newvolume);
    float GetStretchFactor(void) const { return m_stretchFactor; }
    void  SetStretchFactor(float factor);
    bool  IsUpmixing(void);
    bool  EnableUpmix(bool enable, bool toggle = false);
    bool  CanUpmix(void);
    bool  CanPassthrough(int samplerate, int channels, AVCodecID codec, int profile);
    bool  CanDownmix(void);
    bool  CanAC3(void);
    bool  CanDTS(void);
    bool  CanEAC3(void);
    bool  CanTrueHD(void);
    bool  CanDTSHD(void);
    uint  GetMaxChannels(void);
    int   GetMaxHDRate(void);
    std::chrono::milliseconds GetAudioTime(void);
    AudioFormat GetFormat(void) const { return m_state.m_format; }
    bool CanProcess(AudioFormat fmt);
    uint32_t CanProcess(void);
    int   DecodeAudio(AVCodecContext *ctx,
                      uint8_t *buffer, int &data_size,
                      const AVPacket *pkt);

    bool      IsMuted(void) { return GetMuteState() == kMuteAll; }
    bool      SetMuted(bool mute);
    MuteState GetMuteState(void);
    MuteState SetMuteState(MuteState mstate);
    MuteState IncrMuteState(void);

    void AddAudioData(char *buffer, int len, std::chrono::milliseconds timecode, int frames);
    bool NeedDecodingBeforePassthrough(void);
    std::chrono::milliseconds LengthLastData(void);
    bool GetBufferStatus(uint &fill, uint &total);
    bool IsBufferAlmostFull(void);
    std::chrono::milliseconds GetAudioBufferedTime(void);
    
    /**
     * Return internal AudioOutput object
     */
    AudioOutput *GetAudioOutput(void) const { return m_audioOutput; }

  private:
    void AddVisuals(void);
    void RemoveVisuals(void);
    void ResetVisuals(void);

  private:
    MythPlayer  *m_parent            {nullptr};
    AudioOutput *m_audioOutput       {nullptr};
    MythAudioPlayerState m_state     { };
    float        m_stretchFactor     {1.0F};
    QRecursiveMutex m_lock;
    bool         m_mutedOnCreation   {false};
    QString      m_mainDevice;
    QString      m_passthruDevice;
    bool         m_noAudioIn         {false};
    bool         m_noAudioOut        {true};
    bool         m_controlsVolume    {true};
    std::vector<MythTV::Visual*> m_visuals;
};

#endif // AUDIOPLAYER_H
