#ifndef AUDIOOUTPUTDX
#define AUDIOOUTPUTDX

// MythTV headers
#include "audiooutputbase.h"

class AudioOutputDXPrivate;

class AudioOutputDX : public AudioOutputBase
{
  friend class AudioOutputDXPrivate;
  public:
    AudioOutputDX(const AudioSettings &settings);
    virtual ~AudioOutputDX();

    virtual int GetVolumeChannel(int channel) const;
    virtual void SetVolumeChannel(int channel, int volume);

  protected:
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *buffer, int size);
    virtual void Pause(bool pause);
    virtual int GetSpaceOnSoundcard(void) const;
    virtual int GetBufferedOnSoundcard(void) const;    

  protected:
    AudioOutputDXPrivate    *m_priv;
    bool                     m_UseSPDIF;
    static const uint        kFramesNum;
};

#endif // AUDIOOUTPUTDX
