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

#include "audiooutputpulse.h"

// QT headers
#include <QString>

// C++ headers
#include <algorithm>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#define LOC     QString("PulseAudio: ")

static constexpr int8_t PULSE_MAX_CHANNELS { 8 };

AudioOutputPulseAudio::AudioOutputPulseAudio(const AudioSettings &settings) :
    AudioOutputBase(settings)
{
    m_volumeControl.channels = 0;
    for (uint & value : m_volumeControl.values)
        value = PA_VOLUME_MUTED;

    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
}

AudioOutputPulseAudio::~AudioOutputPulseAudio()
{
    KillAudio();
    if (m_pcontext)
    {
        pa_context_unref(m_pcontext);
        m_pcontext = nullptr;
    }
    if (m_ctproplist)
    {
        pa_proplist_free(m_ctproplist);
        m_ctproplist = nullptr;
    }
    if (m_stproplist)
    {
        pa_proplist_free(m_stproplist);
        m_stproplist = nullptr;
    }
}

AudioOutputSettings* AudioOutputPulseAudio::GetOutputSettings(bool /*digital*/)
{
    m_aoSettings = new AudioOutputSettings();
    QString fn_log_tag = "OpenDevice, ";

    /* Start the mainloop and connect a context so we can retrieve the
       parameters of the default sink */
    m_mainloop = pa_threaded_mainloop_new();
    if (!m_mainloop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "Failed to get new threaded mainloop");
        delete m_aoSettings;
        return nullptr;
    }

    pa_threaded_mainloop_start(m_mainloop);
    pa_threaded_mainloop_lock(m_mainloop);

    if (!ContextConnect())
    {
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_stop(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
        if (m_ctproplist)
        {
            pa_proplist_free(m_ctproplist);
            m_ctproplist = nullptr;
        }
        if (m_stproplist)
        {
            pa_proplist_free(m_stproplist);
            m_stproplist = nullptr;
        }
        delete m_aoSettings;
        return nullptr;
    }

    /* Get the samplerate and channel count of the default sink, supported rate
       and channels are added in SinkInfoCallback */
    /* We should in theory be able to feed pulse any samplerate but allowing it
       to resample results in weird behaviour (odd channel maps, static) post
       pause / reset */
    pa_operation *op = pa_context_get_sink_info_by_index(m_pcontext, 0,
                                                         SinkInfoCallback,
                                                         this);
    if (op)
    {
        pa_operation_unref(op);
        pa_threaded_mainloop_wait(m_mainloop);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to determine default sink samplerate");
    }

    pa_threaded_mainloop_unlock(m_mainloop);

    // All formats except S24 (pulse wants S24LSB)
    AudioFormat fmt = FORMAT_NONE;
    while ((fmt = m_aoSettings->GetNextFormat()))
    {
        if (fmt == FORMAT_S24
// define from PA 0.9.15 only
#ifndef PA_MAJOR
            || fmt == FORMAT_S24LSB
#endif
            )
            continue;
        m_aoSettings->AddSupportedFormat(fmt);
    }

    pa_context_disconnect(m_pcontext);
    pa_context_unref(m_pcontext);
    m_pcontext = nullptr;
    pa_threaded_mainloop_stop(m_mainloop);
    pa_threaded_mainloop_free(m_mainloop);
    m_mainloop = nullptr;
    if (m_ctproplist)
    {
        pa_proplist_free(m_ctproplist);
        m_ctproplist = nullptr;
    }
    if (m_stproplist)
    {
        pa_proplist_free(m_stproplist);
        m_stproplist = nullptr;
    }

    return m_aoSettings;
}

