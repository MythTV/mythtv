#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputalsa.h"


AudioOutputALSA::AudioOutputALSA(QString audiodevice, int laudio_bits, 
                                 int laudio_channels, int laudio_samplerate)
               : AudioOutput()
{
    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);
    pthread_cond_init(&audio_bufsig, NULL);

    this->audiodevice = audiodevice;
    pcm_handle = NULL;
    output_audio = 0;
    audio_bits = -1;
    audio_channels = -1;
    audio_samplerate = -1;    

    Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputALSA::~AudioOutputALSA()
{
    KillAudio();

    pthread_mutex_destroy(&audio_buflock);
    pthread_mutex_destroy(&avsync_lock);
    pthread_cond_destroy(&audio_bufsig);
}

void AudioOutputALSA::Reconfigure(int laudio_bits, int laudio_channels, 
                                  int laudio_samplerate)
{
    snd_pcm_t *new_pcm_handle;
    snd_pcm_format_t format;
    unsigned int buffer_time = 500000, period_time = 100000;

    int err;

    // if pcm_handle is NULL allow Reconfigure
    if (pcm_handle != NULL && laudio_bits == audio_bits &&
          laudio_channels == audio_channels &&
          laudio_samplerate == audio_samplerate)
        return;

    KillAudio();
    
    new_pcm_handle = pcm_handle;
    pcm_handle = NULL;

    if (new_pcm_handle != NULL)
        snd_pcm_hw_free(new_pcm_handle);

    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);

    lastaudiolen = 0;
    waud = raud = 0;
    audio_actually_paused = false;
    
    audio_channels = laudio_channels;
    audio_bits = laudio_bits;
    audio_samplerate = laudio_samplerate;

    if (audio_bits != 8 && audio_bits != 16)
    {
        Error("AudioOutputALSA only supports 8 or 16bit audio.");
        return;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    
    killaudio = false;
    pauseaudio = false;
    
    numbadioctls = 0;
    numlowbuffer = 0;

    VERBOSE(VB_GENERAL, QString("Opening ALSA audio device '%1'.")
            .arg(audiodevice));
    
    err = snd_pcm_open(&pcm_handle, audiodevice,
          SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK); 

    if (err < 0)
    { 
        Error(QString("snd_pcm_open(%1) error %2")
              .arg(audiodevice).arg(snd_strerror(err)));
    }

    SetFragSize();

    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    
    fragment_size = 4096;

    VERBOSE(VB_GENERAL, QString("Audio: fragment_size=%1, bytes_per_sample=%2")
            .arg(fragment_size).arg(audio_bytes_per_sample));

    snd_pcm_uframes_t avail = 0;
    snd_pcm_hw_params_t *hw_params;
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        Error(QString("cannot allocate hardware parameter structure (%1)")
              .arg(snd_strerror(err)));
    }
    snd_pcm_hw_params_current(pcm_handle, hw_params);
    snd_pcm_hw_params_get_buffer_size(hw_params, &avail); // frames
    snd_pcm_hw_params_free(hw_params);

    audio_buffer_unused = (avail * audio_bytes_per_sample) - 
                          (fragment_size * 4);

    VERBOSE(VB_AUDIO, QString("Audio buffer size=%1 bytes unused=%2")
            .arg(avail*audio_bytes_per_sample).arg(audio_buffer_unused));
    
    if (audio_buffer_unused < 0)
        audio_buffer_unused = 0;

    if (!gContext->GetNumSetting("AggressiveSoundcardBuffer", 0))
        audio_buffer_unused = 0;

    if (audio_bits == 8)
        format = SND_PCM_FORMAT_S8;
    else if (audio_bits == 16)
        // is the sound data coming in really little-endian or is it
        // CPU-endian?
        format = SND_PCM_FORMAT_S16_LE;
    else if (audio_bits == 24)
        format = SND_PCM_FORMAT_S24_LE;
    else
    {
        Error(QString("Unknown sample format: %1 bits.").arg(audio_bits));
        return;
    }

    err = SetParameters(pcm_handle, SND_PCM_ACCESS_MMAP_INTERLEAVED,
                        format, audio_channels, audio_samplerate, buffer_time,
                        period_time);
    if (err < 0) 
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
        return;
    }    

    audbuf_timecode = 0;
    audiotime = 0;
    effdsp = audio_samplerate * 100;
    gettimeofday(&audiotime_updated, NULL);

    if (output_audio == 0)  // in case we already started thread
        pthread_create(&output_audio, NULL, kickoffOutputAudioLoop, this);
    
    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
    VERBOSE(VB_AUDIO, "Ending reconfigure");
}

/**
 * Set the fragsize to something slightly smaller than the number of bytes of
 * audio for one frame of video.
 */
