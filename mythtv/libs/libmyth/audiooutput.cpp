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
#if CONFIG_DARWIN
#include "audiooutputca.h"
#endif
#ifdef USE_JACK
#include "audiooutputjack.h"
#endif
#ifdef USING_PULSEOUTPUT
#include "audiooutputpulse.h"
#endif
#ifdef USING_PULSE
#include "audiopulseutil.h"
#endif

AudioOutput *AudioOutput::OpenAudio(
    const QString &main_device, const QString &passthru_device,
    AudioFormat format, int channels, int codec, int samplerate,
    AudioOutputSource source, bool set_initial_vol, bool passthru,
    int upmixer_startup)
{
    AudioOutput *ret = NULL;
#ifdef USING_PULSE
    bool pulsestatus = false;
#endif

    AudioSettings settings(
        main_device, passthru_device, format, channels, codec, samplerate,
        source, set_initial_vol, passthru, upmixer_startup);

    settings.FixPassThrough();

    if (main_device.startsWith("PulseAudio:"))
    {
#ifdef USING_PULSEOUTPUT
        return new AudioOutputPulseAudio(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to PulseAudio "
                "but PulseAudio support is not compiled in!");
        return NULL;
#endif
    }

#ifdef USING_PULSE
    if (!main_device.startsWith("ALSA:pulse") &&
        pulseaudio_handle_startup() > 0)
    {
        pulsestatus = true;
    }
#endif

    if (main_device.startsWith("ALSA:"))
    {
#ifdef USE_ALSA
        settings.TrimDeviceType();
        ret = new AudioOutputALSA(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to an ALSA device "
                              "but ALSA support is not compiled in!");
        return NULL;
#endif
    }
    else if (main_device.startsWith("NULL"))
    {
        ret = new AudioOutputNULL(settings);
    }
    else if (main_device.startsWith("JACK:"))
    {
#ifdef USE_JACK
        settings.TrimDeviceType();
        ret = new AudioOutputJACK(settings);
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
        ret = new AudioOutputWin(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to a Windows device "
                              "but Windows support is not compiled in!");
        return NULL;
#endif
    }
#if defined(USING_OSS)
    else
        ret = new AudioOutputOSS(settings);
#elif CONFIG_DARWIN
    else
        ret = new AudioOutputCA(settings);
#endif
    
    if (!ret)
    {
        VERBOSE(VB_IMPORTANT, "No useable audio output driver found.");
        VERBOSE(VB_IMPORTANT, "Don't disable OSS support unless you're "
                              "not running on Linux.");        
        return NULL;
    }
#ifdef USING_PULSE
    ret->pulsewassuspended = pulsestatus;
#endif
    return ret;
}

AudioOutput::~AudioOutput()
{
#ifdef USING_PULSE
    if (pulsewassuspended)
        pulseaudio_handle_teardown();
#endif
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

void AudioOutput::AudioSetup(int &max_channels, bool &allow_ac3_passthru,
                             bool &allow_dts_passthru, bool &allow_multipcm,
                             bool &upmix_default)
{
    max_channels = gCoreContext->GetNumSetting("MaxChannels", 2);

    /* AC-3, upmix and PCM options are not shown if max_channels set to 2 in settings
     screen, force decode */
    allow_ac3_passthru = max_channels > 2 ?
                            gCoreContext->GetNumSetting("AC3PassThru", false) :
                            false;
    
    allow_dts_passthru = max_channels > 2 ?
                            gCoreContext->GetNumSetting("DTSPassThru", false) :
                            false;

    /* If PCM/Analog is unchecked, we then only supports stereo output
     unless AC3 passthrough is also enabled */
    allow_multipcm = max_channels > 2 ?
                            gCoreContext->GetNumSetting("MultiChannelPCM", false) :
                            false;

    if (!allow_multipcm && max_channels > 2)
    {
        if (!allow_ac3_passthru)
            max_channels = 2;
        else if (max_channels > 6)
            max_channels = 6;   // Maximum 5.1 channels with AC3
    }

    upmix_default = max_channels > 2 ?
                            gCoreContext->GetNumSetting("AudioDefaultUpmix", false) :
                            false;
}
