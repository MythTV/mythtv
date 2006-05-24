#include <qstring.h>
#include <cstdio>
#include <cstdlib>

using namespace std;

#include "config.h"
#include "audiooutput.h"

/* Required to define BOOL first with friendly _WINDEF_H */
#ifdef _WIN32
#include <windows.h>
#endif

#include "audiooutputnull.h"
#ifdef USING_DIRECTX
#include "audiooutputdx.h"
#endif
#ifdef USING_OSS
#include "audiooutputoss.h"
#endif
#ifdef USE_ALSA
#include "audiooutputalsa.h"
#endif
#ifdef USE_ARTS
#include "audiooutputarts.h"
#endif
#ifdef CONFIG_DARWIN
#include "audiooutputca.h"
#endif
#ifdef USE_JACK
#include "audiooutputjack.h"
#endif

AudioOutput *AudioOutput::OpenAudio(QString audiodevice, int audio_bits, 
                                    int audio_channels, int audio_samplerate,
                                    AudioOutputSource source,
                                    bool set_initial_vol, bool audio_passthru)
{
    if (audiodevice.startsWith("ALSA:"))
    {
#ifdef USE_ALSA
        return new AudioOutputALSA(audiodevice.remove(0, 5), audio_bits,
                                   audio_channels, audio_samplerate, source,
                                   set_initial_vol, audio_passthru);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to an ALSA device "
                              "but ALSA support is not compiled in!");
        return NULL;
#endif
    }
    else if (audiodevice.startsWith("NULL"))
    {
        return new AudioOutputNULL(audiodevice, audio_bits,
                                   audio_channels, audio_samplerate, source,
                                   set_initial_vol, audio_passthru);
    }
    else if (audiodevice.startsWith("ARTS:"))
    {
#ifdef USE_ARTS
        return new AudioOutputARTS(audiodevice.remove(0, 5), audio_bits,
                                   audio_channels, audio_samplerate, source,
                                   set_initial_vol, audio_passthru);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to an ARTS device "
                              "but ARTS support is not compiled in!");
        return NULL;
#endif
    }
    else if (audiodevice.startsWith("JACK:"))
    {
#ifdef USE_JACK
        return new AudioOutputJACK(audiodevice.remove(0, 5), audio_bits,
                                   audio_channels, audio_samplerate, source,
                                   set_initial_vol, audio_passthru);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to a JACK device "
                              "but JACK support is not compiled in!");
        return NULL;
#endif
    }
#if defined(USING_DIRECTX)
    else
        return new AudioOutputDX(audiodevice, audio_bits,
                                 audio_channels, audio_samplerate, source,
                                 set_initial_vol, audio_passthru);
#elif defined(USING_OSS)
    else
        return new AudioOutputOSS(audiodevice, audio_bits,
                                  audio_channels, audio_samplerate, source,
                                  set_initial_vol, audio_passthru);
#elif defined(CONFIG_DARWIN)
    else
        return new AudioOutputCA(audiodevice, audio_bits,
                                 audio_channels, audio_samplerate, source,
                                 set_initial_vol, audio_passthru);
#endif

    VERBOSE(VB_IMPORTANT, "No useable audio output driver found.");
    VERBOSE(VB_IMPORTANT, "Don't disable OSS support unless you're "
                          "not running on Linux.");

    return NULL;
}

void AudioOutput::SetStretchFactor(float /*factor*/)
{
}