void AudioOutputALSA::SetFragSize()
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
        // ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &frag);
        // ignore failure, since we check the actual fragsize before use
    }
}

void AudioOutputALSA::KillAudio()
{
    killAudioLock.lock();

    VERBOSE(VB_AUDIO, "Killing AudioOutputDSP");
    if (output_audio)
    {
        killaudio = true;
        pthread_join(output_audio, NULL);
        output_audio = 0;
    }

    if (pcm_handle != NULL)
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }

    killAudioLock.unlock();
}

bool AudioOutputALSA::GetPause(void)
{
    return audio_actually_paused;
}

void AudioOutputALSA::Pause(bool paused)
{
    pauseaudio = paused;
    audio_actually_paused = false;
}

void AudioOutputALSA::Reset()
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

void AudioOutputALSA::WriteAudio(unsigned char *aubuf, int size)
{
    if (pcm_handle == NULL)
        return;

    unsigned char *tmpbuf;
    int lw = 0;
    int frames = size / audio_bytes_per_sample;

    tmpbuf = aubuf;

    VERBOSE(VB_AUDIO, QString("Preparing %1 bytes (%2 frames) in WriteAudio")
            .arg(size).arg(frames));
    
    while (frames > 0) 
    {
        lw = snd_pcm_mmap_writei(pcm_handle, tmpbuf, frames);
        if (lw >= 0)
        {
            frames -= lw;
            tmpbuf += lw * audio_bytes_per_sample; // bytes
        } 
        else if (lw == -EAGAIN)
        {
            VERBOSE(VB_AUDIO, QString("Soundcard is blocked.  Waiting for card to become ready"));
            snd_pcm_wait(pcm_handle, 10);
        }
        else if (lw == -EPIPE &&
                 snd_pcm_state(pcm_handle) == SND_PCM_STATE_XRUN &&
                 snd_pcm_prepare(pcm_handle) == 0)
        {
            VERBOSE(VB_AUDIO, "WriteAudio: xrun (buffer underrun)");
            continue;
        }
        else if (lw == -EPIPE &&
                 snd_pcm_state(pcm_handle) == SND_PCM_STATE_SUSPENDED)
        {
            VERBOSE(VB_AUDIO, "WriteAudio: suspended");

            while ((lw = snd_pcm_resume(pcm_handle)) == -EAGAIN)
                usleep(200);

            if (lw < 0 && (lw = snd_pcm_prepare(pcm_handle)) == 0)
                continue;
        }

        if (lw < 0)
        {
            Error(QString("snd_pcm_mmap_writei(%1,frames=%2) error %3: %4")
                  .arg(audiodevice).arg(frames).arg(snd_strerror(lw)));
            snd_pcm_close(pcm_handle);
            pcm_handle = NULL;
            return;
        }
    }
}