bool AudioOutputPulseAudio::OpenDevice()
{
    QString fn_log_tag = "OpenDevice, ";
    if (m_channels > PULSE_MAX_CHANNELS )
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("audio channel limit %1, but %2 requested")
                             .arg(PULSE_MAX_CHANNELS).arg(m_channels));
        return false;
    }

    m_sampleSpec.rate = m_sampleRate;
    m_sampleSpec.channels = m_volumeControl.channels = m_channels;
    switch (m_outputFormat)
    {
        case FORMAT_U8:     m_sampleSpec.format = PA_SAMPLE_U8;         break;
        case FORMAT_S16:    m_sampleSpec.format = PA_SAMPLE_S16NE;      break;
// define from PA 0.9.15 only
#ifdef PA_MAJOR
        case FORMAT_S24LSB: m_sampleSpec.format = PA_SAMPLE_S24_32NE;   break;
#endif
        case FORMAT_S32:    m_sampleSpec.format = PA_SAMPLE_S32NE;      break;
        case FORMAT_FLT:    m_sampleSpec.format = PA_SAMPLE_FLOAT32NE;  break;
        default:
            LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("unsupported sample format %1")
                                 .arg(m_outputFormat));
            return false;
    }

    if (!pa_sample_spec_valid(&m_sampleSpec))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "invalid sample spec");
        return false;
    }
    std::string spec(PA_SAMPLE_SPEC_SNPRINT_MAX,'\0');
    pa_sample_spec_snprint(spec.data(), spec.size(), &m_sampleSpec);
    LOG(VB_AUDIO, LOG_INFO, LOC + fn_log_tag + "using sample spec " + spec.data());

    if(!pa_channel_map_init_auto(&m_channelMap, m_channels, PA_CHANNEL_MAP_WAVEEX))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "failed to init channel map");
        return false;
    }

    m_mainloop = pa_threaded_mainloop_new();
    if (!m_mainloop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "failed to get new threaded mainloop");
        return false;
    }

    pa_threaded_mainloop_start(m_mainloop);
    pa_threaded_mainloop_lock(m_mainloop);

    if (!ContextConnect())
    {
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_stop(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
        if (m_ctproplist)
        {
            pa_proplist_free(m_ctproplist);
            m_ctproplist = nullptr;
        }
        if (m_stproplist)
        {
            pa_proplist_free(m_stproplist);
            m_stproplist = nullptr;
        }
        return false;
    }

    if (!ConnectPlaybackStream())
    {
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_stop(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
        if (m_ctproplist)
        {
            pa_proplist_free(m_ctproplist);
            m_ctproplist = nullptr;
        }
        if (m_stproplist)
        {
            pa_proplist_free(m_stproplist);
            m_stproplist = nullptr;
        }
        return false;
    }

    pa_threaded_mainloop_unlock(m_mainloop);
    return true;
}

void AudioOutputPulseAudio::CloseDevice()
{
    if (m_mainloop)
        pa_threaded_mainloop_lock(m_mainloop);

    if (m_pstream)
    {
        FlushStream("CloseDevice");
        pa_stream_disconnect(m_pstream);
        pa_stream_unref(m_pstream);
        m_pstream = nullptr;
    }

    if (m_pcontext)
    {
        if (m_mainloop)
        {
            pa_operation *op = pa_context_drain(m_pcontext, ContextDrainCallback, m_mainloop);
            if (op)
            {
                // Wait for the asynchronous draining to complete
                while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
                {
                    pa_threaded_mainloop_wait(m_mainloop);
                }
                pa_operation_unref(op);
            }
        }
        pa_context_disconnect(m_pcontext);
        pa_context_unref(m_pcontext);
        m_pcontext = nullptr;
    }

    if (m_mainloop)
    {
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_stop(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
    }

    if (m_ctproplist)
    {
        pa_proplist_free(m_ctproplist);
        m_ctproplist = nullptr;
    }
    if (m_stproplist)
    {
        pa_proplist_free(m_stproplist);
        m_stproplist = nullptr;
    }
}

void AudioOutputPulseAudio::WriteAudio(uchar *aubuf, int size)
{
    QString fn_log_tag = "WriteAudio, ";
    pa_stream_state_t sstate = pa_stream_get_state(m_pstream);

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + fn_log_tag + QString("writing %1 bytes").arg(size));

    /* NB This "if" check can be replaced with PA_STREAM_IS_GOOD() in
       PulseAudio API from 0.9.11. As 0.9.10 is still widely used
       we use the more verbose version for now */

    if (sstate == PA_STREAM_CREATING || sstate == PA_STREAM_READY)
    {
        int write_status = PA_ERR_INVALID;
        size_t to_write = size;
        unsigned char *buf_ptr = aubuf;

        pa_threaded_mainloop_lock(m_mainloop);
        while (to_write > 0)
        {
            write_status = 0;
            size_t writable = pa_stream_writable_size(m_pstream);
            if (writable > 0)
            {
                size_t write = std::min(to_write, writable);
                write_status = pa_stream_write(m_pstream, buf_ptr, write,
                                               nullptr, 0, PA_SEEK_RELATIVE);

                if (0 != write_status)
                    break;

                buf_ptr += write;
                to_write -= write;
            }
            else
            {
                pa_threaded_mainloop_wait(m_mainloop);
            }
        }
        pa_threaded_mainloop_unlock(m_mainloop);

        if (to_write > 0)
        {
            if (write_status != 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("stream write failed: %1")
                                     .arg(write_status == PA_ERR_BADSTATE
                                                ? "PA_ERR_BADSTATE"
                                                : "PA_ERR_INVALID"));
            }

            LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("short write, %1 of %2")
                                 .arg(size - to_write).arg(size));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("stream state not good: %1")
                             .arg(sstate,0,16));
    }
}

