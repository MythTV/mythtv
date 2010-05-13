/*
 * Copyright (C) 2008 Alan Calvert
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <QString>

#include "audiooutputpulse.h"

#define LOC     QString("PulseAudio: ")
#define LOC_ERR QString("PulseAudio Error: ")

#define PULSE_MAX_CHANNELS 6

AudioOutputPulseAudio::AudioOutputPulseAudio(const AudioSettings &settings) :
    AudioOutputBase(settings),
    pcontext(NULL), pstream(NULL), mainloop(NULL),
    sample_rate(settings.samplerate)
{
    for (unsigned int i = 0; i < PA_CHANNELS_MAX; ++i)
        channel_map.map[i] = PA_CHANNEL_POSITION_INVALID;

    volume_control.channels = 0;
    for (unsigned int i = 0; i < PA_CHANNELS_MAX; ++i)
        volume_control.values[i] = PA_VOLUME_MUTED;

    ChooseHost();

    Reconfigure(settings);
}

AudioOutputPulseAudio::~AudioOutputPulseAudio()
{
    KillAudio();
}

bool AudioOutputPulseAudio::OpenDevice()
{
    QString fn_log_tag = "OpenDevice, ";
    if (audio_channels > PULSE_MAX_CHANNELS )
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("audio channel limit %1, but %2 requested")
                .arg(PULSE_MAX_CHANNELS).arg(audio_channels));
        return false;
    }

    sample_spec.rate = sample_rate;
    sample_spec.channels = volume_control.channels = audio_channels;
    switch (audio_bits)
    {
        case 8:
            sample_spec.format = PA_SAMPLE_U8;
            break;
        case 16:
            sample_spec.format = PA_SAMPLE_S16NE;
            break;
        case 32:
            sample_spec.format = PA_SAMPLE_FLOAT32RE;
            break;
        default:
            VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag
                    + QString("unsupported %1 bit sample format")
                    .arg(audio_bits));
            return false;
    }

    if (!pa_sample_spec_valid(&sample_spec))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag
                + QString("invalid sample spec"));
        return false;
    }
    else
    {
        char spec[PA_SAMPLE_SPEC_SNPRINT_MAX];
        pa_sample_spec_snprint(spec, sizeof(spec), &sample_spec);
        VERBOSE(VB_AUDIO, LOC + fn_log_tag +
                QString("using sample spec %1").arg(spec));
    }

    if (!MapChannels())
    {
        return false;
    }
    else
    {
        if (!pa_channel_map_valid(&channel_map))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag
                    + QString("channel map invalid"));
            return false;
        }
    }

    mainloop = pa_threaded_mainloop_new();
    if (!mainloop)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag
                + QString("failed to get new threaded mainloop"));
        return false;
    }

    pa_threaded_mainloop_start(mainloop);
    pa_threaded_mainloop_lock(mainloop);

    if (!ContextConnect())
    {
        pa_threaded_mainloop_unlock(mainloop);
        pa_threaded_mainloop_stop(mainloop);
        return false;
    }

    if (!ConnectPlaybackStream())
    {
        pa_threaded_mainloop_unlock(mainloop);
        pa_threaded_mainloop_stop(mainloop);
        return false;
    }

    pa_threaded_mainloop_unlock(mainloop);
    pa_usec_t usec;
    int neg;
    pa_stream_get_latency(pstream, &usec, &neg);

    VERBOSE(VB_AUDIO, LOC + fn_log_tag
            + QString("total stream latency: %1%2 usecs")
            .arg((neg == 1) ? "-" : "")
            .arg(usec));

    return true;
}

void AudioOutputPulseAudio::CloseDevice()
{
    if (mainloop)
        pa_threaded_mainloop_lock(mainloop);

    if (pstream)
    {
        FlushStream("CloseDevice");
        pa_stream_disconnect(pstream);
        pa_stream_unref(pstream);
        pstream = NULL;
    }

    if (pcontext)
    {
        pa_context_drain(pcontext, NULL, NULL);
        pa_context_disconnect(pcontext);
        pcontext = NULL;
    }

    if (mainloop)
    {
        pa_threaded_mainloop_unlock(mainloop);
        pa_threaded_mainloop_stop(mainloop);
        mainloop = NULL;
    }
}

void AudioOutputPulseAudio::WriteAudio(unsigned char *aubuf, int size)
{
    if (audio_actually_paused)
        return;

    QString fn_log_tag = "WriteAudio, ";
    pa_stream_state_t sstate = pa_stream_get_state(pstream);

    /* NB This "if" check can be replaced with PA_STREAM_IS_GOOD() in 
       PulseAudio API from 0.9.11. As 0.9.10 is still widely used
       we use the more verbose version for now */

    if (sstate == PA_STREAM_CREATING || sstate == PA_STREAM_READY)
    {
        int retries = 0;
        int write_status = PA_ERR_INVALID;
        size_t write;
        size_t writable;
        size_t to_write = size;
        unsigned char *buf_ptr = aubuf;
        pa_context_state_t cstate;

        pa_threaded_mainloop_lock(mainloop);
        while (to_write > 0 && retries < 3)
        {
            write_status = 0;
            writable = pa_stream_writable_size(pstream);
            if (writable > 0)
            {
                write = min(to_write, writable);
                write_status = pa_stream_write(pstream, buf_ptr, write,
                                               NULL, 0, PA_SEEK_RELATIVE);
                if (!write_status)
                {
                    buf_ptr += write;
                    to_write -= write;
                }
                else
                    break;
            }
            else if (writable < 0)
                break;
            else // writable == 0
                pa_threaded_mainloop_wait(mainloop);
            ++retries;
        }
        pa_threaded_mainloop_unlock(mainloop);
        if (to_write > 0)
        {
            if (writable < 0)
            {
                cstate = pa_context_get_state(pcontext);
                sstate = pa_stream_get_state(pstream);
                VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                        QString("stream unfit for writing (writable < 0), "
                                "context state: %1, stream state: %2")
                        .arg(cstate,0,16).arg(sstate,0,16));
            }

            if (write_status != 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                        QString("stream write failed: %1")
                        .arg((write_status == PA_ERR_BADSTATE)
                             ? "PA_ERR_BADSTATE"
                             : "PA_ERR_INVALID"));
            }

            VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                    QString("short write, %1 of %2, tries %3")
                    .arg(size - to_write).arg(size).arg(retries));
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("stream state not good: %1").arg(sstate,0,16));
    }
}

