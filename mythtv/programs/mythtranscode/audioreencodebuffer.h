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

    uint8_t    *m_buffer;
    int         m_size;
    int         m_realsize;
    int         m_frames;
    long long   m_time;
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

    virtual void      Reconfigure(const AudioSettings &settings);
    virtual void      SetEffDsp(int dsprate);
    virtual void      Reset(void);
    virtual bool      AddFrames(void *buffer, int frames, int64_t timecode);
    virtual bool      AddData(void *buffer, int len, int64_t timecode,
                              int frames);
    AudioBuffer      *GetData(long long time);
    long long         GetSamples(long long time);
    virtual void      SetTimecode(int64_t timecode);
    virtual bool      IsPaused(void) const          { return false; }
    virtual void      Pause(bool paused)            { (void)paused; }
    virtual void      PauseUntilBuffered(void)      { }
    virtual void      Drain(void)                   { }
    virtual int64_t   GetAudiotime(void)            { return m_last_audiotime; }
    virtual int       GetVolumeChannel(int) const   { return 100; }
    virtual void      SetVolumeChannel(int, int)    { }
    virtual void      SetVolumeAll(int)             { }
    virtual uint      GetCurrentVolume(void) const  { return 100; }
    virtual void      SetCurrentVolume(int)         { }
    virtual void      AdjustCurrentVolume(int)      { }
    virtual void      SetMute(bool)                 { }
    virtual void      ToggleMute(void)              { }
    virtual MuteState GetMuteState(void) const      { return kMuteOff; }
    virtual MuteState IterateMutedChannels(void)    { return kMuteOff; }
    virtual void      SetSWVolume(int new_volume, bool save) { return; }
    virtual int       GetSWVolume(void)             { return 100; }
    virtual bool      CanPassthrough(int, int, int, int) const
                      { return m_initpassthru; }

    //  These are pure virtual in AudioOutput, but we don't need them here
    virtual void      bufferOutputData(bool)        { return; }
    virtual int       readOutputData(unsigned char*, int ) { return 0; }

    int                  m_channels;
    int                  m_bytes_per_frame;
    int                  m_eff_audiorate;
    long long            m_last_audiotime;
    bool                 m_passthru;
    int                  m_audioFrameSize;

  private:
    bool                 m_initpassthru;
    QMutex               m_bufferMutex;
    QList<AudioBuffer *> m_bufferList;
    AudioBuffer         *m_saveBuffer;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

