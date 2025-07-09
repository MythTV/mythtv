/*
 *  JACK AudioOutput module
 *  Written by Ed Wildgoose in 2010 with improvements from various authors
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"

#include "audiooutputjack.h"

#define LOC QString("AOJack: ")

AudioOutputJACK::AudioOutputJACK(const AudioSettings &settings) :
    AudioOutputBase(settings)
{
    m_ports.fill( nullptr);
    m_chanVolumes.fill(100);

    // Set everything up
    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
}

AudioOutputSettings* AudioOutputJACK::GetOutputSettings(bool /*digital*/)
{
    // Set up to automatically close client at function exit
    auto cleanup_fn = [&](AudioOutputJACK* /*p*/) {
        if (m_client) JackClientClose(&m_client);
    };
    std::unique_ptr<AudioOutputJACK, decltype(cleanup_fn)> cleanup { this, cleanup_fn };

    int rate = 0;
    auto settings = std::make_unique<AudioOutputSettings>();

    m_client = JackClientOpen();
    if (!m_client)
    {
        Error(LOC + tr("Cannot start/connect to jack server "
               "(to check supported rate/channels)"));
        return nullptr;
    }

    if (m_client)
        rate = jack_get_sample_rate(m_client);

    if (!rate)
    {
        Error(LOC + tr("Unable to retrieve jack server sample rate"));
        return nullptr;
    }
    settings->AddSupportedRate(rate);

    // Jack only wants float format samples (de-interleaved for preference)
    settings->AddSupportedFormat(FORMAT_FLT);

    // Find some Jack ports to connect to. Release at function exit.
    std::unique_ptr<const char *, decltype(&jack_free)>
        matching_ports { JackGetPorts(), &jack_free };
    if (!matching_ports || !matching_ports.get()[0])
    {
        Error(LOC + tr("No ports available to connect to"));
        return nullptr;
    }
    // Count matching ports from 2nd port upwards
    for (int i=1; (i < JACK_CHANNELS_MAX) && matching_ports.get()[i]; i++)
    {
        settings->AddSupportedChannels(i+1);
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Adding channels: %1").arg(i+1));
    }

    // Return settings instead of deleting it
    return settings.release();
}


AudioOutputJACK::~AudioOutputJACK()
{
    // Close down all audio stuff
    KillAudio();
}

bool AudioOutputJACK::OpenDevice()
{
    // Set up to automatically close client at function exit
    auto cleanup_fn = [](AudioOutputJACK *p)
        { if (p->m_client) p->JackClientClose(&p->m_client); };
    std::unique_ptr<AudioOutputJACK, decltype(cleanup_fn)>
        cleanup { this, cleanup_fn };

    // We have a hard coded channel limit - check we haven't exceeded it
    if (m_channels > JACK_CHANNELS_MAX)
    {
        Error(LOC + tr("Requested more channels: (%1), than the maximum: %2")
                   .arg(m_channels).arg(JACK_CHANNELS_MAX));
        return false;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC +  QString("Opening JACK audio device: '%1'.")
            .arg(m_mainDevice));

    // Setup volume control
    if (m_internalVol)
        VolumeInit();

    // Connect to the Jack audio server
    m_client = JackClientOpen();
    if (!m_client)
    {
        Error(LOC + tr("Cannot start/connect to jack server"));
        return false;
    }

    // Find some Jack ports to connect to. Release at function exit.
    std::unique_ptr<const char *, decltype(&jack_free)>
        matching_ports { JackGetPorts(), &jack_free };
    if (!matching_ports || !matching_ports.get()[0])
    {
        Error(LOC + tr("No ports available to connect to"));
        return false;
    }

    // Count matching ports
    int i = 1;
    while (matching_ports.get()[i])
        i++;
    // ensure enough ports to satisfy request
    if (m_channels > i)
    {
        Error(LOC + tr("Not enough ports available to connect to"));
        return false;
    }

    // Create our output ports
    for (i = 0; i < m_channels; i++)
    {
        QString port_name = QString("out_%1").arg(i);
        m_ports[i] = jack_port_register(m_client, port_name.toLatin1().constData(),
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput, 0);
        if (!m_ports[i])
        {
            Error(LOC + tr("Error while registering new jack port: %1").arg(i));
            return false;
        }
    }

    // Note some basic soundserver parameters
    m_sampleRate = jack_get_sample_rate(m_client);

    // Get the size of our callback buffer in bytes
    m_fragmentSize = jack_get_buffer_size(m_client) * m_outputBytesPerFrame;

    // Allocate a new temp buffer to de-interleave our audio data
    delete[] m_auBuf;
    m_auBuf = new unsigned char[m_fragmentSize];

    // Set our callbacks
    // These will actually get called after jack_activate()!
    // ...Possibly even before this OpenDevice sub returns...
    if (jack_set_process_callback(m_client, JackCallbackHelper, this))
        Error(LOC + tr("Error. Unable to set process callback?!"));
    if (jack_set_xrun_callback(m_client, JackXRunCallbackHelper, this))
        Error(LOC + tr("Error. Unable to set xrun callback?!"));
    if (jack_set_graph_order_callback(m_client, JackGraphOrderCallbackHelper, this))
        Error(LOC + tr("Error. Unable to set graph order change callback?!"));

    // Activate! Everything comes into life after here. Beware races
    if (jack_activate(m_client))
    {
        Error(LOC + tr("Calling jack_activate failed"));
        return false;
    }

    // Connect our output ports
    if (! JackConnectPorts(matching_ports.get()))
        return false;

    // Device opened successfully. Skip client cleanup function.
    cleanup.release();
    return true;
}