void AudioOutputALSA::SetTimecode(long long timecode)
{
    pthread_mutex_lock(&audio_buflock);
    audbuf_timecode = timecode;
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputALSA::SetEffDsp(int dsprate)
{
    VERBOSE(VB_AUDIO, QString("SetEffDsp: %1").arg(dsprate));
    effdsp = dsprate;
}

void AudioOutputALSA::SetBlocking(bool blocking)
{
    this->blocking = blocking;
}

int AudioOutputALSA::audiolen(bool use_lock)
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

int AudioOutputALSA::audiofree(bool use_lock)
{
    return AUDBUFSIZE - audiolen(use_lock) - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is AUDBUFSIZE - 1. */
}

int AudioOutputALSA::GetAudiotime(void)
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

void AudioOutputALSA::SetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return;

    int totalbuffer, err;

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
 
    snd_pcm_uframes_t soundcard_buffer = 0;
    snd_pcm_hw_params_t *hw_params;
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        Error(QString("cannot allocate hardware parameter structure (%1)")
              .arg(snd_strerror(err)));
    }
    snd_pcm_hw_params_current(pcm_handle, hw_params);
    snd_pcm_hw_params_get_buffer_size(hw_params, &soundcard_buffer); // frames
    snd_pcm_hw_params_free(hw_params);

    totalbuffer = audiolen(false) + (soundcard_buffer * audio_bytes_per_sample);
               
    audiotime = audbuf_timecode - (int)(totalbuffer * 100000.0 /
                                        (audio_bytes_per_sample * effdsp));
 
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputALSA::AddSamples(char *buffers[], int samples, 
                                long long timecode)
{
    pthread_mutex_lock(&audio_buflock);

    int audio_bytes = audio_bits / 8;
    // VERBOSE(VB_AUDIO, QString("audio_bytes : %1").arg(audio_bytes));
    int afree = audiofree(false);
    // VERBOSE(VB_AUDIO, QString("afree : %1").arg(afree));

    VERBOSE(VB_AUDIO, QString("AddSamples[] bytes=%1, free=%2")
            .arg(samples * audio_bytes_per_sample).arg(afree));
    
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
            VERBOSE(VB_IMPORTANT, QString("Audio buffer overflow, audio data lost!"
                                          "[] afree=%1 need=%2")
                    .arg(afree).arg(samples * audio_bytes_per_sample));
            samples = afree / audio_bytes_per_sample;
        }
    }
    
    for (int itemp = 0; itemp < samples*audio_bytes; itemp+=audio_bytes)
    {
        for(int chan = 0; chan < audio_channels; chan++)
        {
            audiobuffer[waud++] = buffers[chan][itemp];
            if (audio_bits == 16)
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

void AudioOutputALSA::AddSamples(char *buffer, int samples, long long timecode)
{
    pthread_mutex_lock(&audio_buflock);

    int afree = audiofree(false);

    VERBOSE(VB_AUDIO, QString("AddSamples bytes=%1 free=%2")
            .arg(samples * audio_bytes_per_sample).arg(afree));

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
            VERBOSE(VB_IMPORTANT, QString("Audio buffer overflow, audio data"
                                          "lost! afree=%1 need=%2")
                    .arg(afree).arg(samples * audio_bytes_per_sample));
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

inline int AudioOutputALSA::getSpaceOnSoundcard(void)
{
    // audio_buf_info info;
    // long avail = 0;
    int space = 0;
    int err = 0;

    if (pcm_handle == NULL)
      return 0;

    snd_pcm_uframes_t soundcard_buffer = 0; // total buffer on soundcard
    snd_pcm_hw_params_t *hw_params;
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        Error(QString("cannot allocate hardware parameter structure (%1)")
              .arg(snd_strerror(err)));
    }
    snd_pcm_hw_params_current(pcm_handle, hw_params);
    snd_pcm_hw_params_get_buffer_size(hw_params, &soundcard_buffer); // frames
    snd_pcm_hw_params_free(hw_params);

    snd_pcm_sframes_t avail = 0;
    snd_pcm_avail_update(pcm_handle);
    snd_pcm_delay(pcm_handle, &avail);

    // make sure that we are actually in the running state otherwise
    // snd_pcm_delay return is meaningless
    if (snd_pcm_state(pcm_handle) < SND_PCM_STATE_RUNNING)
        VERBOSE(VB_IMPORTANT, QString("Not in the running state, state=%1")
                .arg(snd_pcm_state(pcm_handle)));

    // Free space is the total buffer minus the frames waiting to be written
    space = ((soundcard_buffer - avail) * audio_bytes_per_sample) - audio_buffer_unused;
    VERBOSE(VB_AUDIO, QString("getSpaceOnSoundcard : %1 %2 %3 %4")
            .arg(soundcard_buffer).arg(avail).arg(audio_buffer_unused).arg(space));

    if (space < 0)
    {
        numbadioctls++;
        if (numbadioctls > 2 || space < -5000)
        {
            VERBOSE(VB_IMPORTANT, "Your soundcard is not reporting free space"
                    " correctly. Falling back to old method...");
            audio_buffer_unused = 0;
            // space = info.bytes;
            space = avail;
        }
    }
    else
        numbadioctls = 0;

    return space;
}

void AudioOutputALSA::OutputAudioLoop(void)
{
    int space_on_soundcard;
    unsigned char zeros[fragment_size];
 
    bzero(zeros, fragment_size);

    while (!killaudio)
    {
        // if we don't have an audio output handle try to open again
        // this can happen if WriteAudio fails and we close pcm_handle
        if (pcm_handle == NULL)
            Reconfigure(audio_bits, audio_channels, audio_samplerate);

        // if pcm_handle still NULL break the loop
        if (pcm_handle == NULL) 
            break;

        if (pauseaudio)
        {
            audio_actually_paused = true;
            
            //usleep(50);
            audiotime = 0; // mark 'audiotime' as invalid.

            space_on_soundcard = getSpaceOnSoundcard();
            if (fragment_size <= space_on_soundcard)
            {
                WriteAudio(zeros, fragment_size);
            }
            else
            {
                VERBOSE(VB_AUDIO, QString("waiting for space on soundcard to write zeros: have %1 need %2")
                        .arg(space_on_soundcard).arg(fragment_size));
                usleep(50);
            }

            continue;
        }
        
        SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
        
        // wait for the buffer to fill with enough to play
        if (fragment_size > audiolen(true))
        {
             if (audiolen(true) > 0)  // don't log unless we are sending some audio
                 VERBOSE(VB_AUDIO, QString("audio waiting for buffer to fill: have %1 want %2")
                         .arg(audiolen(true)).arg(fragment_size));

            usleep(200);
            continue;
        }
        
        // wait for there to be free space on the sound card so we can write
        // without blocking.  We don't want to block while holding audio_buflock
        
        space_on_soundcard = getSpaceOnSoundcard();
        if (fragment_size > space_on_soundcard)
        {
            VERBOSE(VB_AUDIO, QString("audio waiting for space on soundcard: have %1 need %2")
                    .arg(space_on_soundcard).arg(fragment_size));
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
        if (fragment_size <= audiolen(false))
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

void *AudioOutputALSA::kickoffOutputAudioLoop(void *player)
{
    VERBOSE(VB_AUDIO, QString("kickoffOutputAudioLoop: pid = %1")
                              .arg(getpid()));
    ((AudioOutputALSA *)player)->OutputAudioLoop();
    VERBOSE(VB_AUDIO, "kickoffOutputAudioLoop exiting");
    return NULL;
}

int AudioOutputALSA::SetParameters(snd_pcm_t *handle, snd_pcm_access_t access,
                                   snd_pcm_format_t format, unsigned int channels,
                                   unsigned int rate, unsigned int buffer_time,
                                   unsigned int period_time)
{
    int err, dir;
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;

    VERBOSE(VB_AUDIO, QString("in SetParameters(format=%1, channels=%2, "
                              "rate=%3, buffer_time=%4, period_time=%5")
            .arg(format).arg(channels).arg(rate).arg(buffer_time).arg(period_time));
    
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    
    /* choose all parameters */
    if ((err = snd_pcm_hw_params_any(handle, params)) < 0)
    {
        Error(QString("Broken configuration for playback; no configurations"
              " available: %1").arg(snd_strerror(err)));
        return err;
    }

    /* set the interleaved read/write format */
    if ((err = snd_pcm_hw_params_set_access(handle, params, access)) < 0)
    {
        Error(QString("Access type not available: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* set the sample format */
    if ((err = snd_pcm_hw_params_set_format(handle, params, format)) < 0)
    {
        Error(QString("Sample format not available: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* set the count of channels */
    if ((err = snd_pcm_hw_params_set_channels(handle, params, channels)) < 0)
    {
        Error(QString("Channels count (%i) not available: %1")
              .arg(channels).arg(snd_strerror(err)));
        return err;
    }

    /* set the stream rate */
    unsigned int rrate = rate;
    if ((err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0)) < 0)
    {
        Error(QString("Samplerate (%1Hz) not available: %2")
              .arg(rate).arg(snd_strerror(err)));
        return err;
    }

    if (rrate != rate)
    {
        Error(QString("Rate doesn't match (requested %1Hz, got %2Hz)")
              .arg(rate).arg(rrate));
        return -EINVAL;
    }

    /* set the buffer time */
    if ((err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                     &buffer_time, &dir)) < 0)
    {
        Error(QString("Unable to set buffer time %1 for playback: %2")
              .arg(buffer_time).arg(snd_strerror(err)));
        return err;
    }

    if ((err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size)) < 0)
    {
        Error(QString("Unable to get buffer size for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    } else {
        VERBOSE(VB_AUDIO, QString("get_buffer_size returned %1").arg(buffer_size));
    }

    /* set the period time */
    if ((err = snd_pcm_hw_params_set_period_time_near(
                    handle, params, &period_time, &dir)) < 0)
    {
        Error(QString("Unable to set period time %1 for playback: %2")
              .arg(period_time).arg(snd_strerror(err)));
        return err;
    } else {
        VERBOSE(VB_AUDIO, QString("set_period_time_near returned %1").arg(period_time));
    }

    if ((err = snd_pcm_hw_params_get_period_size(params, &period_size,
                                                &dir)) < 0) {
        Error(QString("Unable to get period size for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    } else {
        VERBOSE(VB_AUDIO, QString("get_period_size returned %1").arg(period_size));
    }

    /* write the parameters to device */
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        Error(QString("Unable to set hw params for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }
    
    /* get the current swparams */
    if ((err = snd_pcm_sw_params_current(handle, swparams)) < 0)
    {
        Error(QString("Unable to determine current swparams for playback:"
                      " %1").arg(snd_strerror(err)));
        return err;
    }
    /* start the transfer after period_size */
    if ((err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 
                                                    period_size)) < 0)
    {
        Error(QString("Unable to set start threshold mode for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* allow the transfer when at least period_size samples can be processed */
    if ((err = snd_pcm_sw_params_set_avail_min(handle, swparams,
                                              period_size)) < 0)
    {
        Error(QString("Unable to set avail min for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* align all transfers to 1 sample */
    if ((err = snd_pcm_sw_params_set_xfer_align(handle, swparams, 1)) < 0)
    {
        Error(QString("Unable to set transfer align for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* write the parameters to the playback device */
    if ((err = snd_pcm_sw_params(handle, swparams)) < 0)
    {
        Error(QString("Unable to set sw params for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    return 0;
}
