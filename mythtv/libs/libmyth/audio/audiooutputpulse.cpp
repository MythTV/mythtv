/*
 * Copyright (C) 2008 Alan Calvert, 2010 foobum@gmail.com
 * 
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

#define PULSE_MAX_CHANNELS 8

AudioOutputPulseAudio::AudioOutputPulseAudio(const AudioSettings &settings) :
    AudioOutputBase(settings),
    pcontext(NULL), pstream(NULL), mainloop(NULL),
    m_aosettings(NULL)
{
    volume_control.channels = 0;
    for (unsigned int i = 0; i < PA_CHANNELS_MAX; ++i)
        volume_control.values[i] = PA_VOLUME_MUTED;

    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

AudioOutputPulseAudio::~AudioOutputPulseAudio()
{
    KillAudio();
    if (pcontext)
    {
        pa_context_unref(pcontext);
        pcontext = NULL;
    }
}

AudioOutputSettings* AudioOutputPulseAudio::GetOutputSettings(bool /*digital*/)
{
    AudioFormat fmt;
    m_aosettings = new AudioOutputSettings();
    QString fn_log_tag = "OpenDevice, ";

    /* Start the mainloop and connect a context so we can retrieve the
       parameters of the default sink */
    mainloop = pa_threaded_mainloop_new();
    if (!mainloop)
    {
        VBERROR(fn_log_tag + "Failed to get new threaded mainloop");
        delete m_aosettings;
        return NULL;
    }

    pa_threaded_mainloop_start(mainloop);
    pa_threaded_mainloop_lock(mainloop);

    if (!ContextConnect())
    {
        pa_threaded_mainloop_unlock(mainloop);
        pa_threaded_mainloop_stop(mainloop);
        delete m_aosettings;
        return NULL;
    }

    /* Get the samplerate and channel count of the default sink, supported rate
       and channels are added in SinkInfoCallback */
    /* We should in theory be able to feed pulse any samplerate but allowing it
       to resample results in weird behaviour (odd channel maps, static) post
       pause / reset */
    pa_operation *op = pa_context_get_sink_info_by_index(pcontext, 0,
                                                         SinkInfoCallback,
                                                         this);
    if (op)
    {
        pa_operation_unref(op);
        pa_threaded_mainloop_wait(mainloop);
    }
    else
        VBERROR("Failed to determine default sink samplerate");

    pa_threaded_mainloop_unlock(mainloop);

    // All formats except S24 (pulse wants S24LSB)
    while ((fmt = m_aosettings->GetNextFormat()))
    {
        if (fmt == FORMAT_S24
// define from PA 0.9.15 only
#ifndef PA_MAJOR
            || fmt == FORMAT_S24LSB
#endif
            )
            continue;
        m_aosettings->AddSupportedFormat(fmt);
    }

    pa_context_disconnect(pcontext);
    pa_context_unref(pcontext);
    pcontext = NULL;
    pa_threaded_mainloop_stop(mainloop);
    mainloop = NULL;

    return m_aosettings;
}

bool AudioOutputPulseAudio::OpenDevice()
{
    QString fn_log_tag = "OpenDevice, ";
    if (channels > PULSE_MAX_CHANNELS )
    {
        VBERROR(fn_log_tag + QString("audio channel limit %1, but %2 requested")
                             .arg(PULSE_MAX_CHANNELS).arg(channels));
        return false;
    }

    sample_spec.rate = samplerate;
    sample_spec.channels = volume_control.channels = channels;
    switch (output_format)
    {
        case FORMAT_U8:     sample_spec.format = PA_SAMPLE_U8;         break;
        case FORMAT_S16:    sample_spec.format = PA_SAMPLE_S16NE;      break;
// define from PA 0.9.15 only
#ifdef PA_MAJOR
        case FORMAT_S24LSB: sample_spec.format = PA_SAMPLE_S24_32NE;   break;
#endif
        case FORMAT_S32:    sample_spec.format = PA_SAMPLE_S32NE;      break;
        case FORMAT_FLT:    sample_spec.format = PA_SAMPLE_FLOAT32NE;  break;
        default:
            VBERROR(fn_log_tag + QString("unsupported sample format %1")
                                 .arg(output_format));
            return false;
    }

    if (!pa_sample_spec_valid(&sample_spec))
    {
        VBERROR(fn_log_tag + "invalid sample spec");
        return false;
    }
    else
    {
        char spec[PA_SAMPLE_SPEC_SNPRINT_MAX];
        pa_sample_spec_snprint(spec, sizeof(spec), &sample_spec);
        VBAUDIO(fn_log_tag + QString("using sample spec %1").arg(spec));
    }

    pa_channel_map *pmap = NULL;

    if(!(pmap = pa_channel_map_init_auto(&channel_map, channels,
                                         PA_CHANNEL_MAP_WAVEEX)))
    {
        VBERROR(fn_log_tag + "failed to init channel map");
        return false;
    }

    channel_map = *pmap;

    mainloop = pa_threaded_mainloop_new();
    if (!mainloop)
    {
        VBERROR(fn_log_tag + "failed to get new threaded mainloop");
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
        pa_context_unref(pcontext);
        pcontext = NULL;
    }

    if (mainloop)
    {
        pa_threaded_mainloop_unlock(mainloop);
        pa_threaded_mainloop_stop(mainloop);
        mainloop = NULL;
    }
}

