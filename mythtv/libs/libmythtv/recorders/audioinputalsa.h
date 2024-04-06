/*
 * Copyright (C) 2007  Anand K. Mistry
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

#ifndef AUDIOINPUTALSA_H
#define AUDIOINPUTALSA_H

#include "audioinput.h"

#ifdef USING_ALSA
#include <alsa/asoundlib.h>
#else
using snd_pcm_t = int;
using snd_pcm_uframes_t = int;
#endif // USING_ALSA

class AudioInputALSA : public AudioInput
{
  public:
    explicit AudioInputALSA(const QString &device)
        : AudioInput(device)
        , m_alsaDevice(device.right(device.size()-5).toLatin1()) {}
    ~AudioInputALSA() override { AudioInputALSA::Close(); }

    bool Open(uint sample_bits, uint sample_rate, uint channels) override; // AudioInput
    bool IsOpen(void) override // AudioInput
        { return (m_pcmHandle != nullptr); }
    void Close(void) override; // AudioInput

    bool Start(void) override // AudioInput
        { return (m_pcmHandle != nullptr); }
    bool Stop(void) override; // AudioInput

    int GetBlockSize(void) override // AudioInput
        { return m_mythBlockBytes; }
    int GetSamples(void* buf, uint nbytes) override; // AudioInput
    int GetNumReadyBytes(void) override; // AudioInput

  private:
    bool PrepHwParams(void);
    bool PrepSwParams(void);
    int  PcmRead(void* buf, uint nbytes);
    bool Recovery(int err);
    bool AlsaBad(int op_result, const QString& errmsg);

    QByteArray          m_alsaDevice;
    snd_pcm_t*          m_pcmHandle      {nullptr};
    snd_pcm_uframes_t   m_periodSize     {0};
    int                 m_mythBlockBytes {0};
};
#endif /* AUDIOINPUTALSA_H */
/* vim: set expandtab tabstop=4 shiftwidth=4: */

