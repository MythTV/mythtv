/*
 * Copyright (C) 2008  Alan Calvert
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

#ifndef AUDIOOUTPUTPULSE
#define AUDIOOUTPUTPULSE

#include <pulse/pulseaudio.h>

#include "audiooutputbase.h"

class AudioOutputPulseAudio : public AudioOutputBase
{
  public:
    AudioOutputPulseAudio(const AudioSettings &settings);
   ~AudioOutputPulseAudio();

    int GetVolumeChannel(int channel) const;
    void SetVolumeChannel(int channel, int volume);
    void Pause(bool paused);
    void Reset(void);
    void Drain(void);

  protected:
    bool OpenDevice(void);
    void CloseDevice(void);
    void WriteAudio(unsigned char *aubuf, int size);
    int GetSpaceOnSoundcard(void) const;
    int GetBufferedOnSoundcard(void) const;

  private:
    char *ChooseHost(void);
    bool MapChannels(void);
    bool ContextConnect(void);
    bool ConnectPlaybackStream(void);
    void FlushStream(const char *caller);

    static void ContextStateCallback(pa_context *c, void *arg);
    static void StreamStateCallback(pa_stream *s, void *arg);
    static void OpCompletionCallback(pa_context *c, int ok, void *arg);
    static void BufferFlowCallback(pa_stream *s, void *tag);
    static void ServerInfoCallback(pa_context *context,
                                   const pa_server_info *inf, void *arg);

    pa_context             *pcontext;
    pa_stream              *pstream;
    pa_threaded_mainloop   *mainloop;
    size_t                  sample_rate;
    pa_sample_spec          sample_spec;
    pa_channel_map          channel_map;
    pa_cvolume              volume_control;
    pa_buffer_attr          buffer_settings;
};
#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

