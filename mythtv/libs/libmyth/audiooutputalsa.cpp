#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputalsa.h"
	

AudioOutputALSA::AudioOutputALSA(QString audiodevice, int laudio_bits, 
                                 int laudio_channels, int laudio_samplerate)
               : AudioOutputBase(audiodevice)
{
    // our initalisation
    pcm_handle = NULL;
    numbadioctls = 0;

    // Set everything up
    Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputALSA::~AudioOutputALSA()
{
    KillAudio();
}

bool AudioOutputALSA::OpenDevice()
{
    snd_pcm_t *new_pcm_handle;
    snd_pcm_format_t format;
    unsigned int buffer_time = 500000, period_time = 100000;

    int err;

    new_pcm_handle = pcm_handle;
    pcm_handle = NULL;

//    if (new_pcm_handle != NULL)
//        snd_pcm_hw_free(new_pcm_handle);

    
    numbadioctls = 0;
    
    err = snd_pcm_open(&pcm_handle, audiodevice,
          SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK); 

    if (err < 0)
    { 
        Error(QString("snd_pcm_open(%1) error %2")
              .arg(audiodevice).arg(snd_strerror(err)));
    }

    fragment_size = 4096;

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
        return false;
    }

    err = SetParameters(pcm_handle, SND_PCM_ACCESS_MMAP_INTERLEAVED,
                        format, audio_channels, audio_samplerate, buffer_time,
                        period_time);
    if (err < 0) 
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
        return false;
    }    

    // Device opened successfully
    return true;
}

void AudioOutputALSA::CloseDevice()
{
    if (pcm_handle != NULL)
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }
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

inline int AudioOutputALSA::getBufferedOnSoundcard(void)
{ 
    int err;
    snd_pcm_uframes_t soundcard_buffer = 0;
    snd_pcm_hw_params_t *hw_params;
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        Error(QString("cannot allocate hardware parameter structure (%1)")
              .arg(snd_strerror(err)));
    }
    snd_pcm_hw_params_current(pcm_handle, hw_params);
    snd_pcm_hw_params_get_buffer_size(hw_params, &soundcard_buffer); // frames
    snd_pcm_hw_params_free(hw_params);

    return soundcard_buffer;
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
