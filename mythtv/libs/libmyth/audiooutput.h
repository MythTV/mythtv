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
        int upmixer_startup = 0);
    static AudioOutput *OpenAudio(AudioSettings &settings,
                                  bool willsuspendpa = true);
    static AudioOutput *OpenAudio(
        const QString &main_device,
        const QString &passthru_device = QString::null,
        bool willsuspendpa = true);

    AudioOutput() :
        VolumeBase(),             OutputListeners(),
        lastError(QString::null), lastWarn(QString::null) {}

    virtual ~AudioOutput();

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings) = 0;

    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const { return 1.0f; }

    virtual AudioOutputSettings* GetOutputSettingsCleaned(void)
        { return new AudioOutputSettings; }
    virtual AudioOutputSettings* GetOutputSettingsUsers(void)
        { return new AudioOutputSettings; }
    virtual bool CanPassthrough(int samplerate, int channels) const = 0;
    virtual bool CanDownmix(void) const { return false; };

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate) = 0;

    virtual void Reset(void) = 0;

    virtual bool AddFrames(void *buffer, int samples, int64_t timecode) = 0;

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

    virtual bool ToggleUpmix(void) = 0;
    bool PulseStatus(void) { return pulsewassuspended; }

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
