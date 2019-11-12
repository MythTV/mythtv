#ifndef AUDIOREENCODEBUFFER_H
#define AUDIOREENCODEBUFFER_H

#include "mythconfig.h"
#include "audiooutput.h"

#define ABLOCK_SIZE   8192

class AudioBuffer
{
  public:
    AudioBuffer();
    AudioBuffer(const AudioBuffer &old);
    ~AudioBuffer();

    void appendData(unsigned char *buffer, int len, int frames, long long time);
    char *data(void) { return (char *)m_buffer; }
    int   size(void) { return m_size; }

    uint8_t    *m_buffer   {nullptr};
    int         m_size     {0};
    int         m_realsize {ABLOCK_SIZE};
    int         m_frames   {0};
    long long   m_time     {-1};
};

/**
 * This class is to act as a fake audio output device to store the data
 * for reencoding.
 */
class AudioReencodeBuffer : public AudioOutput
{
  public:
    AudioReencodeBuffer(AudioFormat audio_format, int audio_channels,
                        bool passthru);
    ~AudioReencodeBuffer();

    void      Reconfigure(const AudioSettings &settings) override; // AudioOutput
    void      SetEffDsp(int dsprate) override; // AudioOutput
    void      Reset(void) override; // AudioOutput
    bool      AddFrames(void *buffer, int frames, int64_t timecode) override; // AudioOutput
    bool      AddData(void *buffer, int len, int64_t timecode,
                      int frames) override; // AudioOutput
    AudioBuffer      *GetData(long long time);
    long long         GetSamples(long long time);
    void      SetTimecode(int64_t timecode) override; // AudioOutput
    bool      IsPaused(void) const override         { return false; } // AudioOutput
    void      Pause(bool paused) override           { (void)paused; } // AudioOutput
    void      PauseUntilBuffered(void) override     { } // AudioOutput
    void      Drain(void) override                  { } // AudioOutput
    int64_t   GetAudiotime(void) override           { return m_last_audiotime; } // AudioOutput
    int       GetVolumeChannel(int) const override  { return 100; } // VolumeBase
    void      SetVolumeChannel(int, int) override   { } // VolumeBase
    void      SetVolumeAll(int)                     { }
    uint      GetCurrentVolume(void) const override { return 100; } // VolumeBase
    void      SetCurrentVolume(int) override        { } // VolumeBase
    void      AdjustCurrentVolume(int) override     { } // VolumeBase
    virtual void      SetMute(bool)                 { }
    void      ToggleMute(void) override             { } // VolumeBase
    MuteState GetMuteState(void) const override     { return kMuteOff; } // VolumeBase
    virtual MuteState IterateMutedChannels(void)    { return kMuteOff; }
    void      SetSWVolume(int, bool) override       { return; } // VolumeBase
    int       GetSWVolume(void) override            { return 100; } // VolumeBase
    bool      CanPassthrough(int, int, AVCodecID, int) const override // AudioOutput
                      { return m_initpassthru; }

    //  These are pure virtual in AudioOutput, but we don't need them here
    void      bufferOutputData(bool) override       { return; } // AudioOutput
    int       readOutputData(unsigned char*, int ) override { return 0; } // AudioOutput

    int                  m_channels        {-1};
    int                  m_bytes_per_frame {-1};
    int                  m_eff_audiorate   {-1};
    long long            m_last_audiotime  {0};
    bool                 m_passthru        {false};
    int                  m_audioFrameSize  {0};

  private:
    bool                 m_initpassthru    {false};
    QMutex               m_bufferMutex;
    QList<AudioBuffer *> m_bufferList;
    AudioBuffer         *m_saveBuffer      {nullptr};
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

