#ifndef AUDIOOUTPUTBASE
#define AUDIOOUTPUTBASE

// POSIX headers
#include <sys/time.h> // for struct timeval

// Qt headers
#include <QString>
#include <QMutex>
#include <QWaitCondition>

// MythTV headers
#include "audiooutput.h"
#include "mythlogging.h"
#include "mthread.h"

#include "samplerate.h"

#define VBAUDIO(str)   LOG(VB_AUDIO, LOG_INFO, LOC + (str))
#define VBAUDIOTS(str) LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + (str))
#define VBGENERAL(str) LOG(VB_GENERAL, LOG_INFO, LOC + (str))
#define VBERROR(str)   LOG(VB_GENERAL, LOG_ERR, LOC + (str))
#define VBWARN(str)    LOG(VB_GENERAL, LOG_WARNING, LOC + (str))
#define VBERRENO(str)  Error(LOC + (str) + ": " + ENO)
#define VBERRNOCONST(str)   LOG(VB_GENERAL, LOG_ERR, LOC + (str) + ": " + ENO)

namespace soundtouch {
class SoundTouch;
};
class FreeSurround;
class AudioOutputDigitalEncoder;
struct AVCodecContext;

class AsyncLooseLock
{
public:
    AsyncLooseLock() = default;
    void Clear() { m_head = m_tail = 0; }
    void Ref() { m_head++; }
    bool TestAndDeref() { bool r; if ((r=(m_head != m_tail))) m_tail++; return r; }
private:
    int m_head {0};
    int m_tail {0};
};

// Forward declaration of SPDIF encoder
class SPDIFEncoder;

class AudioOutputBase : public AudioOutput, public MThread
{
 public:
    static const char *quality_string(int q);
    explicit AudioOutputBase(const AudioSettings &settings);
    ~AudioOutputBase() override;

    AudioOutputSettings* GetOutputSettingsCleaned(bool digital = true) override; // AudioOutput
    AudioOutputSettings* GetOutputSettingsUsers(bool digital = false) override; // AudioOutput

    // reconfigure sound out for new params
    void Reconfigure(const AudioSettings &settings) override; // AudioOutput

    // dsprate is in 100 * samples/second
    void SetEffDsp(int dsprate) override; // AudioOutput

    // timestretch
    void SetStretchFactor(float factor) override; // AudioOutput
    float GetStretchFactor(void) const override; // AudioOutput
    int GetChannels(void) const override { return m_channels; } // AudioOutput
    AudioFormat GetFormat(void) const override { return m_format; }; // AudioOutput
    int GetBytesPerFrame(void) const override // AudioOutput
        { return m_source_bytes_per_frame; }

    bool CanPassthrough(int samplerate, int channels,
                        AVCodecID codec, int profile) const override; // AudioOutput
    bool CanDownmix(void) const override { return true; } // AudioOutput
    bool IsUpmixing(void) override; // AudioOutput
    bool ToggleUpmix(void) override; // AudioOutput
    bool CanUpmix(void) override; // AudioOutput
    bool CanProcess(AudioFormat /*fmt*/) override { return true; } // AudioOutput
    uint32_t CanProcess(void) override // AudioOutput
    {
        // we support all codec
        return ~(((~0ULL) >> FORMAT_FLT) << FORMAT_FLT);
    }

    void Reset(void) override; // AudioOutput

    void SetSWVolume(int new_volume, bool save) override; // VolumeBase
    int GetSWVolume(void) override; // VolumeBase

    // timecode is in milliseconds.
    bool AddFrames(void *buffer, int frames, int64_t timecode) override; // AudioOutput
    bool AddData(void *buffer, int len, int64_t timecode, int frames) override; // AudioOutput
    bool NeedDecodingBeforePassthrough() const override { return false; }; // AudioOutput
    int64_t LengthLastData(void) const override { return m_length_last_data; } // AudioOutput