int AudioOutputPulseAudio::GetBufferedOnSoundcard(void) const
{
    pa_usec_t latency = 0;
    size_t buffered = 0;

    if (!m_pcontext || pa_context_get_state(m_pcontext) != PA_CONTEXT_READY)
        return 0;

    if (!m_pstream || pa_stream_get_state(m_pstream) != PA_STREAM_READY)
        return 0;

    const pa_buffer_attr *buf_attr =  pa_stream_get_buffer_attr(m_pstream);
    size_t bfree = pa_stream_writable_size(m_pstream);
    buffered = buf_attr->tlength - bfree;

    pa_threaded_mainloop_lock(m_mainloop);

    while (pa_stream_get_latency(m_pstream, &latency, nullptr) < 0)
    {
        if (pa_context_errno(m_pcontext) != PA_ERR_NODATA)
        {
            latency = 0;
            break;
        }
        pa_threaded_mainloop_wait(m_mainloop);
    }

    pa_threaded_mainloop_unlock(m_mainloop);

    return (latency * m_sampleRate *
            m_outputBytesPerFrame / 1000000) + buffered;
}

int AudioOutputPulseAudio::GetVolumeChannel(int channel) const
{
    return (float)m_volumeControl.values[channel] /
           (float)PA_VOLUME_NORM * 100.0F;
}

void AudioOutputPulseAudio::SetVolumeChannel(int channel, int volume)
{
    QString fn_log_tag = "SetVolumeChannel, ";

    if (channel < 0 || channel > PULSE_MAX_CHANNELS || volume < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("bad volume params, channel %1, volume %2")
                             .arg(channel).arg(volume));
        return;
    }

    m_volumeControl.values[channel] =
        (float)volume / 100.0F * (float)PA_VOLUME_NORM;

