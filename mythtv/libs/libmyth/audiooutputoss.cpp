#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>

#include <iostream>
#include <qdatetime.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputoss.h"

AudioOutputOSS::AudioOutputOSS(QString audiodevice, int laudio_bits, 
                               int laudio_channels, int laudio_samplerate)
              : AudioOutputBase(audiodevice)
{
    // our initalisation
    audiofd = -1;
    numbadioctls = 0;

    // Set everything up
    Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputOSS::~AudioOutputOSS()
{
    KillAudio();
}

bool AudioOutputOSS::OpenDevice()
{
    numbadioctls = 0;

    QTime timer;
    timer.start();

    VERBOSE(VB_GENERAL, QString("Opening OSS audio device '%1'.")
            .arg(audiodevice));
    
    while (timer.elapsed() < 2000 && audiofd == -1)
    {
        audiofd = open(audiodevice.ascii(), O_WRONLY | O_NONBLOCK);
        if (audiofd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                Error(QString("WARNING: something is currently"
                              " using: %1, retrying.").arg(audiodevice));
                return false;
            }
            VERBOSE(VB_IMPORTANT, QString("Error opening audio device (%1), the"
                    " error was: %2").arg(audiodevice).arg(strerror(errno)));
            perror(audiodevice.ascii());
        }
        if (audiofd < 0)
            usleep(50);
    }

    if (audiofd == -1)
    {
        Error(QString("Error opening audio device (%1), the error was: %2")
              .arg(audiodevice).arg(strerror(errno)));
        return false;
    }

    fcntl(audiofd, F_SETFL, fcntl(audiofd, F_GETFL) & ~O_NONBLOCK);

    SetFragSize();

    bool err = false;

    if (audio_channels > 2)
    {
        if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
            err = true;
    }
    else
    {
        int stereo = audio_channels - 1;
        if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_STEREO, &stereo) < 0 ||
            ioctl(audiofd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
            err = true;
    }

    if (err)
    {
        Error(QString("Unable to set audio device (%1) to %2 kHz / %3 bits"
                      " / %4 channels").arg(audiodevice).arg(audio_samplerate)
                      .arg(audio_bits).arg(audio_channels));
        close(audiofd);
        audiofd = -1;
        return false;
    }

    audio_buf_info info;
    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    fragment_size = info.fragsize;

    audio_buffer_unused = info.bytes - (fragment_size * 4);

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
              " continue. The error was: %2").arg(audiodevice)
              .arg(strerror(errno)));
        close(audiofd);
        audiofd = -1;
        return;
    }
}


inline int AudioOutputOSS::getBufferedOnSoundcard(void) 
{
    int soundcard_buffer=0;
    ioctl(audiofd, SNDCTL_DSP_GETODELAY, &soundcard_buffer); // bytes
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

