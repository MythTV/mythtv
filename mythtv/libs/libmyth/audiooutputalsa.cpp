#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>
#include "config.h"

using namespace std;

#include "mythcontext.h"
#include "audiooutputalsa.h"
    
#define LOC QString("ALSA: ")
#define LOC_WARN QString("ALSA, Warning: ")
#define LOC_ERR QString("ALSA, Error: ")

// redefine assert as no-op to quiet some compiler warnings
// about assert always evaluating true in alsa headers.
#undef assert
#define assert(x)

AudioOutputALSA::AudioOutputALSA(const AudioSettings &settings) :
    AudioOutputBase(settings),
    pcm_handle(NULL),
    numbadioctls(0),
    mixer_handle(NULL),
    mixer_control(QString::null)
{
    // Set everything up
    Reconfigure(settings);
}

AudioOutputALSA::~AudioOutputALSA()
{
    KillAudio();
    SetIECStatus(true);
}

void AudioOutputALSA::SetIECStatus(bool audio)
{
    snd_ctl_t *ctl;
    const char *spdif_str = SND_CTL_NAME_IEC958("", PLAYBACK, DEFAULT);
    int spdif_index = -1;
    snd_ctl_elem_list_t *clist;
    snd_ctl_elem_id_t *cid;
    snd_ctl_elem_value_t *cval;
    snd_aes_iec958_t iec958;
    int cidx, controls;

    VERBOSE(VB_AUDIO, QString("Setting IEC958 status: %1")
                      .arg(audio ? "audio" : "non-audio"));

    int err;
    if ((err = snd_ctl_open(&ctl, "default", 0)) < 0)
    {
        Error(QString("AudioOutputALSA::SetIECStatus: snd_ctl_open(default): %1")
              .arg(snd_strerror(err)));
        return;
    }
    snd_ctl_elem_list_alloca(&clist);
    snd_ctl_elem_list(ctl, clist);
    snd_ctl_elem_list_alloc_space(clist, snd_ctl_elem_list_get_count(clist));
    snd_ctl_elem_list(ctl, clist);
    controls = snd_ctl_elem_list_get_used(clist);
    for (cidx = 0; cidx < controls; cidx++)
    {
        if (!strcmp(snd_ctl_elem_list_get_name(clist, cidx), spdif_str))
            if (spdif_index < 0 ||
                snd_ctl_elem_list_get_index(clist, cidx) == (uint)spdif_index)
                    break;
    }

    if (cidx >= controls)
        return;

    snd_ctl_elem_id_alloca(&cid);
    snd_ctl_elem_list_get_id(clist, cidx, cid);
    snd_ctl_elem_value_alloca(&cval);
    snd_ctl_elem_value_set_id(cval, cid);
    snd_ctl_elem_read(ctl,cval);
    snd_ctl_elem_value_get_iec958(cval, &iec958);

    if (!audio) 
        iec958.status[0] |= IEC958_AES0_NONAUDIO;
    else
        iec958.status[0] &= ~IEC958_AES0_NONAUDIO;

    snd_ctl_elem_value_set_iec958(cval, &iec958);
    snd_ctl_elem_write(ctl, cval);
}

vector<int> AudioOutputALSA::GetSupportedRates()
{
    snd_pcm_hw_params_t *params;
    int err;
    const int srates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 64000, 88200, 96000, 176400, 192000 };
    vector<int> rates(srates, srates + sizeof(srates) / sizeof(int) );
    QString real_device;

    if (audio_passthru || audio_enc)
        real_device = audio_passthru_device;
    else
        real_device = audio_main_device;

    VERBOSE(VB_AUDIO, QString("AudioOutputALSA::GetSupportedRates opening %1")
            .arg(real_device));

    if((err = snd_pcm_open(&pcm_handle, real_device.toAscii(),
                           SND_PCM_STREAM_PLAYBACK, 
                           SND_PCM_NONBLOCK|SND_PCM_NO_AUTO_RESAMPLE)) < 0)
    { 
        Error(QString("snd_pcm_open(%1): %2")
              .arg(real_device).arg(snd_strerror(err)));

        if (pcm_handle)
        {
            snd_pcm_close(pcm_handle);
            pcm_handle = NULL;
        }
        rates.clear();
        return rates;
    }

    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_hw_params_any(pcm_handle, params)) < 0)
    {
        Error(QString("Broken configuration for playback; no configurations"
              " available: %1").arg(snd_strerror(err)));
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
        rates.clear();
        return rates;
    }

    vector<int>::iterator it = rates.begin();

    while (it != rates.end())
    {
        if(snd_pcm_hw_params_test_rate(pcm_handle, params, *it, 0) < 0)
            it = rates.erase(it);
        else
            it++;
    }

    snd_pcm_close(pcm_handle);
    pcm_handle = NULL;

    return rates;
}

