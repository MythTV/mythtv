#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>

#include <QFile>
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "audiooutputalsa.h"

#define LOC QString("ALSA: ")

// redefine assert as no-op to quiet some compiler warnings
// about assert always evaluating true in alsa headers.
#undef assert
#define assert(x)

static constexpr uint8_t CHANNELS_MIN { 1 };
static constexpr uint8_t CHANNELS_MAX { 8 };

static constexpr int OPEN_FLAGS
    { SND_PCM_NO_AUTO_RESAMPLE | SND_PCM_NO_AUTO_FORMAT | SND_PCM_NO_AUTO_CHANNELS };

static constexpr int FILTER_FLAGS { ~(SND_PCM_NO_AUTO_FORMAT) };

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define AERROR(str)   LOG(VB_GENERAL, LOG_ERR, LOC + (str) + QString(": %1").arg(snd_strerror(err)))
#define CHECKERR(str) { if (err < 0) { AERROR(str); return err; } }
// NOLINTEND(cppcoreguidelines-macro-usage)

AudioOutputALSA::AudioOutputALSA(const AudioSettings &settings) :
    AudioOutputBase(settings)
{
    // Set everything up
    if (m_passthruDevice == "auto")
    {
        m_passthruDevice = m_mainDevice;

            /*
             * AES description:
             * AES0=6 AES1=0x82 AES2=0x00 AES3=0x01.
             * AES0 = NON_AUDIO | PRO_MODE
             * AES1 = original stream, original PCM coder
             * AES2 = source and channel unspecified
             * AES3 = sample rate unspecified
             */
        bool s48k = gCoreContext->GetBoolSetting("SPDIFRateOverride", false);
        QString iecarg = QString("AES0=6,AES1=0x82,AES2=0x00") +
            (s48k ? QString() : QString(",AES3=0x01"));
        QString iecarg2 = QString("AES0=6 AES1=0x82 AES2=0x00") +
            (s48k ? QString() : QString(" AES3=0x01"));

        QStringList parts = m_passthruDevice.split(':');
        if (parts.length() == 1)
        {
            /* no existing parameters: add it behind device name */
            m_passthruDevice += ":" + iecarg;
        }
        else
        {
            QString params = parts[1].trimmed();
            if (params.isEmpty())
            {
                /* ":" but no parameters */
                m_passthruDevice += iecarg;
            }
            else if (params[0] != '{')
            {
                /* a simple list of parameters: add it at the end of the list */
                m_passthruDevice += "," + iecarg;
            }
            else
            {
                /* parameters in config syntax: add it inside the { } block */
                /* the whitespace has been trimmed, so the curly brackets */
                /* should be the first and last characters in the string. */
                QString oldparams = params.mid(1,params.size()-2);
                m_passthruDevice = QString("%1:{ %2 %3 }")
                    .arg(parts[0], oldparams.trimmed(), iecarg2);
            }
        }
    }
    else if (m_passthruDevice.toLower() == "default")
    {
        m_passthruDevice = m_mainDevice;
    }
    else
    {
        m_discreteDigital = true;
    }

    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
}

AudioOutputALSA::~AudioOutputALSA()
{
    KillAudio();
}

int AudioOutputALSA::TryOpenDevice(int open_mode, bool try_ac3)
{
    QByteArray dev_ba;
    int err = -1;

    if (try_ac3)
    {
        dev_ba = m_passthruDevice.toLatin1();
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("OpenDevice %1 for passthrough").arg(m_passthruDevice));
        err = snd_pcm_open(&m_pcmHandle, dev_ba.constData(),
                           SND_PCM_STREAM_PLAYBACK, open_mode);

        m_lastDevice = m_passthruDevice;

        if (m_discreteDigital)
            return err;

        if (err < 0)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Auto setting passthrough failed (%1), defaulting "
                            "to main device").arg(snd_strerror(err)));
        }
    }
    if (!try_ac3 || err < 0)
    {
        // passthru open failed, retry default device
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("OpenDevice %1").arg(m_mainDevice));
        dev_ba = m_mainDevice.toLatin1();
        err = snd_pcm_open(&m_pcmHandle, dev_ba.constData(),
                           SND_PCM_STREAM_PLAYBACK, open_mode);
        m_lastDevice = m_mainDevice;
    }
    return err;
}

