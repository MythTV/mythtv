#ifndef AUDIOOUTPUTALSA
#define AUDIOOUTPUTALSA

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include <QMap>

#include "audiooutputbase.h"

using namespace std;

class MPUBLIC AudioOutputALSA : public AudioOutputBase
{
  public:
    AudioOutputALSA(const AudioSettings &settings);
    virtual ~AudioOutputALSA();

    // Volume control
    virtual int GetVolumeChannel(int channel) const; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume); // range 0-100 for vol
    static QMap<QString, QString> *GetALSADevices(const char *type);

  protected:
    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(uchar *aubuf, int size);
    virtual int  GetBufferedOnSoundcard(void) const;
    AudioOutputSettings* GetOutputSettings(void);

  private:
    int GetPCMInfo(int &card, int &device, int &subdevice);
    int SetIECStatus(bool audio);
    bool SetPreallocBufferSize(int size);
    bool IncPreallocBufferSize(int buffer_time);
    inline int SetParameters(snd_pcm_t *handle, snd_pcm_format_t format,
                             uint channels, uint rate, uint buffer_time,
                             uint period_time);
    // Volume related
    bool OpenMixer(void);

  private:
    snd_pcm_t   *pcm_handle;
    int          numbadioctls;
    int          pbufsize;
    int          m_card, m_device, m_subdevice;
    QMutex       killAudioLock;
    snd_pcm_sframes_t (*pcm_write_func)(snd_pcm_t*, const void*,
                                        snd_pcm_uframes_t);
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
