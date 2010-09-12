#ifndef AUDIOOUTPUTCA
#define AUDIOOUTPUTCA

#include "audiooutputbase.h"

using namespace std;

#undef AUDBUFSIZE
#define AUDBUFSIZE 512000

// We hide Core Audio-specific items, to avoid
// pulling in Mac-specific header files.
class CoreAudioData;

class AudioOutputCA : public AudioOutputBase
{
public:
    AudioOutputCA(const AudioSettings &settings);
    virtual ~AudioOutputCA();
    
    AudioOutputSettings* GetOutputSettings(void);

    virtual int64_t GetAudiotime(void);

    // callback for delivering audio to output device
    bool RenderAudio(unsigned char *aubuf, int size,
                     unsigned long long timestamp);

    // Volume control
    virtual int  GetVolumeChannel(int channel) const;
    virtual void SetVolumeChannel(int channel, int volume);

    void Debug(QString msg)
    {   VERBOSE(VB_AUDIO,     "AudioOutputCA::" + msg);   }

    void Error(QString msg)
    {   VERBOSE(VB_IMPORTANT, "AudioOutputCA Error: " + msg);   }

    void Warn(QString msg)
    {   VERBOSE(VB_IMPORTANT, "AudioOutputCA Warning: " + msg);   }

protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual int  GetBufferedOnSoundcard(void) const;
    
    virtual bool StartOutputThread(void) { return true; }
    virtual void StopOutputThread(void) {}

private:

    CoreAudioData * d;
    friend class    CoreAudioData;

    int             bufferedBytes;
    long            CA_audiotime_updated;
};

#endif