int AudioOutputALSA::GetPCMInfo(int &card, int &device, int &subdevice)
{
    // Check for saved values
    if (m_card != -1 && m_device != -1 && m_subdevice != -1)
    {
        card = m_card;
        device = m_device;
        subdevice = m_subdevice;
        return 0;
    }

    if (!m_pcmHandle)
        return -1;

    snd_pcm_info_t *pcm_info = nullptr;

    snd_pcm_info_alloca(&pcm_info);

    int err = snd_pcm_info(m_pcmHandle, pcm_info);
    CHECKERR("snd_pcm_info");

    err = snd_pcm_info_get_card(pcm_info);
    CHECKERR("snd_pcm_info_get_card");
    int tcard = err;

    err = snd_pcm_info_get_device(pcm_info);
    CHECKERR("snd_pcm_info_get_device");
    int tdevice = err;

    err = snd_pcm_info_get_subdevice(pcm_info);
    CHECKERR("snd_pcm_info_get_subdevice");
    int tsubdevice = err;

    m_card = card = tcard;
    m_device = device = tdevice;
    m_subdevice = subdevice = tsubdevice;

    return 0;
 }

bool AudioOutputALSA::IncPreallocBufferSize(int requested, int buffer_time)
{
    int card = 0;
    int device = 0;
    int subdevice = 0;

    m_pbufSize = 0;

    if (GetPCMInfo(card, device, subdevice) < 0)
        return false;

    const QString pf = QString("/proc/asound/card%1/pcm%2p/sub%3/prealloc")
                       .arg(card).arg(device).arg(subdevice);

    QFile pfile(pf);
    QFile mfile(pf + "_max");

    if (!pfile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error opening %1. Fix reading permissions.").arg(pf));
        return false;
    }

    if (!mfile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error opening %1").arg(pf + "_max"));
        return false;
    }

    int cur  = pfile.readAll().trimmed().toInt();
    int max  = mfile.readAll().trimmed().toInt();

    int size = ((int)(cur * (float)requested / (float)buffer_time)
                / 64 + 1) * 64;

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Hardware audio buffer cur: %1 need: %2 max allowed: %3")
            .arg(cur).arg(size).arg(max));

    if (cur == max)
    {
        // It's already the maximum it can be, no point trying further
        pfile.close();
        mfile.close();
        return false;
    }
    if (size > max || !size)
    {
        size = max;
    }

    pfile.close();
    mfile.close();

    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Try to manually increase audio buffer with: echo %1 "
                   "| sudo tee %2").arg(size).arg(pf));
    return false;
}

QByteArray *AudioOutputALSA::GetELD(int card, int device, int subdevice)
{
    QByteArray *result = nullptr;
    snd_hctl_t *hctl = nullptr;
    snd_ctl_elem_info_t *info = nullptr;
    snd_ctl_elem_id_t *id = nullptr;
    snd_ctl_elem_value_t *control = nullptr;

    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
    snd_ctl_elem_id_set_name(id, "ELD");
    snd_ctl_elem_id_set_device(id, device);
    snd_ctl_elem_id_set_subdevice(id, subdevice);

    int err = snd_hctl_open(&hctl,
                            QString("hw:%1").arg(card).toLatin1().constData(),
                            0);
    if (err < 0)
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Control %1 open error: %2")
                .arg(card)
                .arg(snd_strerror(err)));
        return nullptr;
    }
    err = snd_hctl_load(hctl);
    if (err  < 0)
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Control %1 load error: %2")
                .arg(card)
                .arg(snd_strerror(err)));
        /* frees automatically the control which cannot be added. */
        return nullptr;
    }
    snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);
    if (elem)
    {
        err = snd_hctl_elem_info(elem, info);
        if (err < 0)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Control %1 snd_hctl_elem_info error: %2")
                    .arg(card)
                    .arg(snd_strerror(err)));
            snd_hctl_close(hctl);
            return nullptr;
        }
        unsigned int count = snd_ctl_elem_info_get_count(info);
        snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);
        if (!snd_ctl_elem_info_is_readable(info))
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Control %1 element info is not readable")
                    .arg(card));
            snd_hctl_close(hctl);
            return nullptr;
        }
        err = snd_hctl_elem_read(elem, control);
        if (err < 0)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Control %1 element read error: %2")
                    .arg(card)
                    .arg(snd_strerror(err)));
            snd_hctl_close(hctl);
            return nullptr;
        }
        if (type != SND_CTL_ELEM_TYPE_BYTES)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Control %1 element is of the wrong type")
                    .arg(card));
            snd_hctl_close(hctl);
            return nullptr;
        }
        result = new QByteArray((char *)snd_ctl_elem_value_get_bytes(control),
                                count);
    }
    snd_hctl_close(hctl);
    return result;
}

