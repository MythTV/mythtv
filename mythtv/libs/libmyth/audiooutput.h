#ifndef AUDIOOUTPUT
#define AUDIOOUTPUT

#include <QString>

#include "audiosettings.h"
#include "mythcorecontext.h"
#include "volumebase.h"
#include "output.h"

class MPUBLIC AudioOutput : public VolumeBase, public OutputListeners
{
 public:
    static void AudioSetup(int &max_channels, bool &allow_ac3_passthru,
                           bool &allow_dts_passthru, bool &allow_multipcm,
                           bool &upmix_default);

    // opens one of the concrete subclasses
    static AudioOutput *OpenAudio(
        const QString &audiodevice, const QString &passthrudevice,
        AudioFormat format, int channels, int codec, int samplerate,
        AudioOutputSource source, bool set_initial_vol, bool passthru,
        int upmixer_startup = 0);

    AudioOutput() :
        VolumeBase(),             OutputListeners(),
        lastError(QString::null), lastWarn(QString::null) {}

    virtual ~AudioOutput() { };

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings) = 0;
    
    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const { return 1.0f; }

    virtual bool CanPassthrough(bool willreencode) const { return false; }

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate) = 0;

    virtual void Reset(void) = 0;

    virtual bool AddFrames(void *buffer, int samples, long long timecode) = 0;

    virtual void SetTimecode(long long timecode) = 0;
    virtual bool IsPaused(void) const = 0;
    virtual void Pause(bool paused) = 0;
    virtual void PauseUntilBuffered(void) = 0;
 
    // Wait for all data to finish playing
    virtual void Drain(void) = 0;

    virtual int GetAudiotime(void) = 0;

    /// report amount of audio buffered in milliseconds.
    virtual int GetAudioBufferedTime(void) { return 0; }

    virtual void SetSourceBitrate(int ) { }

    QString GetError(void)   const { return lastError; }
    QString GetWarning(void) const { return lastWarn; }

    virtual void GetBufferStatus(uint &fill, uint &total)
        { fill = total = 0; }

    //  Only really used by the AudioOutputNULL object
    virtual void bufferOutputData(bool y) = 0;
    virtual int readOutputData(unsigned char *read_buffer, int max_length) = 0;

    virtual bool ToggleUpmix(void) = 0;

  protected:
    void Error(const QString &msg);
    void Warn(const QString &msg);
    void ClearError(void);
    void ClearWarning(void);

  protected:
    QString lastError;
    QString lastWarn;
};

#endif

