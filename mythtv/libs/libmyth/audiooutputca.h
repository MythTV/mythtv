#ifndef AUDIOOUTPUTCA
#define AUDIOOUTPUTCA

#include <vector>
#include <qstring.h>

#include "audiooutputbase.h"

using namespace std;

#undef AUDBUFSIZE
#define AUDBUFSIZE 512000

// We hide Core Audio-specific items, to avoid
// pulling in Mac-specific header files.
struct CoreAudioData;

class AudioOutputCA : public AudioOutputBase
{
public:
    AudioOutputCA(QString laudio_main_device,
                  QString laudio_passthru_device,
                  int laudio_bits,
                  int laudio_channels, int laudio_samplerate,
                  AudioOutputSource lsource,
                  bool lset_initial_vol, bool laudio_passthru);
    virtual ~AudioOutputCA();
    
    virtual int GetAudiotime(void);
    void        SetAudiotime(void);

    // callback for delivering audio to output device
    bool RenderAudio(unsigned char *aubuf, int size,
                     unsigned long long timestamp);

    // Volume control
    virtual int GetVolumeChannel(int channel); // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume); // range 0-100 for vol

protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual inline int getSpaceOnSoundcard(void);
    virtual inline int getBufferedOnSoundcard(void);
    
    // The following functions may be overridden, but don't need to be
    virtual inline void StartOutputThread(void);
    virtual inline void StopOutputThread(void);

private:

    CoreAudioData * coreaudio_data;
    int             bufferedBytes;
    long            CA_audiotime_updated;
};

#endif