int AudioOutputPulseAudio::GetBufferedOnSoundcard(void) const
{
    pa_threaded_mainloop_lock(mainloop);
    int writable = pa_stream_writable_size(pstream);
    pa_threaded_mainloop_unlock(mainloop);
    return soundcard_buffer_size - writable;
}

int AudioOutputPulseAudio::GetSpaceOnSoundcard(void) const
{
    pa_threaded_mainloop_lock(mainloop);
    int writable = pa_stream_writable_size(pstream);
    pa_threaded_mainloop_unlock(mainloop);
    return writable;
}

int AudioOutputPulseAudio::GetVolumeChannel(int channel) const
{
    return (float)volume_control.values[channel]
        / (float)PA_VOLUME_NORM * 100.0f;
}

void AudioOutputPulseAudio::SetVolumeChannel(int channel, int volume)
{
    QString fn_log_tag = "SetVolumeChannel, ";
    if (channel < 0 || channel > PULSE_MAX_CHANNELS || volume < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("bad volume params, channel %1, volume %2")
                .arg(channel).arg(volume));
        return;
    }
    volume_control.values[channel] =
        (float)volume / 100.0f * (float)PA_VOLUME_NORM;
    volume = min(100, volume);
    volume = max(0, volume);
    uint32_t sink_index = pa_stream_get_device_index(pstream);
    pa_threaded_mainloop_lock(mainloop);
    pa_operation *op =
        pa_context_set_sink_volume_by_index(pcontext, sink_index,
                                            &volume_control,
                                            OpCompletionCallback, this);
    pa_threaded_mainloop_unlock(mainloop);
    if (op)
        pa_operation_unref(op);
    else
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("set sink volume operation failed, sink: %1, "
                        "error: %2 ")
                .arg(sink_index)
                .arg(pa_strerror(pa_context_errno(pcontext))));
}

