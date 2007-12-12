#ifndef AUDIOOUTPUTWIN
#define AUDIOOUTPUTWIN

// Qt headers
#include <qstring.h>

// MythTV headers
#include "audiooutputbase.h"

class AudioOutputWinPrivate;

class AudioOutputWin : public AudioOutputBase
{
  friend class AudioOutputWinPrivate;
  public:
    AudioOutputWin(QString laudio_main_device,
                   QString laudio_passthru_device,
                   int laudio_bits,
                   int laudio_channels, int laudio_samplerate,
                   AudioOutputSource lsource,
                   bool lset_initial_vol, bool laudio_passthru);
    virtual ~AudioOutputWin();

    // Volume control
    virtual int  GetVolumeChannel(int channel);
    virtual void SetVolumeChannel(int channel, int volume);

  protected:
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual inline int getSpaceOnSoundcard(void);
    virtual inline int getBufferedOnSoundcard(void);

  protected:
    AudioOutputWinPrivate *m_priv;
    long                   m_nPkts;
    int                    m_CurrentPkt;
    unsigned char        **m_OutPkts;

    static const uint      kPacketCnt;
};

#endif // AUDIOOUTPUTWIN
