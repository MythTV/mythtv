#ifndef AUDIOOUTPUTDX
#define AUDIOOUTPUTDX

/* ACK! <windows.h> and <dsound.h> should only be in cpp's compiled
 * in windows only. Some of the variables in AudioOutputDX need to
 * be moved to a private class before removing these includes though.
 */
#include <windows.h> // HACK HACK HACK
#include <dsound.h>  // HACK HACK HACK

// Qt headers
#include <qstring.h>

// MythTV headers
#include "audiooutputbase.h"

class AudioOutputDX : public AudioOutputBase
{
public:
    AudioOutputDX(QString laudio_main_device,
                  QString laudio_passthru_device,
                  int laudio_bits,
                  int laudio_channels, int laudio_samplerate,
                  AudioOutputSource lsource,
                  bool lset_initial_vol, bool laudio_passthru);
    virtual ~AudioOutputDX();

    /// BEGIN HACK HACK HACK HACK These need to actually be implemented!
    bool OpenDevice(void) { return false; }
    void CloseDevice(void) {}
    void WriteAudio(unsigned char*, int) {}
    virtual int getSpaceOnSoundcard(void) { return 0; }
    virtual int getBufferedOnSoundcard(void) { return 0; }
#warning Several methods in AudioOutputDX need to be implemented...
    /// END HACK HACK HACK HACK
	
    virtual void Reset(void);
    virtual void Reconfigure(int audio_bits,       int audio_channels,
                             int audio_samplerate, int audio_passthru);
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
