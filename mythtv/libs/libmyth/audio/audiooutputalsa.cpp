#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>
#include "config.h"

using namespace std;

#include <QFile>
#include "mythcorecontext.h"
#include "audiooutputalsa.h"

#define LOC QString("ALSA: ")

// redefine assert as no-op to quiet some compiler warnings
// about assert always evaluating true in alsa headers.
#undef assert
#define assert(x)

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

#define OPEN_FLAGS (SND_PCM_NO_AUTO_RESAMPLE|SND_PCM_NO_AUTO_FORMAT|    \
                    SND_PCM_NO_AUTO_CHANNELS)

#define FILTER_FLAGS ~(SND_PCM_NO_AUTO_FORMAT)

#define AERROR(str)   VBERROR(str + QString(": %1").arg(snd_strerror(err)))
#define CHECKERR(str) { if (err < 0) { AERROR(str); return err; } }

AudioOutputALSA::AudioOutputALSA(const AudioSettings &settings) :
    AudioOutputBase(settings),
    pcm_handle(NULL),
    m_pbufsize(-1),
    m_card(-1),
    m_device(-1),
    m_subdevice(-1)
{
    m_mixer.handle = NULL;
    m_mixer.elem = NULL;

    // Set everything up
    if (passthru_device == "auto")
    {
        passthru_device = main_device;

        int len = passthru_device.length();
        int args = passthru_device.indexOf(":");

            /*
             * AES description:
             * AES0=6 AES1=0x82 AES2=0x00 AES3=0x01.
             * AES0 = NON_AUDIO | PRO_MODE
             * AES1 = original stream, original PCM coder
             * AES2 = source and channel unspecified
             * AES3 = sample rate unspecified
             */
        bool s48k = gCoreContext->GetNumSetting("SPDIFRateOverride", false);
        QString iecarg = QString("AES0=6,AES1=0x82,AES2=0x00") +
            (s48k ? QString() : QString(",AES3=0x01"));
        QString iecarg2 = QString("AES0=6 AES1=0x82 AES2=0x00") +
            (s48k ? QString() : QString(" AES3=0x01"));

        if (args < 0)
        {
            /* no existing parameters: add it behind device name */
            passthru_device += ":" + iecarg;
        }
        else
        {
            do
                ++args;
            while (args < passthru_device.length() &&
                   passthru_device[args].isSpace());
            if (args == passthru_device.length())
            {
                /* ":" but no parameters */
                passthru_device += iecarg;
            }
            else if (passthru_device[args] != '{')
            {
                /* a simple list of parameters: add it at the end of the list */
                passthru_device += "," + iecarg;
            }
            else
            {
                /* parameters in config syntax: add it inside the { } block */
                do
                    --len;
                while (len > 0 && passthru_device[len].isSpace());
                if (passthru_device[len] == '}')
                    passthru_device =
                        passthru_device.insert(len, " " + iecarg2);
            }
        }
    }
    else if (passthru_device.toLower() == "default")
        passthru_device = main_device;
    else
        m_discretedigital = true;

    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

AudioOutputALSA::~AudioOutputALSA()
{
    KillAudio();
}

int AudioOutputALSA::TryOpenDevice(int open_mode, int try_ac3)
{
    QString real_device;
    QByteArray dev_ba;
    int err = -1;

    if (try_ac3)
    {
        dev_ba = passthru_device.toLatin1();
        VBAUDIO(QString("OpenDevice %1 for passthrough").arg(passthru_device));
        err = snd_pcm_open(&pcm_handle, dev_ba.constData(),
                           SND_PCM_STREAM_PLAYBACK, open_mode);

        m_lastdevice = passthru_device;

        if (m_discretedigital)
            return err;

        if (err < 0)
        {
            VBAUDIO(QString("Auto setting passthrough failed (%1), defaulting "
                            "to main device").arg(snd_strerror(err)));
        }
    }
    if (!try_ac3 || err < 0)
    {
        // passthru open failed, retry default device
        VBAUDIO(QString("OpenDevice %1").arg(main_device));
        dev_ba = main_device.toLatin1();
        err = snd_pcm_open(&pcm_handle, dev_ba.constData(),
                           SND_PCM_STREAM_PLAYBACK, open_mode);
        m_lastdevice = main_device;
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

    if (!pcm_handle)
        return -1;

    int err;
    snd_pcm_info_t *pcm_info = NULL;
    int tcard, tdevice, tsubdevice;

    snd_pcm_info_alloca(&pcm_info);

    err = snd_pcm_info(pcm_handle, pcm_info);
    CHECKERR("snd_pcm_info");

    err = snd_pcm_info_get_card(pcm_info);
    CHECKERR("snd_pcm_info_get_card");
    tcard = err;

    err = snd_pcm_info_get_device(pcm_info);
    CHECKERR("snd_pcm_info_get_device");
    tdevice = err;

    err = snd_pcm_info_get_subdevice(pcm_info);
    CHECKERR("snd_pcm_info_get_subdevice");
    tsubdevice = err;

    m_card = card = tcard;
    m_device = device = tdevice;
    m_subdevice = subdevice = tsubdevice;

    return 0;
 }

bool AudioOutputALSA::IncPreallocBufferSize(int requested, int buffer_time)
{
    int card, device, subdevice;

    m_pbufsize = 0;

    if (GetPCMInfo(card, device, subdevice) < 0)
        return false;

    const QString pf = QString("/proc/asound/card%1/pcm%2p/sub%3/prealloc")
                       .arg(card).arg(device).arg(subdevice);

    QFile pfile(pf);
    QFile mfile(pf + "_max");

    if (!pfile.open(QIODevice::ReadOnly))
    {
        VBERROR(QString("Error opening %1. Fix reading permissions.").arg(pf));
        return false;
    }

    if (!mfile.open(QIODevice::ReadOnly))
    {
        VBERROR(QString("Error opening %1").arg(pf + "_max"));
        return false;
    }

    int cur  = pfile.readAll().trimmed().toInt();
    int max  = mfile.readAll().trimmed().toInt();

    int size = ((int)(cur * (float)requested / (float)buffer_time)
                / 64 + 1) * 64;

    VBAUDIO(QString("Hardware audio buffer cur: %1 need: %2 max allowed: %3")
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

    VBERROR(QString("Try to manually increase audio buffer with: echo %1 "
                    "| sudo tee %2").arg(size).arg(pf));
    return false;
}

QByteArray *AudioOutputALSA::GetELD(int card, int device, int subdevice)
{
    QByteArray *result = NULL;
    snd_hctl_t *hctl;
    snd_hctl_elem_t *elem;
    snd_ctl_elem_info_t *info;     snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_t *id;         snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_t *control; snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_type_t type;

    unsigned int count;

    int err;

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
    snd_ctl_elem_id_set_name(id, "ELD");
    snd_ctl_elem_id_set_device(id, device);
    if ((err = snd_hctl_open(&hctl,
                             QString("hw:%1").arg(card).toLatin1().constData(),
                             0)) < 0)
    {
        VBAUDIO(QString("Control %1 open error: %2")
                .arg(card)
                .arg(snd_strerror(err)));
        return NULL;
    }
    if ((err = snd_hctl_load(hctl)) < 0)
    {
        VBAUDIO(QString("Control %1 load error: %2")
                .arg(card)
                .arg(snd_strerror(err)));
        /* frees automatically the control which cannot be added. */
        return NULL;
    }
    elem = snd_hctl_find_elem(hctl, id);
    if (elem)
    {
        if ((err = snd_hctl_elem_info(elem, info)) < 0)
        {
            VBAUDIO(QString("Control %1 snd_hctl_elem_info error: %2")
                    .arg(card)
                    .arg(snd_strerror(err)));
            snd_hctl_close(hctl);
            return NULL;
        }
        count = snd_ctl_elem_info_get_count(info);
        type = snd_ctl_elem_info_get_type(info);
        if (!snd_ctl_elem_info_is_readable(info))
        {
            VBAUDIO(QString("Control %1 element info is not readable")
                    .arg(card));
            snd_hctl_close(hctl);
            return NULL;
        }
        if ((err = snd_hctl_elem_read(elem, control)) < 0)
        {
            VBAUDIO(QString("Control %1 element read error: %2")
                    .arg(card)
                    .arg(snd_strerror(err)));
            snd_hctl_close(hctl);
            return NULL;
        }
        if (type != SND_CTL_ELEM_TYPE_BYTES)
        {
            VBAUDIO(QString("Control %1 element is of the wrong type")
                    .arg(card));
            snd_hctl_close(hctl);
            return NULL;
        }
        result = new QByteArray((char *)snd_ctl_elem_value_get_bytes(control),
                                count);
    }
    snd_hctl_close(hctl);
    return result;
}

AudioOutputSettings* AudioOutputALSA::GetOutputSettings(bool passthrough)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_format_t afmt = SND_PCM_FORMAT_UNKNOWN;
    AudioFormat fmt;
    int rate;
    int err;

    AudioOutputSettings *settings = new AudioOutputSettings();

    if (pcm_handle)
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }

    if ((err = TryOpenDevice(OPEN_FLAGS, passthrough)) < 0)
    {
        AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastdevice));
        delete settings;
        return NULL;
    }

    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_hw_params_any(pcm_handle, params)) < 0)
    {
        snd_pcm_close(pcm_handle);
        if ((err = TryOpenDevice(OPEN_FLAGS&FILTER_FLAGS, passthrough)) < 0)
        {
            AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastdevice));
            delete settings;
            return NULL;
        }
        if ((err = snd_pcm_hw_params_any(pcm_handle, params)) < 0)
        {
            AERROR("No playback configurations available");
            snd_pcm_close(pcm_handle);
            pcm_handle = NULL;
            delete settings;
            return NULL;
        }
        Warn("Supported audio format detection will be inacurrate "
             "(using plugin?)");
    }

    while ((rate = settings->GetNextRate()))
        if(snd_pcm_hw_params_test_rate(pcm_handle, params, rate, 0) >= 0)
            settings->AddSupportedRate(rate);

    while ((fmt = settings->GetNextFormat()))
    {
        switch (fmt)
        {
            case FORMAT_U8:     afmt = SND_PCM_FORMAT_U8;    break;
            case FORMAT_S16:    afmt = SND_PCM_FORMAT_S16;   break;
            case FORMAT_S24LSB: afmt = SND_PCM_FORMAT_S24;   break;
            case FORMAT_S24:    afmt = SND_PCM_FORMAT_S32;   break;
            case FORMAT_S32:    afmt = SND_PCM_FORMAT_S32;   break;
            case FORMAT_FLT:    afmt = SND_PCM_FORMAT_FLOAT; break;
            default:         continue;
        }
        if (snd_pcm_hw_params_test_format(pcm_handle, params, afmt) >= 0)
            settings->AddSupportedFormat(fmt);
    }

    for (uint channels = CHANNELS_MIN; channels <= CHANNELS_MAX; channels++)
        if (snd_pcm_hw_params_test_channels(pcm_handle, params, channels) >= 0)
            settings->AddSupportedChannels(channels);

    int card, device, subdevice;
    if (GetPCMInfo(card, device, subdevice) >= 0)
    {
        // Check if we can retrieve ELD for this device
        QByteArray *eld = GetELD(card, device, subdevice);
        if (eld != NULL)
        {
            VBAUDIO(QString("Successfully retrieved ELD data"));
            settings->setELD(eld);
            delete eld;
       }
    }
    else
    {
        VBAUDIO("Can't get card and device number");
    }

    snd_pcm_close(pcm_handle);
    pcm_handle = NULL;

    /* Check if name or description contains information
       to know if device can accept passthrough or not */
    QMap<QString, QString> *alsadevs = GetDevices("pcm");
    while(1)
    {
        QString real_device = ((passthrough && m_discretedigital) ?
                               passthru_device : main_device);

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
    snd_pcm_format_t format;
    uint buffer_time, period_time;
    int err;

    if (pcm_handle != NULL)
        CloseDevice();

    if ((err = TryOpenDevice(0, passthru || enc)) < 0)
    {
        AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastdevice));
        if (pcm_handle)
            CloseDevice();
        return false;
    }

    switch (output_format)
    {
        case FORMAT_U8:     format = SND_PCM_FORMAT_U8;    break;
        case FORMAT_S16:    format = SND_PCM_FORMAT_S16;   break;
        case FORMAT_S24LSB: format = SND_PCM_FORMAT_S24;   break;
        case FORMAT_S24:    format = SND_PCM_FORMAT_S32;   break;
        case FORMAT_S32:    format = SND_PCM_FORMAT_S32;   break;
        case FORMAT_FLT:    format = SND_PCM_FORMAT_FLOAT; break;
        default:
            Error(QString("Unknown sample format: %1").arg(output_format));
            return false;
    }

    // buffer 0.5s worth of samples
    buffer_time = gCoreContext->GetNumSetting("ALSABufferOverride", 500) * 1000;

    period_time = 4;      // aim for an interrupt every (1/4th of buffer_time)

    err = SetParameters(pcm_handle, format, channels, samplerate,
                        buffer_time, period_time);
    if (err < 0)
    {
        AERROR("Unable to set ALSA parameters");
        CloseDevice();
        return false;
    }

    if (internal_vol && !OpenMixer())
        VBERROR("Unable to open audio mixer. Volume control disabled");

    // Device opened successfully
    return true;
}

