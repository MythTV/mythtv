#ifndef AUDIOOUTPUTALSA
#define AUDIOOUTPUTALSA

#include <qstring.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

using namespace std;

#include "audiooutput.h"

class AudioOutputALSA : public AudioOutput
{
public:
    AudioOutputALSA(QString audiodevice, int audio_bits, 
                   int audio_channels, int audio_samplerate);
    virtual ~AudioOutputALSA();

    virtual void Reset(void);
    virtual void Reconfigure(int audio_bits, 
                             int audio_channels, int audio_samplerate);
    virtual void SetBlocking(bool blocking);

    virtual void AddSamples(char *buffer, int samples, long long timecode);
    virtual void AddSamples(char *buffers[], int samples, long long timecode);
    virtual void SetEffDsp(int dsprate);
    virtual void SetTimecode(long long timecode);
    
    virtual bool GetPause(void);
    virtual void Pause(bool paused);
 
    virtual int GetAudiotime(void);

 private:
    QString audiodevice;
    snd_pcm_t *pcm_handle;
    int effdsp; // from the recorded stream
    int audio_bytes_per_sample;
    int audbuf_timecode;    /* timecode of audio most recently placed into
                   buffer */
    bool can_hw_pause;
    bool paused;
};

#endif // AUDIOOUTPUTALSA