// FIXME: This code did nothing at all so has been commented out for now
//        until it's decided whether it was ever required
//     volume = std::clamp(volume, 0, 100);

    if (gCoreContext->GetSetting("MixerControl", "PCM").toLower() == "pcm")
    {
        uint32_t stream_index = pa_stream_get_index(m_pstream);
        pa_threaded_mainloop_lock(m_mainloop);
        pa_operation *op =
            pa_context_set_sink_input_volume(m_pcontext, stream_index,
                                             &m_volumeControl,
                                             OpCompletionCallback, this);
        pa_threaded_mainloop_unlock(m_mainloop);
        if (op)
            pa_operation_unref(op);
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag +
                    QString("set stream volume operation failed, stream %1, "
                            "error %2 ")
                    .arg(stream_index)
                    .arg(pa_strerror(pa_context_errno(m_pcontext))));
        }
    }
    else
    {
        uint32_t sink_index = pa_stream_get_device_index(m_pstream);
        pa_threaded_mainloop_lock(m_mainloop);
        pa_operation *op =
            pa_context_set_sink_volume_by_index(m_pcontext, sink_index,
                                                &m_volumeControl,
                                                OpCompletionCallback, this);
        pa_threaded_mainloop_unlock(m_mainloop);
        if (op)
            pa_operation_unref(op);
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag +
                    QString("set sink volume operation failed, sink %1, "
                            "error %2 ")
                    .arg(sink_index)
                    .arg(pa_strerror(pa_context_errno(m_pcontext))));
        }
    }
}

void AudioOutputPulseAudio::Drain(void)
{
    AudioOutputBase::Drain();
    pa_threaded_mainloop_lock(m_mainloop);
    pa_operation *op = pa_stream_drain(m_pstream, nullptr, this);
    pa_threaded_mainloop_unlock(m_mainloop);

    if (op)
        pa_operation_unref(op);
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "Drain, stream drain failed");
}

bool AudioOutputPulseAudio::ContextConnect(void)
{
    QString fn_log_tag = "ContextConnect, ";
    if (m_pcontext)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "context appears to exist, but shouldn't (yet)");
        pa_context_unref(m_pcontext);
        m_pcontext = nullptr;
        return false;
    }
    m_ctproplist = pa_proplist_new();
    if (!m_ctproplist)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("failed to create new proplist"));
        return false;
    }
    pa_proplist_sets(m_ctproplist, PA_PROP_APPLICATION_NAME, "MythTV");
    pa_proplist_sets(m_ctproplist, PA_PROP_APPLICATION_ICON_NAME, "mythtv");
    pa_proplist_sets(m_ctproplist, PA_PROP_MEDIA_ROLE, "video");
    m_pcontext =
        pa_context_new_with_proplist(pa_threaded_mainloop_get_api(m_mainloop),
                                     "MythTV", m_ctproplist);
    if (!m_pcontext)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "failed to acquire new context");
        pa_proplist_free(m_ctproplist);
        m_ctproplist = nullptr;
        return false;
    }
    pa_context_set_state_callback(m_pcontext, ContextStateCallback, this);

    QString pulse_host = ChooseHost();
    int chk = pa_context_connect(m_pcontext,
                                 !pulse_host.isEmpty() ? qPrintable(pulse_host) : nullptr,
                                 (pa_context_flags_t)0, nullptr);

    if (chk < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("context connect failed: %1")
                             .arg(pa_strerror(pa_context_errno(m_pcontext))));
        pa_proplist_free(m_ctproplist);
        m_ctproplist = nullptr;
        return false;
    }
    bool connected = false;
    pa_context_state_t state = pa_context_get_state(m_pcontext);
    for (; !connected; state = pa_context_get_state(m_pcontext))
    {
        switch(state)
        {
            case PA_CONTEXT_READY:
                LOG(VB_AUDIO, LOG_INFO, LOC + fn_log_tag +"context connection ready");
                connected = true;
                continue;

            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag +
                        QString("context connection failed or terminated: %1")
                        .arg(pa_strerror(pa_context_errno(m_pcontext))));
                pa_proplist_free(m_ctproplist);
                m_ctproplist = nullptr;
                return false;

            default:
                LOG(VB_AUDIO, LOG_INFO, LOC + fn_log_tag + "waiting for context connection ready");
                pa_threaded_mainloop_wait(m_mainloop);
                break;
        }
    }

    pa_operation *op =
        pa_context_get_server_info(m_pcontext, ServerInfoCallback, this);

    if (op)
        pa_operation_unref(op);
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "failed to get PulseAudio server info");

    return true;
}

