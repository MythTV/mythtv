/*
 * Copyright (C) 2007  Anand K. Mistry
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
/* vim: set expandtab tabstop=4 shiftwidth=4: */

#ifndef _AUDIOINPUT_H_
#define _AUDIOINPUT_H_

#include <QString>
#include <unistd.h>

class AudioInput
{
  public:
    virtual ~AudioInput() {}

    virtual bool Open(uint sample_bits, uint sample_rate, uint channels) = 0;
    virtual bool IsOpen(void) = 0;
    virtual void Close(void) = 0;

    virtual bool Start(void) = 0;
    virtual bool Stop(void) = 0;

    virtual int GetBlockSize(void) = 0;
    virtual int GetSamples(void *buf, uint nbytes) = 0;
    virtual int GetNumReadyBytes(void) = 0;

    // Factory function
    static AudioInput *CreateDevice(const QByteArray &device);

  protected:
    AudioInput(const QString &device);

    QByteArray m_audio_device;
    int        m_audio_channels;
    int        m_audio_sample_bits;
    int        m_audio_sample_rate;
};
#endif /* _AUDIOINPUT_H_ */
