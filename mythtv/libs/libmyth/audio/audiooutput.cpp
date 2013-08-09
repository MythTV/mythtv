#include <cstdio>
#include <cstdlib>

using namespace std;

// Qt utils: to parse audio list
#include <QFile>
#include <QDateTime>
#include <QDir>

#include "mythconfig.h"
#include "audiooutput.h"
#include "mythmiscutil.h"
#include "compat.h"

#include "audiooutputnull.h"
#ifdef USING_MINGW
#include "audiooutputdx.h"
#include "audiooutputwin.h"
#endif
#ifdef USING_OSS
#include "audiooutputoss.h"
#endif
#ifdef USING_ALSA
#include "audiooutputalsa.h"
#endif
#if CONFIG_DARWIN
#include "audiooutputca.h"
#endif
#ifdef USING_JACK
#include "audiooutputjack.h"
#endif
#ifdef USING_PULSEOUTPUT
#include "audiooutputpulse.h"
#endif
#ifdef USING_PULSE
#include "audiopulsehandler.h"
#endif

extern "C" {
#include "libavcodec/avcodec.h"  // to get codec id
}
#include "audioconvert.h"

#define LOC QString("AO: ")

void AudioOutput::Cleanup(void)
{
#ifdef USING_PULSE
    PulseHandler::Suspend(PulseHandler::kPulseCleanup);
#endif
}