    void SetTimecode(int64_t timecode) override; // AudioOutput
    bool IsPaused(void) const override { return m_actually_paused; } // AudioOutput
    void Pause(bool paused) override; // AudioOutput
    void PauseUntilBuffered(void) override; // AudioOutput

    // Wait for all data to finish playing
    void Drain(void) override; // AudioOutput

    int64_t GetAudiotime(void) override; // AudioOutput
    int64_t GetAudioBufferedTime(void) override; // AudioOutput

    // Send output events showing current progress
    virtual void Status(void);

    void SetSourceBitrate(int rate) override; // AudioOutput

    void GetBufferStatus(uint &fill, uint &total) override; // AudioOutput

    //  Only really used by the AudioOutputNULL object
    void bufferOutputData(bool y) override // AudioOutput
        { m_buffer_output_data_for_use = y; }
    int readOutputData(unsigned char *read_buffer, int max_length) override; // AudioOutput

    static const uint kAudioSRCInputSize = 16384;

    /// Audio Buffer Size -- should be divisible by 32,24,16,12,10,8,6,4,2..
    // In other words, divisible by 96.
    static const uint kAudioRingBufferSize   = 10239936U;

 protected:
    // Following function must be called from subclass constructor
    void InitSettings(const AudioSettings &settings);

    // You need to implement the following functions
    virtual bool OpenDevice(void) = 0;
    virtual void CloseDevice(void) = 0;
    virtual void WriteAudio(unsigned char *aubuf, int size) = 0;
    /**
     * Return the size in bytes of frames currently in the audio buffer adjusted
     * with the audio playback latency
     */
    virtual int  GetBufferedOnSoundcard(void) const = 0;
    // Default implementation only supports 2ch s16le at 48kHz
    virtual AudioOutputSettings* GetOutputSettings(bool /*digital*/)
        { return new AudioOutputSettings; }
    // You need to call this from any implementation in the dtor.
    void KillAudio(void);

    // The following functions may be overridden, but don't need to be
    virtual bool StartOutputThread(void);
    virtual void StopOutputThread(void);

    int GetAudioData(uchar *buffer, int buf_size, bool full_buffer,
                     volatile uint *local_raud = nullptr);

    void OutputAudioLoop(void);

    void run() override; // MThread

    int CheckFreeSpace(int &frames);

    inline int audiolen() const; // number of valid bytes in audio buffer
    int audiofree() const;       // number of free bytes in audio buffer
    int audioready() const;      // number of bytes ready to be written

    void SetStretchFactorLocked(float factor);

    // For audiooutputca
    int GetBaseAudBufTimeCode() const { return m_audbuf_timecode; }

    bool usesSpdif() const { return m_usesSpdif; }

  protected:
    // Basic details about the audio stream
    int               m_channels                   {-1};
    AVCodecID         m_codec                      {AV_CODEC_ID_NONE};
    int               m_bytes_per_frame            {0};
    int               m_output_bytes_per_frame     {0};
    AudioFormat       m_format                     {FORMAT_NONE};
    AudioFormat       m_output_format              {FORMAT_NONE};
    int               m_samplerate                 {-1};
    //int             m_bitrate;
    int               m_effdsp                     {0}; // from the recorded stream (NuppelVideo)
    int               m_fragment_size              {0};
    long              m_soundcard_buffer_size      {0};

    QString           m_main_device;
    QString           m_passthru_device;
    bool              m_discretedigital            {false};

    bool              m_passthru                   {false};
    bool              m_enc                        {false};
    bool              m_reenc                      {false};

    float             m_stretchfactor              {1.0F};
    int               m_eff_stretchfactor          {100000}; // scaled to 100000 ase ffdsp is
    AudioOutputSource source;

    bool              m_killaudio                  {false};

    bool              m_pauseaudio                 {false};
    bool              m_actually_paused            {false};
    bool              m_was_paused                 {false};
    bool              m_unpause_when_ready         {false};
    bool              m_set_initial_vol;
    bool              m_buffer_output_data_for_use {false}; //  used by AudioOutputNULL

