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
              : AudioOutput()
{
    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);
    pthread_cond_init(&audio_bufsig, NULL);
    this->audiodevice = audiodevice;
    audiofd = -1;
    output_audio = 0;
    audio_bits = -1;
    audio_channels = -1;
    audio_samplerate = -1;    

    Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputOSS::~AudioOutputOSS()
{
    KillAudio();
    pthread_mutex_destroy(&audio_buflock);
    pthread_mutex_destroy(&avsync_lock);
    pthread_cond_destroy(&audio_bufsig);
}

void AudioOutputOSS::Reconfigure(int laudio_bits, int laudio_channels, 
                                 int laudio_samplerate)
{
    if (laudio_bits == audio_bits && laudio_channels == audio_channels &&
        laudio_samplerate == audio_samplerate)
        return;

//  printf("Starting reconfigure\n");
    KillAudio();
    
    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);

    audiofd = -1;
    lastaudiolen = 0;
    waud = raud = 0;
    audio_actually_paused = false;
    
    audio_channels = laudio_channels;
    audio_bits = laudio_bits;
    audio_samplerate = laudio_samplerate;
    if(audio_bits != 8 && audio_bits != 16)
    {
        Error("AudioOutputOSS only supports 8 or 16bit audio.");
        return;
    }
    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    
    killaudio = false;
    pauseaudio = false;
    
    numbadioctls = 0;
    numlowbuffer = 0;

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
                return;
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
        return;
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
        return;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    
    audio_buf_info info;
    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    fragment_size = info.fragsize;

    VERBOSE(VB_GENERAL, QString("Audio fragment size: %1")
                                 .arg(fragment_size));

    audio_buffer_unused = info.bytes - (fragment_size * 4);
    if(audio_buffer_unused < 0)
       audio_buffer_unused = 0;

    if (!gContext->GetNumSetting("AggressiveSoundcardBuffer", 0))
        audio_buffer_unused = 0;

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

    audbuf_timecode = 0;
    audiotime = 0;
    effdsp = audio_samplerate * 100;
    gettimeofday(&audiotime_updated, NULL);

    pthread_create(&output_audio, NULL, kickoffOutputAudioLoop, this);
    
    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
    VERBOSE(VB_AUDIO, "Ending reconfigure");
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

void AudioOutputOSS::KillAudio()
{
    killAudioLock.lock();

    VERBOSE(VB_AUDIO, "Killing AudioOutputDSP");
    if (output_audio)
    {
        killaudio = true;
        pthread_join(output_audio, NULL);
        output_audio = 0;
    }

    if (audiofd != -1)
        close(audiofd);

    killAudioLock.unlock();
}

bool AudioOutputOSS::GetPause(void)
{
    return audio_actually_paused;
}

void AudioOutputOSS::Pause(bool paused)
{
    pauseaudio = paused;
    audio_actually_paused = false;
}

void AudioOutputOSS::Reset()
{
    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);

    raud = waud = 0;
    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
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

void AudioOutputOSS::SetTimecode(long long timecode)
{
    pthread_mutex_lock(&audio_buflock);
    audbuf_timecode = timecode;
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputOSS::SetEffDsp(int dsprate)
{
    VERBOSE(VB_AUDIO, QString("SetEffDsp: %1").arg(dsprate));
    effdsp = dsprate;
}

void AudioOutputOSS::SetBlocking(bool blocking)
{
    this->blocking = blocking;
}

int AudioOutputOSS::audiolen(bool use_lock)
{
    /* Thread safe, returns the number of valid bytes in the audio buffer */
    int ret;
    
    if (use_lock) 
        pthread_mutex_lock(&audio_buflock);

    if (waud >= raud)
        ret = waud - raud;
    else
        ret = AUDBUFSIZE - (raud - waud);

    if (use_lock)
        pthread_mutex_unlock(&audio_buflock);

    return ret;
}

int AudioOutputOSS::audiofree(bool use_lock)
{
    return AUDBUFSIZE - audiolen(use_lock) - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is AUDBUFSIZE - 1. */
}

int AudioOutputOSS::GetAudiotime(void)
{
    /* Returns the current timecode of audio leaving the soundcard, based
       on the 'audiotime' computed earlier, and the delay since it was computed.

       This is a little roundabout...

       The reason is that computing 'audiotime' requires acquiring the audio 
       lock, which the video thread should not do. So, we call 'SetAudioTime()'
       from the audio thread, and then call this from the video thread. */
    int ret;
    struct timeval now;

    if (audiotime == 0)
        return 0;

    pthread_mutex_lock(&avsync_lock);

    gettimeofday(&now, NULL);

    ret = audiotime;
 
    ret += (now.tv_sec - audiotime_updated.tv_sec) * 1000;
    ret += (now.tv_usec - audiotime_updated.tv_usec) / 1000;

    pthread_mutex_unlock(&avsync_lock);
    return ret;
}

void AudioOutputOSS::SetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return;

    int soundcard_buffer = 0;
    int totalbuffer;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is samples/sec, multiplied by 100.
       Bytes per sample is assumed to be 4.

       'audiotimecode' is the timecode of the audio that has just been 
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer.

       'ms/byte' is given by '25000/effdsp'...
     */

    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);
 
    ioctl(audiofd, SNDCTL_DSP_GETODELAY, &soundcard_buffer); // bytes
    totalbuffer = audiolen(false) + soundcard_buffer;
               
    audiotime = audbuf_timecode - (int)(totalbuffer * 100000.0 /
                                        (audio_bytes_per_sample * effdsp));
 
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputOSS::AddSamples(char *buffers[], int samples, 
                                long long timecode)
{
    VERBOSE(VB_AUDIO, QString("AddSamples[] %1")
                              .arg(samples * audio_bytes_per_sample));
    pthread_mutex_lock(&audio_buflock);

    int audio_bytes = audio_bits / 8;
    int afree = audiofree(false);
    
    while (samples * audio_bytes_per_sample > afree)
    {
        if (blocking)
        {
            VERBOSE(VB_AUDIO, "Waiting for free space");
            // wait for more space
            pthread_cond_wait(&audio_bufsig, &audio_buflock);
            afree = audiofree(false);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
            samples = afree / audio_bytes_per_sample;
        }
    }
    
    for (int itemp = 0; itemp < samples*audio_bytes; itemp+=audio_bytes)
    {
        for(int chan = 0; chan < audio_channels; chan++)
        {
            audiobuffer[waud++] = buffers[chan][itemp];
            if(audio_bits == 16)
                audiobuffer[waud++] = buffers[chan][itemp+1];
            
            if (waud >= AUDBUFSIZE)
                waud -= AUDBUFSIZE;
        }
    }

    lastaudiolen = audiolen(false);

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    audbuf_timecode = timecode + (int)((samples * 100000.0) / effdsp);

    pthread_mutex_unlock(&audio_buflock);
    
}