void AudioOutputALSA::CloseDevice()
{
    if (m_mixer.handle)
        snd_mixer_close(m_mixer.handle);
    m_mixer.handle = NULL;
    if (pcm_handle)
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }
}

template <class AudioDataType>
static inline void _ReorderSmpteToAlsa(AudioDataType *buf, uint frames,
                                       uint extrach)
{
    AudioDataType tmpC, tmpLFE, *buf2;

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
        case  8: _ReorderSmpteToAlsa((uchar *)buf, frames, extrach); break;
        case 16: _ReorderSmpteToAlsa((short *)buf, frames, extrach); break;
        default: _ReorderSmpteToAlsa((int   *)buf, frames, extrach); break;
    }
}

void AudioOutputALSA::WriteAudio(uchar *aubuf, int size)
{
    uchar *tmpbuf = aubuf;
    uint frames = size / output_bytes_per_frame;
    int err, lw = 0;

    if (pcm_handle == NULL)
    {
        Error("WriteAudio() called with pcm_handle == NULL!");
        return;
    }

    //Audio received is in SMPTE channel order, reorder to ALSA unless passthru
    if (!passthru && (channels  == 6 || channels == 8))
    {
        ReorderSmpteToAlsa(aubuf, frames, output_format, channels - 6);
    }

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO,
            QString("WriteAudio: Preparing %1 bytes (%2 frames)")
            .arg(size).arg(frames));

    while (frames > 0)
    {
        lw = snd_pcm_writei(pcm_handle, tmpbuf, frames);

        if (lw >= 0)
        {
            if ((uint)lw < frames)
                VBAUDIO(QString("WriteAudio: short write %1 bytes (ok)")
                        .arg(lw * output_bytes_per_frame));

            frames -= lw;
            tmpbuf += lw * output_bytes_per_frame; // bytes
            continue;
        }

        err = lw;

        switch (err)
        {
            case -EPIPE:
                 if (snd_pcm_state(pcm_handle) == SND_PCM_STATE_XRUN)
                 {
                    VBAUDIO("WriteAudio: buffer underrun");
                    if ((err = snd_pcm_prepare(pcm_handle)) < 0)
                    {
                        AERROR("WriteAudio: unable to recover from xrun");
                        return;
                    }
                }
                break;

            case -ESTRPIPE:
                VBAUDIO("WriteAudio: device is suspended");
                while ((err = snd_pcm_resume(pcm_handle)) == -EAGAIN)
                    usleep(200);

                if (err < 0)
                {
                    VBERROR("WriteAudio: resume failed");
                    if ((err = snd_pcm_prepare(pcm_handle)) < 0)
                    {
                        AERROR("WriteAudio: unable to recover from suspend");
                        return;
                    }
                }
                break;

            case -EBADFD:
                Error(
                    QString("WriteAudio: device is in a bad state (state = %1)")
                    .arg(snd_pcm_state(pcm_handle)));
                return;

            default:
                AERROR(QString("WriteAudio: Write failed, state: %1, err")
                       .arg(snd_pcm_state(pcm_handle)));
                return;
        }
    }
}