AudioOutputSettings* AudioOutputALSA::GetOutputSettings(bool passthrough)
{
    snd_pcm_hw_params_t *params = nullptr;
    snd_pcm_format_t afmt = SND_PCM_FORMAT_UNKNOWN;
    AudioFormat fmt = FORMAT_NONE;

    auto *settings = new AudioOutputSettings();

    if (m_pcmHandle)
    {
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = nullptr;
    }

    int err = TryOpenDevice(OPEN_FLAGS, passthrough);
    if (err < 0)
    {
        AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastDevice));
        delete settings;
        return nullptr;
    }

    snd_pcm_hw_params_alloca(&params);

    if (snd_pcm_hw_params_any(m_pcmHandle, params) < 0)
    {
        snd_pcm_close(m_pcmHandle);
        err = TryOpenDevice(OPEN_FLAGS&FILTER_FLAGS, passthrough);
        if (err  < 0)
        {
            AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastDevice));
            delete settings;
            return nullptr;
        }
        err = snd_pcm_hw_params_any(m_pcmHandle, params);
        if (err < 0)
        {
            AERROR("No playback configurations available");
            snd_pcm_close(m_pcmHandle);
            m_pcmHandle = nullptr;
            delete settings;
            return nullptr;
        }
        Warn("Supported audio format detection will be inacurrate "
             "(using plugin?)");
    }

    // NOLINTNEXTLINE(bugprone-infinite-loop)
    while (int rate = settings->GetNextRate())
        if(snd_pcm_hw_params_test_rate(m_pcmHandle, params, rate, 0) >= 0)
            settings->AddSupportedRate(rate);

    while ((fmt = settings->GetNextFormat()))
    {
        switch (fmt)
        {
            case FORMAT_U8:     afmt = SND_PCM_FORMAT_U8;    break;
            case FORMAT_S16:    afmt = SND_PCM_FORMAT_S16;   break;
            // NOLINTNEXTLINE(bugprone-branch-clone)
            case FORMAT_S24LSB: afmt = SND_PCM_FORMAT_S24;   break;
            case FORMAT_S24:    afmt = SND_PCM_FORMAT_S24;   break;
            case FORMAT_S32:    afmt = SND_PCM_FORMAT_S32;   break;
            case FORMAT_FLT:    afmt = SND_PCM_FORMAT_FLOAT; break;
            default:         continue;
        }
        if (snd_pcm_hw_params_test_format(m_pcmHandle, params, afmt) >= 0)
            settings->AddSupportedFormat(fmt);
    }

    for (uint channels = CHANNELS_MIN; channels <= CHANNELS_MAX; channels++)
        if (snd_pcm_hw_params_test_channels(m_pcmHandle, params, channels) >= 0)
            settings->AddSupportedChannels(channels);

    int card = 0;
    int device = 0;
    int subdevice = 0;
    if (GetPCMInfo(card, device, subdevice) >= 0)
    {
        // Check if we can retrieve ELD for this device
        QByteArray *eld = GetELD(card, device, subdevice);
        if (eld != nullptr)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Successfully retrieved ELD data"));
            settings->setELD(eld);
            delete eld;
       }
    }
    else
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "Can't get card and device number");
    }

    snd_pcm_close(m_pcmHandle);
    m_pcmHandle = nullptr;

    /* Check if name or description contains information
       to know if device can accept passthrough or not */
    QMap<QString, QString> *alsadevs = GetDevices("pcm");
    while (true)
    {
        QString real_device = ((passthrough && m_discreteDigital) ?
                               m_passthruDevice : m_mainDevice);

        QString desc = alsadevs->value(real_device);

        settings->setPassthrough(1);   // yes passthrough
        if (real_device.contains("digital", Qt::CaseInsensitive) ||
            desc.contains("digital", Qt::CaseInsensitive))
            break;
        if (real_device.contains("iec958", Qt::CaseInsensitive))
            break;
        if (real_device.contains("spdif", Qt::CaseInsensitive))
            break;
        if (real_device.contains("hdmi", Qt::CaseInsensitive))
            break;

        settings->setPassthrough(-1);   // no passthrough
        // PulseAudio does not support passthrough
        if (real_device.contains("pulse", Qt::CaseInsensitive) ||
            desc.contains("pulse", Qt::CaseInsensitive))
            break;
        if (real_device.contains("analog", Qt::CaseInsensitive) ||
            desc.contains("analog", Qt::CaseInsensitive))
            break;
        if (real_device.contains("surround", Qt::CaseInsensitive) ||
            desc.contains("surround", Qt::CaseInsensitive))
            break;

        settings->setPassthrough(0);   // maybe passthrough
        break;
    }
    delete alsadevs;
    return settings;
}

