#ifndef AUDIOOUTPUTCA
#define AUDIOOUTPUTCA

#include <vector>
#include <qstring.h>

#include "audiooutput.h"

using namespace std;

#define AUDBUFSIZE 512000

// We hide Core Audio-specific items, to avoid
// pulling in Mac-specific header files.
struct CoreAudioData;

class AudioOutputCA_Friend;

class AudioOutputCA : public AudioOutput
{
    friend class AudioOutputCA_Friend;

public:
    AudioOutputCA(QString audiodevice, int laudio_bits, 
                  int laudio_channels, int laudio_samplerate);
    virtual ~AudioOutputCA();
    
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


protected:
    void KillAudio();

private:
    int audiolen(bool use_lock);  // number of valid bytes in audio buffer
    int audiofree(bool use_lock); // number of free bytes in audio buffer

    // updates timecode of audio leaving the soundcard
    void SetAudiotime(unsigned long long timestamp, int audlen);

    // callback for delivering audio to output device
    bool RenderAudio(char *fragment, int fragment_size,
                     unsigned long long timestamp);
    
    bool killaudio;
    bool pauseaudio;

    int effdsp;      // from the recorded stream

    int audio_channels;
    int audio_bytes_per_sample;
    int audio_bits;
    int audio_samplerate;
    
    bool blocking;   // do AddSamples calls block?
    
    pthread_mutex_t audio_buflock; // adjustments to audiotimecode,
                                   // waud, and raud can only be made
                                   // while holding this lock
    pthread_cond_t audio_bufsig;   // condition is signaled when the buffer
                                   // gets more free space. Must be holding
                                   // audio_buflock to use.
    
    pthread_mutex_t avsync_lock;      // must hold avsync_lock to read or write
                                      // 'audiotime' and 'audiotime_updated'
    int audiotime;                    // timecode of audio leaving the soundcard
                                      // (same units as timecodes) ...
    struct timeval audiotime_updated; // ... which was last updated at this time

    // Audio circular buffer
    unsigned char audiobuffer[AUDBUFSIZE];  // buffer
    int raud, waud;                         // read and write positions
    int audbuf_timecode;                    // timecode of audio most recently
                                            // placed into buffer

    CoreAudioData * coreaudio_data;
};

#endif

