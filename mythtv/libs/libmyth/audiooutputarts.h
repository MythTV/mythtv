#ifndef AUDIOOUTPUTARTS
#define AUDIOOUTPUTARTS

#include <qstring.h>

#define ARTS_PCM_NEW_HW_PARAMS_API
#define ARTS_PCM_NEW_SW_PARAMS_API
#include <artsc/artsc.h>

using namespace std;

#include "audiooutput.h"

class AudioOutputARTS : public AudioOutput
{
public:
     AudioOutputARTS(QString audiodevice, int audio_bits, 
                   int audio_channels, int audio_samplerate);
    virtual ~AudioOutputARTS();

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
    arts_stream_t pcm_handle;
    int effdsp; // from the recorded stream
    int audio_bytes_per_sample;
    int audio_bits;
    int audio_channels;
    int audbuf_timecode;    /* timecode of audio most recently placed into
                   buffer */
    bool can_hw_pause;
    bool paused;
};

#endif // AUDIOOUTPUTARTS
