#ifndef AUDIOOUTPUTARTS
#define AUDIOOUTPUTARTS

#include <qstring.h>

#define ARTS_PCM_NEW_HW_PARAMS_API
#define ARTS_PCM_NEW_SW_PARAMS_API
#include <artsc/artsc.h>

using namespace std;

#include "audiooutputbase.h"

class AudioOutputARTS : public AudioOutputBase
{
  public:
     AudioOutputARTS(QString audiodevice, int audio_bits, 
                     int audio_channels, int audio_samplerate,
                     AudioOutputSource source, bool set_initial_vol);
     virtual ~AudioOutputARTS();

    // Volume control
    virtual int GetVolumeChannel(int channel); // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume); // range 0-100 for vol

  protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual inline int getSpaceOnSoundcard(void);
    virtual inline int getBufferedOnSoundcard(void);

     

  private:
    QString audiodevice;
    arts_stream_t pcm_handle;
//    int audbuf_timecode;    /* timecode of audio most recently placed into
//                   buffer */
    int buff_size;
    bool can_hw_pause;
    bool paused;
};

#endif // AUDIOOUTPUTARTS
