#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>
#include "config.h"

using namespace std;

#include "mythcontext.h"
#include "audiooutputalsa.h"
    

AudioOutputALSA::AudioOutputALSA(QString audiodevice, int laudio_bits, 
                                 int laudio_channels, int laudio_samplerate,
                                 AudioOutputSource source, bool set_initial_vol)
              : AudioOutputBase(audiodevice, laudio_bits,
                              laudio_channels, laudio_samplerate, source, set_initial_vol)
{
    // our initalisation
    pcm_handle = NULL;
    numbadioctls = 0;
    mixer_handle = NULL;

    // Set everything up
    Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputALSA::~AudioOutputALSA()
{
    KillAudio();
}

bool AudioOutputALSA::OpenDevice()
{
    snd_pcm_format_t format;
    unsigned int buffer_time, period_time;
    int err;

    if (pcm_handle != NULL)
        CloseDevice();

    pcm_handle = NULL;
    numbadioctls = 0;
    
    err = snd_pcm_open(&pcm_handle, audiodevice,
          SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK); 

    if (err < 0)
    { 
        Error(QString("snd_pcm_open(%1): %2")
              .arg(audiodevice).arg(snd_strerror(err)));
    }

    /* the audio fragment size was computed by using the next lower power of 2
       of the following:

       const int video_frame_rate = 30;
       const int bits_per_byte = 8;
       int fbytes = (audio_bits * audio_channels * audio_samplerate) / 
                    (bits_per_byte * video_frame_rate);
    */
    fragment_size = 4096;

    if (audio_bits == 8)
        format = SND_PCM_FORMAT_S8;
    else if (audio_bits == 16)
        // is the sound data coming in really little-endian or is it
        // CPU-endian?
#ifdef WORDS_BIGENDIAN
        format = SND_PCM_FORMAT_S16;
#else
        format = SND_PCM_FORMAT_S16_LE;
#endif
    else if (audio_bits == 24)
#ifdef WORDS_BIGENDIAN
        format = SND_PCM_FORMAT_S24;
#else
        format = SND_PCM_FORMAT_S24_LE;
#endif
    else
    {
        Error(QString("Unknown sample format: %1 bits.").arg(audio_bits));
        return false;
    }

    buffer_time = 500000;  // .5 seconds
    period_time = buffer_time / 4;  // 4 interrupts per buffer

    err = SetParameters(pcm_handle, SND_PCM_ACCESS_MMAP_INTERLEAVED,
                        format, audio_channels, audio_samplerate, buffer_time,
                        period_time);
    if (err < 0) 
    {
        CloseDevice();
        return false;
    }    

    // make us think that soundcard buffer is 4 fragments smaller than
    // it really is
    audio_buffer_unused = soundcard_buffer_size - (fragment_size * 4);

    if (internal_vol)
        OpenMixer(set_initial_vol);
    
    // Device opened successfully
    return true;
}

void AudioOutputALSA::CloseDevice()
{
    CloseMixer();
    if (pcm_handle != NULL)
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }
}


