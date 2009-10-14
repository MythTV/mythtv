#ifndef AUDIOOUTPUTJACK
#define AUDIOOUTPUTJACK

#include "audiooutputbase.h"

using namespace std;

class AudioOutputJACK : public AudioOutputBase
{
  public:
    AudioOutputJACK(const AudioSettings &settings);
    virtual ~AudioOutputJACK();
   
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
    vector<int> GetSupportedRates(void); 

  private:

    void VolumeInit(void);

    int audioid;

};

#endif