bool AudioOutputALSA::OpenDevice()
{
    snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;

    if (m_pcmHandle != nullptr)
        CloseDevice();

    int err = TryOpenDevice(0, m_passthru || m_enc);
    if (err < 0)
    {
        AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastDevice));
        if (m_pcmHandle)
            CloseDevice();
        return false;
    }

    switch (m_outputFormat)
    {
        case FORMAT_U8:     format = SND_PCM_FORMAT_U8;    break;
        case FORMAT_S16:    format = SND_PCM_FORMAT_S16;   break;
        // NOLINTNEXTLINE(bugprone-branch-clone)
        case FORMAT_S24LSB: format = SND_PCM_FORMAT_S24;   break;
        case FORMAT_S24:    format = SND_PCM_FORMAT_S24;   break;
        case FORMAT_S32:    format = SND_PCM_FORMAT_S32;   break;
        case FORMAT_FLT:    format = SND_PCM_FORMAT_FLOAT; break;
        default:
            Error(QObject::tr("Unknown sample format: %1").arg(m_outputFormat));
            return false;
    }

    // buffer 0.5s worth of samples
    uint buffer_time = gCoreContext->GetNumSetting("ALSABufferOverride", 500) * 1000;

    uint period_time = 4; // aim for an interrupt every (1/4th of buffer_time)

    err = SetParameters(m_pcmHandle, format, m_channels, m_sampleRate,
                        buffer_time, period_time);
    if (err < 0)
    {
        AERROR("Unable to set ALSA parameters");
        CloseDevice();
        return false;
    }

    if (m_internalVol && !OpenMixer())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to open audio mixer. Volume control disabled");

    // Device opened successfully
    return true;
}

void AudioOutputALSA::CloseDevice()
{
    if (m_mixer.handle)
        snd_mixer_close(m_mixer.handle);
    m_mixer.handle = nullptr;
    if (m_pcmHandle)
    {
        snd_pcm_drain(m_pcmHandle);
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = nullptr;
    }
}

