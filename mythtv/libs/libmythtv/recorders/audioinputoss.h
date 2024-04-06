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
/* vim: set expandtab tabstop=4 shiftwidth=4: */

#ifndef AUDIOINPUTOSS_H
#define AUDIOINPUTOSS_H

#include "audioinput.h"

class AudioInputOSS : public AudioInput
{
    public:
        explicit AudioInputOSS(const QString &device);
        ~AudioInputOSS() override { AudioInputOSS::Close(); }

        bool Open(uint sample_bits, uint sample_rate, uint channels) override; // AudioInput
        bool IsOpen(void) override // AudioInput
            { return (m_dspFd > -1); }
        void Close(void) override; // AudioInput

        bool Start(void) override; // AudioInput
        bool Stop(void) override; // AudioInput

        int GetBlockSize(void) override; // AudioInput
        int GetSamples(void *buffer, uint num_bytes) override; // AudioInput
        int GetNumReadyBytes(void) override; // AudioInput

    private:
        QByteArray m_deviceName;
        int m_dspFd {-1};
};
#endif /* AUDIOINPUTOSS_H */
