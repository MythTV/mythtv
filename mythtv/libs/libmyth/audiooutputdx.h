#ifndef AUDIOOUTPUTDX
#define AUDIOOUTPUTDX

// MythTV headers
#include "audiooutputbase.h"
#include "audiooutputsettings.h"

class AudioOutputDXPrivate;

class AudioOutputDX : public AudioOutputBase
{
  friend class AudioOutputDXPrivate;
  public:
    AudioOutputDX(const AudioSettings &settings);
    virtual ~AudioOutputDX();

    virtual int  GetVolumeChannel(int channel) const;
    virtual void SetVolumeChannel(int channel, int volume);

  protected:
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *buffer, int size);
    virtual int  GetBufferedOnSoundcard(void) const;
    AudioOutputSettings* GetOutputSettings(void);

  protected:
    AudioOutputDXPrivate *m_priv;
    bool                  m_UseSPDIF;
};

#endif // AUDIOOUTPUTDX
