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

// MythTV headers
#include "audiooutput.h"
#include "samplerate.h"
#include "mythlogging.h"
#include "mthread.h"

#define VBAUDIO(str)   LOG(VB_AUDIO, LOG_INFO, LOC + str)
#define VBAUDIOTS(str) LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + str)
#define VBGENERAL(str) LOG(VB_GENERAL, LOG_INFO, LOC + str)
#define VBERROR(str)   LOG(VB_GENERAL, LOG_ERR, LOC + str)
#define VBWARN(str)    LOG(VB_GENERAL, LOG_WARNING, LOC + str)
#define VBERRENO(str)  Error(LOC + str + ": " + ENO)
#define VBERRNOCONST(str)   LOG(VB_GENERAL, LOG_ERR, LOC + str + ": " + ENO)

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

// Forward declaration of SPDIF encoder
class SPDIFEncoder;

class AudioOutputBase : public AudioOutput, public MThread
{
 public:
    const char *quality_string(int q);
    AudioOutputBase(const AudioSettings &settings);
    virtual ~AudioOutputBase();

    AudioOutputSettings* GetOutputSettingsCleaned(bool digital = true);
    AudioOutputSettings* GetOutputSettingsUsers(bool digital = false);

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings);

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate);

    // timestretch
    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const;
    virtual int GetChannels(void) const { return channels; }
    virtual AudioFormat GetFormat(void) const { return format; };
    virtual int GetBytesPerFrame(void) const { return source_bytes_per_frame; };

    virtual bool CanPassthrough(int samplerate, int channels,
                                int codec, int profile) const;
    virtual bool CanDownmix(void) const { return true; };
    virtual bool IsUpmixing(void);
    virtual bool ToggleUpmix(void);
    virtual bool CanUpmix(void);

    virtual void Reset(void);

    void SetSWVolume(int new_volume, bool save);
    int GetSWVolume(void);

    // timecode is in milliseconds.
    virtual bool AddFrames(void *buffer, int frames, int64_t timecode);
    virtual bool AddData(void *buffer, int len, int64_t timecode, int frames);
    virtual bool NeedDecodingBeforePassthrough() const { return false; };
    virtual int64_t LengthLastData(void) const { return m_length_last_data; }

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

    static const uint kAudioSRCInputSize = 16384;

    /// Audio Buffer Size -- should be divisible by 32,24,16,12,10,8,6,4,2..
    static const uint kAudioRingBufferSize   = 3072000u;

 protected:
    // Following function must be called from subclass constructor
    void InitSettings(const AudioSettings &settings);

    // You need to implement the following functions
    virtual bool OpenDevice(void) = 0;
    virtual void CloseDevice(void) = 0;
    virtual void WriteAudio(uchar *aubuf, int size) = 0;
    /**
     * Return the size in bytes of frames currently in the audio buffer adjusted
     * with the audio playback latency
     */
    virtual int  GetBufferedOnSoundcard(void) const = 0;
    // Default implementation only supports 2ch s16le at 48kHz
    virtual AudioOutputSettings* GetOutputSettings(bool digital)
        { return new AudioOutputSettings; }
    // You need to call this from any implementation in the dtor.
    void KillAudio(void);

    // The following functions may be overridden, but don't need to be
    virtual bool StartOutputThread(void);
    virtual void StopOutputThread(void);

    int GetAudioData(uchar *buffer, int buf_size, bool fill_buffer,
                     volatile uint *local_raud = NULL);

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
    //int bitrate;
    int effdsp; // from the recorded stream (NuppelVideo)
    int fragment_size;
    long soundcard_buffer_size;

    QString main_device, passthru_device;
    bool    m_discretedigital;

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
    enum
    {
        QUALITY_DISABLED = -1,
        QUALITY_LOW      =  0,
        QUALITY_MEDIUM   =  1,
        QUALITY_HIGH     =  2,
    };
    int src_quality;

 private:
    bool SetupPassthrough(int codec, int codec_profile,
                          int &samplerate_tmp, int &channels_tmp);
    AudioOutputSettings* OutputSettings(bool digital = true);
    int CopyWithUpmix(char *buffer, int frames, uint &org_waud);
    void SetAudiotime(int frames, int64_t timecode);
    AudioOutputSettings *output_settingsraw;
    AudioOutputSettings *output_settings;
    AudioOutputSettings *output_settingsdigitalraw;
    AudioOutputSettings *output_settingsdigital;
    bool need_resampler;
    SRC_STATE *src_ctx;
    soundtouch::SoundTouch    *pSoundStretch;
    AudioOutputDigitalEncoder *encoder;
    FreeSurround              *upmixer;

    int source_channels;
    int source_samplerate;
    int source_bytes_per_frame;
    bool upmix_default;
    bool needs_upmix;
    bool needs_downmix;
    int surround_mode;
    float old_stretchfactor;
    int volume;
    QString volumeControl;

    bool processing;

    int64_t frames_buffered;

    bool audio_thread_exists;

    /**
     *  Writes to the audiobuffer, reconfigures and audiobuffer resets can only
     *  take place while holding this lock
     */
    QMutex audio_buflock;

    /**
     *  must hold avsync_lock to read or write 'audiotime' and
     *  'audiotime_updated'
     */
    QMutex avsync_lock;

    /**
     * timecode of audio leaving the soundcard (same units as timecodes)
     */
    int64_t audiotime;

    /**
     * Audio circular buffer
     */
    volatile uint raud, waud;     // read and write positions
    /**
     * timecode of audio most recently placed into buffer
     */
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
    /**
     * main audio buffer
     */
    uchar audiobuffer[kAudioRingBufferSize];
    uint memory_corruption_test3;
    uint m_configure_succeeded;
    int64_t m_length_last_data;

    // SPDIF Encoder for digital passthrough
    SPDIFEncoder     *m_spdifenc;

    // Flag indicating if SetStretchFactor enabled audio float processing
    bool m_forcedprocessing;
    int m_previousbpf;
};

#endif