int AudioOutputALSA::GetBufferedOnSoundcard(void) const
{
    if (pcm_handle == NULL)
    {
        VBERROR("getBufferedOnSoundcard() called with pcm_handle == NULL!");
        return 0;
    }

    snd_pcm_sframes_t delay = 0;

    /* Delay is the total delay from writing to the pcm until the samples
       hit the DAC - includes buffered samples and any fixed latencies */
    if (snd_pcm_delay(pcm_handle, &delay) < 0)
        return 0;

    snd_pcm_state_t state = snd_pcm_state(pcm_handle);

    if (state == SND_PCM_STATE_RUNNING || state == SND_PCM_STATE_DRAINING)
    {
        delay *= output_bytes_per_frame;
    }
    else
    {
        delay = 0;
    }

    return delay;
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
    int err;
    snd_pcm_hw_params_t  *params;
    snd_pcm_sw_params_t  *swparams;
    snd_pcm_uframes_t     period_size, period_size_min, period_size_max;
    snd_pcm_uframes_t     buffer_size, buffer_size_min, buffer_size_max;

    VBAUDIO(QString("SetParameters(format=%1, channels=%2, rate=%3, "
                    "buffer_time=%4, period_time=%5)")
            .arg(format).arg(channels).arg(rate).arg(buffer_time)
            .arg(period_time));

    if (handle == NULL)
    {
        Error("SetParameters() called with handle == NULL!");
        return -1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
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
    if (src_quality == QUALITY_DISABLED)
    {
        err = snd_pcm_hw_params_set_rate_resample(handle, params, 1);
        CHECKERR(QString("Resampling setup failed").arg(rate));

        uint rrate = rate;
        err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
        CHECKERR(QString("Rate %1Hz not available for playback: %s").arg(rate));

        if (rrate != rate)
        {
            VBERROR(QString("Rate doesn't match (requested %1Hz, got %2Hz)")
                    .arg(rate).arg(err));
            return err;
        }
    }
    else
    {
        err = snd_pcm_hw_params_set_rate(handle, params, rate, 0);
        CHECKERR(QString("Samplerate %1 Hz not available").arg(rate));
    }

    /* set the buffer time */
    err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
    err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
    err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min, NULL);
    err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max, NULL);
    VBAUDIO(QString("Buffer size range from %1 to %2")
            .arg(buffer_size_min)
            .arg(buffer_size_max));
    VBAUDIO(QString("Period size range from %1 to %2")
            .arg(period_size_min)
            .arg(period_size_max));

    /* set the buffer time */
    uint original_buffer_time = buffer_time;
    bool canincrease = true;
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                 &buffer_time, NULL);
    if (err < 0)
    {
        int dir         = -1;
        uint buftmp     = buffer_time;
        int attempt     = 0;
        do
        {
            err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                         &buffer_time, &dir);
            if (err < 0)
            {
                AERROR(QString("Unable to set buffer time to %1us, retrying")
                       .arg(buffer_time));
                    /*
                     * with some drivers, snd_pcm_hw_params_set_buffer_time_near
                     * only works once, if that's the case no point trying with
                     * different values
                     */
                if ((buffer_time <= 100000) ||
                    (attempt > 0 && buffer_time == buftmp))
                {
                    VBERROR("Couldn't set buffer time, giving up");
                    return err;
                }
                buffer_time -= 100000;
                canincrease  = false;
                attempt++;
            }
        }
        while (err < 0);
    }

    /* See if we need to increase the prealloc'd buffer size
       If buffer_time is too small we could underrun - make 10% difference ok */
    if (buffer_time * 1.10f < (float)original_buffer_time)
    {
        VBERROR(QString("Requested %1us got %2 buffer time")
                .arg(original_buffer_time).arg(buffer_time));
        // We need to increase preallocated buffer size in the driver
        if (canincrease && m_pbufsize < 0)
        {
            IncPreallocBufferSize(original_buffer_time, buffer_time);
        }
    }

    VBAUDIO(QString("Buffer time = %1 us").arg(buffer_time));

    /* set the period time */
    err = snd_pcm_hw_params_set_periods_near(handle, params,
                                             &period_time, NULL);
    CHECKERR(QString("Unable to set period time %1").arg(period_time));
    VBAUDIO(QString("Period time = %1 periods").arg(period_time));

    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    CHECKERR("Unable to set hw params for playback");

    err = snd_pcm_get_params(handle, &buffer_size, &period_size);
    CHECKERR("Unable to get PCM params");
    VBAUDIO(QString("Buffer size = %1 | Period size = %2")
            .arg(buffer_size).arg(period_size));

    /* set member variables */
    soundcard_buffer_size = buffer_size * output_bytes_per_frame;
    fragment_size = (period_size >> 1) * output_bytes_per_frame;

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

    snd_mixer_selem_channel_id_t chan = (snd_mixer_selem_channel_id_t) channel;
    if (!snd_mixer_selem_has_playback_channel(m_mixer.elem, chan))
        return retvol;

    long mixervol;
    int chk;
    if ((chk = snd_mixer_selem_get_playback_volume(m_mixer.elem,
                                                   chan,
                                                   &mixervol)) < 0)
    {
        VBERROR(QString("failed to get channel %1 volume, mixer %2/%3: %4")
                .arg(channel).arg(m_mixer.device)
                .arg(m_mixer.control)
                .arg(snd_strerror(chk)));
    }
    else
    {
        retvol = (m_mixer.volrange != 0L) ? (mixervol - m_mixer.volmin) *
                                            100.0f / m_mixer.volrange + 0.5f
                                            : 0;
        retvol = max(retvol, 0);
        retvol = min(retvol, 100);
        VBAUDIO(QString("get volume channel %1: %2")
                .arg(channel).arg(retvol));
    }
    return retvol;
}

