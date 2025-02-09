#ifndef AUDIOOUTPUTCA
#define AUDIOOUTPUTCA

#include "audiooutputbase.h"

#undef AUDBUFSIZE
#define AUDBUFSIZE 512000

// We hide Core Audio-specific items, to avoid
// pulling in Mac-specific header files.
class CoreAudioData;

class AudioOutputCA : public AudioOutputBase
{
public:
    explicit AudioOutputCA(const AudioSettings &settings);
    virtual ~AudioOutputCA();
    
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase
    static QMap<QString, QString> *GetDevices(const char *type = nullptr);

    std::chrono::milliseconds GetAudiotime(void) override; // AudioOutputBase

    // callback for delivering audio to output device
    bool RenderAudio(unsigned char *aubuf, int size,
                     unsigned long long timestamp);

    // Volume control
    int  GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase

protected:

    // You need to implement the following functions
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    
    bool StartOutputThread(void) override { return true; } // AudioOutputBase
    void StopOutputThread(void) override {} // AudioOutputBase

private:

    CoreAudioData * d {nullptr};
    friend class    CoreAudioData;

    int             m_bufferedBytes      {-1};
};

#endif

