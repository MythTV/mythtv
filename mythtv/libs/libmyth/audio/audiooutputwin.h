#ifndef AUDIOOUTPUTWIN
#define AUDIOOUTPUTWIN

// MythTV headers
#include "audiooutputbase.h"

class AudioOutputWinPrivate;

class AudioOutputWin : public AudioOutputBase
{
  friend class AudioOutputWinPrivate;
  public:
    explicit AudioOutputWin(const AudioSettings &settings);
    virtual ~AudioOutputWin();

    // Volume control
    int  GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase

  protected:
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase

  protected:
    AudioOutputWinPrivate *m_priv       {nullptr};
    long                   m_nPkts      {0};
    uint                   m_CurrentPkt {0};
    unsigned char        **m_OutPkts    {nullptr};
    bool                   m_UseSPDIF;

    static const uint      kPacketCnt;
};

#endif // AUDIOOUTPUTWIN
