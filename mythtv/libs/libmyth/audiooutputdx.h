#ifndef AUDIOOUTPUTDX
#define AUDIOOUTPUTDX

#include <qstring.h>

using namespace std;

#include "audiooutput.h"

#include <windows.h>
//#include <mmsystem.h>
#include <dsound.h>

class AudioOutputDX : public AudioOutputBase
{
public:
    AudioOutputDX(QString audiodevice, int audio_bits, 
                  int audio_channels, int audio_samplerate,
                  AudioOutputSource source, bool set_initial_vol);
    virtual ~AudioOutputDX();

    virtual void Reset(void);
    virtual void Reconfigure(int audio_bits, 
                         int audio_channels, int audio_samplerate);
    virtual void SetBlocking(bool blocking);

    virtual bool AddSamples(char *buffer, int samples, long long timecode);
    virtual bool AddSamples(char *buffers[], int samples, long long timecode);
    virtual void SetEffDsp(int dsprate);
    virtual void SetTimecode(long long timecode);

    virtual bool GetPause(void);
    virtual void Pause(bool paused);

    virtual void Drain(void);

    virtual int GetAudiotime(void);

    // Volume control
    virtual int GetVolumeChannel(int channel); // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume); // range 0-100 for vol

 private:
    HINSTANCE dsound_dll;      /* handle of the opened dsound dll */
    LPDIRECTSOUND dsobject;
    LPDIRECTSOUNDBUFFER dsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */
    LPDIRECTSOUNDNOTIFY dsnotify;         /* the position notify interface */
    DSBPOSITIONNOTIFY notif[4];

    DWORD write_cursor;
    DWORD buffer_size;
    bool blocking;
    
    bool awaiting_data;
 
    QString audiodevice;
    int effdsp; // from the recorded stream
    int audio_bytes_per_sample;
    int audio_bits;
    int audio_channels;
    int audbuf_timecode;    /* timecode of audio most recently placed into
                   buffer */
    bool can_hw_pause;
    bool paused;
    
    int InitDirectSound();
    int CreateDSBuffer(int audio_bits, int audio_channels, int audio_samplerate, bool b_probe);
    void DestroyDSBuffer();
    int FillBuffer(int frames, char *buffer);
};

#endif // AUDIOOUTPUTDX
