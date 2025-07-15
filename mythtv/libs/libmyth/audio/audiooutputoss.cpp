#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimer.h"

#include "audiooutputoss.h"

#define LOC      QString("AOOSS: ")

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
    auto *settings = new AudioOutputSettings();

    QByteArray device = m_mainDevice.toLatin1();
    m_audioFd = open(device.constData(), O_WRONLY | O_NONBLOCK);

    int formats = 0;

    if (m_audioFd < 0)
    {
        Error(LOC + QString("Error opening audio device (%1").arg(m_mainDevice) + ": " + ENO);
        delete settings;
        return nullptr;
    }

    // NOLINTNEXTLINE(bugprone-infinite-loop)
    while (int rate = settings->GetNextRate())
    {
        int rate2 = rate;
        if(ioctl(m_audioFd, SNDCTL_DSP_SPEED, &rate2) >= 0
           && rate2 == rate)
            settings->AddSupportedRate(rate);
    }

    if(ioctl(m_audioFd, SNDCTL_DSP_GETFMTS, &formats) < 0)
        Error(LOC + "Error retrieving formats" + ": " + ENO);
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
    settings->setPassthrough(static_cast<int>((formats & AFMT_AC3) != 0) - 1);
#endif

    for (int i = 1; i <= 2; i++)
    {
        int channel = i;

        if (ioctl(m_audioFd, SNDCTL_DSP_CHANNELS, &channel) >= 0 &&
            channel == i)
        {
            settings->AddSupportedChannels(i);
        }
    }

    close(m_audioFd);
    m_audioFd = -1;

    return settings;
}

bool AudioOutputOSS::OpenDevice()
{
    m_numBadIoctls = 0;

    MythTimer timer;
    timer.start();

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Opening OSS audio device '%1'.").arg(m_mainDevice));

    while (timer.elapsed() < 2s && m_audioFd == -1)
    {
        QByteArray device = m_mainDevice.toLatin1();
        m_audioFd = open(device.constData(), O_WRONLY);
        if (m_audioFd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Something is currently using: %1.")
                      .arg(m_mainDevice));
                return false;
            }
            Error(LOC + QString("Error opening audio device (%1)")
                         .arg(m_mainDevice) + ": " + ENO);
            return false;
        }
        if (m_audioFd < 0)
            usleep(50us);
    }

    if (m_audioFd == -1)
    {
        Error(QObject::tr("Error opening audio device (%1)").arg(m_mainDevice));
        Error(LOC + QString("Error opening audio device (%1)").arg(m_mainDevice) + ": " + ENO);
        return false;
    }

    if (fcntl(m_audioFd, F_SETFL, fcntl(m_audioFd, F_GETFL) & ~O_NONBLOCK) == -1)
    {
        Error(LOC + QString("Error removing the O_NONBLOCK flag from audio device FD (%1)").arg(m_mainDevice) + ": " + ENO);
    }

    bool err = false;
    int  format = AFMT_QUERY;

    switch (m_outputFormat)
    {
        case FORMAT_U8:  format = AFMT_U8;      break;
        case FORMAT_S16: format = AFMT_S16_NE;  break;
        default:
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown sample format: %1").arg(m_outputFormat));
            close(m_audioFd);
            m_audioFd = -1;
            return false;
    }

#if defined(AFMT_AC3) && defined(SNDCTL_DSP_GETFMTS)
    if (m_passthru)
    {
        int format_support = 0;
        if (!ioctl(m_audioFd, SNDCTL_DSP_GETFMTS, &format_support) &&
            (format_support & AFMT_AC3))
        {
            format = AFMT_AC3;
        }
    }