void AudioOutputOSS::AddSamples(char *buffer, int samples, long long timecode)
{
    VERBOSE(VB_AUDIO, QString("AddSamples %1")
                              .arg(samples * audio_bytes_per_sample));
    pthread_mutex_lock(&audio_buflock);

    int afree = audiofree(false);

    int len = samples * audio_bytes_per_sample;
    
    while (len > afree)
    {
        if (blocking)
        {
            VERBOSE(VB_AUDIO, "Waiting for free space");
            // wait for more space
            pthread_cond_wait(&audio_bufsig, &audio_buflock);
            afree = audiofree(false);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
            len = afree;
        }
    }

    int bdiff = AUDBUFSIZE - waud;
    if (bdiff < len)
    {
        memcpy(audiobuffer + waud, buffer, bdiff);
        memcpy(audiobuffer, buffer + bdiff, len - bdiff);
    }
    else
        memcpy(audiobuffer + waud, buffer, len);

    waud = (waud + len) % AUDBUFSIZE;

    lastaudiolen = audiolen(false);

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((samples * 100000.0) / effdsp);

    pthread_mutex_unlock(&audio_buflock);
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

void AudioOutputOSS::OutputAudioLoop(void)
{
    int space_on_soundcard;
    unsigned char zeros[fragment_size];
 
    bzero(zeros, fragment_size);

    while (!killaudio)
    {
        if (audiofd < 0) 
            break;

        if (pauseaudio)
        {
            audio_actually_paused = true;
            
            //usleep(50);
            audiotime = 0; // mark 'audiotime' as invalid.

            // should this use ioctl(audio_fd, SNDCTL_DSP_POST, 0) instead ?
            
            space_on_soundcard = getSpaceOnSoundcard();
            if (fragment_size < space_on_soundcard)
            {
                WriteAudio(zeros, fragment_size);
            }
            else
            {
                VERBOSE(VB_AUDIO, QString("waiting for space to write 1024 "
                        "zeros on soundcard which has %1 bytes free")
                        .arg(space_on_soundcard));
                usleep(50);
            }

            continue;
        }
        
        SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
        
        // wait for the buffer to fill with enough to play
        if (fragment_size >= audiolen(true))
        {
            VERBOSE(VB_AUDIO, QString("audio thread waiting for buffer to fill"
                                      " fragment_size=%1, audiolen=%2")
                                      .arg(fragment_size).arg(audiolen(true)));
            usleep(200);
            continue;
        }
        
        // wait for there to be free space on the sound card so we can write
        // without blocking.  We don't want to block while holding audio_buflock
        
        space_on_soundcard = getSpaceOnSoundcard();
        if (fragment_size > space_on_soundcard)
        {
            VERBOSE(VB_AUDIO, QString("Waiting for space on soundcard: "
                                 "space=%1").arg(space_on_soundcard));
            numlowbuffer++;
            if (numlowbuffer > 5 && audio_buffer_unused)
            {
                VERBOSE(VB_IMPORTANT, "dropping back audio_buffer_unused");
                audio_buffer_unused /= 2;
            }

            usleep(200);
            continue;
        }
        else
            numlowbuffer = 0;

        pthread_mutex_lock(&audio_buflock); // begin critical section

        // re-check audiolen() in case things changed.
        // for example, ClearAfterSeek() might have run
        if (fragment_size < audiolen(false))
        {
            int bdiff = AUDBUFSIZE - raud;
            if (fragment_size > bdiff)
            {
                // always want to write whole fragments
                unsigned char fragment[fragment_size];
                memcpy(fragment, audiobuffer + raud, bdiff);
                memcpy(fragment + bdiff, audiobuffer, fragment_size - bdiff);
                WriteAudio(fragment, fragment_size);
            }
            else
            {
                WriteAudio(audiobuffer + raud, fragment_size);
            }

            /* update raud */
            raud = (raud + fragment_size) % AUDBUFSIZE;
            VERBOSE(VB_AUDIO, "Broadcasting free space avail");
            pthread_cond_broadcast(&audio_bufsig);
        }
        pthread_mutex_unlock(&audio_buflock); // end critical section
    }
    //ioctl(audiofd, SNDCTL_DSP_RESET, NULL);
}

void *AudioOutputOSS::kickoffOutputAudioLoop(void *player)
{
    VERBOSE(VB_AUDIO, QString("kickoffOutputAudioLoop: pid = %1")
                              .arg(getpid()));
    ((AudioOutputOSS *)player)->OutputAudioLoop();
    VERBOSE(VB_AUDIO, "kickoffOutputAudioLoop exiting");
    return NULL;
}