void AudioOutputALSA::WriteAudio(unsigned char *aubuf, int size)
{
    unsigned char *tmpbuf;
    int lw = 0;
    int frames = size / audio_bytes_per_sample;

    if (pcm_handle == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("WriteAudio() called with pcm_handle == NULL!"));
        return;
    }
    
    tmpbuf = aubuf;

    VERBOSE(VB_AUDIO, QString("WriteAudio: Preparing %1 bytes (%2 frames)")
            .arg(size).arg(frames));
    
    while (frames > 0) 
    {
        lw = snd_pcm_mmap_writei(pcm_handle, tmpbuf, frames);
        
        if (lw >= 0)
        {
            if (lw < frames)
                VERBOSE(VB_AUDIO, QString("WriteAudio: short write %1 bytes (ok)")
                        .arg(lw * audio_bytes_per_sample));

            frames -= lw;
            tmpbuf += lw * audio_bytes_per_sample; // bytes
        } 
        else if (lw == -EAGAIN)
        {
            VERBOSE(VB_AUDIO, QString("WriteAudio: device is blocked - waiting"));

            snd_pcm_wait(pcm_handle, 10);
        }
        else if (lw == -EPIPE &&
                 snd_pcm_state(pcm_handle) == SND_PCM_STATE_XRUN)
        {
            VERBOSE(VB_IMPORTANT, "WriteAudio: buffer underrun");

            if ((lw = snd_pcm_prepare(pcm_handle)) < 0)
            {
                Error(QString("WriteAudio: unable to recover from xrun: %1")
                      .arg(snd_strerror(lw)));
                return;
            }
        }
        else if (lw == -ESTRPIPE)
        {
            VERBOSE(VB_IMPORTANT, "WriteAudio: device is suspended");

            while ((lw = snd_pcm_resume(pcm_handle)) == -EAGAIN)
                usleep(200);

            if (lw < 0)
            {
                VERBOSE(VB_IMPORTANT, "WriteAudio: resume failed");

                if ((lw = snd_pcm_prepare(pcm_handle)) < 0)
                {
                    Error(QString("WriteAudio: unable to recover from suspend: %1")
                          .arg(snd_strerror(lw)));
                    return;
                }
            }
        }
        else if (lw == -EBADFD)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("WriteAudio: device is in a bad state (state = %1)")
                    .arg(snd_pcm_state(pcm_handle)));
            return;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("snd_pcm_mmap_writei: %1 (%2)")
                    .arg(snd_strerror(lw)).arg(lw));
            VERBOSE(VB_IMPORTANT, QString("WriteAudio: snd_pcm_state == %1")
                    .arg(snd_pcm_state(pcm_handle)));

            // CloseDevice();
            return;
        }
    }
}

inline int AudioOutputALSA::getBufferedOnSoundcard(void)
{ 
    if (pcm_handle == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("getBufferedOnSoundcard() called with pcm_handle == NULL!"));
        return 0;
    }

    // this should be more like what you want, previously this function
    // was returning the soundcard buffer size -dag

    snd_pcm_sframes_t delay = 0;

    snd_pcm_state_t state = snd_pcm_state(pcm_handle);
    if (state == SND_PCM_STATE_RUNNING || 
        state == SND_PCM_STATE_DRAINING)
    {
        snd_pcm_delay(pcm_handle, &delay);
    }

    if (delay < 0)
        delay = 0;

    int buffered = delay * audio_bytes_per_sample;

    return buffered;
}


inline int AudioOutputALSA::getSpaceOnSoundcard(void)
{
    if (pcm_handle == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("getSpaceOnSoundcard() called with pcm_handle == NULL!"));
        return 0;
    }

    snd_pcm_sframes_t avail, delay;

    snd_pcm_state_t state = snd_pcm_state(pcm_handle);
    if (state == SND_PCM_STATE_RUNNING || 
        state == SND_PCM_STATE_DRAINING)
    {
        snd_pcm_delay(pcm_handle, &delay);
    }

    avail = snd_pcm_avail_update(pcm_handle);
    if (avail < 0 || (snd_pcm_uframes_t)avail > soundcard_buffer_size)
        avail = soundcard_buffer_size;

    int space = (avail * audio_bytes_per_sample) - audio_buffer_unused;

    if (space < 0)
        space = 0;

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
                              "rate=%3, buffer_time=%4, period_time=%5)")
            .arg(format).arg(channels).arg(rate).arg(buffer_time).arg(period_time));

    if (handle == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("SetParameters() called with handle == NULL!"));
        return 0;
    }
        
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
        Error(QString("Channels count (%1) not available: %2")
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
    soundcard_buffer_size = buffer_size * audio_bytes_per_sample;

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

    if ((err = snd_pcm_prepare(handle)) < 0)
        Error(QString("Initial pcm prepare err %1 %2")
              .arg(err).arg(snd_strerror(err)));

    return 0;
}


int AudioOutputALSA::GetVolumeChannel(int channel)
{
    long actual_volume, volume;

    if (mixer_handle == NULL)
        return 100;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, mixer_control.ascii());

    if ((elem = snd_mixer_find_selem(mixer_handle, sid)) == NULL)
    {
        Error(QString("mixer unable to find control %1").arg(mixer_control));
        CloseMixer();
        return 0;
    }

    GetVolumeRange();

    snd_mixer_selem_get_playback_volume(elem, (snd_mixer_selem_channel_id_t)channel,
                                        &actual_volume);
    volume = (int)((actual_volume - playback_vol_min) *
                   volume_range_multiplier);

    return volume;
}
void AudioOutputALSA::SetVolumeChannel(int channel, int volume)
{
    SetCurrentVolume(mixer_control, channel, volume);
}

