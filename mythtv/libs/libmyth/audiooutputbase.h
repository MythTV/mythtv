#ifndef AUDIOOUTPUTBASE
#define AUDIOOUTPUTBASE

// POSIX headers
#include <sys/time.h> // for struct timeval

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>

// MythTV headers
#include "audiooutput.h"
#include "audiooutputsettings.h"
#include "samplerate.h"
#include "mythverbose.h"

#define VBAUDIO(str)   VERBOSE(VB_AUDIO, LOC + str)
#define VBAUDIOTS(str) VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC + str)
#define VBGENERAL(str) VERBOSE(VB_GENERAL, LOC + str)
#define VBERROR(str)   VERBOSE(VB_IMPORTANT, LOC_ERR + str)

namespace soundtouch {
class SoundTouch;
};
class FreeSurround;
class AudioOutputDigitalEncoder;
struct AVCodecContext;

class AudioOutputBase : public AudioOutput, public QThread
{
 public:
    AudioOutputBase(const AudioSettings &settings);
    virtual ~AudioOutputBase();

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings);

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate);

    // timestretch
    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const;

    virtual bool CanPassthrough(void) const;
    virtual bool ToggleUpmix(void);

    virtual void Reset(void);

    void SetSWVolume(int new_volume, bool save);
    int GetSWVolume(void);

    // timecode is in milliseconds.
    virtual bool AddSamples(void *buffer, int frames, long long timecode);

    virtual void SetTimecode(long long timecode);
    virtual bool IsPaused(void) const { return actually_paused; }
    virtual void Pause(bool paused);
    void PauseUntilBuffered(void);

    // Wait for all data to finish playing
    virtual void Drain(void);

    virtual int GetAudiotime(void);
    virtual int GetAudioBufferedTime(void);

    // Send output events showing current progress
    virtual void Status(void);

    virtual void SetSourceBitrate(int rate);

    virtual void GetBufferStatus(uint &fill, uint &total);

    //  Only really used by the AudioOutputNULL object
    virtual void bufferOutputData(bool y){ buffer_output_data_for_use = y; }
    virtual int readOutputData(unsigned char *read_buffer, int max_length);

    static const uint kAudioSRCInputSize  = 16384<<1;
    static const uint kAudioSRCOutputSize = 16384<<3;
    /// Audio Buffer Size -- should be divisible by 12,10,8,6,4,2..
    static const uint kAudioRingBufferSize   = 1536000;

 protected:
    // You need to implement the following functions
    virtual bool OpenDevice(void) = 0;
    virtual void CloseDevice(void) = 0;
    virtual void WriteAudio(uchar *aubuf, int size) = 0;
    virtual int  GetBufferedOnSoundcard(void) const = 0;
    // Default implementation only supports 2ch s16le at 48kHz
    virtual AudioOutputSettings* GetOutputSettings(void)
        { return new AudioOutputSettings; }
    /// You need to call this from any implementation in the dtor.
    void KillAudio(void);

    // The following functions may be overridden, but don't need to be
    virtual bool StartOutputThread(void);
    virtual void StopOutputThread(void);

    int GetAudioData(uchar *buffer, int buf_size, bool fill_buffer);

    void OutputAudioLoop(void);

    virtual void run();

    int CheckFreeSpace(int &frames);

    inline int audiolen(); // number of valid bytes in audio buffer
    int audiofree();       // number of free bytes in audio buffer
    int audioready();      // number of bytes ready to be written

    void SetStretchFactorLocked(float factor);

    // For audiooutputca
    int GetBaseAudBufTimeCode() const { return audbuf_timecode; }

  protected:

    // Basic details about the audio stream
    int channels;
    int codec;
    int bytes_per_frame;
    int output_bytes_per_frame;
    AudioFormat format;
    AudioFormat output_format;
    int samplerate;
    int effdsp; // from the recorded stream (NuppelVideo)
    int fragment_size;
    long soundcard_buffer_size;

    QString main_device, passthru_device;

    bool passthru, enc, reenc;

    float stretchfactor;
    AudioOutputSource source;

    bool killaudio;

    bool pauseaudio, actually_paused, was_paused, unpause_when_ready;
    bool set_initial_vol;
    bool buffer_output_data_for_use; //  used by AudioOutputNULL

    int configured_channels;
    int max_channels;
    int src_quality;
    bool allow_multipcm;

 private:
    int CopyWithUpmix(char *buffer, int frames, int &org_waud);
    AudioOutputSettings *output_settings;
    bool need_resampler;
    SRC_STATE *src_ctx;
    soundtouch::SoundTouch    *pSoundStretch;
    AudioOutputDigitalEncoder *encoder;
    FreeSurround              *upmixer;

    int source_channels;
    int source_samplerate;
    int source_bytes_per_frame;
    bool needs_upmix;
    bool needs_downmix;
    int surround_mode;
    bool allow_ac3_passthru;
    float old_stretchfactor;
    int volume;
    QString volumeControl;

    bool pulsewassuspended;

    bool processing;

    long long frames_buffered;

    bool audio_thread_exists;

    /* Writes to the audiobuffer, reconfigures and audiobuffer resets can only
       take place while holding this lock */
    QMutex audio_buflock;

    /** must hold avsync_lock to read or write 'audiotime' and
        'audiotime_updated' */
    QMutex avsync_lock;

    // timecode of audio leaving the soundcard (same units as timecodes)
    long long audiotime;

    /* Audio circular buffer */
    int raud, waud;     /* read and write positions */
    // timecode of audio most recently placed into buffer
    long long audbuf_timecode;

    QMutex killAudioLock;

    long current_seconds;
    long source_bitrate;

    float *src_in;

    // All actual buffers
    SRC_DATA src_data;
    uint memory_corruption_test0;
    float src_in_buf[kAudioSRCInputSize + 16];
    uint memory_corruption_test1;
    float src_out[kAudioSRCOutputSize];
    uint memory_corruption_test2;
    /** main audio buffer */
    uchar audiobuffer[kAudioRingBufferSize];
    uint memory_corruption_test3;
};

#endif