template <class AudioDataType>
static inline void tReorderSmpteToAlsa(AudioDataType *buf, uint frames,
                                       uint extrach)
{
    AudioDataType tmpC;
    AudioDataType tmpLFE;
    AudioDataType *buf2 = nullptr;

    for (uint i = 0; i < frames; i++)
    {
        buf = buf2 = buf + 2;

        tmpC = *buf++;
        tmpLFE = *buf++;
        *buf2++ = *buf++;
        *buf2++ = *buf++;
        *buf2++ = tmpC;
        *buf2++ = tmpLFE;
        buf += extrach;
    }
}

static inline void ReorderSmpteToAlsa(void *buf, uint frames,
                                      AudioFormat format, uint extrach)
{
    switch(AudioOutputSettings::FormatToBits(format))
    {
        case  8: tReorderSmpteToAlsa((uchar *)buf, frames, extrach); break;
        case 16: tReorderSmpteToAlsa((short *)buf, frames, extrach); break;
        default: tReorderSmpteToAlsa((int   *)buf, frames, extrach); break;
    }
}

void AudioOutputALSA::WriteAudio(uchar *aubuf, int size)
{
    uchar *tmpbuf = aubuf;
    uint frames = size / m_outputBytesPerFrame;

    if (m_pcmHandle == nullptr)
    {
        Error("WriteAudio() called with pcm_handle == nullptr!");
        return;
    }

    //Audio received is in SMPTE channel order, reorder to ALSA unless passthru
    if (!m_passthru && (m_channels  == 6 || m_channels == 8))
    {
        ReorderSmpteToAlsa(aubuf, frames, m_outputFormat, m_channels - 6);
    }

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO,
            QString("WriteAudio: Preparing %1 bytes (%2 frames)")
            .arg(size).arg(frames));

    while (frames > 0)
    {
        int lw = snd_pcm_writei(m_pcmHandle, tmpbuf, frames);

        if (lw >= 0)
        {
            if ((uint)lw < frames)
                LOG(VB_AUDIO, LOG_INFO, LOC + QString("WriteAudio: short write %1 bytes (ok)")
                        .arg(lw * m_outputBytesPerFrame));

            frames -= lw;
            tmpbuf += static_cast<ptrdiff_t>(lw) * m_outputBytesPerFrame; // bytes
            continue;
        }

        int err = lw;

        switch (err)
        {
            case -EPIPE:
                 if (snd_pcm_state(m_pcmHandle) == SND_PCM_STATE_XRUN)
                 {
                    LOG(VB_AUDIO, LOG_INFO, LOC + "WriteAudio: buffer underrun");
                    err = snd_pcm_prepare(m_pcmHandle);
                    if (err  < 0)
                    {
                        AERROR("WriteAudio: unable to recover from xrun");
                        return;
                    }
                }
                break;

#if ESTRPIPE != EPIPE
            case -ESTRPIPE:
                LOG(VB_AUDIO, LOG_INFO, LOC + "WriteAudio: device is suspended");
                while ((err = snd_pcm_resume(m_pcmHandle)) == -EAGAIN)
                    usleep(200us);

                if (err < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "WriteAudio: resume failed");
                    err = snd_pcm_prepare(m_pcmHandle);
                    if (err < 0)
                    {
                        AERROR("WriteAudio: unable to recover from suspend");
                        return;
                    }
                }
                break;
#endif

            case -EBADFD:
                Error(
                    QString("WriteAudio: device is in a bad state (state = %1)")
                    .arg(snd_pcm_state(m_pcmHandle)));
                return;

            default:
                AERROR(QString("WriteAudio: Write failed, state: %1, err")
                       .arg(snd_pcm_state(m_pcmHandle)));
                return;
        }
    }
}

int AudioOutputALSA::GetBufferedOnSoundcard(void) const
{
    if (m_pcmHandle == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "getBufferedOnSoundcard() called with pcm_handle == nullptr!");
        return 0;
    }

    snd_pcm_sframes_t delay = 0;

    /* Delay is the total delay from writing to the pcm until the samples
       hit the DAC - includes buffered samples and any fixed latencies */
    if (snd_pcm_delay(m_pcmHandle, &delay) < 0)
        return 0;

    // BUG: calling snd_pcm_state causes noise and repeats on the Raspberry Pi
    return delay * m_outputBytesPerFrame;
}

