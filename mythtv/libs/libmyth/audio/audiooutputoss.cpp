#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>

#include <iostream>
#include "config.h"

#if HAVE_SYS_SOUNDCARD_H
    #include <sys/soundcard.h>
#elif HAVE_SOUNDCARD_H
    #include <soundcard.h>
#endif

using namespace std;

#define LOC      QString("AOOSS: ")

#include "mythcorecontext.h"
#include "audiooutputoss.h"
#include "mythtimer.h"
#include "mythdate.h"

AudioOutputOSS::AudioOutputOSS(const AudioSettings &settings) :
    AudioOutputBase(settings),
    audiofd(-1), numbadioctls(0),
    mixerfd(-1), control(SOUND_MIXER_VOLUME)
{
    // Set everything up
    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

AudioOutputOSS::~AudioOutputOSS()
{
    KillAudio();
}

AudioOutputSettings* AudioOutputOSS::GetOutputSettings(bool /*digital*/)
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    QByteArray device = main_device.toLatin1();
    audiofd = open(device.constData(), O_WRONLY | O_NONBLOCK);

    AudioFormat fmt;
    int rate, formats = 0;

    if (audiofd < 0)
    {
        VBERRENO(QString("Error opening audio device (%1)").arg(main_device));
        delete settings;
        return NULL;
    }

    while ((rate = settings->GetNextRate()))
    {
        int rate2 = rate;
        if(ioctl(audiofd, SNDCTL_DSP_SPEED, &rate2) >= 0
           && rate2 == rate)
            settings->AddSupportedRate(rate);
    }

    if(ioctl(audiofd, SNDCTL_DSP_GETFMTS, &formats) < 0)
        VBERRENO("Error retrieving formats");
    else
    {
        int ofmt;

        while ((fmt = settings->GetNextFormat()))
        {
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

        if (ioctl(audiofd, SNDCTL_DSP_CHANNELS, &channel) >= 0 &&
            channel == i)
        {
            settings->AddSupportedChannels(i);
        }
    }

    close(audiofd);
    audiofd = -1;

    return settings;
}

bool AudioOutputOSS::OpenDevice()
{
    numbadioctls = 0;

    MythTimer timer;
    timer.start();

    VBAUDIO(QString("Opening OSS audio device '%1'.").arg(main_device));

    while (timer.elapsed() < 2000 && audiofd == -1)
    {
        QByteArray device = main_device.toLatin1();
        audiofd = open(device.constData(), O_WRONLY);
        if (audiofd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                VBWARN(QString("Something is currently using: %1.")
                      .arg(main_device));
                return false;
            }
            VBERRENO(QString("Error opening audio device (%1)")
                         .arg(main_device));
        }
        if (audiofd < 0)
            usleep(50);
    }

    if (audiofd == -1)
    {
        VBERRENO(QString("Error opening audio device (%1)").arg(main_device));
        return false;
    }

    fcntl(audiofd, F_SETFL, fcntl(audiofd, F_GETFL) & ~O_NONBLOCK);

    bool err = false;
    int  format;

    switch (output_format)
    {
        case FORMAT_U8:  format = AFMT_U8;      break;
        case FORMAT_S16: format = AFMT_S16_NE;  break;
        default:
            VBERROR(QString("Unknown sample format: %1").arg(output_format));
            close(audiofd);
            audiofd = -1;
            return false;
    }

#if defined(AFMT_AC3) && defined(SNDCTL_DSP_GETFMTS)
    if (passthru)
    {
        int format_support = 0;
        if (!ioctl(audiofd, SNDCTL_DSP_GETFMTS, &format_support) &&
            (format_support & AFMT_AC3))
        {
            format = AFMT_AC3;
        }
    }
#endif

    if (channels > 2)
    {
        if (ioctl(audiofd, SNDCTL_DSP_CHANNELS, &channels) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_SPEED, &samplerate) < 0  ||
            ioctl(audiofd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }
    else
    {
        int stereo = channels - 1;
        if (ioctl(audiofd, SNDCTL_DSP_STEREO, &stereo) < 0     ||
            ioctl(audiofd, SNDCTL_DSP_SPEED, &samplerate) < 0  ||
            ioctl(audiofd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }

    if (err)
    {
        VBERRENO(QString("Unable to set audio device (%1) to %2 kHz, %3 bits, "
                         "%4 channels")
                     .arg(main_device).arg(samplerate)
                     .arg(AudioOutputSettings::FormatToBits(output_format))
                     .arg(channels));

        close(audiofd);
        audiofd = -1;
        return false;
    }

    audio_buf_info info;
    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    // align by frame size
    fragment_size = info.fragsize - (info.fragsize % output_bytes_per_frame);

    soundcard_buffer_size = info.bytes;

    int caps;

    if (ioctl(audiofd, SNDCTL_DSP_GETCAPS, &caps) == 0)
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
    if (audiofd != -1)
        close(audiofd);

    audiofd = -1;

    VolumeCleanup();
}


void AudioOutputOSS::WriteAudio(uchar *aubuf, int size)
{
    if (audiofd < 0)
        return;

    uchar *tmpbuf;
    int written = 0, lw = 0;

    tmpbuf = aubuf;

    while ((written < size) &&
           ((lw = write(audiofd, tmpbuf, size - written)) > 0))
    {
        written += lw;
        tmpbuf += lw;
    }

    if (lw < 0)
    {
        VBERRENO(QString("Error writing to audio device (%1)")
                     .arg(main_device));
        return;
    }
}


int AudioOutputOSS::GetBufferedOnSoundcard(void) const
{
    int soundcard_buffer=0;
//GREG This is needs to be fixed for sure!
#ifdef SNDCTL_DSP_GETODELAY
    ioctl(audiofd, SNDCTL_DSP_GETODELAY, &soundcard_buffer); // bytes
#endif
    return soundcard_buffer;
}

void AudioOutputOSS::VolumeInit()
{
    mixerfd = -1;

    QString device = gCoreContext->GetSetting("MixerDevice", "/dev/mixer");
    if (device.toLower() == "software")
        return;

    QByteArray dev = device.toLatin1();
    mixerfd = open(dev.constData(), O_RDONLY);

    QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");

    if (controlLabel == "Master")
        control = SOUND_MIXER_VOLUME;
    else
        control = SOUND_MIXER_PCM;

    if (mixerfd < 0)
    {
        VBERROR(QString("Unable to open mixer: '%1'").arg(device));
        return;
    }

    if (set_initial_vol)
    {
        int tmpVol;
        int volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        int ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_VOLUME), &tmpVol);
        if (ret < 0)
            VBERROR(QString("Error Setting initial Master Volume") + ENO);

        volume = gCoreContext->GetNumSetting("PCMMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_PCM), &tmpVol);
        if (ret < 0)
            VBERROR(QString("Error setting initial PCM Volume") + ENO);
    }
}

void AudioOutputOSS::VolumeCleanup()
{
    if (mixerfd >= 0)
    {
        close(mixerfd);
        mixerfd = -1;
    }
}

int AudioOutputOSS::GetVolumeChannel(int channel) const
{
    int volume=0;
    int tmpVol=0;

    if (mixerfd <= 0)
        return 100;

    int ret = ioctl(mixerfd, MIXER_READ(control), &tmpVol);
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

    if (mixerfd >= 0)
    {
        int tmpVol = 0;
        if (channel == 0)
            tmpVol = (GetVolumeChannel(1) << 8) + volume;
        else
            tmpVol = (volume << 8) + GetVolumeChannel(0);

        int ret = ioctl(mixerfd, MIXER_WRITE(control), &tmpVol);
        if (ret < 0)
            VBERROR(QString("Error setting volume on channel %1").arg(channel));
    }
}

