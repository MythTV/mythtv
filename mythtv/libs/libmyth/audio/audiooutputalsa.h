#ifndef AUDIOOUTPUTALSA
#define AUDIOOUTPUTALSA

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include <QMap>

#include "audiooutputbase.h"

class AudioOutputALSA : public AudioOutputBase
{
  public:
    explicit AudioOutputALSA(const AudioSettings &settings);
    ~AudioOutputALSA() override;

    // Volume control
    int GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase
    static QMap<QString, QString> *GetDevices(const char *type);

  protected:
    // You need to implement the following functions
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool passthrough) override; // AudioOutputBase

  private:
    int TryOpenDevice(int open_mode, bool try_ac3);
    int GetPCMInfo(int &card, int &device, int &subdevice);
    bool IncPreallocBufferSize(int requested, int buffer_time);
    inline int SetParameters(snd_pcm_t *handle, snd_pcm_format_t format,
                             uint channels, uint rate, uint buffer_time,
                             uint period_time);
    static QByteArray *GetELD(int card, int device, int subdevice);
    // Volume related
    bool OpenMixer(void);

  private:
    snd_pcm_t   *m_pcm_handle {nullptr};
    int          m_pbufsize   {-1};
    int          m_card       {-1};
    int          m_device     {-1};
    int          m_subdevice  {-1};
    QMutex       m_killAudioLock;
    QString      m_lastdevice;

    struct {
        QString            device;
        QString            control;
        snd_mixer_t*       handle;
        snd_mixer_elem_t*  elem;
        long               volmin;
        long               volmax;
        long               volrange;
    } m_mixer;

};
#endif