void AudioOutputPulseAudio::WriteAudio(uchar *aubuf, int size)
{
    QString fn_log_tag = "WriteAudio, ";
    pa_stream_state_t sstate = pa_stream_get_state(pstream);

    VBAUDIOTS(fn_log_tag + QString("writing %1 bytes").arg(size));

    /* NB This "if" check can be replaced with PA_STREAM_IS_GOOD() in
       PulseAudio API from 0.9.11. As 0.9.10 is still widely used
       we use the more verbose version for now */

    if (sstate == PA_STREAM_CREATING || sstate == PA_STREAM_READY)
    {
        int write_status = PA_ERR_INVALID;
        size_t to_write = size;
        unsigned char *buf_ptr = aubuf;

        pa_threaded_mainloop_lock(mainloop);
        while (to_write > 0)
        {
            write_status = 0;
            size_t writable = pa_stream_writable_size(pstream);
            if (writable > 0)
            {
                size_t write = min(to_write, writable);
                write_status = pa_stream_write(pstream, buf_ptr, write,
                                               NULL, 0, PA_SEEK_RELATIVE);

                if (0 != write_status)
                    break;

                buf_ptr += write;
                to_write -= write;
            }
            else
            {
                pa_threaded_mainloop_wait(mainloop);
            }
        }
        pa_threaded_mainloop_unlock(mainloop);

        if (to_write > 0)
        {
            if (write_status != 0)
                VBERROR(fn_log_tag + QString("stream write failed: %1")
                                     .arg(write_status == PA_ERR_BADSTATE
                                                ? "PA_ERR_BADSTATE"
                                                : "PA_ERR_INVALID"));

            VBERROR(fn_log_tag + QString("short write, %1 of %2")
                                 .arg(size - to_write).arg(size));
        }
    }
    else
        VBERROR(fn_log_tag + QString("stream state not good: %1")
                             .arg(sstate,0,16));
}

int AudioOutputPulseAudio::GetBufferedOnSoundcard(void) const
{
    pa_usec_t latency = 0;
    size_t buffered = 0;

    if (!pcontext || pa_context_get_state(pcontext) != PA_CONTEXT_READY)
        return 0;

    if (!pstream || pa_stream_get_state(pstream) != PA_STREAM_READY)
        return 0;

    const pa_buffer_attr *buf_attr =  pa_stream_get_buffer_attr(pstream);
    size_t bfree = pa_stream_writable_size(pstream);
    buffered = buf_attr->tlength - bfree;

    pa_threaded_mainloop_lock(mainloop);

    while (pa_stream_get_latency(pstream, &latency, NULL) < 0)
    {
        if (pa_context_errno(pcontext) != PA_ERR_NODATA)
        {
            latency = 0;
            break;
        }
        pa_threaded_mainloop_wait(mainloop);
    }

    pa_threaded_mainloop_unlock(mainloop);

    return ((uint64_t)latency * samplerate *
            output_bytes_per_frame / 1000000) + buffered;
}

int AudioOutputPulseAudio::GetVolumeChannel(int channel) const
{
    return (float)volume_control.values[channel] /
           (float)PA_VOLUME_NORM * 100.0f;
}