void AudioOutputALSA::SetVolumeChannel(int channel, int volume)
{
    if (!(internal_vol && m_mixer.elem))
        return;

    long mixervol = volume * m_mixer.volrange / 100.0f - m_mixer.volmin + 0.5f;
    mixervol = max(mixervol, m_mixer.volmin);
    mixervol = min(mixervol, m_mixer.volmax);

    snd_mixer_selem_channel_id_t chan = (snd_mixer_selem_channel_id_t) channel;

    if (snd_mixer_selem_has_playback_switch(m_mixer.elem))
        snd_mixer_selem_set_playback_switch(m_mixer.elem, chan, (volume > 0));

    if (snd_mixer_selem_set_playback_volume(m_mixer.elem, chan, mixervol) < 0)
        VBERROR(QString("failed to set channel %1 volume").arg(channel));
    else
        VBAUDIO(QString("channel %1 volume set %2 => %3")
                .arg(channel).arg(volume).arg(mixervol));
}

bool AudioOutputALSA::OpenMixer(void)
{
    if (!pcm_handle)
    {
        VBERROR("mixer setup without a pcm");
        return false;
    }
    m_mixer.device = gCoreContext->GetSetting("MixerDevice", "default");
    m_mixer.device = m_mixer.device.remove(QString("ALSA:"));
    if (m_mixer.device.toLower() == "software")
        return true;

    m_mixer.control = gCoreContext->GetSetting("MixerControl", "PCM");

    QString mixer_device_tag = QString("mixer device %1").arg(m_mixer.device);

    int chk;
    if ((chk = snd_mixer_open(&m_mixer.handle, 0)) < 0)
    {
        VBERROR(QString("failed to open mixer device %1: %2")
                .arg(mixer_device_tag).arg(snd_strerror(chk)));
        return false;
    }

    QByteArray dev_ba = m_mixer.device.toLatin1();
    struct snd_mixer_selem_regopt regopts =
        {1, SND_MIXER_SABSTRACT_NONE, dev_ba.constData(), NULL, NULL};

    if ((chk = snd_mixer_selem_register(m_mixer.handle, &regopts, NULL)) < 0)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        VBERROR(QString("failed to register %1: %2")
                .arg(mixer_device_tag).arg(snd_strerror(chk)));
        return false;
    }

    if ((chk = snd_mixer_load(m_mixer.handle)) < 0)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        VBERROR(QString("failed to load %1: %2")
                .arg(mixer_device_tag).arg(snd_strerror(chk)));
        return false;
    }

    m_mixer.elem = NULL;
    uint elcount = snd_mixer_get_count(m_mixer.handle);
    snd_mixer_elem_t* elx = snd_mixer_first_elem(m_mixer.handle);

    for (uint ctr = 0; elx != NULL && ctr < elcount; ctr++)
    {
        QString tmp = QString(snd_mixer_selem_get_name(elx));
        if (m_mixer.control == tmp &&
            !snd_mixer_selem_is_enumerated(elx) &&
            snd_mixer_selem_has_playback_volume(elx) &&
            snd_mixer_selem_is_active(elx))
        {
            m_mixer.elem = elx;
            VBAUDIO(QString("found playback control %1 on %2")
                    .arg(m_mixer.control)
                    .arg(mixer_device_tag));
            break;
        }
        elx = snd_mixer_elem_next(elx);
    }
    if (!m_mixer.elem)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        VBERROR(QString("no playback control %1 found on %2")
                .arg(m_mixer.control).arg(mixer_device_tag));
        return false;
    }
    if ((snd_mixer_selem_get_playback_volume_range(m_mixer.elem,
                                                   &m_mixer.volmin,
                                                   &m_mixer.volmax) < 0))
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        VBERROR(QString("failed to get volume range on %1/%2")
                .arg(mixer_device_tag).arg(m_mixer.control));
        return false;
    }

    m_mixer.volrange = m_mixer.volmax - m_mixer.volmin;
    VBAUDIO(QString("mixer volume range on %1/%2 - min %3, max %4, range %5")
            .arg(mixer_device_tag).arg(m_mixer.control)
            .arg(m_mixer.volmin).arg(m_mixer.volmax).arg(m_mixer.volrange));
    VBAUDIO(QString("%1/%2 set up successfully")
            .arg(mixer_device_tag)
            .arg(m_mixer.control));

    if (set_initial_vol)
    {
        int initial_vol;
        if (m_mixer.control == "PCM")
            initial_vol = gCoreContext->GetNumSetting("PCMMixerVolume", 80);
        else
            initial_vol = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        for (int ch = 0; ch < channels; ++ch)
            SetVolumeChannel(ch, initial_vol);
    }

    return true;
}

QMap<QString, QString> *AudioOutputALSA::GetDevices(const char *type)
{
    QMap<QString, QString> *alsadevs = new QMap<QString, QString>();
    void **hints, **n;
    char *name, *desc;

    if (snd_device_name_hint(-1, type, &hints) < 0)
        return alsadevs;
    n = hints;

    while (*n != NULL)
    {
          name = snd_device_name_get_hint(*n, "NAME");
          desc = snd_device_name_get_hint(*n, "DESC");
          if (name && desc && strcmp(name, "null"))
              alsadevs->insert(name, desc);
          if (name)
              free(name);
          if (desc)
              free(desc);
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
