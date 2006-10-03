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
#include <qdatetime.h>
#include "config.h"

#ifdef HAVE_SYS_SOUNDCARD_H
    #include <sys/soundcard.h>
#elif HAVE_SOUNDCARD_H
    #include <soundcard.h>
#endif

using namespace std;

#include "mythcontext.h"
#include "audiooutputoss.h"
#include "util.h"

AudioOutputOSS::AudioOutputOSS(
    QString laudio_main_device, QString           laudio_passthru_device,
    int     laudio_bits,        int               laudio_channels,
    int     laudio_samplerate,  AudioOutputSource lsource,
    bool    lset_initial_vol,   bool              laudio_passthru) :
    AudioOutputBase(laudio_main_device, laudio_passthru_device,
                    laudio_bits,        laudio_channels,
                    laudio_samplerate,  lsource,
                    lset_initial_vol,   laudio_passthru),
    audiofd(-1), numbadioctls(0),
    mixerfd(-1), control(SOUND_MIXER_VOLUME)
{
    // Set everything up
    Reconfigure(laudio_bits,       laudio_channels,
                laudio_samplerate, laudio_passthru);
}

AudioOutputOSS::~AudioOutputOSS()
{
    KillAudio();
}

bool AudioOutputOSS::OpenDevice()
{
    numbadioctls = 0;

    MythTimer timer;
    timer.start();

    VERBOSE(VB_GENERAL, QString("Opening OSS audio device '%1'.")
            .arg(audio_main_device));
    
    while (timer.elapsed() < 2000 && audiofd == -1)
    {
        audiofd = open(audio_main_device.ascii(), O_WRONLY | O_NONBLOCK);
        if (audiofd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                Error(QString("WARNING: something is currently"
                              " using: %1, retrying.").arg(audio_main_device));
                return false;
            }
            VERBOSE(VB_IMPORTANT, QString("Error opening audio device (%1), the"
                    " error was: %2").arg(audio_main_device).arg(strerror(errno)));
            perror(audio_main_device.ascii());
        }
        if (audiofd < 0)
            usleep(50);
    }

    if (audiofd == -1)
    {
        Error(QString("Error opening audio device (%1), the error was: %2")
              .arg(audio_main_device).arg(strerror(errno)));
        return false;
    }

    fcntl(audiofd, F_SETFL, fcntl(audiofd, F_GETFL) & ~O_NONBLOCK);

    SetFragSize();

    bool err = false;
    int  format;

    switch (audio_bits)
    {
        case 8:
            format = AFMT_S8;
            break;
        case 16:
#ifdef WORDS_BIGENDIAN
            format = AFMT_S16_BE;
#else
            format = AFMT_S16_LE;
#endif
            break;
        default: Error(QString("AudioOutputOSS() - Illegal bitsize of %1")
                       .arg(audio_bits));
    }

#if defined(AFMT_AC3) && defined(SNDCTL_DSP_GETFMTS)
    if (audio_passthru)
    {
        int format_support;
        if (!ioctl(audiofd, SNDCTL_DSP_GETFMTS, &format_support) &&
            (format_support & AFMT_AC3))
        {
            format = AFMT_AC3;
        }
    }
#endif

    if (audio_channels > 2)
    {
        if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }
    else
    {
        int stereo = audio_channels - 1;
        if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_STEREO, &stereo) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }

    if (err)
    {
        Error(QString("Unable to set audio device (%1) to %2 kHz / %3 bits"
                      " / %4 channels").arg(audio_main_device).arg(audio_samplerate)
                      .arg(audio_bits).arg(audio_channels));
        close(audiofd);
        audiofd = -1;
        return false;
    }

    audio_buf_info info;
    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    fragment_size = info.fragsize;

    audio_buffer_unused = info.bytes - (fragment_size * 4);
    soundcard_buffer_size = info.bytes;

    int caps;
    
    if (ioctl(audiofd, SNDCTL_DSP_GETCAPS, &caps) == 0)
    {
        if (!(caps & DSP_CAP_REALTIME))
        {
            VERBOSE(VB_IMPORTANT, "The audio device cannot report buffer state"
                    " accurately! audio/video sync will be bad, continuing...");
        }
    } else {
        VERBOSE(VB_IMPORTANT, QString("Unable to get audio card capabilities,"
                " the error was: %1").arg(strerror(errno)));
    }

    // Setup volume control
    if (internal_vol)
        VolumeInit();

    // Device opened successfully
    return true;
}

/**
 * Set the fragsize to something slightly smaller than the number of bytes of
 * audio for one frame of video.
 */
