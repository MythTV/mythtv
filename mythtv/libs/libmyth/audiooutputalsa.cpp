#include <cstdio>
#include <cstdlib>

using namespace std;

#include "mythcontext.h"
#include "audiooutputalsa.h"

AudioOutputALSA::AudioOutputALSA(QString audiodevice, int audio_bits, 
                                 int audio_channels, int audio_samplerate)
               : AudioOutput()
{
    this->audiodevice = audiodevice;
    pcm_handle = NULL;
    effdsp = 0;
    paused = false;
    Reconfigure(audio_bits, audio_channels, audio_samplerate);
}

AudioOutputALSA::~AudioOutputALSA()
{
    if(pcm_handle != NULL)
        snd_pcm_close(pcm_handle);
}

void AudioOutputALSA::Reconfigure(int audio_bits, 
                                  int audio_channels, int audio_samplerate)
{
    int err;
    snd_pcm_format_t format;
    unsigned int buffer_time = 500000, period_time = 100000;
    snd_pcm_t *new_pcm_handle;

    new_pcm_handle = pcm_handle;
    pcm_handle = NULL;

    if(new_pcm_handle)
    {
        snd_pcm_hw_free(new_pcm_handle);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Opening ALSA audio device '%1'.")
                              .arg(audiodevice));
        err = snd_pcm_open(&new_pcm_handle, audiodevice, 
                           SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if(err < 0)
        {
            Error(QString("Error opening ALSA device for playback: %1")
                         .arg(snd_strerror(err)));
            return;
        }
    }
    
    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    
    if(audio_bits == 8)
        format = SND_PCM_FORMAT_S8;
    else if(audio_bits == 16)
        // is the sound data coming in really little-endian or is it
        // CPU-endian?
        format = SND_PCM_FORMAT_S16_LE;
    else if(audio_bits == 24)
        format = SND_PCM_FORMAT_S24_LE;
    else
    {
        Error(QString("Unknown sample format: %1 bits.").arg(audio_bits));
        return;
    }

    err = SetParameters(new_pcm_handle, SND_PCM_ACCESS_MMAP_INTERLEAVED,
                        format, audio_channels, audio_samplerate, buffer_time,
                        period_time, &can_hw_pause);
    if(err < 0)
        return;
    
    effdsp = audio_samplerate * 100;
    
    pcm_handle = new_pcm_handle;
}

void AudioOutputALSA::SetBlocking(bool blocking)
{
    if(pcm_handle != NULL)
        snd_pcm_nonblock(pcm_handle, !blocking);
}

int AudioOutputALSA::XRunRecovery(snd_pcm_t *handle, int err)
{
    if (err == -EPIPE) {    /* under-run */
        if((err = snd_pcm_prepare(handle)) < 0)
            VERBOSE(VB_IMPORTANT, QString("Can't recovery from underrun,"
                    " prepare failed: %1").arg(snd_strerror(err)));
        return 0;
    } else if (err == -ESTRPIPE) {
        // this happens only via power management turning soundcard off.
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);       /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                VERBOSE(VB_IMPORTANT, QString("Can't recover from suspend,"
                        " prepare failed: %1").arg(snd_strerror(err)));
        }
    }
    return err;
}

void AudioOutputALSA::Reset(void)
{
    int err;
    if(pcm_handle == NULL)
        return;
    audbuf_timecode = 0;
    if((err = snd_pcm_drop(pcm_handle)) < 0)
        VERBOSE(VB_IMPORTANT, QString("Error resetting sound: %1")
                              .arg(snd_strerror(err)));
    if((err = snd_pcm_prepare(pcm_handle)) < 0)
        if(XRunRecovery(pcm_handle, err) < 0)
            VERBOSE(VB_IMPORTANT, QString("Error preparing sound after reset:"
                                          " %1").arg(snd_strerror(err)));
}

