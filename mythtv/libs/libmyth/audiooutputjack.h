#ifndef AUDIOOUTPUTJACK
#define AUDIOOUTPUTJACK

#include <vector>
#include <qstring.h>
#include <qmutex.h>

#include "audiooutputbase.h"

using namespace std;

class AudioOutputJACK : public AudioOutputBase
{
  public:
    AudioOutputJACK(QString audiodevice, int laudio_bits, 
                   int laudio_channels, int laudio_samplerate);
    virtual ~AudioOutputJACK();
    
  protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual inline int getSpaceOnSoundcard(void);
    virtual inline int getBufferedOnSoundcard(void);

  private:

    int audioid;

};

#endif

