#ifndef TTS_H_
#define TTS_H_

#include "wavfile.h"


class tts
{
  public:
    tts();
    ~tts();
    void setVoice(const char *voice);
    void say(const char *sentence);
    void toWavFile(const char *sentence, wavfile &outWave);

};


#endif
