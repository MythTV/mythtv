#include <cstdio>
#include <cstdlib>

using namespace std;

#include "mythconfig.h"
#include "audiooutput.h"
#include "compat.h"

#include "audiooutputnull.h"
#ifdef USING_MINGW
#include "audiooutputdx.h"
#include "audiooutputwin.h"
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
#if CONFIG_DARWIN
#include "audiooutputca.h"
#endif
#ifdef USE_JACK
#include "audiooutputjack.h"
#endif

AudioOutput *AudioOutput::OpenAudio(
    const QString &main_device,
    const QString &passthru_device,
    int audio_bits, int audio_channels, int audio_samplerate,
    AudioOutputSource source,
    bool set_initial_vol, bool audio_passthru)
{
    AudioSettings settings(
        main_device, passthru_device, audio_bits,
        audio_channels, audio_samplerate, source,
        set_initial_vol, audio_passthru);

    settings.FixPassThrough();

    if (main_device.startsWith("ALSA:"))
    {
#ifdef USE_ALSA
        settings.TrimDeviceType();
        return new AudioOutputALSA(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to an ALSA device "
                              "but ALSA support is not compiled in!");
        return NULL;
#endif
    }
    else if (main_device.startsWith("NULL"))
    {
        return new AudioOutputNULL(settings);
    }
    else if (main_device.startsWith("ARTS:"))
    {
#ifdef USE_ARTS
        settings.TrimDeviceType();
        return new AudioOutputARTS(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to an ARTS device "
                              "but ARTS support is not compiled in!");
        return NULL;
#endif
    }
    else if (main_device.startsWith("JACK:"))
    {
#ifdef USE_JACK
        settings.TrimDeviceType();
        return new AudioOutputJACK(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to a JACK device "
                              "but JACK support is not compiled in!");
        return NULL;
#endif
    }
    else if (main_device.startsWith("DirectX:"))
    {
#ifdef USING_MINGW
        return new AudioOutputDX(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to DirectX device "
                              "but DirectX support is not compiled in!");
        return NULL;
#endif
    }
    else if (main_device.startsWith("Windows:"))
    {
#ifdef USING_MINGW
        return new AudioOutputWin(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to a Windows device "
                              "but Windows support is not compiled in!");
        return NULL;
#endif
    }
#if defined(USING_OSS)
    else
        return new AudioOutputOSS(settings);
#elif CONFIG_DARWIN
    else
        return new AudioOutputCA(settings);
#endif

    VERBOSE(VB_IMPORTANT, "No useable audio output driver found.");
    VERBOSE(VB_IMPORTANT, "Don't disable OSS support unless you're "
                          "not running on Linux.");

    return NULL;
}

void AudioOutput::SetStretchFactor(float /*factor*/)
{
}

void AudioOutput::Error(const QString &msg)
{
    lastError = msg;
    lastError.detach();
    VERBOSE(VB_IMPORTANT, "AudioOutput Error: " + lastError);
}

void AudioOutput::Warn(const QString &msg)
{
    lastWarn = msg;
    lastWarn.detach();
    VERBOSE(VB_IMPORTANT, "AudioOutput Warning: " + lastWarn);
}

void AudioOutput::ClearError(void)
{
    lastError = QString::null;
}

void AudioOutput::ClearWarning(void)
{
    lastWarn = QString::null;
}