void AudioOutputOSS::SetFragSize()
{
    // I think video_frame_rate isn't necessary. Someone clearly thought it was
    // useful but I don't see why. Let's just hardcode 30 for now...
    // if there's a problem, it can be added back.
    const int video_frame_rate = 30;
    const int bits_per_byte = 8;

    // get rough measurement of audio bytes per frame of video
    int fbytes = (audio_bits * audio_channels * audio_samplerate) / 
                        (bits_per_byte * video_frame_rate);

    // find the next smaller number that's a power of 2 
    // there's probably a better way to do this
    int count = 0;
    while ( fbytes >> 1 )
    {
        fbytes >>= 1;
        count++;
    }

    if (count > 4)
    {
        // High order word is the max number of fragments
        int frag = 0x7fff0000 + count;
        ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &frag);
        // ignore failure, since we check the actual fragsize before use
    }
}

void AudioOutputOSS::CloseDevice()
{
    if (audiofd != -1)
        close(audiofd);

    audiofd = -1;
    
    VolumeCleanup();
}


void AudioOutputOSS::WriteAudio(unsigned char *aubuf, int size)
{
    if (audiofd < 0)
        return;

    unsigned char *tmpbuf;
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
        Error(QString("Error writing to audio device (%1), unable to"
              " continue. The error was: %2").arg(audio_main_device)
              .arg(strerror(errno)));
        close(audiofd);
        audiofd = -1;
        return;
    }
}


inline int AudioOutputOSS::getBufferedOnSoundcard(void) 
{
    int soundcard_buffer=0;
//GREG This is needs to be fixed for sure!
#ifdef SNDCTL_DSP_GETODELAY
    ioctl(audiofd, SNDCTL_DSP_GETODELAY, &soundcard_buffer); // bytes
#endif
    return soundcard_buffer;
}


inline int AudioOutputOSS::getSpaceOnSoundcard(void)
{
    audio_buf_info info;
    int space = 0;

    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    space = info.bytes - audio_buffer_unused;

    if (space < 0)
    {
        numbadioctls++;
        if (numbadioctls > 2 || space < -5000)
        {
            VERBOSE(VB_IMPORTANT, "Your soundcard is not reporting free space"
                    " correctly. Falling back to old method...");
            audio_buffer_unused = 0;
            space = info.bytes;
        }
    }
    else
        numbadioctls = 0;

    return space;
}

void AudioOutputOSS::VolumeInit()
{
    mixerfd = -1;
    int volume = 0;

    QString device = gContext->GetSetting("MixerDevice", "/dev/mixer");
    mixerfd = open(device.ascii(), O_RDONLY);

    QString controlLabel = gContext->GetSetting("MixerControl", "PCM");

    if (controlLabel == "Master")
    {
        control = SOUND_MIXER_VOLUME;
    }
    else
    {
        control = SOUND_MIXER_PCM;
    }

    if (mixerfd < 0)
    {
        cerr << "Unable to open mixer: '" << device << "'\n";
        return;
    }

    if (set_initial_vol)
    {
        int tmpVol;
        volume = gContext->GetNumSetting("MasterMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        int ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_VOLUME), &tmpVol);
        if (ret < 0) {
            VERBOSE(VB_IMPORTANT, QString("Error Setting initial Master Volume"));
            perror("Setting master volume: ");
        }

        volume = gContext->GetNumSetting("PCMMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        ret = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_PCM), &tmpVol);
        if (ret < 0) {
            VERBOSE(VB_IMPORTANT, QString("Error setting initial PCM Volume"));
            perror("Setting PCM volume: ");
        }
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

int AudioOutputOSS::GetVolumeChannel(int channel)
{
    int volume=0;
    int tmpVol=0;

    if (mixerfd <= 0)
        return 100;

    int ret = ioctl(mixerfd, MIXER_READ(control), &tmpVol);
    if (ret < 0) {
        VERBOSE(VB_IMPORTANT, QString("Error reading volume for channel %1")
                          .arg(channel));
        perror("Reading PCM volume: ");
        return 0;
    }

    if (channel == 0) {
        volume = tmpVol & 0xff; // left
    } else if (channel == 1) {
        volume = (tmpVol >> 8) & 0xff; // right
    } else {
        VERBOSE(VB_IMPORTANT, QString("Invalid channel. Only stereo volume supported"));
    }

    return volume;
}

void AudioOutputOSS::SetVolumeChannel(int channel, int volume)
{
    if (channel > 1) {
        // Don't support more than two channels!
    VERBOSE(VB_IMPORTANT, QString("Error setting channel: %1.  Only stereo volume supported")
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
        {
            VERBOSE(VB_IMPORTANT, QString("Error setting volume on channel: %1").arg(channel));
            perror("Setting volume: ");
        }
    }
}

