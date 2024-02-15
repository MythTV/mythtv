/*
 * Copyright (C) 2007  Anand K. Mistry
 * Copyright (C) 2007  Daniel Kristjansson
 * Copyright (C) 2003-2007 Others who contributed to NuppelVideoRecorder.cpp
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
#include "audioinputoss.h"

#include <fcntl.h>
#include <sys/ioctl.h>

#if __has_include(<sys/soundcard.h>)
#   include <sys/soundcard.h>
#elif __has_include(<soundcard.h>)
#   include <soundcard.h>
#else
#   error attemping to compile OSS support without soundcard.h
#endif

#include <QtGlobal>

#include "libmythbase/mythlogging.h"

#define LOC     QString("AudioInOSS: ")
#define LOC_DEV QString("AudioInOSS(%1): ").arg(m_deviceName.constData())

AudioInputOSS::AudioInputOSS(const QString &device) : AudioInput(device)
{
    if (!device.isEmpty())
        m_deviceName = device.toLatin1();
    else
        m_deviceName = QByteArray();
}

bool AudioInputOSS::Open(uint sample_bits, uint sample_rate, uint channels)
{
    m_audioSampleBits = sample_bits;
    m_audioSampleRate = sample_rate;
    //m_audio_channels = channels;

    if (IsOpen())
        Close();

    // Open the device
    m_dspFd = open(m_deviceName.constData(), O_RDONLY);
    if (m_dspFd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV + QString("open failed: ") + ENO);
        Close();
        return false;
    }

    int chk = 0; // disable input for now
    if (ioctl(m_dspFd, SNDCTL_DSP_SETTRIGGER, &chk) < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            LOC_DEV + "failed to disable audio device: " + ENO);
    }

    // Set format
    int choice = 0;
    QString tag;
    switch (sample_bits)
    {
        case 8:
            choice = AFMT_U8;
            tag = "AFMT_U8";
            break;
        case 16:
        default:
#if (Q_BYTE_ORDER == Q_BIG_ENDIAN)
            choice = AFMT_S16_BE;
            tag = "AFMT_S16_BE";
#else
            choice = AFMT_S16_LE;
            tag = "AFMT_S16_LE";
#endif
            break;
    }
    int format = choice;
    if (ioctl(m_dspFd, SNDCTL_DSP_SETFMT, &format) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("failed to set audio format %1: ").arg(tag) + ENO);
        Close();
        return false;
    }
    if (format != choice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("set audio format not %1 as requested").arg(tag));
        Close();
        return false;
    }

    // sample size
    m_audioSampleBits = choice = sample_bits;
    if (ioctl(m_dspFd, SNDCTL_DSP_SAMPLESIZE, &m_audioSampleBits) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("failed to set audio sample bits to %1: ")
                .arg(sample_bits) + ENO);
        Close();
        return false;
    }
    if (m_audioSampleBits != choice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("requested %1 sample bits, got %2")
                            .arg(choice).arg(m_audioSampleBits));
    }
    // channels
    m_audioChannels = choice = channels;
    if (ioctl(m_dspFd, SNDCTL_DSP_CHANNELS, &m_audioChannels) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("failed to set audio channels to %1: ").arg(channels)+ENO);
        Close();
        return false;
    }
    if (m_audioChannels != choice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("requested %1 channels, got %2")
                .arg(choice).arg(m_audioChannels));
    }

    // sample rate
    int choice_sample_rate = sample_rate;
    m_audioSampleRate = choice_sample_rate;
    if (ioctl(m_dspFd, SNDCTL_DSP_SPEED, &m_audioSampleRate) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("failed to set sample rate to %1: ").arg(sample_rate)+ENO);
        Close();
        return false;
    }
    if (m_audioSampleRate != choice_sample_rate)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("requested sample rate %1, got %2")
                .arg(choice_sample_rate).arg(m_audioSampleRate));
    }
    LOG(VB_AUDIO, LOG_INFO, LOC_DEV + "device open");
    return true;
}

void AudioInputOSS::Close(void)
{
    if (IsOpen())
        close(m_dspFd);
    m_dspFd = -1;
    m_audioSampleBits = 0;
    m_audioSampleRate = 0;
    m_audioChannels = 0;
    LOG(VB_AUDIO, LOG_INFO, LOC_DEV + "device closed");
}

bool AudioInputOSS::Start(void)
{
    bool started = false;
    if (IsOpen())
    {
        int trig = 0; // clear
        if (ioctl(m_dspFd, SNDCTL_DSP_SETTRIGGER, &trig) < 0)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                LOC_DEV + "failed to disable audio device: " + ENO);
        }
        trig = PCM_ENABLE_INPUT; // enable input
        if (ioctl(m_dspFd, SNDCTL_DSP_SETTRIGGER, &trig) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                QString("Start() failed: ") + ENO);
        }
        else
        {
            LOG(VB_AUDIO, LOG_INFO, LOC_DEV + "capture started");
            started = true;
        }
    }
    return started;
}

bool AudioInputOSS::Stop(void)
{
    bool stopped = false;
    int trig = 0;
    if (ioctl(m_dspFd, SNDCTL_DSP_SETTRIGGER, &trig) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            QString("stop action failed: ") + ENO);
    }
    else
    {
        stopped = true;
        LOG(VB_AUDIO, LOG_INFO, LOC_DEV + "capture stopped");
    }
    return stopped;
}

int AudioInputOSS::GetBlockSize(void)
{
    int frag = 0;
    if (IsOpen())
    {
        if (ioctl(m_dspFd, SNDCTL_DSP_GETBLKSIZE, &frag) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                QString("fragment size query failed, returned %1: ").arg(frag) +
                ENO);
            frag = 0;
        }
    }
    LOG(VB_AUDIO, LOG_INFO, LOC_DEV + QString("block size %1").arg(frag));
    return frag;
}

int AudioInputOSS::GetSamples(void *buffer, uint num_bytes)
{
    uint bytes_read = 0;
    if (IsOpen())
    {
        int retries = 0;
        while (bytes_read < num_bytes && retries < 3)
        {
            int this_read = read(m_dspFd, buffer, num_bytes - bytes_read);
            if (this_read < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                    QString("GetSamples read failed: ") + ENO);
            }
            else
            {
                bytes_read += this_read;
            }
            ++retries;
        }
        if (num_bytes > bytes_read)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                QString("GetSamples short read, %1 of %2 bytes")
                    .arg(bytes_read).arg(num_bytes));
        }
    }
    return bytes_read;
}

int AudioInputOSS::GetNumReadyBytes(void)
{
    int readies = 0;
    if (IsOpen())
    {
        audio_buf_info ispace;
        if (ioctl(m_dspFd, SNDCTL_DSP_GETISPACE, &ispace) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                QString("get ready bytes failed, returned %1: ")
                    .arg(ispace.bytes) + ENO);
        }
        else
        {
            readies = ispace.bytes;
            if (readies > 0)
                LOG(VB_AUDIO, LOG_DEBUG, LOC_DEV + QString("ready bytes %1")
                    .arg(readies));
        }
    }
    return readies;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