void AudioOutputPulseAudio::Pause(bool paused)
{
    pa_operation *op;
    if (paused && !audio_actually_paused)
    {
        FlushStream("Pause");
        pa_threaded_mainloop_lock(mainloop);
        op = pa_stream_cork(pstream, 1, NULL, this);
        pa_threaded_mainloop_unlock(mainloop);
        if (op)
        {
            pa_operation_unref(op);
            audio_actually_paused = true;
            VERBOSE(VB_AUDIO, LOC + "Pause, audio actually paused");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Pause, cork stream failed");
            audio_actually_paused = false;
        }
        pauseaudio = true;
    }
    else if(!paused && audio_actually_paused)
    {
        pa_threaded_mainloop_lock(mainloop);
        op = pa_stream_cork(pstream, 0, NULL, this);
        pa_threaded_mainloop_unlock(mainloop);

        if (op)
        {
            pa_operation_unref(op);
            VERBOSE(VB_AUDIO, LOC + "Pause, audio actually unpaused");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Pause, un-cork stream failed");
        }

        FlushStream("(un)Pause");
        pauseaudio = false;
        audio_actually_paused = false;
    }
}

void AudioOutputPulseAudio::Reset(void)
{
    AudioOutputBase::Reset();
    FlushStream("Reset");
}

void AudioOutputPulseAudio::Drain(void)
{
    AudioOutputBase::Drain();
    pa_threaded_mainloop_lock(mainloop);
    pa_operation *op = pa_stream_drain(pstream, NULL, this);
    pa_threaded_mainloop_unlock(mainloop);

    if (op)
        pa_operation_unref(op);
    else
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Drain, stream drain failed ");
}

bool AudioOutputPulseAudio::MapChannels(void)
{
    QString fn_log_tag = "MapChannels, ";
    channel_map.channels = audio_channels;
    for (int ch = 0; ch < channel_map.channels; ++ch)
    {
        switch (ch)
        {
            case 0:
                channel_map.map[0] =
                    (audio_channels < 2) ? PA_CHANNEL_POSITION_MONO
                    : PA_CHANNEL_POSITION_FRONT_LEFT;
                break;
            case 1:
                channel_map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
                break;
            case 2:
                channel_map.map[2] = PA_CHANNEL_POSITION_REAR_LEFT;
                break;
            case 3:
                channel_map.map[3] = PA_CHANNEL_POSITION_REAR_RIGHT;
                break;
            case 4:
                channel_map.map[4] = PA_CHANNEL_POSITION_FRONT_CENTER;
                break;
            case 5:
                channel_map.map[5] = PA_CHANNEL_POSITION_LFE;
                break;
            default:
                VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                        QString("invalid channel map count: %1 channels")
                        .arg(audio_channels));
                return false;
        }
    }
    return true;
}

