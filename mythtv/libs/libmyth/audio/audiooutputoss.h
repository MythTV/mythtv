#ifndef AUDIOOUTPUTOSS
#define AUDIOOUTPUTOSS

#include "audiooutputbase.h"

class AudioOutputOSS : public AudioOutputBase
{
  public:
    explicit AudioOutputOSS(const AudioSettings &settings);
    virtual ~AudioOutputOSS();

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

    int audiofd;
    mutable int numbadioctls;

    // Volume related
    int mixerfd;
    int control;
};

#endif

