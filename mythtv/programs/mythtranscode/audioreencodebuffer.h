#ifndef AUDIOREENCODEBUFFER_H
#define AUDIOREENCODEBUFFER_H

#include "libmyth/audio/audiooutput.h"
#include "libmythbase/mythconfig.h"

static constexpr size_t ABLOCK_SIZE { 8192 };

class AudioBuffer
{
  public:
    AudioBuffer();
    AudioBuffer(const AudioBuffer &old);
    ~AudioBuffer();

    void appendData(unsigned char *buffer, int len, int frames, std::chrono::milliseconds time);
    char *data(void) const { return (char *)m_buffer; }
    int   size(void) const { return m_size; }

    uint8_t    *m_buffer   {nullptr};
    size_t      m_size     {0};
    size_t      m_realsize {ABLOCK_SIZE};
    int         m_frames   {0};
    std::chrono::milliseconds   m_time     {-1ms};
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
    ~AudioReencodeBuffer() override;

    void      Reconfigure(const AudioSettings &settings) override; // AudioOutput
    void      SetEffDsp(int dsprate) override; // AudioOutput
    void      Reset(void) override; // AudioOutput
    bool      AddFrames(void *buffer, int frames, std::chrono::milliseconds timecode) override; // AudioOutput
    bool      AddData(void *buffer, int len, std::chrono::milliseconds timecode,
                      int frames) override; // AudioOutput
    AudioBuffer      *GetData(std::chrono::milliseconds time);
    long long         GetSamples(std::chrono::milliseconds time);
    void      SetTimecode(std::chrono::milliseconds timecode) override; // AudioOutput
    bool      IsPaused(void) const override         { return false; } // AudioOutput
    void      Pause(bool paused) override           { (void)paused; } // AudioOutput
    void      PauseUntilBuffered(void) override     { } // AudioOutput
    void      Drain(void) override                  { } // AudioOutput
    std::chrono::milliseconds GetAudiotime(void) override { return m_last_audiotime; } // AudioOutput
    int       GetVolumeChannel(int /*channel*/) const override  { return 100; } // VolumeBase
    void      SetVolumeChannel(int /*channel*/, int /*volume*/) override   { } // VolumeBase
    uint      GetCurrentVolume(void) const override { return 100; } // VolumeBase
    void      SetCurrentVolume(int /*value*/) override { } // VolumeBase
    void      AdjustCurrentVolume(int /*change*/) override { } // VolumeBase
    virtual void      SetMute(bool /*mute  */)      { }
    void      ToggleMute(void) override             { } // VolumeBase
    MuteState GetMuteState(void) const override     { return kMuteOff; } // VolumeBase
    virtual MuteState IterateMutedChannels(void)    { return kMuteOff; }
    void      SetSWVolume(int /*new_volume*/, bool /*save*/) override       { } // VolumeBase
    int       GetSWVolume(void) override            { return 100; } // VolumeBase
    bool      CanPassthrough(int /*samplerate*/, int /*channels*/, AVCodecID /*codec*/, int /*profile*/) const override // AudioOutput
                      { return m_initpassthru; }

    //  These are pure virtual in AudioOutput, but we don't need them here
    void      bufferOutputData(bool /*y*/) override       { } // AudioOutput
    int       readOutputData(unsigned char */*read_buffer*/,
                             size_t /*max_length*/) override { return 0; } // AudioOutput

    int                  m_channels        {-1};
    int                  m_bytes_per_frame {-1};
    int                  m_eff_audiorate   {-1};
    std::chrono::milliseconds m_last_audiotime  {0ms};
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