QString AudioOutputPulseAudio::ChooseHost(void)
{
    QString fn_log_tag = "ChooseHost, ";
    QStringList parts = m_mainDevice.split(':');
    QString host = parts.size() > 1 ? parts[1] : QString();
    QString pulse_host;

    if (host != "default")
        pulse_host = host;

    if (pulse_host.isEmpty() && host != "default")
    {
        QString env_pulse_host = qEnvironmentVariable("PULSE_SERVER");
        if (!env_pulse_host.isEmpty())
            pulse_host = env_pulse_host;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + fn_log_tag + QString("chosen PulseAudio server: %1")
                         .arg((pulse_host != nullptr) ? pulse_host : "default"));

    return pulse_host;
}

bool AudioOutputPulseAudio::ConnectPlaybackStream(void)
{
    QString fn_log_tag = "ConnectPlaybackStream, ";
    m_stproplist = pa_proplist_new();
    if (!m_stproplist)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("failed to create new proplist"));
        return false;
    }
    pa_proplist_sets(m_stproplist, PA_PROP_MEDIA_ROLE, "video");
    m_pstream =
        pa_stream_new_with_proplist(m_pcontext, "MythTV playback", &m_sampleSpec,
                                    &m_channelMap, m_stproplist);
    if (!m_pstream)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "failed to create new playback stream");
        pa_proplist_free(m_stproplist);
        m_stproplist = nullptr;
        return false;
    }
    pa_stream_set_state_callback(m_pstream, StreamStateCallback, this);
    pa_stream_set_write_callback(m_pstream, WriteCallback, this);
    pa_stream_set_overflow_callback(m_pstream, BufferFlowCallback, (char*)"over");
    pa_stream_set_underflow_callback(m_pstream, BufferFlowCallback,
                                     (char*)"under");
    if (m_setInitialVol)
    {
        int volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);
        pa_cvolume_set(&m_volumeControl, m_channels,
                       (float)volume * (float)PA_VOLUME_NORM / 100.0F);
    }
    else
    {
        pa_cvolume_reset(&m_volumeControl, m_channels);
    }

    m_fragmentSize = (m_sampleRate * 25 * m_outputBytesPerFrame) / 1000;

    m_bufferSettings.maxlength   = UINT32_MAX;
    m_bufferSettings.tlength     = m_fragmentSize * 4;
    m_bufferSettings.prebuf      = UINT32_MAX;
    m_bufferSettings.minreq      = UINT32_MAX;
    m_bufferSettings.fragsize    = UINT32_MAX;

    int flags = PA_STREAM_INTERPOLATE_TIMING
        | PA_STREAM_ADJUST_LATENCY
        | PA_STREAM_AUTO_TIMING_UPDATE
        | PA_STREAM_NO_REMIX_CHANNELS;

    pa_stream_connect_playback(m_pstream, nullptr, &m_bufferSettings,
                               (pa_stream_flags_t)flags, nullptr, nullptr);

    pa_stream_state_t sstate = PA_STREAM_UNCONNECTED;
    bool connected = false;
    bool failed = false;

    while (!(connected || failed))
    {
        pa_context_state_t cstate = pa_context_get_state(m_pcontext);
        switch (cstate)
        {
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("context is stuffed, %1")
                            .arg(pa_strerror(pa_context_errno(m_pcontext))));
                pa_proplist_free(m_stproplist);
                m_stproplist = nullptr;
                failed = true;
                break;
            default:
                switch (sstate = pa_stream_get_state(m_pstream))
                {
                    case PA_STREAM_READY:
                        connected = true;
                        break;
                    case PA_STREAM_FAILED:
                    case PA_STREAM_TERMINATED:
                        LOG(VB_GENERAL, LOG_ERR, LOC + QString("stream failed or was terminated, "
                                        "context state %1, stream state %2")
                                    .arg(cstate).arg(sstate));
                        pa_proplist_free(m_stproplist);
                        m_stproplist = nullptr;
                        failed = true;
                        break;
                    default:
                        pa_threaded_mainloop_wait(m_mainloop);
                        break;
                }
        }
    }

    const pa_buffer_attr *buf_attr = pa_stream_get_buffer_attr(m_pstream);
    m_fragmentSize = buf_attr->tlength >> 2;
    m_soundcardBufferSize = buf_attr->maxlength;

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("fragment size %1, soundcard buffer size %2")
                .arg(m_fragmentSize).arg(m_soundcardBufferSize));

    return (connected && !failed);
}

