#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

class NuppelVideoPlayer;
class AudioOutput;

class AudioPlayer
{
  public:
    AudioPlayer(NuppelVideoPlayer *parent, bool muted);
   ~AudioPlayer();

    void  Reset(void);
    void  DeleteOutput(void);
    QString ReinitAudio(void);
    void  SetAudioOutput(AudioOutput *ao);
    void  SetAudioInfo(const QString &main_device,
                       const QString &passthru_device,
                       uint           samplerate);
    void  SetAudioParams(AudioFormat format, int channels, int codec,
                         int samplerate, bool passthru);
    void  SetEffDsp(int dsprate);

    void  SetNoAudio(void)        { no_audio_out = true;  }
    bool  HasAudioIn(void) const  { return !no_audio_in;  }
    bool  HasAudioOut(void) const { return !no_audio_out; }

    bool  Pause(bool pause);
    bool  IsPaused(void);
    void  PauseAudioUntilBuffered(void);
    uint  GetVolume(void);
    uint  AdjustVolume(int change);
    float GetStretchFactor(void) { return m_stretchfactor;   }
    void  SetStretchFactor(float factor);
    bool  ToggleUpmix(void);
    long long GetAudioTime(void);

    bool      IsMuted(void) { return GetMuteState() == kMuteAll; }
    bool      SetMuted(bool mute);
    MuteState GetMuteState(void);
    MuteState SetMuteState(MuteState);
    MuteState IncrMuteState(void);

    void AddAudioData(char *buffer, int len, long long timecode);
    bool GetBufferStatus(uint &fill, uint &total);

  private:
    NuppelVideoPlayer *m_parent;
    AudioOutput *m_audioOutput;
    int          m_channels;
    int          m_codec;
    AudioFormat  m_format;
    int          m_samplerate;
    float        m_stretchfactor;
    bool         m_passthru;
    QMutex       m_lock;
    bool         m_muted_on_creation;
    QString      m_main_device;
    QString      m_passthru_device;
    bool         no_audio_in;
    bool         no_audio_out;
};

#endif // AUDIOPLAYER_H
