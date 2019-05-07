#ifndef AUDIOOUTPUTDX
#define AUDIOOUTPUTDX

#include <QMap>

// MythTV headers
#include "audiooutputbase.h"
#include "audiooutputsettings.h"

class AudioOutputDXPrivate;

class AudioOutputDX : public AudioOutputBase
{
  friend class AudioOutputDXPrivate;
  public:
    explicit AudioOutputDX(const AudioSettings &settings);
    virtual ~AudioOutputDX();

    int  GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase
    static QMap<int, QString> *GetDXDevices(void);

  protected:
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *buffer, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool passthrough) override; // AudioOutputBase

  protected:
    AudioOutputDXPrivate *m_priv {nullptr};
    bool                  m_UseSPDIF;
};

#endif // AUDIOOUTPUTDX
