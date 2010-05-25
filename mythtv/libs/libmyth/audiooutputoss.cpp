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
#define LOC_WARN QString("AOOSS, Warning: ")
#define LOC_ERR  QString("AOOSS, Error: ")

#define OERROR(str) Error(str + QString(": %1").arg(strerror(errno)))

#include "mythcorecontext.h"
#include "audiooutputoss.h"
#include "util.h"

AudioOutputOSS::AudioOutputOSS(const AudioSettings &settings) :
    AudioOutputBase(settings),
    audiofd(-1), numbadioctls(0),
    mixerfd(-1), control(SOUND_MIXER_VOLUME)
{
    // Set everything up
    Reconfigure(settings);
}

AudioOutputOSS::~AudioOutputOSS()
{
    KillAudio();
}

AudioOutputSettings* AudioOutputOSS::GetOutputSettings()
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    QByteArray device = main_device.toAscii();
    audiofd = open(device.constData(), O_WRONLY | O_NONBLOCK);

    AudioFormat fmt;
    int rate, formats;

    if (audiofd < 0)
    {
        OERROR(QString("Error opening audio device (%1)").arg(main_device));
        return settings;
    }

    while ((rate = settings->GetNextRate()))
        if(ioctl(audiofd, SNDCTL_DSP_SPEED, &rate) >= 0)
            settings->AddSupportedRate(rate);

    if(ioctl(audiofd, SNDCTL_DSP_GETFMTS, &formats) < 0)
        OERROR("Error retrieving formats");
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

    for (uint i = 2; i < 9; i++)
        if (ioctl(audiofd, SNDCTL_DSP_CHANNELS, &i) >= 0)
            settings->AddSupportedChannels(i);

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
        QByteArray device = main_device.toAscii();
        audiofd = open(device.constData(), O_WRONLY | O_NONBLOCK);
        if (audiofd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                Error(QString("WARNING: something is currently using: %1.")
                      .arg(main_device));
                return false;
            }
            OERROR(QString("Error opening audio device (%1)").arg(main_device));
        }
        if (audiofd < 0)
            usleep(50);
    }

    if (audiofd == -1)
    {
        OERROR(QString("Error opening audio device (%1)").arg(main_device));
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
            Error(QString("Unknown sample format: %1").arg(output_format));
            close(audiofd);
            audiofd = -1;
            return false;
    }

#if defined(AFMT_AC3) && defined(SNDCTL_DSP_GETFMTS)
    if (passthru)
    {
        int format_support;
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
        OERROR(QString("Unable to set audio device (%1) to %2 kHz, %3 bits"
                       ", %4 channels")
               .arg(main_device).arg(samplerate)
               .arg(AudioOutputSettings::FormatToBits(output_format))
               .arg(channels));

        close(audiofd);
        audiofd = -1;
        return false;
    }

    audio_buf_info info;
    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    fragment_size = info.fragsize;

    soundcard_buffer_size = info.bytes;

    int caps;

    if (ioctl(audiofd, SNDCTL_DSP_GETCAPS, &caps) == 0)
    {
        if (!(caps & DSP_CAP_REALTIME))
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "The audio device cannot report buffer state"
                    " accurately! audio/video sync will be bad, continuing...");
    }
    else
        OERROR("Unable to get audio card capabilities");

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
        OERROR(QString("Error writing to audio device (%1)")
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
    int volume = 0;

    QString device = gCoreContext->GetSetting("MixerDevice", "/dev/mixer");
    if (device.toLower() == "software")
        return;

    QByteArray dev = device.toAscii();
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
        volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
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