/**
 * Set the various ALSA audio parameters.
 * Returns:
 * < 0 : an error occurred
 * 0   : Succeeded
 * > 0 : Buffer timelength returned by ALSA which is less than what we asked for
 */
int AudioOutputALSA::SetParameters(snd_pcm_t *handle, snd_pcm_format_t format,
                                   uint channels, uint rate, uint buffer_time,
                                   uint period_time)
{
    snd_pcm_hw_params_t  *params = nullptr;
    snd_pcm_sw_params_t  *swparams = nullptr;
    snd_pcm_uframes_t     period_size = 0;
    snd_pcm_uframes_t     period_size_min = 0;
    snd_pcm_uframes_t     period_size_max = 0;
    snd_pcm_uframes_t     buffer_size = 0;
    snd_pcm_uframes_t     buffer_size_min = 0;
    snd_pcm_uframes_t     buffer_size_max = 0;

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("SetParameters(format=%1, channels=%2, rate=%3, "
                    "buffer_time=%4, period_time=%5)")
            .arg(format).arg(channels).arg(rate).arg(buffer_time)
            .arg(period_time));

    if (handle == nullptr)
    {
        Error(QObject::tr("SetParameters() called with handle == nullptr!"));
        return -1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    /* choose all parameters */
    int err = snd_pcm_hw_params_any(handle, params);
    CHECKERR("No playback configurations available");

    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    CHECKERR(QString("Interleaved RW audio not available"));

    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, format);
    CHECKERR(QString("Sample format %1 not available").arg(format));

    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    CHECKERR(QString("Channels count %1 not available").arg(channels));

    /* set the stream rate */
    if (m_srcQuality == QUALITY_DISABLED)
    {
        err = snd_pcm_hw_params_set_rate_resample(handle, params, 1);
        CHECKERR(QString("Resampling setup failed").arg(rate));

        uint rrate = rate;
        err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, nullptr);
        CHECKERR(QString("Rate %1Hz not available for playback: %s").arg(rate));

        if (rrate != rate)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Rate doesn't match (requested %1Hz, got %2Hz)")
                    .arg(rate).arg(err));
            return err;
        }
    }
    else
    {
        err = snd_pcm_hw_params_set_rate(handle, params, rate, 0);
        CHECKERR(QString("Samplerate %1 Hz not available").arg(rate));
    }

    /* get the buffer parameters */
    (void) snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
    (void) snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
    (void) snd_pcm_hw_params_get_period_size_min(params, &period_size_min, nullptr);
    (void) snd_pcm_hw_params_get_period_size_max(params, &period_size_max, nullptr);
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Buffer size range from %1 to %2")
            .arg(buffer_size_min)
            .arg(buffer_size_max));
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Period size range from %1 to %2")
            .arg(period_size_min)
            .arg(period_size_max));

    /* set the buffer time */
    uint original_buffer_time = buffer_time;
    bool canincrease = true;
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                 &buffer_time, nullptr);
    if (err < 0)
    {
        int dir         = -1;
        uint buftmp     = buffer_time;
        int attempt     = 0;
        // Try again with a fourth parameter
        err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                     &buffer_time, &dir);
        while (err < 0)
        {
            /*
             * with some drivers, snd_pcm_hw_params_set_buffer_time_near
             * only works once, if that's the case no point trying with
             * different values
             */
            if ((buffer_time <= 100000) ||
                (attempt > 0 && buffer_time == buftmp))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't set buffer time, giving up");
                return err;
            }
            AERROR(QString("Unable to set buffer time to %1us, retrying")
                   .arg(buffer_time));
            buffer_time -= 100000;
            canincrease  = false;
            attempt++;
            err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                         &buffer_time, &dir);
        }
    }

    /* See if we need to increase the prealloc'd buffer size
       If buffer_time is too small we could underrun - make 10% difference ok */
    if (buffer_time * 1.10F < (float)original_buffer_time)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Requested %1us got %2 buffer time")
                .arg(original_buffer_time).arg(buffer_time));
        // We need to increase preallocated buffer size in the driver
        if (canincrease && m_pbufSize < 0)
        {
            IncPreallocBufferSize(original_buffer_time, buffer_time);
        }
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Buffer time = %1 us").arg(buffer_time));

    /* set the period time */
    err = snd_pcm_hw_params_set_periods_near(handle, params,
                                             &period_time, nullptr);
    CHECKERR(QString("Unable to set period time %1").arg(period_time));
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Period time = %1 periods").arg(period_time));

    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    CHECKERR("Unable to set hw params for playback");

    err = snd_pcm_get_params(handle, &buffer_size, &period_size);
    CHECKERR("Unable to get PCM params");
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Buffer size = %1 | Period size = %2")
            .arg(buffer_size).arg(period_size));

    /* set member variables */
    m_soundcardBufferSize = buffer_size * m_outputBytesPerFrame;
    m_fragmentSize = (period_size >> 1) * m_outputBytesPerFrame;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    CHECKERR("Unable to get current swparams");

    /* start the transfer after period_size */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, period_size);
    CHECKERR("Unable to set start threshold");

    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
    CHECKERR("Unable to set avail min");

    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    CHECKERR("Unable to set sw params");

    err = snd_pcm_prepare(handle);
    CHECKERR("Unable to prepare the PCM");

    return 0;
}

