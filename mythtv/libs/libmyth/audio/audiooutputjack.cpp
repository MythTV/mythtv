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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <cerrno>
#include <cstring>
#include <cmath>

#include <iostream>

using namespace std;

#include "mythcorecontext.h"
#include "audiooutputjack.h"
#include "mythdate.h"

#define LOC QString("AOJack: ")

#define JERROR(str) Error(LOC + str)

AudioOutputJACK::AudioOutputJACK(const AudioSettings &settings) :
    AudioOutputBase(settings),
    client(NULL), stale_client(NULL),
    jack_latency(0), jack_underrun(false), jack_xruns(0), aubuf(NULL)
{
    for (int i = 0; i < JACK_CHANNELS_MAX; i++)
        ports[i] = NULL;
    for (int i = 0; i < JACK_CHANNELS_MAX; i++)
        chan_volumes[i] = 100;

    // Set everything up
    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

AudioOutputSettings* AudioOutputJACK::GetOutputSettings(bool /*digital*/)
{
    int rate = 0;
    int i = 0;
    const char **matching_ports = NULL;
    AudioOutputSettings *settings = new AudioOutputSettings();

    client = _jack_client_open();
    if (!client)
    {
        JERROR(tr("Cannot start/connect to jack server "
               "(to check supported rate/channels)"));
        delete settings;
        return NULL;
    }

    if (client)
        rate = jack_get_sample_rate(client);

    if (!rate)
    {
        JERROR(tr("Unable to retrieve jack server sample rate"));
        goto err_out;
    }
    else
        settings->AddSupportedRate(rate);

    // Jack only wants float format samples (de-interleaved for preference)
    settings->AddSupportedFormat(FORMAT_FLT);

    // Find some Jack ports to connect to
    matching_ports = _jack_get_ports();

    if (!matching_ports || !matching_ports[0])
    {
        JERROR(tr("No ports available to connect to"));
        goto err_out;
    }
    // Count matching ports from 2nd port upwards
    i = 1;
    while (matching_ports[i] && (i < JACK_CHANNELS_MAX))
    {
        settings->AddSupportedChannels(i+1);
        VBAUDIO(QString("Adding channels: %1").arg(i+1));
        i++;
    }

    // Currently this looks very similar to error code - duplicated for safety
    free(matching_ports);
    _jack_client_close(&client);
    return settings;

err_out:
    // Our abstracted exit point in case of error
    free(matching_ports);
    _jack_client_close(&client);
    delete settings;
    return NULL;
}


AudioOutputJACK::~AudioOutputJACK()
{
    // Close down all audio stuff
    KillAudio();
}

bool AudioOutputJACK::OpenDevice()
{
    const char **matching_ports = NULL;
    int i = 0;

    // We have a hard coded channel limit - check we haven't exceeded it
    if (channels > JACK_CHANNELS_MAX)
    {
        JERROR(tr("Requested more channels: (%1), than the maximum: %2")
                   .arg(channels).arg(JACK_CHANNELS_MAX));
        return false;
    }

    VBAUDIO( QString("Opening JACK audio device: '%1'.")
            .arg(main_device));

    // Setup volume control
    if (internal_vol)
        VolumeInit();

    // Connect to the Jack audio server
    client = _jack_client_open();
    if (!client)
    {
        JERROR(tr("Cannot start/connect to jack server"));
        goto err_out;
    }

    // Find some Jack ports to connect to
    matching_ports = _jack_get_ports();
    if (!matching_ports || !matching_ports[0])
    {
        JERROR(tr("No ports available to connect to"));
        goto err_out;
    }

    // Count matching ports
    i = 1;
    while (matching_ports[i])
        i++;
    // ensure enough ports to satisfy request
    if (channels > i)
    {
        JERROR(tr("Not enough ports available to connect to"));
        goto err_out;
    }

    // Create our output ports
    for (i = 0; i < channels; i++)
    {
        QString port_name = QString("out_%1").arg(i);
        ports[i] = jack_port_register(client, port_name.toLatin1().constData(),
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput, 0);
        if (!ports[i])
        {
            JERROR(tr("Error while registering new jack port: %1").arg(i));
            goto err_out;
        }
    }

    // Note some basic soundserver parameters
    samplerate = jack_get_sample_rate(client);

    // Get the size of our callback buffer in bytes
    fragment_size = jack_get_buffer_size(client) * output_bytes_per_frame;

    // Allocate a new temp buffer to de-interleave our audio data
    if (aubuf)
        delete[] aubuf;
    aubuf = new unsigned char[fragment_size];


    // Set our callbacks
    // These will actually get called after jack_activate()!
    // ...Possibly even before this OpenDevice sub returns...
    if (jack_set_process_callback(client, _JackCallback, this))
        JERROR(tr("Error. Unable to set process callback?!"));
    if (jack_set_xrun_callback(client, _JackXRunCallback, this))
        JERROR(tr("Error. Unable to set xrun callback?!"));
    if (jack_set_graph_order_callback(client, _JackGraphOrderCallback, this))
        JERROR(tr("Error. Unable to set graph order change callback?!"));

    // Activate! Everything comes into life after here. Beware races
    if (jack_activate(client))
    {
        JERROR(tr("Calling jack_activate failed"));
        goto err_out;
    }

    // Connect our output ports
    if (! _jack_connect_ports(matching_ports))
        goto err_out;

    // Free up some stuff
    free(matching_ports);

    // Device opened successfully
    return true;

err_out:
    // Our abstracted exit point in case of error
    free(matching_ports);
    _jack_client_close(&client);
    return false;
}

void AudioOutputJACK::CloseDevice()
{
    _jack_client_close(&client);
    _jack_client_close(&stale_client);
    if (aubuf)
    {
        delete[] aubuf;
        aubuf = NULL;
    }

    VBAUDIO("Jack: Stop Event");
    OutputEvent e(OutputEvent::Stopped);
    dispatch(e);
}


int AudioOutputJACK::GetBufferedOnSoundcard(void) const
{
    int frames_played = jack_frames_since_cycle_start (this->client);
    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO,
        QString("Stats: frames_since_cycle_start:%1 fragment_size:%2")
            .arg(frames_played).arg(fragment_size));
    return  (fragment_size * 2) - (frames_played * output_bytes_per_frame);
}

