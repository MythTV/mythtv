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

#ifndef _AUDIOINPUTALSA_H_
#define _AUDIOINPUTALSA_H_

#include "audioinput.h"

#ifdef USING_ALSA
#include <alsa/asoundlib.h>
#else
typedef int snd_pcm_t;
typedef int snd_pcm_uframes_t;
#endif // USING_ALSA

class AudioInputALSA : public AudioInput
{
  public:
    AudioInputALSA(const QString &device);
    ~AudioInputALSA() { Close(); };

    bool Open(uint sample_bits, uint sample_rate, uint channels);
    inline bool IsOpen(void) { return (pcm_handle != NULL); }
    void Close(void);

    bool Start(void) { return (pcm_handle != NULL); }
    bool Stop(void);

    inline int GetBlockSize(void) { return myth_block_bytes; };
    int GetSamples(void* buf, uint nbytes);
    int GetNumReadyBytes(void);

  private:
    bool PrepHwParams(void);
    bool PrepSwParams(void);
    int  PcmRead(void* buf, uint nbytes);
    bool Recovery(int err);
    bool AlsaBad(int op_result, QString errmsg);

    QByteArray          alsa_device;
    snd_pcm_t*          pcm_handle;
    snd_pcm_uframes_t   period_size;
    int                 myth_block_bytes;
};
#endif /* _AUDIOINPUTALSA_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4: */

