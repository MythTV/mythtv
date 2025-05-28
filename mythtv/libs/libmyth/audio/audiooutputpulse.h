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
    explicit AudioOutputPulseAudio(const AudioSettings &settings);
   ~AudioOutputPulseAudio() override;

    int GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase
    void Drain(void) override; // AudioOutputBase

  protected:
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int GetBufferedOnSoundcard(void) const override; // AudioOutputBase

  private:
    QString ChooseHost(void);
    bool MapChannels(void);
    bool ContextConnect(void);
    bool ConnectPlaybackStream(void);
    void FlushStream(const char *caller);

    static void ContextDrainCallback(pa_context *c, void *arg);
    static void ContextStateCallback(pa_context *c, void *arg);
    static void StreamStateCallback(pa_stream *s, void *arg);
    static void OpCompletionCallback(pa_context *c, int ok, void *arg);
    static void WriteCallback(pa_stream *s, size_t size, void *arg);
    static void BufferFlowCallback(pa_stream *s, void *tag);
    static void ServerInfoCallback(pa_context *context,
                                   const pa_server_info *inf, void *arg);
    static void SinkInfoCallback(pa_context *c, const pa_sink_info *info,
                                 int eol, void *arg);

    pa_proplist            *m_ctproplist {nullptr};
    pa_proplist            *m_stproplist {nullptr};
    pa_context             *m_pcontext   {nullptr};
    pa_stream              *m_pstream    {nullptr};
    pa_threaded_mainloop   *m_mainloop   {nullptr};
    pa_sample_spec          m_sampleSpec      {};
    pa_channel_map          m_channelMap      {};
    pa_cvolume              m_volumeControl   {};
    pa_buffer_attr          m_bufferSettings  {};
    AudioOutputSettings    *m_aoSettings {nullptr};
};
#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