AudioOutput *AudioOutput::OpenAudio(
    const QString &main_device, const QString &passthru_device,
    AudioFormat format, int channels, int codec, int samplerate,
    AudioOutputSource source, bool set_initial_vol, bool passthru,
    int upmixer_startup, AudioOutputSettings *custom)
{
    AudioSettings settings(
        main_device, passthru_device, format, channels, codec, samplerate,
        source, set_initial_vol, passthru, upmixer_startup, custom);

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
#else
    {
        static bool warned = false;
        if (!warned && IsPulseAudioRunning())
        {
            warned = true;
            LOG(VB_GENERAL, LOG_WARNING,
                "WARNING: ***Pulse Audio is running***");
        }
    }
#endif

    settings.FixPassThrough();

    if (main_device.startsWith("PulseAudio:"))
    {
#ifdef USING_PULSEOUTPUT
        return new AudioOutputPulseAudio(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to PulseAudio "
                                 "but PulseAudio support is not compiled in!");
        return NULL;
#endif
    }
    else if (main_device.startsWith("NULL"))
    {
        return new AudioOutputNULL(settings);
    }

#ifdef USING_PULSE
    if (willsuspendpa)
    {
        bool ispulse = false;
#ifdef USING_ALSA
        // Check if using ALSA, that the device doesn't contain the word
        // "pulse" in its hint
        if (main_device.startsWith("ALSA:"))
        {
            QString device_name = main_device;

            device_name.remove(0, 5);
            QMap<QString, QString> *alsadevs =
                AudioOutputALSA::GetDevices("pcm");
            if (!alsadevs->empty() && alsadevs->contains(device_name))
            {
                if (alsadevs->value(device_name).contains("pulse",
                                                          Qt::CaseInsensitive))
                {
                    ispulse = true;
                }
            }
            delete alsadevs;
        }
#endif
        if (main_device.contains("pulse", Qt::CaseInsensitive))
        {
            ispulse = true;
        }
        if (!ispulse)
        {
            pulsestatus = PulseHandler::Suspend(PulseHandler::kPulseSuspend);
        }
    }
#endif

    if (main_device.startsWith("ALSA:"))
    {
#ifdef USING_ALSA
        settings.TrimDeviceType();
        ret = new AudioOutputALSA(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to an ALSA device "
                                 "but ALSA support is not compiled in!");
#endif
    }
    else if (main_device.startsWith("JACK:"))
    {
#ifdef USING_JACK
        settings.TrimDeviceType();
        ret = new AudioOutputJACK(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to a JACK device "
                                 "but JACK support is not compiled in!");
#endif
    }
    else if (main_device.startsWith("DirectX:"))
    {
#ifdef USING_MINGW
        ret = new AudioOutputDX(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to DirectX device "
                                 "but DirectX support is not compiled in!");
#endif
    }
    else if (main_device.startsWith("Windows:"))
    {
#ifdef USING_MINGW
        ret = new AudioOutputWin(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to a Windows "
                                 "device but Windows support is not compiled "
                                 "in!");
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
        LOG(VB_GENERAL, LOG_CRIT, "No useable audio output driver found.");
        LOG(VB_GENERAL, LOG_ERR, "Don't disable OSS support unless you're "
                                 "not running on Linux.");
#ifdef USING_PULSE
        if (pulsestatus)
            PulseHandler::Suspend(PulseHandler::kPulseResume);
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
        PulseHandler::Suspend(PulseHandler::kPulseResume);
#endif
}

void AudioOutput::SetStretchFactor(float /*factor*/)
{
}

AudioOutputSettings* AudioOutput::GetOutputSettingsCleaned(bool /*digital*/)
{
    return new AudioOutputSettings;
}

AudioOutputSettings* AudioOutput::GetOutputSettingsUsers(bool /*digital*/)
{
    return new AudioOutputSettings;
}

bool AudioOutput::CanPassthrough(int /*samplerate*/,
                                 int /*channels*/,
                                 int /*codec*/,
                                 int /*profile*/) const
{
    return false;
}

// TODO: get rid of this if possible...  need to see what uses GetError() and
//       GetWarning() and why.  These would give more useful logs as macros
void AudioOutput::Error(const QString &msg)
{
    lastError = msg;
    lastError.detach();
    LOG(VB_GENERAL, LOG_ERR, "AudioOutput Error: " + lastError);
}

void AudioOutput::SilentError(const QString &msg)
{
    lastError = msg;
    lastError.detach();
}

void AudioOutput::Warn(const QString &msg)
{
    lastWarn = msg;
    lastWarn.detach();
    LOG(VB_GENERAL, LOG_WARNING, "AudioOutput Warning: " + lastWarn);
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
            QString msg = tr("Invalid or unuseable audio device");
            return new AudioOutput::AudioDeviceConfig(name, msg);
        }
    }

    QString capabilities = desc;
    int max_channels = aosettings.BestSupportedChannelsELD();
    if (aosettings.hasELD())
    {
        if (aosettings.getELD().isValid())
        {
            capabilities += tr(" (%1 connected to %2)")
                .arg(aosettings.getELD().product_name().simplified())
                .arg(aosettings.getELD().connection_name());
        }
        else
        {
            capabilities += tr(" (No connection detected)");
        }
    }

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

    capabilities += tr("\nDevice supports up to %1")
        .arg(speakers);
    if (aosettings.canPassthrough() >= 0)
    {
        if (aosettings.hasELD() && aosettings.getELD().isValid())
        {
                // We have an ELD, show actual reported capabilities
            capabilities += " (" + aosettings.getELD().codecs_desc() + ")";
        }
        else
        {
                // build capabilities string, in a similar fashion as reported
                // by ELD
            int mask = 0;
            mask |=
                (aosettings.canLPCM() << 0) |
                (aosettings.canAC3()  << 1) |
                (aosettings.canDTS()  << 2);
            static const char *type_names[] = { "LPCM", "AC3", "DTS" };

            if (mask != 0)
            {
                capabilities += QObject::tr(" (guessing: ");
                bool found_one = false;
                for (unsigned int i = 0; i < 3; i++)
                {
                    if ((mask & (1 << i)) != 0)
                    {
                        if (found_one)
                            capabilities += ", ";
                        capabilities += type_names[i];
                        found_one = true;
                    }
                }
                capabilities += QString(")");
            }
        }
    }
    LOG(VB_AUDIO, LOG_INFO, QString("Found %1 (%2)")
                                .arg(name).arg(capabilities));
    adc = new AudioOutput::AudioDeviceConfig(name, capabilities);
    adc->settings = aosettings;
    return adc;
}

#ifdef USING_OSS
static void fillSelectionsFromDir(const QDir &dir,
                                  AudioOutput::ADCVect *list)
{
    QFileInfoList il = dir.entryInfoList();
    for (QFileInfoList::Iterator it = il.begin();
         it != il.end(); ++it )
    {
        QFileInfo &fi = *it;
        QString name = fi.absoluteFilePath();
        QString desc = AudioOutput::tr("OSS device");
        AudioOutput::AudioDeviceConfig *adc =
            AudioOutput::GetAudioDeviceConfig(name, desc);
        if (!adc)
            continue;
        list->append(*adc);
        delete adc;
    }
}
#endif

