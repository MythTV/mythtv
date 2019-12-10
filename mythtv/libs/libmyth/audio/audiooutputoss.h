#ifndef AUDIOOUTPUTOSS
#define AUDIOOUTPUTOSS

#if HAVE_SYS_SOUNDCARD_H
    #include <sys/soundcard.h>
#elif HAVE_SOUNDCARD_H
    #include <soundcard.h>
#endif

#include "audiooutputbase.h"

class AudioOutputOSS : public AudioOutputBase
{
  public:
    explicit AudioOutputOSS(const AudioSettings &settings);
    ~AudioOutputOSS() override;

    // Volume control
    int GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase

  protected:
    // You need to implement the following functions
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase

  private:
    void VolumeInit(void);
    void VolumeCleanup(void);

    void SetFragSize(void);

    int         m_audioFd      {-1};
    mutable int m_numBadIoctls {0};

    // Volume related
    int         m_mixerFd      {-1};
    int         m_control      {SOUND_MIXER_VOLUME};
};

#endif

