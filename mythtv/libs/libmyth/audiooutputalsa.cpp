#include <stdio.h>
#include <stdlib.h>


using namespace std;

#include "mythcontext.h"
#include "audiooutputalsa.h"


static int set_params(snd_pcm_t *handle,
                      snd_pcm_access_t access,
                      snd_pcm_format_t format,
                      unsigned int channels,
                      unsigned int rate,
                      unsigned int buffer_time,
                      unsigned int period_time,
					  bool *can_pause)
{
    unsigned int rrate;
    int err, dir;
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    
    /* choose all parameters */
    if((err = snd_pcm_hw_params_any(handle, params)) < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    if((err = snd_pcm_hw_params_set_access(handle, params, access)) < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    if((err = snd_pcm_hw_params_set_format(handle, params, format)) < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    if((err = snd_pcm_hw_params_set_channels(handle, params, channels)) < 0) {
        printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    if((err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0)) < 0) {
        printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    if((err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir)) < 0) {
        printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    if((err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size)) < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the period time */
    if((err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir)) < 0) {
        printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    if((err = snd_pcm_hw_params_get_period_size(params, &period_size, &dir)) < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* write the parameters to device */
    if((err = snd_pcm_hw_params(handle, params)) < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    
	if(can_pause)
		*can_pause = snd_pcm_hw_params_can_pause(params);

    /* get the current swparams */
    if((err = snd_pcm_sw_params_current(handle, swparams)) < 0) {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer after period_size */
    if((err = snd_pcm_sw_params_set_start_threshold(handle, swparams, period_size)) < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    if((err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size)) < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* align all transfers to 1 sample */
    if((err = snd_pcm_sw_params_set_xfer_align(handle, swparams, 1)) < 0) {
        printf("Unable to set transfer align for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* write the parameters to the playback device */
    if((err = snd_pcm_sw_params(handle, swparams)) < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}
AudioOutputALSA::AudioOutputALSA(QString audiodevice, int audio_bits, 
                                 int audio_channels, int audio_samplerate)
{
    this->audiodevice = audiodevice;
    pcm_handle = NULL;
    effdsp = 0;
    paused = false;
    Reconfigure(audio_bits, audio_channels, audio_samplerate);
}

void AudioOutputALSA::SetBlocking(bool blocking)
{
    if(pcm_handle != NULL)
        snd_pcm_nonblock(pcm_handle, !blocking);
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
        printf("Opening ALSA audio device '%s'.\n", audiodevice.ascii());
        err = snd_pcm_open(&new_pcm_handle, audiodevice, 
                           SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if(err < 0)
        {
            printf("Playback open error: %s\n", snd_strerror(err));
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
        printf("Error: Unknown sample format: %d bits.\n", audio_bits);
        return;
    }

    err = set_params(new_pcm_handle, SND_PCM_ACCESS_MMAP_INTERLEAVED, format, audio_channels, audio_samplerate, buffer_time, period_time, &can_hw_pause);
    if(err < 0)
    {
        printf("Error setting audio params: %s\n", snd_strerror(err));
        return;
    }
    
    effdsp = audio_samplerate * 100;
    
    pcm_handle = new_pcm_handle;
}

AudioOutputALSA::~AudioOutputALSA()
{
    if(pcm_handle != NULL)
        snd_pcm_close(pcm_handle);
}

static int xrun_recovery(snd_pcm_t *handle, int err)
{
//    printf("xrun_recovery\n");
    if (err == -EPIPE) {    /* under-run */
        if((err = snd_pcm_prepare(handle)) < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        // this happens only via power management turning soundcard off.
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);       /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
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
        printf("Error resetting sound: %s\n", snd_strerror(err));
    if((err = snd_pcm_prepare(pcm_handle)) < 0)
        if(xrun_recovery(pcm_handle, err) < 0)
            printf("Error preparing sound after reset: %s\n", snd_strerror(err));
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
            if (xrun_recovery(pcm_handle, err) < 0) 
            {
                printf("Write error: %s\n", snd_strerror(err));
                printf("Disabling sound output.\n");
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
        printf("Audio buffer overflow, audio data lost!\n");
    else if (err < 0) {
        if (xrun_recovery(pcm_handle, err) < 0) {
            printf("Write error: %s\n", snd_strerror(err));
            printf("Disabling sound output.\n");
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
    int err;
	if(pcm_handle != NULL && can_hw_pause)
	{
		snd_pcm_state_t state = snd_pcm_state(pcm_handle);
		if((state == SND_PCM_STATE_RUNNING && paused) ||
		   (state == SND_PCM_STATE_PAUSED && !paused))
		{
			if ((err = snd_pcm_pause(pcm_handle, true)) < 0)
				printf("Couldn't %spause: %s\n", (paused?"":"un"), snd_strerror(err));
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
        printf("Error determining sound output delay: %s\n", snd_strerror(err));
        frame_delay = 0;
    }
    
    if(frame_delay < 0) // underrun
        frame_delay = 0;

    int result =  audbuf_timecode - (int)((frame_delay*100000.0) / effdsp);
//    printf("GetAudiotime returning: %d\n", result);
    return result;
}
