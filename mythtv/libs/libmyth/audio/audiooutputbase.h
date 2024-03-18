#ifndef AUDIOOUTPUTBASE
#define AUDIOOUTPUTBASE

// POSIX headers
#include <sys/time.h> // for struct timeval

// Qt headers
#include <QString>
#include <QMutex>
#include <QWaitCondition>

// MythTV headers
#include "libmythbase/mthread.h"
#include "libmythbase/mythlogging.h"

#include "audiooutput.h"
#include "samplerate.h"

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define VBAUDIO(str)   LOG(VB_AUDIO, LOG_INFO, LOC + (str))
#define VBAUDIOTS(str) LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + (str))
#define VBGENERAL(str) LOG(VB_GENERAL, LOG_INFO, LOC + (str))
#define VBERROR(str)   LOG(VB_GENERAL, LOG_ERR, LOC + (str))
#define VBWARN(str)    LOG(VB_GENERAL, LOG_WARNING, LOC + (str))
#define VBERRENO(str)  Error(LOC + (str) + ": " + ENO)
#define VBERRNOCONST(str)   LOG(VB_GENERAL, LOG_ERR, LOC + (str) + ": " + ENO)
// NOLINTEND(cppcoreguidelines-macro-usage)

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
    bool TestAndDeref() { bool r = (m_head != m_tail); if (r) m_tail++; return r; }
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
        { return m_sourceBytesPerFrame; }

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
    bool AddFrames(void *buffer, int frames, std::chrono::milliseconds timecode) override; // AudioOutput
    bool AddData(void *buffer, int len, std::chrono::milliseconds timecode, int frames) override; // AudioOutput
    bool NeedDecodingBeforePassthrough() const override { return false; }; // AudioOutput
    std::chrono::milliseconds LengthLastData(void) const override { return m_lengthLastData; } // AudioOutput

    void SetTimecode(std::chrono::milliseconds timecode) override; // AudioOutput
    bool IsPaused(void) const override { return m_actuallyPaused; } // AudioOutput
    void Pause(bool paused) override; // AudioOutput
    void PauseUntilBuffered(void) override; // AudioOutput

    // Wait for all data to finish playing
    void Drain(void) override; // AudioOutput

    std::chrono::milliseconds GetAudiotime(void) override; // AudioOutput
    std::chrono::milliseconds GetAudioBufferedTime(void) override; // AudioOutput

    // Send output events showing current progress
    virtual void Status(void);

    void SetSourceBitrate(int rate) override; // AudioOutput

    void GetBufferStatus(uint &fill, uint &total) override; // AudioOutput

    //  Only really used by the AudioOutputNULL object
    void bufferOutputData(bool y) override // AudioOutput
        { m_bufferOutputDataForUse = y; }
    int readOutputData(unsigned char *read_buffer, size_t max_length) override; // AudioOutput

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
    std::chrono::milliseconds GetBaseAudBufTimeCode() const
        { return m_audbufTimecode; }

    bool usesSpdif() const { return m_usesSpdif; }

  protected:
    // Basic details about the audio stream
    int               m_channels                   {-1};
    AVCodecID         m_codec                      {AV_CODEC_ID_NONE};
    int               m_bytesPerFrame              {0};
    int               m_outputBytesPerFrame        {0};
    AudioFormat       m_format                     {FORMAT_NONE};
    AudioFormat       m_outputFormat               {FORMAT_NONE};
    int               m_sampleRate                 {-1};
    int               m_effDsp                     {0}; // from the recorded stream (NuppelVideo)
    int               m_fragmentSize               {0};
    long              m_soundcardBufferSize        {0};

    QString           m_mainDevice;
    QString           m_passthruDevice;
    bool              m_discreteDigital            {false};

    bool              m_passthru                   {false};
    bool              m_enc                        {false};
    bool              m_reEnc                      {false};

    float             m_stretchFactor              {1.0F};
    int               m_effStretchFactor           {100000}; // scaled to 100000 ase ffdsp is
    AudioOutputSource m_source;

    bool              m_killAudio                  {false};

    bool              m_pauseAudio                 {false};
    bool              m_actuallyPaused             {false};
    bool              m_wasPaused                  {false};
    bool              m_unpauseWhenReady           {false};
    bool              m_setInitialVol;
    bool              m_bufferOutputDataForUse     {false}; //  used by AudioOutputNULL

    int               m_configuredChannels         {0};
    int               m_maxChannels                {0};
    enum : std::int8_t
    {
        QUALITY_DISABLED = -1,
        QUALITY_LOW      =  0,
        QUALITY_MEDIUM   =  1,
        QUALITY_HIGH     =  2,
    };
    int  m_srcQuality                              {QUALITY_MEDIUM};
    long m_sourceBitRate                           {-1};
    int  m_sourceSampleRate                        {0};

 private:
    bool SetupPassthrough(AVCodecID codec, int codec_profile,
                          int &samplerate_tmp, int &channels_tmp);
    AudioOutputSettings* OutputSettings(bool digital = true);
    int CopyWithUpmix(char *buffer, int frames, uint &org_waud);
    void SetAudiotime(int frames, std::chrono::milliseconds timecode);
    AudioOutputSettings       *m_outputSettingsRaw         {nullptr};
    AudioOutputSettings       *m_outputSettings            {nullptr};
    AudioOutputSettings       *m_outputSettingsDigitalRaw  {nullptr};
    AudioOutputSettings       *m_outputSettingsDigital     {nullptr};
    bool                       m_needResampler             {false};
    SRC_STATE                 *m_srcCtx                    {nullptr};
    soundtouch::SoundTouch    *m_pSoundStretch             {nullptr};
    AudioOutputDigitalEncoder *m_encoder                   {nullptr};
    FreeSurround              *m_upmixer                   {nullptr};

    int               m_sourceChannels            {-1};
    int               m_sourceBytesPerFrame       {0};
    bool              m_upmixDefault              {false};
    bool              m_needsUpmix                {false};
    bool              m_needsDownmix              {false};
    int               m_surroundMode              {QUALITY_LOW};
    float             m_oldStretchFactor          {1.0F};
    int               m_volume                    {80};
    QString           m_volumeControl;

    bool              m_processing                {false};

    int64_t           m_framesBuffered            {0};

    bool              m_audioThreadExists         {false};

    /**
     *  Writes to the audiobuffer, reconfigures and audiobuffer resets can only
     *  take place while holding this lock
     */
    QMutex            m_audioBufLock;

    /**
     *  must hold avsync_lock to read or write 'audiotime' and
     *  'audiotime_updated'
     */
    QMutex            m_avsyncLock;

    /**
     * timecode of audio leaving the soundcard (same units as timecodes)
     */
    std::chrono::milliseconds m_audioTime                 {0ms};

    /**
     * Audio circular buffer
     */
    volatile uint     m_raud                              {0}; // read position
    volatile uint     m_waud                              {0}; // write position
    /**
     * timecode of audio most recently placed into buffer
     */
    std::chrono::milliseconds m_audbufTimecode            {0ms};
    AsyncLooseLock    m_resetActive;

    QMutex            m_killAudioLock;

    std::chrono::seconds m_currentSeconds                 {-1s};

    float            *m_srcIn;

    // All actual buffers
    SRC_DATA          m_srcData                           {};
    [[maybe_unused]] uint m_memoryCorruptionTest0         {0xdeadbeef};
    alignas(16) std::array<float,kAudioSRCInputSize> m_srcInBuf {};
    [[maybe_unused]] uint m_memoryCorruptionTest1         {0xdeadbeef};;
    float            *m_srcOut                            {nullptr};
    int               m_kAudioSRCOutputSize               {0};
    [[maybe_unused]] uint m_memoryCorruptionTest2         {0xdeadbeef};;
    /**
     * main audio buffer
     */
    std::array<uchar,kAudioRingBufferSize> m_audioBuffer  {0};
    [[maybe_unused]] uint m_memoryCorruptionTest3         {0xdeadbeef};;
    bool              m_configureSucceeded                {false};
    std::chrono::milliseconds m_lengthLastData            {0ms};

    // SPDIF Encoder for digital passthrough
    bool              m_usesSpdif                         {true};
    SPDIFEncoder     *m_spdifEnc                          {nullptr};

    // Flag indicating if SetStretchFactor enabled audio float processing
    bool              m_forcedProcessing                  {false};
    int               m_previousBpf                       {0};
};

#endif
