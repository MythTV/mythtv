#include <qstring.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
#include "audiooutput.h"
#include "audiooutputoss.h"
#ifdef USE_ALSA
#include "audiooutputalsa.h"
#endif

AudioOutput *AudioOutput::OpenAudio(QString audiodevice, int audio_bits, 
                                    int audio_channels, int audio_samplerate)
{
    if(audiodevice.startsWith("ALSA:"))
    {
#ifdef USE_ALSA
        return new AudioOutputALSA(audiodevice.remove(0, 5), audio_bits,
                                   audio_channels, audio_samplerate);
#else
        printf("Audio output device is set to an ALSA device but ALSA support is not compiled in!\n");
        return NULL;
#endif
    }
    else
        return new AudioOutputOSS(audiodevice, audio_bits,
                                  audio_channels, audio_samplerate);
}
