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
                   int laudio_channels, int laudio_samplerate,
                   AudioOutputSource source, bool set_initial_vol);
    virtual ~AudioOutputALSA();

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

    snd_pcm_t *pcm_handle;

    int numbadioctls;

    QMutex killAudioLock;

    inline int SetParameters(snd_pcm_t *handle, snd_pcm_access_t access,
                             snd_pcm_format_t format, unsigned int channels,
                             unsigned int rate, unsigned int buffer_time,
                             unsigned int period_time);


    // Volume related
    void SetCurrentVolume(QString control, int channel, int volume);
    void OpenMixer(bool setstartingvolume);
    void CloseMixer(void);
    void SetupMixer(void);
    inline void GetVolumeRange(void);

    snd_mixer_t          *mixer_handle;
    snd_mixer_elem_t     *elem;
    snd_mixer_selem_id_t *sid;

    QString mixer_control;  // e.g. "PCM"

    float volume_range_multiplier;
    long playback_vol_min, playback_vol_max;
};

#endif