/* Converts buffer to jack buffers
  Input: aubuf: interleaved buffer of currently assumed to be 32bit floats
        nframes: number of frames of output required
  Output: bufs: non interleaved float values.
*/
void AudioOutputJACK::DeinterleaveAudio(float *aubuf, float **bufs, int nframes,
                                        int* channel_volumes)
{
    // Convert to floats and de-interleave
    // TODO: Implicit assumption dealing with float input only.
    short sample = 0;

    // Create a local float version of the channel_volumes array
    // TODO: This can probably be removed
    //       if we have float software volume control in AOB?
    float volumes[channels];
    for (int channel = 0; channel < channels; channel++)
    {
        if (internal_vol)
        {
            // Software volume control - we use an exponential adjustment
            // (perhaps should be log?)
            volumes[channel] = (float) (( channel_volumes[channel] *
                                          channel_volumes[channel] ) /
                                                                10000.0);
        }
        else
            volumes[channel] = 1.0 / 1.0; // ie no effect
    }

    if (channels == 2)
    {
        for (int frame = 0; frame < nframes; frame++)
        {
            bufs[0][frame] = aubuf[sample++] * volumes[0];
            bufs[1][frame] = aubuf[sample++] * volumes[1];
        }
    }
    else if (channels == 6)
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
    else if (channels == 8)
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
            for (int channel = 0; channel < channels; channel++)
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
int AudioOutputJACK::_JackCallback(jack_nframes_t nframes, void *arg)
{
    AudioOutputJACK *aoj = static_cast<AudioOutputJACK*>(arg);
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
    float *bufs[JACK_CHANNELS_MAX];
    int bytes_needed = nframes * output_bytes_per_frame;
    int bytes_read = 0;
    int i;

    // Check for stale_client set during shutdown callback
    _jack_client_close(&stale_client);

    // Deal with xruns which may have occured
    // Basically read and discard the data which should have been played
    int t_jack_xruns = jack_xruns;
    for (i = 0; i < t_jack_xruns; i++)
    {
        bytes_read = GetAudioData(aubuf, fragment_size, true);
        VBERROR("Discarded one audio fragment to compensate for xrun");
    }
    jack_xruns -= t_jack_xruns;

    // Get jack output buffers
    for (i = 0; i < channels; i++)
        bufs[i] = (float*)jack_port_get_buffer(ports[i], nframes);

    if (pauseaudio || killaudio)
    {
        if (!actually_paused)
        {
            VBAUDIO("JackCallback: audio paused");
            OutputEvent e(OutputEvent::Paused);
            dispatch(e);
            was_paused = true;
        }

        actually_paused = true;
    }
    else
    {
        if (was_paused)
        {
            VBAUDIO("JackCallback: Play Event");
            OutputEvent e(OutputEvent::Playing);
            dispatch(e);
            was_paused = false;
        }
        bytes_read = GetAudioData(aubuf, bytes_needed, false);
    }

    // Pad with silence
    if (bytes_needed > bytes_read)
    {
        // play silence on buffer underrun
        memset(aubuf + bytes_read, 0, bytes_needed - bytes_read);
        if (!pauseaudio)
            VBERROR(QString("Having to insert silence because GetAudioData "
                            "hasn't returned enough data. Wanted: %1 Got: %2")
                                    .arg(bytes_needed).arg(bytes_read));
    }
    // Now deinterleave audio (and convert to float)
    DeinterleaveAudio((float*)aubuf, bufs, nframes, chan_volumes);

    if (!pauseaudio)
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
int AudioOutputJACK::_JackXRunCallback(void *arg)
{
    AudioOutputJACK *aoj = static_cast<AudioOutputJACK*>(arg);
    return aoj->JackXRunCallback();
}

/* Useful bit of the XRun callback
  Return: 0 on success, non zero on error
*/
int AudioOutputJACK::JackXRunCallback(void)
{
    float delay = jack_get_xrun_delayed_usecs(client); // usecs
    // Increment our counter of "callbacks missed".
    // All we want to do is chuck away some audio from the ring buffer
    // to keep our audio output roughly where it should be if we didn't xrun
    int fragments = (int)ceilf( ((delay / 1000000.0) * samplerate )
                            / (float)(fragment_size / output_bytes_per_frame) );
    jack_xruns += fragments; //should be at least 1...
    VBERROR(QString("Jack XRun Callback: %1 usecs delayed, xruns now %2")
                        .arg(delay).arg(jack_xruns) );

    return 0;
}

/* Our Jack Graph Order Change callback.
  Jack will call this from a separate thread whenever an xrun occurs
  Simply calls our real code
*/
int AudioOutputJACK::_JackGraphOrderCallback(void *arg)
{
    AudioOutputJACK *aoj = static_cast<AudioOutputJACK*>(arg);
    return aoj->JackGraphOrderCallback();
}

/* Useful bit of the Graph Order Change callback
  Called when the Jack graph changes.  We update our latency info
  Return: 0 on success, non zero on error
*/
int AudioOutputJACK::JackGraphOrderCallback(void)
{
    int i;
    jack_nframes_t port_latency, max_latency = 0;

    for (i = 0; i < channels; ++i)
    {
        port_latency = jack_port_get_total_latency( client, ports[i] );
        if (port_latency > max_latency)
            max_latency = port_latency;
    }

    jack_latency = max_latency;
    VBAUDIO(QString("JACK graph reordered. Maximum latency=%1")
                    .arg(jack_latency));

    return 0;
}


/* Volume related handling */
void AudioOutputJACK::VolumeInit(void)
{
    int volume = 100;
    if (set_initial_vol)
    {
        QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");
        controlLabel += "MixerVolume";
        volume = gCoreContext->GetNumSetting(controlLabel, 80);
    }

    for (int i=0; i<JACK_CHANNELS_MAX; i++)
        chan_volumes[i] = volume;
}

int AudioOutputJACK::GetVolumeChannel(int channel) const
{
    unsigned int vol = 0;

    if (!internal_vol)
        return 100;

    if (channel < JACK_CHANNELS_MAX)
        vol = chan_volumes[channel];

    return vol;
}

void AudioOutputJACK::SetVolumeChannel(int channel, int volume)
{
    if (internal_vol && (channel < JACK_CHANNELS_MAX))
    {
        chan_volumes[channel] = volume;
        if (channel == 0)
        {
            // Left
            chan_volumes[2] = volume; // left rear
        }
        else if (channel == 1)
        {
            // Right
            chan_volumes[3] = volume; // right rear
        }

        // LFE and Center
        chan_volumes[4] = chan_volumes[5] =
                                    (chan_volumes[0] + chan_volumes[1]) / 2;
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
void AudioOutputJACK::WriteAudio(unsigned char *aubuf, int size)
{
    (void)aubuf;
    (void)size;
    return;     // unneeded and unused in JACK
}

/* **********************************************
  Jack wrapper and convenience functions
 ***********************************************/

jack_client_t* AudioOutputJACK::_jack_client_open(void)
{
    jack_client_t* client = NULL;
    QString client_name = QString("mythtv_%1").arg(getpid());
    jack_options_t open_options =
        (jack_options_t)(JackUseExactName | JackNoStartServer);
    jack_status_t open_status;

    client = jack_client_open(client_name.toLatin1().constData(),
                              open_options, &open_status);

    return client;
}

const char** AudioOutputJACK::_jack_get_ports(void)
{
    const char **matching_ports = NULL;
    unsigned long port_flags=JackPortIsInput;
    const char *port_name = NULL;

    // Have we been given a target port to connect to
    if (!main_device.isEmpty())
    {
        port_name = main_device.toLatin1().constData();
    }
    else
    {
        port_flags |= JackPortIsPhysical;
    }

    // list matching ports
    matching_ports = jack_get_ports(client, port_name, NULL, port_flags);
    return matching_ports;
}


bool AudioOutputJACK::_jack_connect_ports(const char** matching_ports)
{
    int i=0;

    // Connect our output ports
    for (i = 0; i < channels; i++)
    {
        if (jack_connect(client, jack_port_name(ports[i]), matching_ports[i]))
        {
            JERROR(tr("Calling jack_connect failed on port: %1\n").arg(i));
            return false;
        }
    }

    return true;
}

void AudioOutputJACK::_jack_client_close(jack_client_t **client)
{
    if (*client)
    {
        int err = jack_client_close(*client);
        if (err != 0)
            JERROR(tr("Error closing Jack output device. Error: %1")
                       .arg(err));
        *client = NULL;
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