int AudioOutputALSA::GetVolumeChannel(int channel) const
{
    int retvol = 0;

    if (!m_mixer.elem)
        return retvol;

    auto chan = (snd_mixer_selem_channel_id_t) channel;
    if (!snd_mixer_selem_has_playback_channel(m_mixer.elem, chan))
        return retvol;

    long mixervol = 0;
    int chk = snd_mixer_selem_get_playback_volume(m_mixer.elem, chan,
                                                  &mixervol);
    if (chk < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("failed to get channel %1 volume, mixer %2/%3: %4")
                .arg(QString::number(channel), m_mixer.device,
                     m_mixer.control, snd_strerror(chk)));
    }
    else
    {
        retvol = (m_mixer.volrange != 0L)
            ? ((mixervol - m_mixer.volmin) * 100.0F / m_mixer.volrange) + 0.5F
            : 0;
        retvol = std::max(retvol, 0);
        retvol = std::min(retvol, 100);
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("get volume channel %1: %2")
                .arg(channel).arg(retvol));
    }
    return retvol;
}

void AudioOutputALSA::SetVolumeChannel(int channel, int volume)
{
    if (!(m_internalVol && m_mixer.elem))
        return;

    long mixervol = ((int64_t(volume) * m_mixer.volrange) / 100) + m_mixer.volmin;
    mixervol = std::max(mixervol, m_mixer.volmin);
    mixervol = std::min(mixervol, m_mixer.volmax);

    auto chan = (snd_mixer_selem_channel_id_t) channel;

    if (snd_mixer_selem_has_playback_switch(m_mixer.elem))
        snd_mixer_selem_set_playback_switch(m_mixer.elem, chan, static_cast<int>(volume > 0));

    if (snd_mixer_selem_set_playback_volume(m_mixer.elem, chan, mixervol) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("failed to set channel %1 volume").arg(channel));
    else
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("channel %1 volume set %2 => %3")
                .arg(channel).arg(volume).arg(mixervol));
}