bool AudioOutputALSA::OpenDevice()
{
    snd_pcm_format_t format;
    unsigned int buffer_time, period_time;
    int err;
    QString real_device;

    if (pcm_handle != NULL)
        CloseDevice();

    pcm_handle = NULL;
    numbadioctls = 0;

    if (audio_passthru || audio_enc)
    {
        real_device = audio_passthru_device;
        SetIECStatus(false);
    }
    else 
    {
        real_device = audio_main_device;
        SetIECStatus(true);
    }

    VERBOSE(VB_GENERAL, QString("Opening ALSA audio device '%1'.")
            .arg(real_device));

    QByteArray dev_ba = real_device.toLocal8Bit();
    err = snd_pcm_open(&pcm_handle, dev_ba.constData(),
                       SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

    if (err < 0)
    { 
        Error(QString("snd_pcm_open(%1): %2")
              .arg(real_device).arg(snd_strerror(err)));

        if (pcm_handle)
            CloseDevice();
        return false;
    }

    /* the audio fragment size was computed by using the next lower power of 2
       of the following:

       const int video_frame_rate = 30;
       const int bits_per_byte = 8;
       int fbytes = (audio_bits * audio_channels * audio_samplerate) / 
                    (bits_per_byte * video_frame_rate);
                    
        For telephony apps, a much shorter fragment size is needed to reduce the
        delay, and fragments should be multiples of the RTP packet size (10ms). 
        20ms delay should be the max introduced by the driver, which equates
        to 320 bytes at 8000 samples/sec and mono 16-bit samples
    */
    if (source == AUDIOOUTPUT_TELEPHONY)
    {
        fragment_size = 320;
        buffer_time = 80000;  // 80 ms
        period_time = buffer_time / 4;  // 20ms
    }
    else
    {
        fragment_size = 1536 * audio_channels * audio_bits / 8;
        period_time = 25000;  // in usec, interrupt period time
        // in usec, for driver buffer alloc (64k max)
        buffer_time = period_time * 16;
    }

    if (audio_bits == 8)
        format = SND_PCM_FORMAT_S8;
    else if (audio_bits == 16)
        // is the sound data coming in really little-endian or is it
        // CPU-endian?
#if HAVE_BIGENDIAN
        format = SND_PCM_FORMAT_S16;
#else
        format = SND_PCM_FORMAT_S16_LE;
#endif
    else if (audio_bits == 24)
#if HAVE_BIGENDIAN
        format = SND_PCM_FORMAT_S24;
#else
        format = SND_PCM_FORMAT_S24_LE;
#endif
    else
    {
        Error(QString("Unknown sample format: %1 bits.").arg(audio_bits));
        return false;
    }

    err = SetParameters(pcm_handle,
                        format, audio_channels, audio_samplerate, buffer_time,
                        period_time);
    if (err < 0) 
    {
        Error("Unable to set ALSA parameters");
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

void AudioOutputALSA::ReorderSmpteToAlsa6ch(void *buf, int frames)
{
    if (audio_bits == 8)
        _ReorderSmpteToAlsa6ch((unsigned char *)buf, frames);
    else if (audio_bits == 16)
        _ReorderSmpteToAlsa6ch((short *)buf, frames);
}

template <class AudioDataType>
void AudioOutputALSA::_ReorderSmpteToAlsa6ch(AudioDataType *buf, int frames)
{
    AudioDataType tmpC, tmpLFE, *buf2;

    for (int i = 0; i < frames; i++)
    {
        buf = buf2 = buf + 2;

        tmpC = *buf++;
        tmpLFE = *buf++;
        *buf2++ = *buf++;
        *buf2++ = *buf++;
        *buf2++ = tmpC;
        *buf2++ = tmpLFE;
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

    if (!audio_passthru && audio_channels == 6)
        ReorderSmpteToAlsa6ch(aubuf, frames);

    tmpbuf = aubuf;

    VERBOSE(VB_AUDIO+VB_TIMESTAMP,
            QString("WriteAudio: Preparing %1 bytes (%2 frames)")
            .arg(size).arg(frames));
    
    while (frames > 0) 
    {
        lw = pcm_write_func(pcm_handle, tmpbuf, frames);
        
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
            VERBOSE(VB_IMPORTANT, QString("pcm_write_func: %1 (%2)")
                    .arg(snd_strerror(lw)).arg(lw));
            VERBOSE(VB_IMPORTANT, QString("WriteAudio: snd_pcm_state == %1")
                    .arg(snd_pcm_state(pcm_handle)));

            // CloseDevice();
            return;
        }
    }
}

int AudioOutputALSA::GetBufferedOnSoundcard(void) const
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


int AudioOutputALSA::GetSpaceOnSoundcard(void) const
{
    if (pcm_handle == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("GetSpaceOnSoundcard() ") +
                "called with pcm_handle == NULL!");

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
    int avail_bytes = avail * audio_bytes_per_sample;
    if (avail < 0 ||
        avail_bytes > soundcard_buffer_size)
        avail_bytes = soundcard_buffer_size;

    int space = avail_bytes - audio_buffer_unused;

    if (space < 0)
        space = 0;

    return space;
}


int AudioOutputALSA::SetParameters(snd_pcm_t *handle,
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

    /* set the interleaved read/write format, use mmap if available */
    pcm_write_func = &snd_pcm_mmap_writei;
    err = snd_pcm_hw_params_set_access(
        handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (err < 0)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "mmap not available, attempting to fall back to slow writes.");
        QString old_err = snd_strerror(err);
        pcm_write_func = &snd_pcm_writei;
        err = snd_pcm_hw_params_set_access(
            handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (err < 0)
        {
            Error("Interleaved sound types MMAP & RW are not available");
            VERBOSE(VB_IMPORTANT,
                    QString("MMAP Error: %1\n\t\t\tRW Error: %2")
                    .arg(old_err).arg(snd_strerror(err)));
            return err;
        }
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


int AudioOutputALSA::GetVolumeChannel(int channel) const
{
    long actual_volume;

    if (mixer_handle == NULL)
        return 100;

    QByteArray mix_ctl = mixer_control.toAscii();
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, mix_ctl.constData());

    snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer_handle, sid);
    if (!elem)
    {
        VERBOSE(VB_IMPORTANT, QString("Mixer unable to find control %1")
                .arg(mixer_control));
        return 100;
    }

    snd_mixer_selem_channel_id_t chan = (snd_mixer_selem_channel_id_t) channel;
    if (!snd_mixer_selem_has_playback_channel(elem, chan))
    {
        snd_mixer_selem_id_set_index(sid, channel);
        if ((elem = snd_mixer_find_selem(mixer_handle, sid)) == NULL)
        {
            VERBOSE(VB_IMPORTANT, QString("Mixer unable to find control %1 %2")
                    .arg(mixer_control).arg(channel));
            return 100;
        }
    }

    ALSAVolumeInfo vinfo = GetVolumeRange(elem);

    snd_mixer_selem_get_playback_volume(
        elem, (snd_mixer_selem_channel_id_t)channel, &actual_volume);

    return vinfo.ToMythRange(actual_volume);
}

void AudioOutputALSA::SetVolumeChannel(int channel, int volume)
{
    SetCurrentVolume(mixer_control, channel, volume);
}

void AudioOutputALSA::SetCurrentVolume(QString control, int channel, int volume)
{
    VERBOSE(VB_AUDIO, QString("Setting %1 volume to %2")
            .arg(control).arg(volume));

    if (!mixer_handle)
        return; // no mixer, nothing to do

    QByteArray ctl = control.toAscii();
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, ctl.constData());

    snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer_handle, sid);
    if (!elem)
    {
        VERBOSE(VB_IMPORTANT, QString("Mixer unable to find control %1")
                .arg(control));
        return;
    }

    snd_mixer_selem_channel_id_t chan = (snd_mixer_selem_channel_id_t) channel;
    if (!snd_mixer_selem_has_playback_channel(elem, chan))
    {
        snd_mixer_selem_id_set_index(sid, channel);
        if ((elem = snd_mixer_find_selem(mixer_handle, sid)) == NULL)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("mixer unable to find control %1 %2")
                    .arg(control).arg(channel));
            return;
        }
    }

    ALSAVolumeInfo vinfo = GetVolumeRange(elem);

    long set_vol = vinfo.ToALSARange(volume);

    int err = snd_mixer_selem_set_playback_volume(elem, chan, set_vol);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("mixer set channel %1 err %2: %3")
                .arg(channel).arg(err).arg(snd_strerror(err)));
    }
    else
    {
        VERBOSE(VB_AUDIO, QString("channel %1 vol set to %2")
                .arg(channel).arg(set_vol));
    }

    if (snd_mixer_selem_has_playback_switch(elem))
    {
        int unmute = (0 != set_vol);
        if (snd_mixer_selem_has_playback_switch_joined(elem))
        {
            // Only mute if all the channels should be muted.
            for (int i = 0; i < audio_channels; i++)
            {
                if (0 != GetVolumeChannel(i))
                    unmute = 1;
            }
        }

        err = snd_mixer_selem_set_playback_switch(elem, chan, unmute);
        if (err < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Mixer set playback switch %1 err %2: %3")
                    .arg(channel).arg(err).arg(snd_strerror(err)));
        }
        else
        {
            VERBOSE(VB_AUDIO, LOC +
                    QString("channel %1 playback switch set to %2")
                    .arg(channel).arg(unmute));
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

    QString alsadevice = gContext->GetSetting("MixerDevice", "default");
    QString device = alsadevice.remove(QString("ALSA:"));

    if (mixer_handle != NULL)
        CloseMixer();

    if (alsadevice.toLower() == "software")
        return;

    VERBOSE(VB_AUDIO, QString("Opening mixer %1").arg(device));

    // TODO: This is opening card 0. Fix for case of multiple soundcards
    if ((err = snd_mixer_open(&mixer_handle, 0)) < 0)
    {
        Warn(QString("Mixer device open error %1: %2")
             .arg(err).arg(snd_strerror(err)));
        mixer_handle = NULL;
        return;
    }

    QByteArray dev = device.toAscii();
    if ((err = snd_mixer_attach(mixer_handle, dev.constData())) < 0)
    {
        Warn(QString("Mixer attach error %1: %2"
                     "\n\t\t\tCheck Mixer Name in Setup: '%3'")
             .arg(err).arg(snd_strerror(err)).arg(device));
        CloseMixer();
        return;
    }

    if ((err = snd_mixer_selem_register(mixer_handle, NULL, NULL)) < 0)
    {
        Warn(QString("Mixer register error %1: %2")
             .arg(err).arg(snd_strerror(err)));
        CloseMixer();
        return;
    }

    if ((err = snd_mixer_load(mixer_handle)) < 0)
    {
        Warn(QString("Mixer load error %1: %2")
             .arg(err).arg(snd_strerror(err)));
        CloseMixer();
        return;
    }
}

ALSAVolumeInfo AudioOutputALSA::GetVolumeRange(snd_mixer_elem_t *elem) const
{
    long volume_min, volume_max;

    int err = snd_mixer_selem_get_playback_volume_range(
        elem, &volume_min, &volume_max);

    if (err < 0)
    {
        static bool first_time = true;
        if (first_time)
        {
            VERBOSE(VB_IMPORTANT,
                    "snd_mixer_selem_get_playback_volume_range()" + ENO);
            first_time = false;
        }
    }

    ALSAVolumeInfo vinfo(volume_min, volume_max);

    VERBOSE(VB_AUDIO, QString("Volume range is %1 to %2, mult=%3")
            .arg(vinfo.volume_min).arg(vinfo.volume_max)
            .arg(vinfo.range_multiplier));

    return vinfo;
}