void AudioOutputJACK::CloseDevice()
{
    JackClientClose(&m_client);
    JackClientClose(&m_staleClient);
    if (m_auBuf)
    {
        delete[] m_auBuf;
        m_auBuf = nullptr;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + "Jack: Stop Event");
    OutputEvent e(OutputEvent::kStopped);
    dispatch(e);
}


int AudioOutputJACK::GetBufferedOnSoundcard(void) const
{
    int frames_played = jack_frames_since_cycle_start (this->m_client);
    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO,
        QString("Stats: frames_since_cycle_start:%1 fragment_size:%2")
            .arg(frames_played).arg(m_fragmentSize));
    return  (m_fragmentSize * 2) - (frames_played * m_outputBytesPerFrame);
}

/* Converts buffer to jack buffers
  Input: aubuf: interleaved buffer of currently assumed to be 32bit floats
        nframes: number of frames of output required
  Output: bufs: non interleaved float values.
*/
void AudioOutputJACK::DeinterleaveAudio(const float *aubuf, float **bufs, int nframes,
                                        const jack_vol_array& channel_volumes)
{
    // Convert to floats and de-interleave
    // TODO: Implicit assumption dealing with float input only.
    short sample = 0;

    // Create a local float version of the channel_volumes array
    // TODO: This can probably be removed
    //       if we have float software volume control in AOB?
    std::array<float,JACK_CHANNELS_MAX> volumes {};
    for (int channel = 0; channel < m_channels; channel++)
    {
        if (m_internalVol)
        {
            // Software volume control - we use an exponential adjustment
            // (perhaps should be log?)
            volumes[channel] = (float) (( channel_volumes[channel] *
                                          channel_volumes[channel] ) /
                                                                10000.0);
        }
        else
        {
            volumes[channel] = 1.0 / 1.0; // ie no effect
        }
    }

    if (m_channels == 2)
    {
        for (int frame = 0; frame < nframes; frame++)
        {
            bufs[0][frame] = aubuf[sample++] * volumes[0];
            bufs[1][frame] = aubuf[sample++] * volumes[1];
        }
    }
    else if (m_channels == 6)
    {
        for (int frame = 0; frame < nframes; frame++)
        {
            // Audio supplied in SMPTE l,r,ce,lfe,lr,rr
            // We probably want it in ALSA format l,r,lr,rr,ce,lfe
            bufs[0][frame] = aubuf[sample++] * volumes[0];
            bufs[1][frame] = aubuf[sample++] * volumes[1];
            bufs[4][frame] = aubuf[sample++] * volumes[4];
            bufs[5][frame] = aubuf[sample++] * volumes[5];
            bufs[2][frame] = aubuf[sample++] * volumes[2];
            bufs[3][frame] = aubuf[sample++] * volumes[3];
        }
    }
    else if (m_channels == 8)
    {
        for (int frame = 0; frame < nframes; frame++)
        {
            // Audio supplied in SMPTE l,r,ce,lfe,lr,rr,ml,mr ?
            // We probably want it in ALSA format l,r,lr,rr,ce,lfe,ml,mr ?
            // TODO - unknown if this channel ordering is correct?
            bufs[0][frame] = aubuf[sample++] * volumes[0];
            bufs[1][frame] = aubuf[sample++] * volumes[1];
            bufs[4][frame] = aubuf[sample++] * volumes[4];
            bufs[5][frame] = aubuf[sample++] * volumes[5];
            bufs[2][frame] = aubuf[sample++] * volumes[2];
            bufs[3][frame] = aubuf[sample++] * volumes[3];
            bufs[6][frame] = aubuf[sample++] * volumes[6];
            bufs[7][frame] = aubuf[sample++] * volumes[7];
        }
    }
    else
    {
        for (int frame = 0; frame < nframes; frame++)
        {
            // Straightforward de-interleave for all other cases.
            // Note no channel re-ordering...
            for (int channel = 0; channel < m_channels; channel++)
            {
                bufs[channel][frame] = aubuf[sample++] * volumes[channel];
            }
        }
    }

}

