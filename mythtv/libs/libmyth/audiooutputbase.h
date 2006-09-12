#ifndef AUDIOOUTPUTBASE
#define AUDIOOUTPUTBASE

// POSIX headers
#include <pthread.h>
#include <sys/time.h> // for struct timeval

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <qstring.h>
#include <qmutex.h>

// MythTV headers
#include "audiooutput.h"
#include "samplerate.h"
#include "SoundTouch.h"

#define AUDBUFSIZE 768000
#define AUDIO_SRC_IN_SIZE   16384
#define AUDIO_SRC_OUT_SIZE (16384*6)
#define AUDIO_TMP_BUF_SIZE (16384*6)

class AudioOutputBase : public AudioOutput
{
 public:
    AudioOutputBase(QString laudio_main_device,
                    QString laudio_passthru_device,
                    int laudio_bits,
                    int laudio_channels, int laudio_samplerate,
                    AudioOutputSource lsource,
                    bool lset_initial_vol, bool laudio_passthru);
    virtual ~AudioOutputBase();

    // reconfigure sound out for new params
    virtual void Reconfigure(int audio_bits, int audio_channels,
                             int audio_samplerate, bool audio_passthru);
    
    // do AddSamples calls block?
    virtual void SetBlocking(bool blocking);
    
    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate);

    virtual void SetStretchFactor(float factor);

    virtual void Reset(void);

    // timecode is in milliseconds.
    virtual bool AddSamples(char *buffer, int samples, long long timecode);
    virtual bool AddSamples(char *buffers[], int samples, long long timecode);

    virtual void SetTimecode(long long timecode);
    virtual bool GetPause(void);
    virtual void Pause(bool paused);

    // Wait for all data to finish playing
    virtual void Drain(void);
 
    virtual int GetAudiotime(void);

    // Send output events showing current progress
    virtual void Status(void);

    virtual void SetSourceBitrate(int rate);

    //  Only really used by the AudioOutputNULL object
    
    virtual void bufferOutputData(bool y){ buffer_output_data_for_use = y; }
    virtual int readOutputData(unsigned char *read_buffer, int max_length);
    
 protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void) = 0;
    virtual void CloseDevice(void) = 0;
    virtual void WriteAudio(unsigned char *aubuf, int size) = 0;
    virtual int getSpaceOnSoundcard(void) = 0;
    virtual int getBufferedOnSoundcard(void) = 0;
    virtual int GetVolumeChannel(int channel) = 0; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume) = 0; // range 0-100 for vol

    // The following functions may be overridden, but don't need to be
    virtual void StartOutputThread(void);
    virtual void StopOutputThread(void);

    int GetAudioData(unsigned char *buffer, int buf_size, bool fill_buffer);

    void _AddSamples(void *buffer, bool interleaved, int samples, long long timecode);
 
    void KillAudio();
    void OutputAudioLoop(void);
    static void *kickoffOutputAudioLoop(void *player);
    void SetAudiotime(void);
    int WaitForFreeSpace(int len);

    int audiolen(bool use_lock); // number of valid bytes in audio buffer
    int audiofree(bool use_lock); // number of free bytes in audio buffer

    void UpdateVolume(void);
    
    void SetStretchFactorLocked(float factor);

    int GetBaseAudioTime()                    const { return audiotime;       }
    int GetBaseAudBufTimeCode()               const { return audbuf_timecode; }
    soundtouch::SoundTouch *GetSoundStretch() const { return pSoundStretch;   }
    void SetBaseAudioTime(const int inAudioTime) { audiotime = inAudioTime; }

    int effdsp; // from the recorded stream
    int effdspstretched; // from the recorded stream

    // Basic details about the audio stream
    int audio_channels;
    int audio_bytes_per_sample;
    int audio_bits;
    int audio_samplerate;
    int audio_buffer_unused;
    int fragment_size;
    long soundcard_buffer_size;
    QString audio_main_device;
    QString audio_passthru_device;

    bool audio_passthru;

    float audio_stretchfactor;
    AudioOutputSource source;

    bool killaudio;

    bool pauseaudio, audio_actually_paused, was_paused;
    bool set_initial_vol;
    bool buffer_output_data_for_use; //  used by AudioOutputNULL
    
 private:
    // resampler
    bool need_resampler;
    SRC_STATE *src_ctx;
    SRC_DATA src_data;
    float src_in[AUDIO_SRC_IN_SIZE];
    float src_out[AUDIO_SRC_OUT_SIZE];
    short tmp_buff[AUDIO_TMP_BUF_SIZE];

    // timestretch
    soundtouch::SoundTouch * pSoundStretch;

    bool blocking; // do AddSamples calls block?

    int lastaudiolen;
    long long samples_buffered;
    
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

    long current_seconds;
    long source_bitrate;    
    

};

#endif

