#ifndef AUDIOOUTPUTBASE
#define AUDIOOUTPUTBASE

#include <iostream>
using namespace std;

#include <qstring.h>
#include <qmutex.h>

#include "audiooutput.h"
#include "samplerate.h"

#define AUDBUFSIZE 512000

class AudioOutputBase : public AudioOutput
{
 public:
    AudioOutputBase(QString audiodevice);
    virtual ~AudioOutputBase();

    // reconfigure sound out for new params
    virtual void Reconfigure(int audio_bits, 
                             int audio_channels, int audio_samplerate);
    
    // do AddSamples calls block?
    virtual void SetBlocking(bool blocking);
    
    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate);

    virtual void Reset(void);

    // timecode is in milliseconds.
    virtual void AddSamples(char *buffer, int samples, long long timecode);
    virtual void AddSamples(char *buffers[], int samples, long long timecode);

    virtual void SetTimecode(long long timecode);
    virtual bool GetPause(void);
    virtual void Pause(bool paused);
 
    virtual int GetAudiotime(void);

    QString GetError() { return lastError; };

 protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void) = 0;
    virtual void CloseDevice(void) = 0;
    virtual void WriteAudio(unsigned char *aubuf, int size) = 0;
    virtual int getSpaceOnSoundcard(void) = 0;
    virtual int getBufferedOnSoundcard(void) = 0;

    void _AddSamples(void *buffer, bool interleaved, int samples, long long timecode);
	 
    void KillAudio();
    void OutputAudioLoop(void);
    static void *kickoffOutputAudioLoop(void *player);
    void SetAudiotime(void);

    int audiolen(bool use_lock); // number of valid bytes in audio buffer
    int audiofree(bool use_lock); // number of free bytes in audio buffer

    int effdsp; // from the recorded stream

    // Basic details about the audio stream
    int audio_channels;
    int audio_bytes_per_sample;
    int audio_bits;
    int audio_samplerate;
    int audio_buffer_unused;
    int fragment_size;
    QString audiodevice;
    
 private:
    QString lastError;

    // resampler
    bool need_resampler;
    SRC_STATE *src_ctx;
    SRC_DATA src_data;
    float src_in[16384], src_out[16384*6];
    short tmp_buff[16384*6];


    bool killaudio;

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

    int numlowbuffer;

    QMutex killAudioLock;
};

#endif

