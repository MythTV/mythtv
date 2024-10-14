#include <cstdio>
#include <cstdlib>

// Qt utils: to parse audio list
#include <QtGlobal>
#include <QFile>
#include <QDateTime>
#include <QDir>

#include "libmythbase/compat.h"
#include "libmythbase/mythmiscutil.h"

#include "audiooutput.h"
#include "audiooutputnull.h"
#ifdef _WIN32
#include "audiooutputdx.h"
#include "audiooutputwin.h"
#endif
#ifdef USING_OSS
#include "audiooutputoss.h"
#endif
#ifdef USING_ALSA
#include "audiooutputalsa.h"
#endif
#ifdef Q_OS_DARWIN
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
#ifdef Q_OS_ANDROID
#include "audiooutputopensles.h"
#include "audiooutputaudiotrack.h"
#endif

extern "C" {
#include "libavcodec/avcodec.h"  // to get codec id
}
#include "audioconvert.h"
#include "mythaverror.h"

#define LOC QString("AO: ")

void AudioOutput::Cleanup(void)
{
#ifdef USING_PULSE
    PulseHandler::Suspend(PulseHandler::kPulseCleanup);
#endif
}

AudioOutput *AudioOutput::OpenAudio(
    const QString &main_device, const QString &passthru_device,
    AudioFormat format, int channels, AVCodecID codec, int samplerate,
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
                                    [[maybe_unused]] bool willsuspendpa)
{
    QString &main_device = settings.m_mainDevice;
    AudioOutput *ret = nullptr;

    // Don't suspend Pulse if unnecessary.  This can save 100mS
    if (settings.m_format == FORMAT_NONE || settings.m_channels <= 0)
        willsuspendpa = false;

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
#endif // USING_PULSE

    settings.FixPassThrough();

    if (main_device.startsWith("PulseAudio:"))
    {
#ifdef USING_PULSEOUTPUT
        return new AudioOutputPulseAudio(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to PulseAudio "
                                 "but PulseAudio support is not compiled in!");
        return nullptr;
#endif // USING_PULSEOUTPUT
    }
    if (main_device.startsWith("NULL"))
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
#endif // USING_ALSA
        if (main_device.contains("pulse", Qt::CaseInsensitive))
        {
            ispulse = true;
        }
        if (!ispulse)
        {
            pulsestatus = PulseHandler::Suspend(PulseHandler::kPulseSuspend);
        }
    }
#endif // USING_PULSE

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
#ifdef _WIN32
        ret = new AudioOutputDX(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to DirectX device "
                                 "but DirectX support is not compiled in!");
#endif
    }
    else if (main_device.startsWith("Windows:"))
    {
#ifdef _WIN32
        ret = new AudioOutputWin(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to a Windows "
                                 "device but Windows support is not compiled "
                                 "in!");
#endif
    }
    else if (main_device.startsWith("OpenSLES:"))
    {
#ifdef Q_OS_ANDROID
        ret = new AudioOutputOpenSLES(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to a OpenSLES "
                                 "device but Android support is not compiled "
                                 "in!");
#endif
    }
    else if (main_device.startsWith("AudioTrack:"))
    {
#ifdef Q_OS_ANDROID
        ret = new AudioOutputAudioTrack(settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to AudioTrack "
                                 "device but Android support is not compiled "
                                 "in!");
#endif
    }
#if defined(USING_OSS)
    else
    {
        ret = new AudioOutputOSS(settings);
    }
#elif defined(Q_OS_DARWIN)
    else
    {
        ret = new AudioOutputCA(settings);
    }
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
        return nullptr;
    }
#ifdef USING_PULSE
    ret->m_pulseWasSuspended = pulsestatus;
#endif
    return ret;
}

AudioOutput::~AudioOutput()
{
#ifdef USING_PULSE
    if (m_pulseWasSuspended)
        PulseHandler::Suspend(PulseHandler::kPulseResume);
#endif
    av_frame_free(&m_frame);
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
                                 AVCodecID /*codec*/,
                                 int /*profile*/) const
{
    return false;
}

// TODO: get rid of this if possible...  need to see what uses GetError() and
//       GetWarning() and why.  These would give more useful logs as macros
void AudioOutput::Error(const QString &msg)
{
    m_lastError = msg;
    LOG(VB_GENERAL, LOG_ERR, "AudioOutput Error: " + m_lastError);
}

void AudioOutput::SilentError(const QString &msg)
{
    m_lastError = msg;
}

void AudioOutput::Warn(const QString &msg)
{
    m_lastWarn = msg;
    LOG(VB_GENERAL, LOG_WARNING, "AudioOutput Warning: " + m_lastWarn);
}

void AudioOutput::ClearError(void)
{
    m_lastError.clear();
}

void AudioOutput::ClearWarning(void)
{
    m_lastWarn.clear();
}

