#ifndef AUDIOOUTPUTOSS
#define AUDIOOUTPUTOSS

#include <vector>
#include <qstring.h>
#include <qmutex.h>

#include "audiooutputbase.h"

using namespace std;


class AudioOutputOSS : public AudioOutputBase
{
public:
    AudioOutputOSS(QString audiodevice, int laudio_bits, 
                   int laudio_channels, int laudio_samplerate,
                   AudioOutputSource source, bool set_initial_vol,
                   bool laudio_passthru);
    virtual ~AudioOutputOSS();

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

private:
    void VolumeInit(void);
    void VolumeCleanup(void);
    
    void SetFragSize(void);
    
    int audiofd;
    int numbadioctls;

    // Volume related
    int mixerfd;
    int control;

};

#endif

