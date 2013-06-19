#ifndef AUDIOOUTPUT
#define AUDIOOUTPUT

#include <QString>
#include <QVector>

#include "compat.h"
#include "audiosettings.h"
#include "audiooutputsettings.h"
#include "mythcorecontext.h"
#include "volumebase.h"
#include "output.h"

// forward declaration
struct AVCodecContext;
struct AVPacket;

class MPUBLIC AudioOutput : public VolumeBase, public OutputListeners
{
 public:
    class AudioDeviceConfig
    {
      public:
        QString name;
        QString desc;
        AudioOutputSettings settings;
        AudioDeviceConfig(void) :
            name(QString()), desc(QString()),
            settings(AudioOutputSettings(true)) { };
        AudioDeviceConfig(const QString &name,
                          const QString &desc) :
            name(name), desc(desc),
            settings(AudioOutputSettings(true)) { };
    };

    typedef QVector<AudioDeviceConfig> ADCVect;

    static void Cleanup(void);
    static ADCVect* GetOutputList(void);
    static AudioDeviceConfig* GetAudioDeviceConfig(
        QString &name, QString &desc, bool willsuspendpa = false);

    // opens one of the concrete subclasses
    static AudioOutput *OpenAudio(
        const QString &audiodevice, const QString &passthrudevice,
        AudioFormat format, int channels, int codec, int samplerate,
        AudioOutputSource source, bool set_initial_vol, bool passthru,
        int upmixer_startup = 0, AudioOutputSettings *custom = NULL);
    static AudioOutput *OpenAudio(AudioSettings &settings,
                                  bool willsuspendpa = true);
    static AudioOutput *OpenAudio(
        const QString &main_device,
        const QString &passthru_device = QString::null,
        bool willsuspendpa = true);

    AudioOutput() :
        VolumeBase(),             OutputListeners(),
        lastError(QString::null), lastWarn(QString::null), pulsewassuspended(false) {}

    virtual ~AudioOutput();

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings) = 0;

    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const { return 1.0f; }
    virtual int GetChannels(void) const { return 2; }
    virtual AudioFormat GetFormat(void) const { return FORMAT_S16; };
    virtual int GetBytesPerFrame(void) const { return 4; };

    virtual AudioOutputSettings* GetOutputSettingsCleaned(bool digital = true);
    virtual AudioOutputSettings* GetOutputSettingsUsers(bool digital = true);
    virtual bool CanPassthrough(int samplerate, int channels,
                                int codec, int profile) const;
    virtual bool CanDownmix(void) const { return false; };

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate) = 0;

    virtual void Reset(void) = 0;

    virtual bool AddFrames(void *buffer, int frames, int64_t timecode) = 0;
        /**
         * AddData:
         * Add data to the audiobuffer for playback
         *
         * in:
         *     buffer  : pointer to audio data
         *     len     : length of audio data added
         *     timecode: timecode of the first sample added
         *     frames  : number of frames added.
         * out:
         *     return false if there wasn't enough space in audio buffer to
         *     process all the data
         */
    virtual bool AddData(void *buffer, int len,
                         int64_t timecode, int frames) = 0;
        /**
         * NeedDecodingBeforePassthrough:
         * returns true if AudioOutput class can determine the length in
         * millisecond of native audio frames bitstreamed passed to AddData.
         * If false, LengthLastData method must be implemented
         */
    virtual bool NeedDecodingBeforePassthrough(void) const { return true; };
        /**
         * LengthLastData:
         * returns the length of the last data added in millisecond.
         * This function must be implemented if NeedDecodingBeforePassthrough
         * returned false
         */
    virtual int64_t LengthLastData(void) const { return -1; };

    virtual void SetTimecode(int64_t timecode) = 0;
    virtual bool IsPaused(void) const = 0;
    virtual void Pause(bool paused) = 0;
    virtual void PauseUntilBuffered(void) = 0;

    // Wait for all data to finish playing
    virtual void Drain(void) = 0;

    virtual int64_t GetAudiotime(void) = 0;

    /// report amount of audio buffered in milliseconds.
    virtual int64_t GetAudioBufferedTime(void) { return 0; }

    virtual void SetSourceBitrate(int ) { }

    QString GetError(void)   const { return lastError; }
    QString GetWarning(void) const { return lastWarn; }

    virtual void GetBufferStatus(uint &fill, uint &total)
        { fill = total = 0; }

    //  Only really used by the AudioOutputNULL object
    virtual void bufferOutputData(bool y) = 0;
    virtual int readOutputData(unsigned char *read_buffer,
                               int max_length) = 0;

    virtual bool IsUpmixing(void)   { return false; }
    virtual bool ToggleUpmix(void)  { return false; }
    virtual bool CanUpmix(void)     { return false; }
    bool PulseStatus(void) { return pulsewassuspended; }
    /**
     * CanProcess
     * argument: AudioFormat
     * return true if class can handle AudioFormat
     * All AudioOutput derivative must be able to handle S16
     */
    virtual bool CanProcess(AudioFormat fmt) { return fmt == FORMAT_S16; }
    /**
     * CanProcess
     * return bitmask of all AudioFormat handled
     * All AudioOutput derivative must be able to handle S16
     */
    virtual uint32_t CanProcess(void) { return 1 << FORMAT_S16; }

    /**
     * DecodeAudio
     * Utility routine.
     * Decode an audio packet, and compact it if data is planar
     * Return negative error code if an error occurred during decoding
     * or the number of bytes consumed from the input AVPacket
     * data_size contains the size of decoded data copied into buffer
     * data decoded will be S16 samples if class instance can't handle HD audio
     * or S16 and above otherwise. No U8 PCM format can be returned
     */
    int DecodeAudio(AVCodecContext *ctx,
                    uint8_t *buffer, int &data_size,
                    const AVPacket *pkt);

  protected:
    void Error(const QString &msg);
    void SilentError(const QString &msg);
    void Warn(const QString &msg);
    void ClearError(void);
    void ClearWarning(void);

    QString lastError;
    QString lastWarn;
    bool pulsewassuspended;
};

#endif
