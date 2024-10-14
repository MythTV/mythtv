#ifndef AUDIOOUTPUT
#define AUDIOOUTPUT

#include <utility>

// Qt headers
#include <QCoreApplication>
#include <QString>
#include <QVector>

// MythTV headers
#include "libmyth/audio/audiooutputsettings.h"
#include "libmyth/audio/audiosettings.h"
#include "libmyth/audio/volumebase.h"
#include "libmyth/output.h"
#include "libmythbase/compat.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythcorecontext.h"

// forward declaration
struct AVCodecContext;
struct AVPacket;
struct AVFrame;

class MPUBLIC AudioOutput : public VolumeBase, public OutputListeners
{
    Q_DECLARE_TR_FUNCTIONS(AudioOutput);

 public:
    class AudioDeviceConfig
    {
      public:
        QString             m_name;
        QString             m_desc;
        AudioOutputSettings m_settings;
        AudioDeviceConfig(void) :
            m_settings(AudioOutputSettings(true)) { };
        AudioDeviceConfig(QString name,
                          QString desc) :
            m_name(std::move(name)), m_desc(std::move(desc)),
            m_settings(AudioOutputSettings(true)) { };
        AudioDeviceConfig(const AudioDeviceConfig &) = default;
        AudioDeviceConfig(AudioDeviceConfig &&) = default;
        AudioDeviceConfig &operator= (const AudioDeviceConfig &) = default;
        AudioDeviceConfig &operator= (AudioDeviceConfig &&) = default;
    };

    using ADCVect = QVector<AudioDeviceConfig>;

    static void Cleanup(void);
    static ADCVect* GetOutputList(void);
    static AudioDeviceConfig* GetAudioDeviceConfig(
        QString &name, const QString &desc, bool willsuspendpa = false);

    // opens one of the concrete subclasses
    static AudioOutput *OpenAudio(
        const QString &main_device, const QString &passthru_device,
        AudioFormat format, int channels, AVCodecID codec, int samplerate,
        AudioOutputSource source, bool set_initial_vol, bool passthru,
        int upmixer_startup = 0, AudioOutputSettings *custom = nullptr);
    static AudioOutput *OpenAudio(AudioSettings &settings,
                                  bool willsuspendpa = true);
    static AudioOutput *OpenAudio(
        const QString &main_device,
        const QString &passthru_device = QString(),
        bool willsuspendpa = true);

    AudioOutput() = default;
    ~AudioOutput() override;

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings) = 0;

    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const { return 1.0F; }
    virtual int GetChannels(void) const { return 2; }
    virtual AudioFormat GetFormat(void) const { return FORMAT_S16; };
    virtual int GetBytesPerFrame(void) const { return 4; };

    virtual AudioOutputSettings* GetOutputSettingsCleaned(bool digital = true);
    virtual AudioOutputSettings* GetOutputSettingsUsers(bool digital = true);
    virtual bool CanPassthrough(int samplerate, int channels,
                                AVCodecID codec, int profile) const;
    virtual bool CanDownmix(void) const { return false; };

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate) = 0;

    virtual void Reset(void) = 0;

        /**
         * Add frames to the audiobuffer for playback
         *
         * \param[in] buffer pointer to audio data
         * \param[in] frames number of frames added.
         * \param[in] timecode timecode of the first sample added (in msec)
         *
         * \return false if there wasn't enough space in audio buffer to
         *     process all the data
         */
    virtual bool AddFrames(void *buffer, int frames,
                           std::chrono::milliseconds timecode) = 0;

        /**
         * Add data to the audiobuffer for playback
         *
         * \param[in] buffer pointer to audio data
         * \param[in] len length of audio data added
         * \param[in] timecode timecode of the first sample added (in msec)
         * \param[in] frames number of frames added.
         *
         * \return false if there wasn't enough space in audio buffer to
         *     process all the data
         */
    virtual bool AddData(void *buffer, int len,
                         std::chrono::milliseconds timecode, int frames) = 0;

        /**
         * \return true if AudioOutput class can determine the length in
         * millisecond of native audio frames bitstreamed passed to AddData.
         * If false, LengthLastData method must be implemented
         */
    virtual bool NeedDecodingBeforePassthrough(void) const { return true; };

        /**
         * \return the length of the last data added in millisecond.
         * This function must be implemented if NeedDecodingBeforePassthrough
         * returned false
         */
    virtual std::chrono::milliseconds LengthLastData(void) const { return -1ms; };

    virtual void SetTimecode(std::chrono::milliseconds timecode) = 0;
    virtual bool IsPaused(void) const = 0;
    virtual void Pause(bool paused) = 0;
    virtual void PauseUntilBuffered(void) = 0;

    // Wait for all data to finish playing
    virtual void Drain(void) = 0;

    virtual std::chrono::milliseconds GetAudiotime(void) = 0;

    /// report amount of audio buffered in milliseconds.
    virtual std::chrono::milliseconds GetAudioBufferedTime(void) { return 0ms; }

    virtual void SetSourceBitrate(int /*rate*/ ) { }

    QString GetError(void)   const { return m_lastError; }
    QString GetWarning(void) const { return m_lastWarn; }

    virtual void GetBufferStatus(uint &fill, uint &total)
        { fill = total = 0; }

    //  Only really used by the AudioOutputNULL object
    virtual void bufferOutputData(bool y) = 0;
    virtual int readOutputData(unsigned char *read_buffer,
                               size_t max_length) = 0;

    virtual bool IsUpmixing(void)   { return false; }
    virtual bool ToggleUpmix(void)  { return false; }
    virtual bool CanUpmix(void)     { return false; }
    bool PulseStatus(void) const { return m_pulseWasSuspended; }

    /**
     * \param fmt The audio format in question.
     * return true if class can handle AudioFormat
     * All AudioOutput derivative must be able to handle S16
     */
    virtual bool CanProcess(AudioFormat fmt) { return fmt == FORMAT_S16; }

    /**
     * \return bitmask of all AudioFormat handled
     * All AudioOutput derivative must be able to handle S16
     */
    virtual uint32_t CanProcess(void) { return 1 << FORMAT_S16; }

    /**
     * Utility routine.
     * Decode an audio packet, and compact it if data is planar
     * Return negative error code if an error occurred during decoding
     * or the number of bytes consumed from the input AVPacket
     * data_size contains the size of decoded data copied into buffer
     * data decoded will be S16 samples if class instance can't handle HD audio
     * or S16 and above otherwise. No U8 PCM format can be returned
     *
     * \param[in] ctx The current audio context information.
     * \param[in] buffer Destination for the copy
     * \param[out] data_size The number of bytes copied.
     * \param[in] pkt The source data packet
     */
    virtual int DecodeAudio(AVCodecContext *ctx,
                    uint8_t *buffer, int &data_size,
                    const AVPacket *pkt);
    /**
     * kMaxSizeBuffer is the maximum size of a buffer to be used with DecodeAudio
     */
    static const int kMaxSizeBuffer = 384000;

  protected:
    void Error(const QString &msg);
    void SilentError(const QString &msg);
    void Warn(const QString &msg);
    void ClearError(void);
    void ClearWarning(void);

    QString m_lastError;
    QString m_lastWarn;
    bool    m_pulseWasSuspended {false};
    AVFrame *m_frame            {nullptr};
};

#endif
