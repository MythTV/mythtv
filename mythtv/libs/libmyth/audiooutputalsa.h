#ifndef AUDIOOUTPUTALSA
#define AUDIOOUTPUTALSA

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include <QMap>

#include "audiooutputbase.h"

using namespace std;

class ALSAVolumeInfo
{
  public:
    ALSAVolumeInfo(long  playback_vol_min,
                   long  playback_vol_max) :
        range_multiplier(1.0f),
        volume_min(playback_vol_min), volume_max(playback_vol_max)
    {
        float range = (float) (volume_max - volume_min);
        if (range > 0.0f)
            range_multiplier = 100.0f / range;
        range_multiplier_inv = 1.0f / range_multiplier;
    }

    int ToMythRange(long alsa_volume)
    {
        long toz = alsa_volume - volume_min;
        int val = (int) (toz * range_multiplier);
        val = (val < 0)   ? 0   : val;
        val = (val > 100) ? 100 : val;
        return val;
    }

    long ToALSARange(int myth_volume)
    {
        float tos = myth_volume * range_multiplier_inv;
        long val = (long) (tos + volume_min + 0.5);
        val = (val < volume_min) ? volume_min : val;
        val = (val > volume_max) ? volume_max : val;
        return val;
    }

    float range_multiplier;
    float range_multiplier_inv;
    long  volume_min;
    long  volume_max;
};

class AudioOutputALSA : public AudioOutputBase
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
    void SetCurrentVolume(QString control, uint channel, uint volume);
    void OpenMixer(bool setstartingvolume);
    void CloseMixer(void);
    void SetupMixer(void);
    ALSAVolumeInfo GetVolumeRange(snd_mixer_elem_t *elem) const;

  private:
    snd_pcm_t   *pcm_handle;
    int          numbadioctls;
    int          pbufsize;
    int          m_card, m_device, m_subdevice;
    QMutex       killAudioLock;
    snd_mixer_t *mixer_handle;
    QString      mixer_control; // e.g. "PCM"
    snd_pcm_sframes_t (*pcm_write_func)(snd_pcm_t*, const void*,
                                        snd_pcm_uframes_t);
};
#endif