void AudioOutputPulseAudio::SetVolumeChannel(int channel, int volume)
{
    QString fn_log_tag = "SetVolumeChannel, ";

    if (channel < 0 || channel > PULSE_MAX_CHANNELS || volume < 0)
    {
        VBERROR(fn_log_tag + QString("bad volume params, channel %1, volume %2")
                             .arg(channel).arg(volume));
        return;
    }

    volume_control.values[channel] =
        (float)volume / 100.0f * (float)PA_VOLUME_NORM;

    volume = min(100, volume);
    volume = max(0, volume);

    if (gCoreContext->GetSetting("MixerControl", "PCM").toLower() == "pcm")
    {
        uint32_t stream_index = pa_stream_get_index(pstream);
        pa_threaded_mainloop_lock(mainloop);
        pa_operation *op =
            pa_context_set_sink_input_volume(pcontext, stream_index,
                                             &volume_control,
                                             OpCompletionCallback, this);
        pa_threaded_mainloop_unlock(mainloop);
        if (op)
            pa_operation_unref(op);
        else
            VBERROR(fn_log_tag +
                    QString("set stream volume operation failed, stream %1, "
                            "error %2 ")
                    .arg(stream_index)
                    .arg(pa_strerror(pa_context_errno(pcontext))));
    }
    else
    {
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
            VBERROR(fn_log_tag +
                    QString("set sink volume operation failed, sink %1, "
                            "error %2 ")
                    .arg(sink_index)
                    .arg(pa_strerror(pa_context_errno(pcontext))));
    }
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
        VBERROR("Drain, stream drain failed");
}

bool AudioOutputPulseAudio::ContextConnect(void)
{
    QString fn_log_tag = "ContextConnect, ";
    if (pcontext)
    {
        VBERROR(fn_log_tag + "context appears to exist, but shouldn't (yet)");
        pa_context_unref(pcontext);
        pcontext = NULL;
        return false;
    }
    pa_proplist *proplist = pa_proplist_new();
    if (!proplist)
    {
        VBERROR(fn_log_tag + QString("failed to create new proplist"));
        return false;
    }
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, "MythTV");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ICON_NAME, "mythtv");
    pa_proplist_sets(proplist, PA_PROP_MEDIA_ROLE, "video");
    pcontext =
        pa_context_new_with_proplist(pa_threaded_mainloop_get_api(mainloop),
                                     "MythTV", proplist);
    if (!pcontext)
    {
        VBERROR(fn_log_tag + "failed to acquire new context");
        return false;
    }
    pa_context_set_state_callback(pcontext, ContextStateCallback, this);

    char *pulse_host = ChooseHost();
    int chk = pa_context_connect(
        pcontext, pulse_host, (pa_context_flags_t)0, NULL);

    delete[] pulse_host;

    if (chk < 0)
    {
        VBERROR(fn_log_tag + QString("context connect failed: %1")
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
                VBAUDIO(fn_log_tag +"context connection ready");
                connected = true;
                continue;

            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                VBERROR(fn_log_tag +
                        QString("context connection failed or terminated: %1")
                        .arg(pa_strerror(pa_context_errno(pcontext))));
                return false;

            default:
                VBAUDIO(fn_log_tag + "waiting for context connection ready");
                pa_threaded_mainloop_wait(mainloop);
                break;
        }
    }

    pa_operation *op =
        pa_context_get_server_info(pcontext, ServerInfoCallback, this);

    if (op)
        pa_operation_unref(op);
    else
        VBERROR(fn_log_tag + "failed to get PulseAudio server info");

    return true;
}

char *AudioOutputPulseAudio::ChooseHost(void)
{
    QString fn_log_tag = "ChooseHost, ";
    char *pulse_host = NULL;
    char *device = strdup(main_device.toLatin1().constData());
    const char *host;

    for (host=device; host && *host != ':' && *host != 0; host++);

    if (host && *host != 0)
        host++;

    if ( !(!host || *host == 0 || strcmp(host,"default") == 0))
    {
        if ((pulse_host = new char[strlen(host) + 1]))
            strcpy(pulse_host, host);
        else
            VBERROR(fn_log_tag +
                    QString("allocation of pulse host '%1' char[%2] failed")
                    .arg(host).arg(strlen(host) + 1));
    }

    if (!pulse_host && strcmp(host,"default") != 0)
    {
        char *env_pulse_host = getenv("PULSE_SERVER");
        if (env_pulse_host && (*env_pulse_host != '\0'))
        {
            int host_len = strlen(env_pulse_host) + 1;

            if ((pulse_host = new char[host_len]))
                strcpy(pulse_host, env_pulse_host);
            else
            {
                VBERROR(fn_log_tag +
                        QString("allocation of pulse host '%1' char[%2] failed")
                        .arg(env_pulse_host).arg(host_len));
            }
        }
    }

    VBAUDIO(fn_log_tag + QString("chosen PulseAudio server: %1")
                         .arg((pulse_host != NULL) ? pulse_host : "default"));

    free(device);

    return pulse_host;
}