bool AudioOutputPulseAudio::ContextConnect(void)
{
    QString fn_log_tag = "ContextConnect, ";
    if (pcontext)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("context appears to exist, but shouldn't (yet)"));
        pa_context_unref(pcontext);
        pcontext = NULL;
        return false;
    }
    pcontext = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "MythTV");
    if (!pcontext)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("failed to acquire new context"));
        return false;
    }
    pa_context_set_state_callback(pcontext, ContextStateCallback, this);

    char *pulse_host = ChooseHost();
    int chk = pa_context_connect(
        pcontext, pulse_host, (pa_context_flags_t)0, NULL);

    if (pulse_host)
        delete(pulse_host);

    if (chk < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("context connect failed: %1")
                .arg(pa_strerror(pa_context_errno(pcontext))));
        return false;
    }
    bool connected = false;
    pa_context_state_t state = pa_context_get_state(pcontext);
    for (; !connected; state = pa_context_get_state(pcontext))
    {
        switch(state)
        {
            case PA_CONTEXT_READY:
                VERBOSE(VB_AUDIO, LOC + fn_log_tag +
                        QString("context connection ready, move on"));
                connected = true;
                continue;

            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                        QString("context connection failed or was "
                                "terminated: %1")
                        .arg(pa_strerror(pa_context_errno(pcontext))));
                return false;

            default:
                VERBOSE(VB_AUDIO, LOC + fn_log_tag +
                        QString("waiting for context connection ready"));
                pa_threaded_mainloop_wait(mainloop);
                break;
        }
    }

    pa_operation *op =
        pa_context_get_server_info(pcontext, ServerInfoCallback, this);

    if (op)
    {
        pa_operation_unref(op);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("failed to get PulseAudio server info"));
    }

    return true;
}

char *AudioOutputPulseAudio::ChooseHost(void)
{
    QString fn_log_tag = "ChooseHost, ";
    char *pulse_host = NULL;
    char *device = strdup(audio_main_device.toAscii().constData());
    const char *host;
    for (host=device; host && *host != ':' && *host != 0; host++);
    if (host && *host != 0)
        host++;
    if ( !(!host || *host == 0 || strcmp(host,"default") == 0))
    {
        if ((pulse_host = new char[strlen(host)]))
            strcpy(pulse_host, host);
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                    QString("allocation of pulse host '%1' char[%2] failed")
                    .arg(host).arg(strlen(host) + 1));
        }
    }
    if (!pulse_host && strcmp(host,"default") != 0)
    {
        char *env_pulse_host = getenv("PULSE_SERVER");
        if (env_pulse_host && strlen(env_pulse_host) > 0)
        {
            int host_len = strlen(env_pulse_host) + 1;
            if ((pulse_host = new char[host_len]))
                strcpy(pulse_host, env_pulse_host);
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                        QString("allocation of pulse host '%1' char[%2] failed")
                        .arg(env_pulse_host).arg(host_len));
            }
        }
    }

    VERBOSE(VB_AUDIO, LOC + fn_log_tag +
            QString("chosen PulseAudio server: %1")
            .arg((pulse_host != NULL) ? pulse_host : "default"));

    free(device);

    return pulse_host;
}

bool AudioOutputPulseAudio::ConnectPlaybackStream(void)
{
    QString fn_log_tag = "ConnectPlaybackStream, ";
    pstream = pa_stream_new(pcontext, "MythTV playback", &sample_spec,
                            &channel_map);
    if (!pstream)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("failed to create new playback stream"));
        return false;
    }
    pa_stream_set_state_callback(pstream, StreamStateCallback, this);
    pa_stream_set_overflow_callback(pstream, BufferFlowCallback, (char*)"over");
    pa_stream_set_underflow_callback(pstream, BufferFlowCallback,
                                     (char*)"under");
    if (set_initial_vol)
    {
        int volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        pa_cvolume_set(&volume_control, audio_channels,
                       (float)volume * (float)PA_VOLUME_NORM / 100.0f);
    }
    else
        pa_cvolume_reset(&volume_control, audio_channels);

    // set myth sizes and pa buffer metrics (20 ms)
    // Note: The 20 is an unsigned long (at least 32 bits).
    // sample_rate, audio_bits and audio_channels are at least
    // that size, so the math will be done with a range of at
    // least 2 billion. If we assume a max audio_bits of 32,
    // audio_channels of 16 and sample_rate of <= 448000, then
    // the largest number will be 230 million, well within range.
    fragment_size = 20UL * sample_rate * audio_bits * audio_channels
        / 8 /* 8 bits per byte */ / 1000 /* 1000 ms per second */;

    soundcard_buffer_size = 16 * fragment_size;
    buffer_settings.maxlength = soundcard_buffer_size;
    buffer_settings.tlength = fragment_size * 4;
    buffer_settings.prebuf = (uint32_t)-1;
    buffer_settings.minreq = (uint32_t)-1;
    VERBOSE(VB_AUDIO, LOC + fn_log_tag +
            QString("fragment size %1, soundcard buffer size %2")
            .arg(fragment_size).arg(soundcard_buffer_size));

    int flags = PA_STREAM_INTERPOLATE_TIMING
        | PA_STREAM_AUTO_TIMING_UPDATE
        | PA_STREAM_NO_REMAP_CHANNELS
        | PA_STREAM_NO_REMIX_CHANNELS;
    pa_stream_connect_playback(pstream, NULL, &buffer_settings,
                               (pa_stream_flags_t)flags, &volume_control, NULL);
    pa_context_state_t cstate;
    pa_stream_state_t sstate;
    bool connected = false, failed = false;
    while (!(connected || failed))
    {
        switch (cstate = pa_context_get_state(pcontext))
        {
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                        QString("context is stuffed, %1")
                        .arg(pa_strerror(pa_context_errno(
                                             pcontext))));
                failed = true;
                break;
            default:
                switch (sstate = pa_stream_get_state(pstream))
                {
                    case PA_STREAM_READY:
                        connected = true;
                        break;
                    case PA_STREAM_FAILED:
                    case PA_STREAM_TERMINATED:
                        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                                QString("stream failed or was terminated, "
                                        "context state %1, stream state %2")
                                .arg(cstate).arg(sstate));
                        failed = true;
                        break;
                    default:
                        pa_threaded_mainloop_wait(mainloop);
                        break;
                }
        }
    }
    return (connected && !failed);
}