/* ***************************************************************************
  Jack Callbacks
  ****************************************************************************/

/* Our Jack callback.
  Jack will call this from a separate thread whenever it needs "feeding"
  Simply calls our real code
*/
int AudioOutputJACK::JackCallbackHelper(jack_nframes_t nframes, void *arg)
{
    auto *aoj = static_cast<AudioOutputJACK*>(arg);
    return aoj->JackCallback(nframes);
}

/* Useful bit of the callback
    This callback is called when the sound system requests
    data.  We don't want to block here, because that would
    just cause dropouts anyway, so we always return whatever
    data is available.  If we haven't received enough, either
    because we've finished playing or we have a buffer
    underrun, we play silence to fill the unused space.
  Return: 0 on success, non zero on error
*/
int AudioOutputJACK::JackCallback(jack_nframes_t nframes)
{
    std::array<float*,JACK_CHANNELS_MAX> bufs {};
    int bytes_needed = nframes * m_outputBytesPerFrame;
    int bytes_read = 0;

    // Check for stale_client set during shutdown callback
    JackClientClose(&m_staleClient);

    // Deal with xruns which may have occured
    // Basically read and discard the data which should have been played
    int t_jack_xruns = m_jackXruns;
    for (int i = 0; i < t_jack_xruns; i++)
    {
        bytes_read = GetAudioData(m_auBuf, m_fragmentSize, true);
        LOG(VB_GENERAL, LOG_ERR, LOC + "Discarded one audio fragment to compensate for xrun");
    }
    m_jackXruns -= t_jack_xruns;

    // Get jack output buffers.  Zero out the extra space in the array
    // to prevent clang-tidy from complaining that DeinterleaveAudio()
    // can reference a garbage value.
    int i = 0;
    for ( ; i < m_channels; i++)
        bufs[i] = (float*)jack_port_get_buffer(m_ports[i], nframes);
    for ( ; i < JACK_CHANNELS_MAX; i++)
        bufs[i] = nullptr;

    if (m_pauseAudio || m_killAudio)
    {
        if (!m_actuallyPaused)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + "JackCallback: audio paused");
            OutputEvent e(OutputEvent::kPaused);
            dispatch(e);
            m_wasPaused = true;
        }

        m_actuallyPaused = true;
    }
    else
    {
        if (m_wasPaused)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + "JackCallback: Play Event");
            OutputEvent e(OutputEvent::kPlaying);
            dispatch(e);
            m_wasPaused = false;
        }
        bytes_read = GetAudioData(m_auBuf, bytes_needed, false);
    }

    // Pad with silence
    if (bytes_needed > bytes_read)
    {
        // play silence on buffer underrun
        memset(m_auBuf + bytes_read, 0, bytes_needed - bytes_read);
        if (!m_pauseAudio)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Having to insert silence because GetAudioData "
                            "hasn't returned enough data. Wanted: %1 Got: %2")
                                    .arg(bytes_needed).arg(bytes_read));
        }
    }
    // Now deinterleave audio (and convert to float)
    DeinterleaveAudio((float*)m_auBuf, bufs.data(), nframes, m_chanVolumes);

    if (!m_pauseAudio)
    {
        // Output a status event - needed for Music player
        Status();
    }

    return 0;
}


/* Our Jack XRun callback.
  Jack will call this from a separate thread whenever an xrun occurs
  Simply calls our real code
*/
int AudioOutputJACK::JackXRunCallbackHelper(void *arg)
{
    auto *aoj = static_cast<AudioOutputJACK*>(arg);
    return aoj->JackXRunCallback();
}

/* Useful bit of the XRun callback
  Return: 0 on success, non zero on error
*/
int AudioOutputJACK::JackXRunCallback(void)
{
    float delay = jack_get_xrun_delayed_usecs(m_client); // usecs
    // Increment our counter of "callbacks missed".
    // All we want to do is chuck away some audio from the ring buffer
    // to keep our audio output roughly where it should be if we didn't xrun
    int fragments = (int)ceilf( ((delay / 1000000.0F) * m_sampleRate )
                            / ((float)m_fragmentSize / m_outputBytesPerFrame) );
    m_jackXruns += fragments; //should be at least 1...
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Jack XRun Callback: %1 usecs delayed, xruns now %2")
                        .arg(delay).arg(m_jackXruns) );

    return 0;
}

