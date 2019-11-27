#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#include "config.h"

using namespace std;

#define LOC      QString("AOOSS: ")

#include "mythcorecontext.h"
#include "audiooutputoss.h"
#include "mythtimer.h"
#include "mythdate.h"

AudioOutputOSS::AudioOutputOSS(const AudioSettings &settings) :
    AudioOutputBase(settings)
{
    // Set everything up
    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
}

AudioOutputOSS::~AudioOutputOSS()
{
    KillAudio();
}

AudioOutputSettings* AudioOutputOSS::GetOutputSettings(bool /*digital*/)
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    QByteArray device = m_main_device.toLatin1();
    m_audiofd = open(device.constData(), O_WRONLY | O_NONBLOCK);

    int formats = 0;

    if (m_audiofd < 0)
    {
        VBERRENO(QString("Error opening audio device (%1)").arg(m_main_device));
        delete settings;
        return nullptr;
    }

    // NOLINTNEXTLINE(bugprone-infinite-loop)
    while (int rate = settings->GetNextRate())
    {
        int rate2 = rate;
        if(ioctl(m_audiofd, SNDCTL_DSP_SPEED, &rate2) >= 0
           && rate2 == rate)
            settings->AddSupportedRate(rate);
    }

    if(ioctl(m_audiofd, SNDCTL_DSP_GETFMTS, &formats) < 0)
        VBERRENO("Error retrieving formats");
    else
    {
        while (AudioFormat fmt = settings->GetNextFormat())
        {
            int ofmt = AFMT_QUERY;

            switch (fmt)
            {
                case FORMAT_U8:  ofmt = AFMT_U8;       break;
                case FORMAT_S16: ofmt = AFMT_S16_NE;   break;
                default: continue;
            }

            if (formats & ofmt)
                settings->AddSupportedFormat(fmt);
        }
    }

#if defined(AFMT_AC3)
        // Check if drivers supports AC3
    settings->setPassthrough(((formats & AFMT_AC3) != 0) - 1);
#endif

    for (int i = 1; i <= 2; i++)
    {
        int channel = i;

        if (ioctl(m_audiofd, SNDCTL_DSP_CHANNELS, &channel) >= 0 &&
            channel == i)
        {
            settings->AddSupportedChannels(i);
        }
    }

    close(m_audiofd);
    m_audiofd = -1;

    return settings;
}

bool AudioOutputOSS::OpenDevice()
{
    m_numbadioctls = 0;

    MythTimer timer;
    timer.start();

    VBAUDIO(QString("Opening OSS audio device '%1'.").arg(m_main_device));

    while (timer.elapsed() < 2000 && m_audiofd == -1)
    {
        QByteArray device = m_main_device.toLatin1();
        m_audiofd = open(device.constData(), O_WRONLY);
        if (m_audiofd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                VBWARN(QString("Something is currently using: %1.")
                      .arg(m_main_device));
                return false;
            }
            VBERRENO(QString("Error opening audio device (%1)")
                         .arg(m_main_device));
        }
        if (m_audiofd < 0)
            usleep(50);
    }

    if (m_audiofd == -1)
    {
        Error(QObject::tr("Error opening audio device (%1)").arg(m_main_device));
        VBERRENO(QString("Error opening audio device (%1)").arg(m_main_device));
        return false;
    }

    if (fcntl(m_audiofd, F_SETFL, fcntl(m_audiofd, F_GETFL) & ~O_NONBLOCK) == -1)
    {
        VBERRENO(QString("Error removing the O_NONBLOCK flag from audio device FD (%1)").arg(m_main_device));
    }

    bool err = false;
    int  format = AFMT_QUERY;

    switch (m_output_format)
    {
        case FORMAT_U8:  format = AFMT_U8;      break;
        case FORMAT_S16: format = AFMT_S16_NE;  break;
        default:
            VBERROR(QString("Unknown sample format: %1").arg(m_output_format));
            close(m_audiofd);
            m_audiofd = -1;
            return false;
    }

#if defined(AFMT_AC3) && defined(SNDCTL_DSP_GETFMTS)
    if (m_passthru)
    {
        int format_support = 0;
        if (!ioctl(m_audiofd, SNDCTL_DSP_GETFMTS, &format_support) &&
            (format_support & AFMT_AC3))
        {
            format = AFMT_AC3;
        }
    }