AudioOutput::ADCVect* AudioOutput::GetOutputList(void)
{
    ADCVect *list = new ADCVect;
    AudioDeviceConfig *adc;

#ifdef USING_PULSE
    bool pasuspended = PulseHandler::Suspend(PulseHandler::kPulseSuspend);
#endif

#ifdef USING_ALSA
    QMap<QString, QString> *alsadevs = AudioOutputALSA::GetDevices("pcm");

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
        QString name = "JACK:";
        QString desc = tr("Use JACK default sound server.");
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif
#if CONFIG_DARWIN

    {
        QMap<QString, QString> *devs = AudioOutputCA::GetDevices(NULL);
        if (!devs->empty())
        {
            for (QMap<QString, QString>::const_iterator i = devs->begin();
                 i != devs->end(); ++i)
            {
                QString key = i.key();
                QString desc = i.value();
                QString devname = QString("CoreAudio:%1").arg(key);

                adc = GetAudioDeviceConfig(devname, desc);
                if (!adc)
                    continue;
                list->append(*adc);
                delete adc;
            }
        }
        delete devs;
        QString name = "CoreAudio:Default Output Device";
        QString desc = tr("CoreAudio default output");
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

        QMap<int, QString> *dxdevs = AudioOutputDX::GetDXDevices();

        if (!dxdevs->empty())
        {
            for (QMap<int, QString>::const_iterator i = dxdevs->begin();
                 i != dxdevs->end(); ++i)
            {
                QString desc = i.value();
                QString devname = QString("DirectX:%1").arg(desc);

                adc = GetAudioDeviceConfig(devname, desc);
                if (!adc)
                    continue;
                list->append(*adc);
                delete adc;
            }
        }
        delete dxdevs;
    }
#endif

#ifdef USING_PULSE
    if (pasuspended)
        PulseHandler::Suspend(PulseHandler::kPulseResume);
#endif

#ifdef USING_PULSEOUTPUT
    {
        QString name = "PulseAudio:default";
        QString desc =  tr("PulseAudio default sound server.");
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

/**
 * DecodeAudio
 * Decode an audio packet, and compact it if data is planar
 * Return negative error code if an error occurred during decoding
 * or the number of bytes consumed from the input AVPacket
 * data_size contains the size of decoded data copied into buffer
 */
int AudioOutput::DecodeAudio(AVCodecContext *ctx,
                             uint8_t *buffer, int &data_size,
                             const AVPacket *pkt)
{
    AVFrame frame;
    int got_frame = 0;
    int ret;
    char error[AV_ERROR_MAX_STRING_SIZE];

    data_size = 0;
    avcodec_get_frame_defaults(&frame);
    ret = avcodec_decode_audio4(ctx, &frame, &got_frame, pkt);
    if (ret < 0)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC +
            QString("audio decode error: %1 (%2)")
            .arg(av_make_error_string(error, sizeof(error), ret))
            .arg(got_frame));
        return ret;
    }

    if (!got_frame)
    {
        LOG(VB_AUDIO, LOG_DEBUG, LOC +
            QString("audio decode, no frame decoded (%1)").arg(ret));
        return ret;
    }

    AVSampleFormat format = (AVSampleFormat)frame.format;
    AudioFormat fmt =
        AudioOutputSettings::AVSampleFormatToFormat(format, ctx->bits_per_raw_sample);

    data_size = frame.nb_samples * frame.channels * av_get_bytes_per_sample(format);

    // May need to convert audio to S16
    AudioConvert converter(fmt, CanProcess(fmt) ? fmt : FORMAT_S16);
    uint8_t* src;

    if (av_sample_fmt_is_planar(format))
    {
        src = buffer;
        converter.InterleaveSamples(frame.channels,
                                    src,
                                    (const uint8_t **)frame.extended_data,
                                    data_size);
    }
    else
    {
        // data is already compacted...
        src = frame.extended_data[0];
    }

    uint8_t* transit = buffer;

    if (!CanProcess(fmt) &&
        av_get_bytes_per_sample(ctx->sample_fmt) < AudioOutputSettings::SampleSize(converter.Out()))
    {
        // this conversion can't be done in place
        transit = (uint8_t*)av_malloc(data_size * av_get_bytes_per_sample(ctx->sample_fmt)
                                      / AudioOutputSettings::SampleSize(converter.Out()));
        if (!transit)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC +
                QString("audio decode, out of memory"));
            data_size = 0;
            return ret;
        }
    }
    if (!CanProcess(fmt) || src != transit)
    {
        data_size = converter.Process(transit, src, data_size, true);
    }
    if (transit != buffer)
    {
        av_free(transit);
    }
    return ret;
}