    int               m_configured_channels        {0};
    int               m_max_channels               {0};
    enum
    {
        QUALITY_DISABLED = -1,
        QUALITY_LOW      =  0,
        QUALITY_MEDIUM   =  1,
        QUALITY_HIGH     =  2,
    };
    int  m_src_quality                             {QUALITY_MEDIUM};
    long m_source_bitrate                          {-1};
    int  m_source_samplerate                       {0};

 private:
    bool SetupPassthrough(AVCodecID codec, int codec_profile,
                          int &samplerate_tmp, int &channels_tmp);
    AudioOutputSettings* OutputSettings(bool digital = true);
    int CopyWithUpmix(char *buffer, int frames, uint &org_waud);
    void SetAudiotime(int frames, int64_t timecode);
    AudioOutputSettings       *m_output_settingsraw        {nullptr};
    AudioOutputSettings       *m_output_settings           {nullptr};
    AudioOutputSettings       *m_output_settingsdigitalraw {nullptr};
    AudioOutputSettings       *m_output_settingsdigital    {nullptr};
    bool                       m_need_resampler            {false};
    SRC_STATE                 *m_src_ctx                   {nullptr};
    soundtouch::SoundTouch    *m_pSoundStretch             {nullptr};
    AudioOutputDigitalEncoder *m_encoder                   {nullptr};
    FreeSurround              *m_upmixer                   {nullptr};

    int               m_source_channels           {-1};
    int               m_source_bytes_per_frame    {0};
    bool              m_upmix_default             {false};
    bool              m_needs_upmix               {false};
    bool              m_needs_downmix             {false};
    int               m_surround_mode             {QUALITY_LOW};
    float             m_old_stretchfactor         {1.0F};
    int               m_volume                    {80};
    QString           m_volumeControl;

    bool              m_processing                {false};

    int64_t           m_frames_buffered           {0};

    bool              m_audio_thread_exists       {false};

    /**
     *  Writes to the audiobuffer, reconfigures and audiobuffer resets can only
     *  take place while holding this lock
     */
    QMutex            m_audio_buflock;

    /**
     *  must hold avsync_lock to read or write 'audiotime' and
     *  'audiotime_updated'
     */
    QMutex            m_avsync_lock;

    /**
     * timecode of audio leaving the soundcard (same units as timecodes)
     */
    int64_t           m_audiotime                         {0};

    /**
     * Audio circular buffer
     */
    volatile uint     m_raud                              {0}; // read position
    volatile uint     m_waud                              {0}; // write position
    /**
     * timecode of audio most recently placed into buffer
     */
    int64_t           m_audbuf_timecode                   {0};
    AsyncLooseLock    m_reset_active;

    QMutex            m_killAudioLock                     {QMutex::NonRecursive};

    long              m_current_seconds                   {-1};

    float            *m_src_in;

    // All actual buffers
    SRC_DATA          m_src_data                          {};
    uint              m_memory_corruption_test0           {0xdeadbeef};
    float             m_src_in_buf[kAudioSRCInputSize + 16] {};
    uint              m_memory_corruption_test1           {0xdeadbeef};;
    float            *m_src_out                           {nullptr};
    int               m_kAudioSRCOutputSize               {0};
    uint              m_memory_corruption_test2           {0xdeadbeef};;
    /**
     * main audio buffer
     */
    uchar             m_audiobuffer[kAudioRingBufferSize] {0};
    uint              m_memory_corruption_test3           {0xdeadbeef};;
    bool              m_configure_succeeded               {false};
    int64_t           m_length_last_data                  {0};

    // SPDIF Encoder for digital passthrough
    bool              m_usesSpdif                         {true};
    SPDIFEncoder     *m_spdifenc                          {nullptr};

    // Flag indicating if SetStretchFactor enabled audio float processing
    bool              m_forcedprocessing                  {false};
    int               m_previousbpf                       {0};
};

#endif
