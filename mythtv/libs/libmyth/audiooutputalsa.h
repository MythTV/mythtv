#ifndef AUDIOOUTPUTALSA
#define AUDIOOUTPUTALSA

#include <vector>
#include <qstring.h>
#include <qmutex.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include "audiooutputbase.h"

using namespace std;

class AudioOutputALSA : public AudioOutputBase
{
  public:
    AudioOutputALSA(QString audiodevice, int laudio_bits, 
                   int laudio_channels, int laudio_samplerate);
    virtual ~AudioOutputALSA();
    
  protected:
    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual inline int getSpaceOnSoundcard(void);
    virtual inline int getBufferedOnSoundcard(void);

  private:
    snd_pcm_t *pcm_handle;

    int numbadioctls;

    QMutex killAudioLock;

    inline int SetParameters(snd_pcm_t *handle, snd_pcm_access_t access,
                             snd_pcm_format_t format, unsigned int channels,
                             unsigned int rate, unsigned int buffer_time,
                             unsigned int period_time);
    
};

#endif

