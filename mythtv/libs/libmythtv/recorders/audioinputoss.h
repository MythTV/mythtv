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

#ifndef _AUDIOINPUTOSS_H_
#define _AUDIOINPUTOSS_H_

#include "audioinput.h"

class AudioInputOSS : public AudioInput
{
    public:
        AudioInputOSS(const QString &device);
        ~AudioInputOSS() { Close(); };

        bool Open(uint sample_bits, uint sample_rate, uint channels);
        inline bool IsOpen(void) { return (dsp_fd > -1); }
        void Close(void);

        bool Start(void);
        bool Stop(void);

        int GetBlockSize(void);
        int GetSamples(void *buffer, uint num_samples);
        int GetNumReadyBytes(void);

    private:
        QByteArray m_device_name;
        int dsp_fd;
};
#endif /* _AUDIOINPUTOSS_H_ */

