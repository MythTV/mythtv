#ifndef AUDIOOUTPUTALSA
#define AUDIOOUTPUTALSA

#include <vector>
#include <qstring.h>
#include <qmutex.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include "audiooutput.h"

using namespace std;

#define AUDBUFSIZE 512000

class AudioOutputALSA : public AudioOutput
{
  public:
    AudioOutputALSA(QString audiodevice, int laudio_bits, 
                   int laudio_channels, int laudio_samplerate);
    virtual ~AudioOutputALSA();
    
    virtual void SetBlocking(bool blocking);
    virtual void Reset(void);
    virtual void Reconfigure(int laudio_bits, 
                             int laudio_channels, int laudio_samplerate);

    virtual void AddSamples(char *buffer, int samples, long long timecode);
    virtual void AddSamples(char *buffers[], int samples, long long timecode);
    virtual void SetTimecode(long long timecode);
    virtual void SetEffDsp(int dsprate);

    virtual bool GetPause(void);
    virtual void Pause(bool paused);
 
    virtual int GetAudiotime(void);

    inline int SetParameters(snd_pcm_t *handle, snd_pcm_access_t access,
                             snd_pcm_format_t format, unsigned int channels,
                             unsigned int rate, unsigned int buffer_time,
                             unsigned int period_time);

  protected:
    void KillAudio();
    void OutputAudioLoop(void);
    static void *kickoffOutputAudioLoop(void *player);

  private:
    int audiolen(bool use_lock); // number of valid bytes in audio buffer
    int audiofree(bool use_lock); // number of free bytes in audio buffer

    void WriteAudio(unsigned char *aubuf, int size);

    inline int getSpaceOnSoundcard(void);
    void SetFragSize(void);
    void SetAudiotime(void);

    bool killaudio;

    QString audiodevice;
    snd_pcm_t *pcm_handle;

    int effdsp; // from the recorded stream

    int audio_channels;
    int audio_bytes_per_sample;
    int audio_bits;
    int audio_samplerate;
    int audio_buffer_unused;
    int fragment_size;


    bool pauseaudio, audio_actually_paused;
    
    bool blocking; // do AddSamples calls block?
    
    int lastaudiolen;

    pthread_t output_audio;
    pthread_mutex_t audio_buflock; /* adjustments to audiotimecode, waud, and
                                      raud can only be made while holding this
                                      lock */
    pthread_cond_t audio_bufsig;  /* condition is signaled when the buffer
                                     gets more free space. Must be holding
                                     audio_buflock to use. */
    
    pthread_mutex_t avsync_lock; /* must hold avsync_lock to read or write
                                    'audiotime' and 'audiotime_updated' */
    int audiotime; // timecode of audio leaving the soundcard (same units as
                   //                                          timecodes) ...
    struct timeval audiotime_updated; // ... which was last updated at this time

    /* Audio circular buffer */
    unsigned char audiobuffer[AUDBUFSIZE];  /* buffer */
    int raud, waud;     /* read and write positions */
    int audbuf_timecode;    /* timecode of audio most recently placed into
                   buffer */

    int numbadioctls;
    int numlowbuffer;

    QMutex killAudioLock;
};

#endif