AudioOutput::AudioDeviceConfig* AudioOutput::GetAudioDeviceConfig(
    QString &name, const QString &desc, bool willsuspendpa)
{
    AudioOutputSettings aosettings(true);

    AudioOutput *ao = OpenAudio(name, QString(), willsuspendpa);
    if (ao)
    {
        aosettings = *(ao->GetOutputSettingsCleaned());
        delete ao;
    }
    if (aosettings.IsInvalid())
    {
        if (!willsuspendpa)
            return nullptr;
        QString msg = tr("Invalid or unuseable audio device");
        return new AudioOutput::AudioDeviceConfig(name, msg);
    }

    QString capabilities = desc;
    int max_channels = aosettings.BestSupportedChannelsELD();
    if (aosettings.hasELD())
    {
        if (aosettings.getELD().isValid())
        {
            capabilities += tr(" (%1 connected to %2)")
                .arg(aosettings.getELD().product_name().simplified(),
                     aosettings.getELD().connection_name());
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
                (static_cast<int>(aosettings.canLPCM()) << 0) |
                (static_cast<int>(aosettings.canAC3())  << 1) |
                (static_cast<int>(aosettings.canDTS())  << 2);
            static const std::array<const std::string,3> s_typeNames { "LPCM", "AC3", "DTS" };

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
                        capabilities += QString::fromStdString(s_typeNames[i]);
                        found_one = true;
                    }
                }
                capabilities += QString(")");
            }
        }
    }
    LOG(VB_AUDIO, LOG_INFO, QString("Found %1 (%2)") .arg(name, capabilities));
    auto *adc = new AudioOutput::AudioDeviceConfig(name, capabilities);
    adc->m_settings = aosettings;
    return adc;
}

#ifdef USING_OSS
static void fillSelectionsFromDir(const QDir &dir,
                                  AudioOutput::ADCVect *list)
{
    QFileInfoList entries = dir.entryInfoList();
    for (const auto& fi : std::as_const(entries))
    {
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
    auto *list = new ADCVect;

#ifdef USING_PULSE
    bool pasuspended = PulseHandler::Suspend(PulseHandler::kPulseSuspend);
#endif

#ifdef USING_ALSA
    QMap<QString, QString> *alsadevs = AudioOutputALSA::GetDevices("pcm");

    if (!alsadevs->empty())
    {
        for (auto i = alsadevs->cbegin(); i != alsadevs->cend(); ++i)
        {
            const QString& key = i.key();
            const QString& desc = i.value();
            QString devname = QString("ALSA:%1").arg(key);

            auto *adc = GetAudioDeviceConfig(devname, desc);
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
        auto *adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif
#ifdef Q_OS_DARWIN

    {
        QMap<QString, QString> *devs = AudioOutputCA::GetDevices(nullptr);
        if (!devs->empty())
        {
            for (QMap<QString, QString>::const_iterator i = devs->begin();
                 i != devs->end(); ++i)
            {
                QString key = i.key();
                QString desc = i.value();
                QString devname = QString("CoreAudio:%1").arg(key);

                auto adc = GetAudioDeviceConfig(devname, desc);
                if (!adc)
                    continue;
                list->append(*adc);
                delete adc;
            }
        }
        delete devs;
        QString name = "CoreAudio:Default Output Device";
        QString desc = tr("CoreAudio default output");
        auto adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif
#ifdef _WIN32
    {
        QString name = "Windows:";
        QString desc = "Windows default output";
        auto adc = GetAudioDeviceConfig(name, desc);
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
                QString devdesc = i.value();
                QString devname = QString("DirectX:%1").arg(devdesc);

                adc = GetAudioDeviceConfig(devname, devdesc);
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
        auto *adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif

#ifdef Q_OS_ANDROID
    {
        QString name = "OpenSLES:";
        QString desc =  tr("OpenSLES default output. Stereo support only.");
        auto adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
    {
        QString name = "AudioTrack:";
        QString desc =  tr("Android AudioTrack output. Supports surround sound.");
        auto adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list->append(*adc);
            delete adc;
        }
    }
#endif

    QString name = "NULL";
    QString desc = "NULL device";
    auto *adc = GetAudioDeviceConfig(name, desc);
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
    bool got_frame = false;

    data_size = 0;
    if (!m_frame)
    {
        m_frame = av_frame_alloc();
        if (m_frame == nullptr)
        {
            return AVERROR(ENOMEM);
        }
    }
    else
    {
        av_frame_unref(m_frame);
    }

//  SUGGESTION
//  Now that avcodec_decode_audio4 is deprecated and replaced
//  by 2 calls (receive frame and send packet), this could be optimized
//  into separate routines or separate threads.
//  Also now that it always consumes a whole buffer some code
//  in the caller may be able to be optimized.
    int ret = avcodec_receive_frame(ctx,m_frame);
    if (ret == 0)
        got_frame = true;
    if (ret == AVERROR(EAGAIN))
        ret = 0;
    if (ret == 0)
        ret = avcodec_send_packet(ctx, pkt);
    if (ret == AVERROR(EAGAIN))
        ret = 0;
    else if (ret < 0)
    {
        std::string error;
        LOG(VB_AUDIO, LOG_ERR, LOC +
            QString("audio decode error: %1 (%2)")
            .arg(av_make_error_stdstring(error, ret))
            .arg(got_frame));
        return ret;
    }
    else
    {
        ret = pkt->size;
    }

    if (!got_frame)
    {
        LOG(VB_AUDIO, LOG_DEBUG, LOC +
            QString("audio decode, no frame decoded (%1)").arg(ret));
        return ret;
    }

    auto format = (AVSampleFormat)m_frame->format;
    AudioFormat fmt =
        AudioOutputSettings::AVSampleFormatToFormat(format, ctx->bits_per_raw_sample);

    data_size = m_frame->nb_samples * m_frame->ch_layout.nb_channels * av_get_bytes_per_sample(format);

    // May need to convert audio to S16
    AudioConvert converter(fmt, CanProcess(fmt) ? fmt : FORMAT_S16);
    uint8_t* src = nullptr;

    if (av_sample_fmt_is_planar(format))
    {
        src = buffer;
        converter.InterleaveSamples(m_frame->ch_layout.nb_channels,
                                    src,
                                    (const uint8_t **)m_frame->extended_data,
                                    data_size);
    }
    else
    {
        // data is already compacted...
        src = m_frame->extended_data[0];
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
