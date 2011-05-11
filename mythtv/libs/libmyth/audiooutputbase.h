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

class AsyncLooseLock
{
public:
    AsyncLooseLock() { head = tail = 0; }
    void Clear() { head = tail = 0; }
    void Ref() { head++; }
    bool TestAndDeref() { bool r; if ((r=(head != tail))) tail++; return r; }
private:
    int head;
    int tail;
};

class AudioOutputBase : public AudioOutput, public QThread
{
 public:
    AudioOutputBase(const AudioSettings &settings);
    virtual ~AudioOutputBase();

    AudioOutputSettings* GetOutputSettingsCleaned(void);
    AudioOutputSettings* GetOutputSettingsUsers(void);

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings);

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate);

    // timestretch
    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const;

    virtual bool CanPassthrough(int samplerate, int channels) const;
    virtual bool CanDownmix(void) const { return true; };
    virtual bool ToggleUpmix(void);

    virtual void Reset(void);

    void SetSWVolume(int new_volume, bool save);
    int GetSWVolume(void);

    // timecode is in milliseconds.
    virtual bool AddFrames(void *buffer, int frames, int64_t timecode);

    virtual void SetTimecode(int64_t timecode);
    virtual bool IsPaused(void) const { return actually_paused; }
    virtual void Pause(bool paused);
    void PauseUntilBuffered(void);

    // Wait for all data to finish playing
    virtual void Drain(void);

    virtual int64_t GetAudiotime(void);
    virtual int64_t GetAudioBufferedTime(void);

    // Send output events showing current progress
    virtual void Status(void);

    virtual void SetSourceBitrate(int rate);

    virtual void GetBufferStatus(uint &fill, uint &total);

    //  Only really used by the AudioOutputNULL object
    virtual void bufferOutputData(bool y){ buffer_output_data_for_use = y; }
    virtual int readOutputData(unsigned char *read_buffer, int max_length);

    static const uint kAudioSRCInputSize = 32768;

    /// Audio Buffer Size -- should be divisible by 32,24,16,12,10,8,6,4,2..
    static const uint kAudioRingBufferSize   = 3072000;

 protected:
    // Following function must be called from subclass constructor
    void InitSettings(const AudioSettings &settings);

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

    int GetAudioData(uchar *buffer, int buf_size, bool fill_buffer, int *local_raud = NULL);

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
    int  eff_stretchfactor;     // scaled to 100000 as effdsp is
    AudioOutputSource source;

    bool killaudio;

    bool pauseaudio, actually_paused, was_paused, unpause_when_ready;
    bool set_initial_vol;
    bool buffer_output_data_for_use; //  used by AudioOutputNULL

    int configured_channels;
    int max_channels;
    int src_quality;

 private:
    int CopyWithUpmix(char *buffer, int frames, int &org_waud);
    void SetAudiotime(int frames, int64_t timecode);
    AudioOutputSettings *output_settingsraw;
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
    bool upmix_default;
    bool needs_downmix;
    int surround_mode;
    float old_stretchfactor;
    int volume;
    QString volumeControl;

    bool processing;

    int64_t frames_buffered;

    bool audio_thread_exists;

    /* Writes to the audiobuffer, reconfigures and audiobuffer resets can only
       take place while holding this lock */
    QMutex audio_buflock;

    /** must hold avsync_lock to read or write 'audiotime' and
        'audiotime_updated' */
    QMutex avsync_lock;

    // timecode of audio leaving the soundcard (same units as timecodes)
    int64_t audiotime;

    /* Audio circular buffer */
    int raud, waud;     /* read and write positions */
    // timecode of audio most recently placed into buffer
    int64_t audbuf_timecode;
    AsyncLooseLock reset_active;

    QMutex killAudioLock;

    long current_seconds;
    long source_bitrate;

    float *src_in;

    // All actual buffers
    SRC_DATA src_data;
    uint memory_corruption_test0;
    float src_in_buf[kAudioSRCInputSize + 16];
    uint memory_corruption_test1;
    float *src_out;
    int kAudioSRCOutputSize;
    uint memory_corruption_test2;
    /** main audio buffer */
    uchar audiobuffer[kAudioRingBufferSize];
    uint memory_corruption_test3;
};

#endif
