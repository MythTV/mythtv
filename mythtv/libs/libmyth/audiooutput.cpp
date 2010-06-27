#include <cstdio>
#include <cstdlib>

using namespace std;

// Qt utils: to parse audio list
#include <QFile>
#include <QDateTime>
#include <QDir>

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
    AudioSettings settings(
        main_device, passthru_device, format, channels, codec, samplerate,
        source, set_initial_vol, passthru, upmixer_startup);

    return OpenAudio(settings);
}

AudioOutput *AudioOutput::OpenAudio(
    const QString &main_device, const QString &passthru_device,
    bool willsuspendpa)
{
    AudioSettings settings(main_device, passthru_device);

    return OpenAudio(settings, willsuspendpa);
}

AudioOutput *AudioOutput::OpenAudio(AudioSettings &settings,
                                    bool willsuspendpa)
{
    QString &main_device = settings.main_device;
    AudioOutput *ret = NULL;

#ifdef USING_PULSE
    bool pulsestatus = false;
#endif 

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
    else if (main_device.startsWith("NULL"))
    {
        return new AudioOutputNULL(settings);
    }

#ifdef USING_PULSE
    if (willsuspendpa &&
        !main_device.contains("pulse", Qt::CaseInsensitive) &&
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
#endif
    }
    else if (main_device.startsWith("JACK:"))
    {
#ifdef USE_JACK
        settings.TrimDeviceType();
        ret = new AudioOutputJACK(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to a JACK device "
                              "but JACK support is not compiled in!");
#endif
    }
    else if (main_device.startsWith("DirectX:"))
    {
#ifdef USING_MINGW
        ret = new AudioOutputDX(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to DirectX device "
                              "but DirectX support is not compiled in!");
#endif
    }
    else if (main_device.startsWith("Windows:"))
    {
#ifdef USING_MINGW
        ret = new AudioOutputWin(settings);
#else
        VERBOSE(VB_IMPORTANT, "Audio output device is set to a Windows device "
                              "but Windows support is not compiled in!");
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
#ifdef USING_PULSE
        if (pulsestatus)
            pulseaudio_handle_teardown();
#endif
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

AudioOutput::AudioDeviceConfig* AudioOutput::GetAudioDeviceConfig(
    QString &name, QString &desc, bool willsuspendpa)
{
    AudioOutputSettings aosettings;
    AudioOutput::AudioDeviceConfig *adc;

    AudioOutput *ao = OpenAudio(name, QString::null, willsuspendpa);
    aosettings = *(ao->GetOutputSettingsCleaned());
    delete ao;

    if (aosettings.IsInvalid())
    {
        if (!willsuspendpa)
            return NULL;
        else
        {
            QString msg = QObject::tr("Invalid or unuseable audio device");
            return new AudioOutput::AudioDeviceConfig(name, msg);
        }
    }

    int max_channels = aosettings.BestSupportedChannels();
    QString speakers;
    switch (max_channels)
    {
        case 6:
            speakers = "5.1";
            break;
        case 8:
            speakers = "7.1";
            break;
        default:
            speakers = "2.0";
            break;
    }
    QString capabilities = desc + QString("\nDevice supports up to %1")
                                      .arg(speakers);
    if (aosettings.canPassthrough() >= 0)
    {
        capabilities += QString(" (");
        if (aosettings.canPassthrough() > 0)
            capabilities += QString("digital output, ");
        if (aosettings.canAC3())
            capabilities += QString("AC3,");
        if (aosettings.canDTS())
            capabilities += QString("DTS,");
        if (aosettings.canLPCM())
            capabilities += QString("multi-channels LPCM");
        capabilities += QString(")");
    }
    VERBOSE(VB_AUDIO,QString("Found %1 (%2)").arg(name).arg(capabilities));
    adc = new AudioOutput::AudioDeviceConfig(name, capabilities);
    adc->settings = aosettings;
    return adc;
}

static void fillSelectionsFromDir(
    const QDir &dir,
    AudioOutput::ADCVect *list)
{
    QFileInfoList il = dir.entryInfoList();
    for (QFileInfoList::Iterator it = il.begin();
         it != il.end(); ++it )
    {
        QFileInfo &fi = *it;
        QString name = fi.absoluteFilePath();
        QString desc = QString("OSS device");
        AudioOutput::AudioDeviceConfig *adc =
            AudioOutput::GetAudioDeviceConfig(name, desc);
        if (!adc)
            continue;
        list->append(*adc);
        delete adc;
    }
}

AudioOutput::ADCVect* AudioOutput::GetOutputList(void)
{
    ADCVect *list = new ADCVect;
    AudioDeviceConfig *adc;

#ifdef USING_PULSE
    bool pasuspended = (pulseaudio_handle_startup() > 0);
#endif

#ifdef USE_ALSA
    QMap<QString, QString> *alsadevs = AudioOutputALSA::GetALSADevices("pcm");

    if (!alsadevs->empty())
    {
        for (QMap<QString, QString>::const_iterator i = alsadevs->begin();
             i != alsadevs->end(); ++i)
        {
            QString key = i.key();
            QString desc = i.value();
            QString devname = QString("ALSA:%1").arg(key);

            adc = GetAudioDeviceConfig(devname, desc);
            if (!adc)
                continue;
            list->append(*adc);
            delete adc;
        }
    }
    delete alsadevs;
#endif
#ifdef USING_OSS
    {
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev, list);
        dev.setNameFilters(QStringList("adsp*"));
        fillSelectionsFromDir(dev, list);

        dev.setPath("/dev/sound");
        if (dev.exists())
        {
            dev.setNameFilters(QStringList("dsp*"));
            fillSelectionsFromDir(dev, list);
            dev.setNameFilters(QStringList("adsp*"));
            fillSelectionsFromDir(dev, list);
        }
    }
#endif
#ifdef USING_JACK
    {
        QString name = "JACK:output";
        QString desc = "Use JACK default sound server.";
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif
#ifdef USING_COREAUDIO
    {
        QString name = "CoreAudio:";
        QString desc = "CoreAudio output";
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif
#ifdef USING_MINGW
    {
        QString name = "Windows:";
        QString desc = "Windows default output";
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }

        name = "DirectX:Primary Sound Driver";
        desc = "";
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif

#ifdef USING_PULSE
    if (pasuspended)
        pulseaudio_handle_teardown();
#endif

#ifdef USING_PULSEOUTPUT
    {
        QString name = "PulseAudio:default";
        QString desc =  QString("PulseAudio default sound server. ");
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif
    QString name = "NULL";
    QString desc = "NULL device";
    adc = GetAudioDeviceConfig(name, desc);
    if (adc)
    {
        list->append(*adc);
        delete adc;
    }
    return list;
}