void AudioOutputPulseAudio::FlushStream(const char *caller)
{
    QString fn_log_tag = QString("FlushStream (%1), ").arg(caller);
    pa_threaded_mainloop_lock(mainloop);
    pa_operation *op = pa_stream_flush(pstream, NULL, this);
    pa_threaded_mainloop_unlock(mainloop);
    if (op)
        pa_operation_unref(op);
    else
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag
                + "stream flush operation failed ");
}

void AudioOutputPulseAudio::ContextStateCallback(pa_context *c, void *arg)
{
    QString fn_log_tag = "_ContextStateCallback, ";
    AudioOutputPulseAudio *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    switch (pa_context_get_state(c))
    {
        case PA_CONTEXT_READY:
            pa_threaded_mainloop_signal(audoutP->mainloop, 0);
            break;
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(audoutP->mainloop, 0);
            break;
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
    }
}

void AudioOutputPulseAudio::StreamStateCallback(pa_stream *s, void *arg)
{
    QString fn_log_tag = "StreamStateCallback, ";
    AudioOutputPulseAudio *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    switch (pa_stream_get_state(s))
    {
        case PA_STREAM_READY:
        case PA_STREAM_TERMINATED:
        case PA_STREAM_FAILED:
            pa_threaded_mainloop_signal(audoutP->mainloop, 0);
            break;
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

void AudioOutputPulseAudio::BufferFlowCallback(pa_stream *s, void *tag)
{
    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("stream buffer %1flow")
            .arg((char*)tag));
}

void AudioOutputPulseAudio::OpCompletionCallback(
    pa_context *c, int ok, void *arg)
{
    QString fn_log_tag = "OpCompletionCallback, ";
    AudioOutputPulseAudio *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + fn_log_tag +
                QString("bummer, an operation failed: %1")
                .arg(pa_strerror(pa_context_errno(c))));
    }
    pa_threaded_mainloop_signal(audoutP->mainloop, 0);
}

void AudioOutputPulseAudio::ServerInfoCallback(
    pa_context *context, const pa_server_info *inf, void *arg)
{
    QString fn_log_tag = "ServerInfoCallback, ";

    VERBOSE(VB_AUDIO, LOC + fn_log_tag +
            QString("PulseAudio server info - host name: %1, server version: "
                    "%2, server name: %3, default sink: %4")
            .arg(inf->host_name).arg(inf->server_version)
            .arg(inf->server_name).arg(inf->default_sink_name));
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