void AudioOutputALSA::AddSamples(char *buffer, int frames, long long timecode)
{
    int err;
    
 retry:
    if (pcm_handle == NULL)
        return;
    
//    printf("Trying to write %d i frames to soundbuffer\n", frames);
    while (frames > 0)
    {
        err = snd_pcm_mmap_writei(pcm_handle, buffer, frames);
        if (err >= 0)
        {
            buffer += err * audio_bytes_per_sample;
            frames -= err;
        }
        else if (err == -EAGAIN)
        {
            snd_pcm_wait(pcm_handle, 10);
        }
        else if (err < 0)
        {
            if (XRunRecovery(pcm_handle, err) < 0) 
            {
                Error(QString("Write error, disabling sound output: %1")
                      .arg(snd_strerror(err)));
                snd_pcm_close(pcm_handle);
                pcm_handle = NULL;
            }
            goto retry;
        }
    }
    
    if(timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((frames*100000.0) / effdsp);
}


void AudioOutputALSA::AddSamples(char *buffers[], int frames, long long timecode)
{
    int err;

 retry:
    if(pcm_handle == NULL)
        return;

//    printf("Trying to write %d non-i frames to soundbuffer\n", frames);
    err = snd_pcm_mmap_writen(pcm_handle, (void **)buffers, frames);
    if (err == -EAGAIN || (err >= 0 && err != frames))
        VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
    else if (err < 0) {
        if (XRunRecovery(pcm_handle, err) < 0) {
            Error(QString("Write error, disabling sound output: %1")
                  .arg(snd_strerror(err)));
            snd_pcm_close(pcm_handle);
            pcm_handle = NULL;
        }
        goto retry;
    }
    
    if(timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((frames*100000.0) / effdsp);
}

void AudioOutputALSA::SetTimecode(long long timecode)
{
    audbuf_timecode = timecode;
}
void AudioOutputALSA::SetEffDsp(int dsprate)
{
//  printf("SetEffDsp: %d\n", dsprate);
    effdsp = dsprate;
}

bool AudioOutputALSA::GetPause(void)
{
    return paused;
}


void AudioOutputALSA::Pause(bool paused)
{
#if DO_HW_PAUSE
    if(pcm_handle != NULL && can_hw_pause)
    {
        snd_pcm_state_t state = snd_pcm_state(pcm_handle);
        if((state == SND_PCM_STATE_RUNNING && paused) ||
           (state == SND_PCM_STATE_PAUSED && !paused))
        {
            int err;
            if ((err = snd_pcm_pause(pcm_handle, true)) < 0)
                VERBOSE(VB_IMPORTANT, QString("Couldn't (un)pause: %1")
                                             .arg(snd_strerror(err)));
        }
    }
#endif
    this->paused = paused;
}

int AudioOutputALSA::GetAudiotime(void)
{
    int err;
    long frame_delay = 0;
//    printf("Entering GetAudiotime\n");

    if(pcm_handle == NULL || audbuf_timecode == 0)
        return 0;
    
    err = snd_pcm_delay(pcm_handle, &frame_delay);
    if(err < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Error determining sound output delay:"
                                      " %1").arg(snd_strerror(err)));
        frame_delay = 0;
    }
    
    if(frame_delay < 0) // underrun
        frame_delay = 0;

    int result =  audbuf_timecode - (int)((frame_delay*100000.0) / effdsp);
//    printf("GetAudiotime returning: %d\n", result);
    return result;
}

int AudioOutputALSA::SetParameters(snd_pcm_t *handle, snd_pcm_access_t access,
                        snd_pcm_format_t format, unsigned int channels,
                        unsigned int rate, unsigned int buffer_time,
                        unsigned int period_time, bool *can_pause)
{
    int err, dir;
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    
    /* choose all parameters */
    if((err = snd_pcm_hw_params_any(handle, params)) < 0)
    {
        Error(QString("Broken configuration for playback; no configurations"
              " available: %1").arg(snd_strerror(err)));
        return err;
    }

    /* set the interleaved read/write format */
    if((err = snd_pcm_hw_params_set_access(handle, params, access)) < 0)
    {
        Error(QString("Access type not available: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* set the sample format */
    if((err = snd_pcm_hw_params_set_format(handle, params, format)) < 0)
    {
        Error(QString("Sample format not available: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* set the count of channels */
    if((err = snd_pcm_hw_params_set_channels(handle, params, channels)) < 0)
    {
        Error(QString("Channels count (%i) not available: %1")
              .arg(channels).arg(snd_strerror(err)));
        return err;
    }

    /* set the stream rate */
    unsigned int rrate = rate;
    if((err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0)) < 0)
    {
        Error(QString("Samplerate (%1Hz) not available: %2")
              .arg(rate).arg(snd_strerror(err)));
        return err;
    }

    if (rrate != rate)
    {
        Error(QString("Rate doesn't match (requested %1Hz, got %2Hz)")
              .arg(rate).arg(err));
        return -EINVAL;
    }

    /* set the buffer time */
    if((err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                     &buffer_time, &dir)) < 0)
    {
        Error(QString("Unable to set buffer time %1 for playback: %2")
              .arg(buffer_time).arg(snd_strerror(err)));
        return err;
    }

    if((err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size)) < 0)
    {
        Error(QString("Unable to get buffer size for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* set the period time */
    if((err = snd_pcm_hw_params_set_period_time_near(
                    handle, params, &period_time, &dir)) < 0)
    {
        Error(QString("Unable to set period time %1 for playback: %2")
              .arg(period_time).arg(snd_strerror(err)));
        return err;
    }

    if((err = snd_pcm_hw_params_get_period_size(params, &period_size,
                                                &dir)) < 0) {
        Error(QString("Unable to get period size for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* write the parameters to device */
    if((err = snd_pcm_hw_params(handle, params)) < 0) {
        Error(QString("Unable to set hw params for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }
    
	if(can_pause)
		*can_pause = snd_pcm_hw_params_can_pause(params);

    /* get the current swparams */
    if((err = snd_pcm_sw_params_current(handle, swparams)) < 0)
    {
        Error(QString("Unable to determine current swparams for playback:"
                      " %1").arg(snd_strerror(err)));
        return err;
    }
    /* start the transfer after period_size */
    if((err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 
                                                    period_size)) < 0)
    {
        Error(QString("Unable to set start threshold mode for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* allow the transfer when at least period_size samples can be processed */
    if((err = snd_pcm_sw_params_set_avail_min(handle, swparams,
                                              period_size)) < 0)
    {
        Error(QString("Unable to set avail min for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* align all transfers to 1 sample */
    if((err = snd_pcm_sw_params_set_xfer_align(handle, swparams, 1)) < 0)
    {
        Error(QString("Unable to set transfer align for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    /* write the parameters to the playback device */
    if((err = snd_pcm_sw_params(handle, swparams)) < 0)
    {
        Error(QString("Unable to set sw params for playback: %1")
              .arg(snd_strerror(err)));
        return err;
    }

    return 0;
}

