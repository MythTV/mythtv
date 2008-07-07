#ifndef AUDIOOUTPUTARTS
#define AUDIOOUTPUTARTS

#define ARTS_PCM_NEW_HW_PARAMS_API
#define ARTS_PCM_NEW_SW_PARAMS_API
#include <artsc.h>

using namespace std;

#include "audiooutputbase.h"

class AudioOutputARTS : public AudioOutputBase
{
  public:
     AudioOutputARTS(const AudioSettings &settings);
     virtual ~AudioOutputARTS();

    // Volume control
    virtual int GetVolumeChannel(int channel) const; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume); // range 0-100 for vol

  protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual int  GetSpaceOnSoundcard(void) const;
    virtual int  GetBufferedOnSoundcard(void) const;

     

  private:
    arts_stream_t pcm_handle;
    int           buff_size;
    bool          can_hw_pause;
};

#endif // AUDIOOUTPUTARTS