/* Our Jack Graph Order Change callback.
  Jack will call this from a separate thread whenever an xrun occurs
  Simply calls our real code
*/
int AudioOutputJACK::JackGraphOrderCallbackHelper(void *arg)
{
    auto *aoj = static_cast<AudioOutputJACK*>(arg);
    return aoj->JackGraphOrderCallback();
}

/* Useful bit of the Graph Order Change callback
  Called when the Jack graph changes.  We update our latency info
  Return: 0 on success, non zero on error
*/
int AudioOutputJACK::JackGraphOrderCallback(void)
{
    jack_latency_range_t latency_range;
    jack_nframes_t max_latency = 0;

    for (int i = 0; i < m_channels; ++i)
    {
        jack_port_get_latency_range( m_ports[i], JackPlaybackLatency, &latency_range );
        jack_nframes_t port_latency = latency_range.max;
        max_latency = std::max(port_latency, max_latency);
    }

    m_jackLatency = max_latency;
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("JACK graph reordered. Maximum latency=%1")
                    .arg(m_jackLatency));

    return 0;
}


/* Volume related handling */
void AudioOutputJACK::VolumeInit(void)
{
    int volume = 100;
    if (m_setInitialVol)
    {
        QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");
        controlLabel += "MixerVolume";
        volume = gCoreContext->GetNumSetting(controlLabel, 80);
    }

    m_chanVolumes.fill(volume);
}

int AudioOutputJACK::GetVolumeChannel(int channel) const
{
    unsigned int vol = 0;

    if (!m_internalVol)
        return 100;

    if (channel < JACK_CHANNELS_MAX)
        vol = m_chanVolumes[channel];

    return vol;
}

void AudioOutputJACK::SetVolumeChannel(int channel, int volume)
{
    if (m_internalVol && (channel < JACK_CHANNELS_MAX))
    {
        m_chanVolumes[channel] = volume;
        if (channel == 0)
        {
            // Left
            m_chanVolumes[2] = volume; // left rear
        }
        else if (channel == 1)
        {
            // Right
            m_chanVolumes[3] = volume; // right rear
        }

        // LFE and Center
        m_chanVolumes[4] = m_chanVolumes[5] =
                                    (m_chanVolumes[0] + m_chanVolumes[1]) / 2;
    }
}


/* We don't need an audio output thread for Jack
  Everything handled by callbacks here
  Therefore we can loose all the Start/StopOutputThread, WriteAudio, etc
*/
bool AudioOutputJACK::StartOutputThread(void)
{
    return true;
}

void AudioOutputJACK::StopOutputThread(void)
{
}

// Don't need WriteAudio - this is only for the base class audio loop
void AudioOutputJACK::WriteAudio([[maybe_unused]] unsigned char *aubuf,
                                 [[maybe_unused]] int size)
{
    // unneeded and unused in JACK
}

/* **********************************************
  Jack wrapper and convenience functions
 ***********************************************/

jack_client_t* AudioOutputJACK::JackClientOpen(void)
{
    QString client_name = QString("mythtv_%1").arg(getpid());
    auto open_options = (jack_options_t)(JackUseExactName | JackNoStartServer);
    jack_status_t open_status = JackFailure;

    jack_client_t *client = jack_client_open(client_name.toLatin1().constData(),
                              open_options, &open_status);

    return client;
}

const char** AudioOutputJACK::JackGetPorts(void)
{
    const char **matching_ports = nullptr;
    unsigned long port_flags=JackPortIsInput;
    const char *port_name = nullptr;

    // Have we been given a target port to connect to
    if (!m_mainDevice.isEmpty())
    {
        port_name = m_mainDevice.toLatin1().constData();
    }
    else
    {
        port_flags |= JackPortIsPhysical;
    }

    // list matching ports
    matching_ports = jack_get_ports(m_client, port_name, nullptr, port_flags);
    return matching_ports;
}


bool AudioOutputJACK::JackConnectPorts(const char** matching_ports)
{
    int i=0;

    // Connect our output ports
    for (i = 0; i < m_channels; i++)
    {
        if (jack_connect(m_client, jack_port_name(m_ports[i]), matching_ports[i]))
        {
            Error(LOC + tr("Calling jack_connect failed on port: %1\n").arg(i));
            return false;
        }
    }

    return true;
}

void AudioOutputJACK::JackClientClose(jack_client_t **client)
{
    if (*client)
    {
        int err = jack_client_close(*client);
        if (err != 0)
            Error(LOC + tr("Error closing Jack output device. Error: %1")
                       .arg(err));
        *client = nullptr;
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