bool AudioOutputPulseAudio::ConnectPlaybackStream(void)
{
    QString fn_log_tag = "ConnectPlaybackStream, ";
    pa_proplist *proplist = pa_proplist_new();
    if (!proplist)
    {
        VBERROR(fn_log_tag + QString("failed to create new proplist"));
        return false;
    }
    pa_proplist_sets(proplist, PA_PROP_MEDIA_ROLE, "video");
    pstream =
        pa_stream_new_with_proplist(pcontext, "MythTV playback", &sample_spec,
                                    &channel_map, proplist);
    if (!pstream)
    {
        VBERROR("failed to create new playback stream");
        return false;
    }
    pa_stream_set_state_callback(pstream, StreamStateCallback, this);
    pa_stream_set_write_callback(pstream, WriteCallback, this);
    pa_stream_set_overflow_callback(pstream, BufferFlowCallback, (char*)"over");
    pa_stream_set_underflow_callback(pstream, BufferFlowCallback,
                                     (char*)"under");
    if (set_initial_vol)
    {
        int volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        pa_cvolume_set(&volume_control, channels,
                       (float)volume * (float)PA_VOLUME_NORM / 100.0f);
    }
    else
        pa_cvolume_reset(&volume_control, channels);

    fragment_size = (samplerate * 25 * output_bytes_per_frame) / 1000;

    buffer_settings.maxlength = (uint32_t)-1;
    buffer_settings.tlength = fragment_size * 4;
    buffer_settings.prebuf = (uint32_t)-1;
    buffer_settings.minreq = (uint32_t)-1;

    int flags = PA_STREAM_INTERPOLATE_TIMING
        | PA_STREAM_ADJUST_LATENCY
        | PA_STREAM_AUTO_TIMING_UPDATE
        | PA_STREAM_NO_REMIX_CHANNELS;

    pa_stream_connect_playback(pstream, NULL, &buffer_settings,
                               (pa_stream_flags_t)flags, NULL, NULL);

    pa_context_state_t cstate;
    pa_stream_state_t sstate;
    bool connected = false, failed = false;

    while (!(connected || failed))
    {
        switch (cstate = pa_context_get_state(pcontext))
        {
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                VBERROR(QString("context is stuffed, %1")
                            .arg(pa_strerror(pa_context_errno(pcontext))));
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
                        VBERROR(QString("stream failed or was terminated, "
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

    const pa_buffer_attr *buf_attr = pa_stream_get_buffer_attr(pstream);
    fragment_size = buf_attr->tlength >> 2;
    soundcard_buffer_size = buf_attr->maxlength;

    VBAUDIO(QString("fragment size %1, soundcard buffer size %2")
                .arg(fragment_size).arg(soundcard_buffer_size));

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
        VBERROR(fn_log_tag + "stream flush operation failed ");
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

void AudioOutputPulseAudio::WriteCallback(pa_stream *s, size_t size, void *arg)
{
    AudioOutputPulseAudio *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    pa_threaded_mainloop_signal(audoutP->mainloop, 0);
}

void AudioOutputPulseAudio::BufferFlowCallback(pa_stream *s, void *tag)
{
    VBERROR(QString("stream buffer %1 flow").arg((char*)tag));
}

void AudioOutputPulseAudio::OpCompletionCallback(
    pa_context *c, int ok, void *arg)
{
    QString fn_log_tag = "OpCompletionCallback, ";
    AudioOutputPulseAudio *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    if (!ok)
    {
        VBERROR(fn_log_tag + QString("bummer, an operation failed: %1")
                             .arg(pa_strerror(pa_context_errno(c))));
    }
    pa_threaded_mainloop_signal(audoutP->mainloop, 0);
}

void AudioOutputPulseAudio::ServerInfoCallback(
    pa_context *context, const pa_server_info *inf, void *arg)
{
    QString fn_log_tag = "ServerInfoCallback, ";

    VBAUDIO(fn_log_tag +
            QString("PulseAudio server info - host name: %1, server version: "
                    "%2, server name: %3, default sink: %4")
            .arg(inf->host_name).arg(inf->server_version)
            .arg(inf->server_name).arg(inf->default_sink_name));
}

void AudioOutputPulseAudio::SinkInfoCallback(
    pa_context *c, const pa_sink_info *info, int eol, void *arg)
{
    AudioOutputPulseAudio *audoutP = static_cast<AudioOutputPulseAudio*>(arg);

    if (!info)
    {
        pa_threaded_mainloop_signal(audoutP->mainloop, 0);
        return;
    }

    audoutP->m_aosettings->AddSupportedRate(info->sample_spec.rate);

    for (uint i = 2; i <= info->sample_spec.channels; i++)
        audoutP->m_aosettings->AddSupportedChannels(i);

    pa_threaded_mainloop_signal(audoutP->mainloop, 0);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