void AudioOutputPulseAudio::FlushStream(const char *caller)
{
    QString fn_log_tag = QString("FlushStream (%1), ").arg(caller);
    pa_threaded_mainloop_lock(m_mainloop);
    pa_operation *op = pa_stream_flush(m_pstream, nullptr, this);
    pa_threaded_mainloop_unlock(m_mainloop);
    if (op)
        pa_operation_unref(op);
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + "stream flush operation failed ");
}

void AudioOutputPulseAudio::ContextDrainCallback(pa_context */*c*/, void *arg)
{
    auto *mloop = (pa_threaded_mainloop *)arg;
    pa_threaded_mainloop_signal(mloop, 0);
}

void AudioOutputPulseAudio::ContextStateCallback(pa_context *c, void *arg)
{
    auto *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    switch (pa_context_get_state(c))
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(audoutP->m_mainloop, 0);
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
    auto *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    switch (pa_stream_get_state(s))
    {
        case PA_STREAM_READY:
        case PA_STREAM_TERMINATED:
        case PA_STREAM_FAILED:
            pa_threaded_mainloop_signal(audoutP->m_mainloop, 0);
            break;
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

void AudioOutputPulseAudio::WriteCallback(pa_stream */*s*/, size_t /*size*/, void *arg)
{
    auto *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    pa_threaded_mainloop_signal(audoutP->m_mainloop, 0);
}

void AudioOutputPulseAudio::BufferFlowCallback(pa_stream */*s*/, void *tag)
{
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("stream buffer %1 flow").arg((char*)tag));
}

void AudioOutputPulseAudio::OpCompletionCallback(
    pa_context *c, int ok, void *arg)
{
    QString fn_log_tag = "OpCompletionCallback, ";
    auto *audoutP = static_cast<AudioOutputPulseAudio*>(arg);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + fn_log_tag + QString("bummer, an operation failed: %1")
                             .arg(pa_strerror(pa_context_errno(c))));
    }
    pa_threaded_mainloop_signal(audoutP->m_mainloop, 0);
}

void AudioOutputPulseAudio::ServerInfoCallback(
    pa_context */*context*/, const pa_server_info *inf, void */*arg*/)
{
    QString fn_log_tag = "ServerInfoCallback, ";

    LOG(VB_AUDIO, LOG_INFO, LOC + fn_log_tag +
            QString("PulseAudio server info - host name: %1, server version: "
                    "%2, server name: %3, default sink: %4")
            .arg(inf->host_name, inf->server_version,
                 inf->server_name, inf->default_sink_name));
}

void AudioOutputPulseAudio::SinkInfoCallback(
    pa_context */*c*/, const pa_sink_info *info, int /*eol*/, void *arg)
{
    auto *audoutP = static_cast<AudioOutputPulseAudio*>(arg);

    if (!info)
    {
        pa_threaded_mainloop_signal(audoutP->m_mainloop, 0);
        return;
    }

    audoutP->m_aoSettings->AddSupportedRate(info->sample_spec.rate);

    for (uint i = 2; i <= info->sample_spec.channels; i++)
        audoutP->m_aoSettings->AddSupportedChannels(i);

    pa_threaded_mainloop_signal(audoutP->m_mainloop, 0);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