void AudioOutputALSA::SetCurrentVolume(QString control, int channel, int volume)
{
    int err, set_vol;

    VERBOSE(VB_AUDIO, QString("Setting %1 volume to %2")
            .arg(control).arg(volume));

    if (mixer_handle != NULL)
    {
        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_index(sid, 0);
        snd_mixer_selem_id_set_name(sid, control.ascii());

        if ((elem = snd_mixer_find_selem(mixer_handle, sid)) == NULL)
        {
            Error(QString("mixer unable to find control %1").arg(control));
            return;
        }

        GetVolumeRange();

        set_vol = (int)(volume / volume_range_multiplier +
                        playback_vol_min + 0.5);

        if ((err = snd_mixer_selem_set_playback_volume(elem,
            (snd_mixer_selem_channel_id_t)channel, set_vol)) < 0)
        {
            Error(QString("mixer set channel %1 err %2: %3")
                  .arg(channel).arg(err).arg(snd_strerror(err)));
            return;
        }
        else
        {
            VERBOSE(VB_AUDIO, QString("channel %1 vol set to %2")
                              .arg(channel).arg(set_vol));
        }
    }
}

void AudioOutputALSA::OpenMixer(bool setstartingvolume)
{
    int volume;

    mixer_control = gContext->GetSetting("MixerControl", "PCM");

    SetupMixer();

    if (mixer_handle != NULL && setstartingvolume)
    {
        volume = gContext->GetNumSetting("MasterMixerVolume", 80);
        SetCurrentVolume("Master", 0, volume);
        SetCurrentVolume("Master", 1, volume);

        volume = gContext->GetNumSetting("PCMMixerVolume", 80);
        SetCurrentVolume("PCM", 0, volume);
        SetCurrentVolume("PCM", 1, volume);
    }
}

void AudioOutputALSA::CloseMixer(void)
{
    if (mixer_handle != NULL)
        snd_mixer_close(mixer_handle);
    mixer_handle = NULL;
}

void AudioOutputALSA::SetupMixer(void)
{
    int err;

    QString device = gContext->GetSetting("MixerDevice", "default");

    if (mixer_handle != NULL)
        CloseMixer();

    VERBOSE(VB_AUDIO, QString("Opening mixer %1").arg(device));

    // TODO: This is opening card 0. Fix for case of multiple soundcards
    if ((err = snd_mixer_open(&mixer_handle, 0)) < 0)
    {
        Error(QString("Mixer device open error %1: %2")
              .arg(err).arg(snd_strerror(err)));
        mixer_handle = NULL;
        return;
    }

    if ((err = snd_mixer_attach(mixer_handle, device.ascii())) < 0)
    {
        Error(QString("Mixer attach error %1: %2\nCheck Mixer Name in Setup: %3")
              .arg(err).arg(snd_strerror(err)).arg(device.ascii()));
        CloseMixer();
        return;
    }

    if ((err = snd_mixer_selem_register(mixer_handle, NULL, NULL)) < 0)
    {
        Error(QString("Mixer register error %1: %2")
              .arg(err).arg(snd_strerror(err)));
        CloseMixer();
        return;
    }

    if ((err = snd_mixer_load(mixer_handle)) < 0)
    {
        Error(QString("Mixer load error %1: %2")
              .arg(err).arg(snd_strerror(err)));
        CloseMixer();
        return;
    }
}

inline void AudioOutputALSA::GetVolumeRange(void)
{
    snd_mixer_selem_get_playback_volume_range(elem, &playback_vol_min,
                                              &playback_vol_max);
    volume_range_multiplier = (100.0 / (float)(playback_vol_max -
                                                   playback_vol_min));
						   

    VERBOSE(VB_AUDIO, QString("Volume range is %1 to %2, mult=%3")
            .arg(playback_vol_min).arg(playback_vol_max)
            .arg(volume_range_multiplier));
}