#endif

    if (m_channels > 2)
    {
        if (ioctl(m_audiofd, SNDCTL_DSP_CHANNELS, &m_channels) < 0 ||
            ioctl(m_audiofd, SNDCTL_DSP_SPEED, &m_samplerate) < 0  ||
            ioctl(m_audiofd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }
    else
    {
        int stereo = m_channels - 1;
        if (ioctl(m_audiofd, SNDCTL_DSP_STEREO, &stereo) < 0     ||
            ioctl(m_audiofd, SNDCTL_DSP_SPEED, &m_samplerate) < 0  ||
            ioctl(m_audiofd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }

    if (err)
    {
        VBERRENO(QString("Unable to set audio device (%1) to %2 kHz, %3 bits, "
                         "%4 channels")
                     .arg(m_main_device).arg(m_samplerate)
                     .arg(AudioOutputSettings::FormatToBits(m_output_format))
                     .arg(m_channels));

        close(m_audiofd);
        m_audiofd = -1;
        return false;
    }

    audio_buf_info info;
    if (ioctl(m_audiofd, SNDCTL_DSP_GETOSPACE, &info) < 0)
        VBERRENO("Error retrieving card buffer size");
    // align by frame size
    m_fragment_size = info.fragsize - (info.fragsize % m_output_bytes_per_frame);

    m_soundcard_buffer_size = info.bytes;

    int caps = 0;

    if (ioctl(m_audiofd, SNDCTL_DSP_GETCAPS, &caps) == 0)
    {
        if (!(caps & DSP_CAP_REALTIME))
            VBWARN("The audio device cannot report buffer state "
                   "accurately! audio/video sync will be bad, continuing...");
    }
    else
        VBERRENO("Unable to get audio card capabilities");

    // Setup volume control
    if (internal_vol)
        VolumeInit();

    // Device opened successfully
    return true;
}

void AudioOutputOSS::CloseDevice()
{
    if (m_audiofd != -1)
        close(m_audiofd);

    m_audiofd = -1;

    VolumeCleanup();
}


void AudioOutputOSS::WriteAudio(uchar *aubuf, int size)
{
    if (m_audiofd < 0)
        return;

    int written = 0, lw = 0;

    uchar *tmpbuf = aubuf;

    while ((written < size) &&
           ((lw = write(m_audiofd, tmpbuf, size - written)) > 0))
    {
        written += lw;
        tmpbuf += lw;
    }

    if (lw < 0)
    {
        VBERRENO(QString("Error writing to audio device (%1)")
                     .arg(m_main_device));
        return;
    }
}


int AudioOutputOSS::GetBufferedOnSoundcard(void) const
{
    int soundcard_buffer=0;
//GREG This is needs to be fixed for sure!
#ifdef SNDCTL_DSP_GETODELAY
    if(ioctl(m_audiofd, SNDCTL_DSP_GETODELAY, &soundcard_buffer) < 0) // bytes
        VBERRNOCONST("Error retrieving buffering delay");
#endif
    return soundcard_buffer;
}

void AudioOutputOSS::VolumeInit()
{
    m_mixerfd = -1;

    QString device = gCoreContext->GetSetting("MixerDevice", "/dev/mixer");
    if (device.toLower() == "software")
        return;

    QByteArray dev = device.toLatin1();
    m_mixerfd = open(dev.constData(), O_RDONLY);

    QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");

    if (controlLabel == "Master")
        m_control = SOUND_MIXER_VOLUME;
    else
        m_control = SOUND_MIXER_PCM;

    if (m_mixerfd < 0)
    {
        VBERROR(QString("Unable to open mixer: '%1'").arg(device));
        return;
    }

    if (m_set_initial_vol)
    {
        int volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        int tmpVol = (volume << 8) + volume;
        int ret = ioctl(m_mixerfd, MIXER_WRITE(SOUND_MIXER_VOLUME), &tmpVol);
        if (ret < 0)
            VBERROR(QString("Error Setting initial Master Volume") + ENO);

        volume = gCoreContext->GetNumSetting("PCMMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        ret = ioctl(m_mixerfd, MIXER_WRITE(SOUND_MIXER_PCM), &tmpVol);
        if (ret < 0)
            VBERROR(QString("Error setting initial PCM Volume") + ENO);
    }
}

void AudioOutputOSS::VolumeCleanup()
{
    if (m_mixerfd >= 0)
    {
        close(m_mixerfd);
        m_mixerfd = -1;
    }
}

int AudioOutputOSS::GetVolumeChannel(int channel) const
{
    int volume=0;
    int tmpVol=0;

    if (m_mixerfd <= 0)
        return 100;

    int ret = ioctl(m_mixerfd, MIXER_READ(m_control), &tmpVol);
    if (ret < 0)
    {
        VBERROR(QString("Error reading volume for channel %1").arg(channel));
        return 0;
    }

    if (channel == 0)
        volume = tmpVol & 0xff; // left
    else if (channel == 1)
        volume = (tmpVol >> 8) & 0xff; // right
    else
        VBERROR("Invalid channel. Only stereo volume supported");

    return volume;
}

void AudioOutputOSS::SetVolumeChannel(int channel, int volume)
{
    if (channel > 1)
    {
        // Don't support more than two channels!
        VBERROR(QString("Error setting channel %1. Only 2 ch volume supported")
                .arg(channel));
        return;
    }

    if (volume > 100)
        volume = 100;
    if (volume < 0)
        volume = 0;

    if (m_mixerfd >= 0)
    {
        int tmpVol = 0;
        if (channel == 0)
            tmpVol = (GetVolumeChannel(1) << 8) + volume;
        else
            tmpVol = (volume << 8) + GetVolumeChannel(0);

        int ret = ioctl(m_mixerfd, MIXER_WRITE(m_control), &tmpVol);
        if (ret < 0)
            VBERROR(QString("Error setting volume on channel %1").arg(channel));
    }
}