#endif

    if (m_channels > 2)
    {
        if (ioctl(m_audioFd, SNDCTL_DSP_CHANNELS, &m_channels) < 0 ||
            ioctl(m_audioFd, SNDCTL_DSP_SPEED, &m_sampleRate) < 0  ||
            ioctl(m_audioFd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }
    else
    {
        int stereo = m_channels - 1;
        if (ioctl(m_audioFd, SNDCTL_DSP_STEREO, &stereo) < 0     ||
            ioctl(m_audioFd, SNDCTL_DSP_SPEED, &m_sampleRate) < 0  ||
            ioctl(m_audioFd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }

    if (err)
    {
        Error(LOC + QString("Unable to set audio device (%1) to %2 kHz, %3 bits, "
                         "%4 channels")
                     .arg(m_mainDevice).arg(m_sampleRate)
                     .arg(AudioOutputSettings::FormatToBits(m_outputFormat))
                     .arg(m_channels) + ": " + ENO);

        close(m_audioFd);
        m_audioFd = -1;
        return false;
    }

    audio_buf_info info;
    if (ioctl(m_audioFd, SNDCTL_DSP_GETOSPACE, &info) < 0)
        Error(LOC + "Error retrieving card buffer size" + ": " + ENO);
    // align by frame size
    m_fragmentSize = info.fragsize - (info.fragsize % m_outputBytesPerFrame);

    m_soundcardBufferSize = info.bytes;

    int caps = 0;

    if (ioctl(m_audioFd, SNDCTL_DSP_GETCAPS, &caps) == 0)
    {
        if (!(caps & DSP_CAP_REALTIME))
            LOG(VB_GENERAL, LOG_WARNING, LOC + "The audio device cannot report buffer state "
                   "accurately! audio/video sync will be bad, continuing...");
    }
    else
    {
        Error(LOC + "Unable to get audio card capabilities" + ": " + ENO);
    }

    // Setup volume control
    if (m_internalVol)
        VolumeInit();

    // Device opened successfully
    return true;
}

void AudioOutputOSS::CloseDevice()
{
    if (m_audioFd != -1)
        close(m_audioFd);

    m_audioFd = -1;

    VolumeCleanup();
}


void AudioOutputOSS::WriteAudio(uchar *aubuf, int size)
{
    if (m_audioFd < 0)
        return;

    int written = 0;
    int lw = 0;

    uchar *tmpbuf = aubuf;

    while ((written < size) &&
           ((lw = write(m_audioFd, tmpbuf, size - written)) > 0))
    {
        written += lw;
        tmpbuf += lw;
    }

    if (lw < 0)
    {
        Error(LOC + QString("Error writing to audio device (%1)")
                     .arg(m_mainDevice) + ": " + ENO);
        return;
    }
}


int AudioOutputOSS::GetBufferedOnSoundcard(void) const
{
    int soundcard_buffer=0;
//GREG This is needs to be fixed for sure!
#ifdef SNDCTL_DSP_GETODELAY
    if(ioctl(m_audioFd, SNDCTL_DSP_GETODELAY, &soundcard_buffer) < 0) // bytes
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error retrieving buffering delay" + ": " + ENO);
#endif
    return soundcard_buffer;
}

void AudioOutputOSS::VolumeInit()
{
    m_mixerFd = -1;

    QString device = gCoreContext->GetSetting("MixerDevice", "/dev/mixer");
    if (device.toLower() == "software")
        return;

    QByteArray dev = device.toLatin1();
    m_mixerFd = open(dev.constData(), O_RDONLY);

    QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");

    if (controlLabel == "Master")
        m_control = SOUND_MIXER_VOLUME;
    else
        m_control = SOUND_MIXER_PCM;

    if (m_mixerFd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to open mixer: '%1'").arg(device));
        return;
    }

    if (m_setInitialVol)
    {
        int volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        int tmpVol = (volume << 8) + volume;
        int ret = ioctl(m_mixerFd, MIXER_WRITE(SOUND_MIXER_VOLUME), &tmpVol);
        if (ret < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error Setting initial Master Volume") + ENO);

        volume = gCoreContext->GetNumSetting("PCMMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        ret = ioctl(m_mixerFd, MIXER_WRITE(SOUND_MIXER_PCM), &tmpVol);
        if (ret < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error setting initial PCM Volume") + ENO);
    }
}

void AudioOutputOSS::VolumeCleanup()
{
    if (m_mixerFd >= 0)
    {
        close(m_mixerFd);
        m_mixerFd = -1;
    }
}

int AudioOutputOSS::GetVolumeChannel(int channel) const
{
    int volume=0;
    int tmpVol=0;

    if (m_mixerFd <= 0)
        return 100;

    int ret = ioctl(m_mixerFd, MIXER_READ(m_control), &tmpVol);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error reading volume for channel %1").arg(channel));
        return 0;
    }

    if (channel == 0)
        volume = tmpVol & 0xff; // left
    else if (channel == 1)
        volume = (tmpVol >> 8) & 0xff; // right
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid channel. Only stereo volume supported");

    return volume;
}

void AudioOutputOSS::SetVolumeChannel(int channel, int volume)
{
    if (channel > 1)
    {
        // Don't support more than two channels!
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error setting channel %1. Only 2 ch volume supported")
                .arg(channel));
        return;
    }

    volume = std::clamp(volume, 0, 100);

    if (m_mixerFd >= 0)
    {
        int tmpVol = 0;
        if (channel == 0)
            tmpVol = (GetVolumeChannel(1) << 8) + volume;
        else
            tmpVol = (volume << 8) + GetVolumeChannel(0);

        int ret = ioctl(m_mixerFd, MIXER_WRITE(m_control), &tmpVol);
        if (ret < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error setting volume on channel %1").arg(channel));
    }
}
