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

#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

#include "audioinput.h"
#include "audioinputalsa.h"
#include "audioinputoss.h"

#define LOC     QString("AudioIn: ")

AudioInput *AudioInput::CreateDevice(const QByteArray &device)
{
    AudioInput *audio = nullptr;
    if (CONFIG_AUDIO_OSS && device.startsWith("/"))
    {
#if defined( CONFIG_AUDIO_OSS ) && (CONFIG_AUDIO_OSS==1)
        audio = new AudioInputOSS(device);
#endif
    }
    else if (CONFIG_AUDIO_ALSA && device.startsWith("ALSA:"))
    {
#if defined( CONFIG_AUDIO_ALSA ) && (CONFIG_AUDIO_ALSA==1)
        audio = new AudioInputALSA(device);
#endif
    }
    else if (device == "NULL")
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "creating NULL audio device");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "unknown or unsupported audio input device '" + device + "'");
    }

    return audio;
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