bool AudioOutputALSA::OpenMixer(void)
{
    if (!m_pcmHandle)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "mixer setup without a pcm");
        return false;
    }
    m_mixer.device = gCoreContext->GetSetting("MixerDevice", "default");
    m_mixer.device = m_mixer.device.remove(QString("ALSA:"));
    if (m_mixer.device.toLower() == "software")
        return true;

    m_mixer.control = gCoreContext->GetSetting("MixerControl", "PCM");

    QString mixer_device_tag = QString("mixer device %1").arg(m_mixer.device);

    int chk = snd_mixer_open(&m_mixer.handle, 0);
    if (chk < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("failed to open mixer device %1: %2")
                .arg(mixer_device_tag, snd_strerror(chk)));
        return false;
    }

    QByteArray dev_ba = m_mixer.device.toLatin1();
    struct snd_mixer_selem_regopt regopts =
        {1, SND_MIXER_SABSTRACT_NONE, dev_ba.constData(), nullptr, nullptr};

    chk = snd_mixer_selem_register(m_mixer.handle, &regopts, nullptr);
    if (chk < 0)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("failed to register %1: %2")
                .arg(mixer_device_tag, snd_strerror(chk)));
        return false;
    }

    chk = snd_mixer_load(m_mixer.handle);
    if (chk < 0)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("failed to load %1: %2")
                .arg(mixer_device_tag, snd_strerror(chk)));
        return false;
    }

    m_mixer.elem = nullptr;
    uint elcount = snd_mixer_get_count(m_mixer.handle);
    snd_mixer_elem_t* elx = snd_mixer_first_elem(m_mixer.handle);

    for (uint ctr = 0; elx != nullptr && ctr < elcount; ctr++)
    {
        QString tmp = QString(snd_mixer_selem_get_name(elx));
        if (m_mixer.control == tmp &&
            !snd_mixer_selem_is_enumerated(elx) &&
            snd_mixer_selem_has_playback_volume(elx) &&
            snd_mixer_selem_is_active(elx))
        {
            m_mixer.elem = elx;
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("found playback control %1 on %2")
                    .arg(m_mixer.control, mixer_device_tag));
            break;
        }
        elx = snd_mixer_elem_next(elx);
    }
    if (!m_mixer.elem)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("no playback control %1 found on %2")
                .arg(m_mixer.control, mixer_device_tag));
        return false;
    }
    if ((snd_mixer_selem_get_playback_volume_range(m_mixer.elem,
                                                   &m_mixer.volmin,
                                                   &m_mixer.volmax) < 0))
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("failed to get volume range on %1/%2")
                .arg(mixer_device_tag, m_mixer.control));
        return false;
    }

    m_mixer.volrange = m_mixer.volmax - m_mixer.volmin;
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("mixer volume range on %1/%2 - min %3, max %4, range %5")
            .arg(mixer_device_tag, m_mixer.control)
            .arg(m_mixer.volmin).arg(m_mixer.volmax).arg(m_mixer.volrange));
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("%1/%2 set up successfully")
            .arg(mixer_device_tag, m_mixer.control));

    if (m_setInitialVol)
    {
        int initial_vol = 80;
        if (m_mixer.control == "PCM")
            initial_vol = gCoreContext->GetNumSetting("PCMMixerVolume", 80);
        else
            initial_vol = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        for (int ch = 0; ch < m_channels; ++ch)
            SetVolumeChannel(ch, initial_vol);
    }

    return true;
}

QMap<QString, QString> *AudioOutputALSA::GetDevices(const char *type)
{
    auto *alsadevs = new QMap<QString, QString>();
    void **hints = nullptr;
    void **n = nullptr;

    if (snd_device_name_hint(-1, type, &hints) < 0)
        return alsadevs;
    n = hints;

    while (*n != nullptr)
    {
          QString name = snd_device_name_get_hint(*n, "NAME");
          QString desc = snd_device_name_get_hint(*n, "DESC");
          if (!name.isEmpty() && !desc.isEmpty() && (name != "null"))
              alsadevs->insert(name, desc);
          n++;
    }
    snd_device_name_free_hint(hints);
        // Work around ALSA bug < 1.0.22 ; where snd_device_name_hint can corrupt
        // global ALSA memory context
#if SND_LIB_MAJOR == 1
#if SND_LIB_MINOR == 0
#if SND_LIB_SUBMINOR < 22
    snd_config_update_free_global();
#endif
#endif
#endif
    return alsadevs;
}
