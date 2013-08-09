/*
 * Copyright (C) 2007  Anand K. Mistry
 * Copyright (C) 2007  Daniel Kristjansson
 * Copyright (C) 2009  Alan Calvert
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

#include "mythlogging.h"
#include "audioinputalsa.h"

#define LOC     QString("AudioInALSA: ")
#define LOC_DEV QString("AudioInALSA(%1): ").arg(alsa_device.constData())

AudioInputALSA::AudioInputALSA(const QString &device):
    AudioInput(device),
    pcm_handle(NULL),
    period_size(0),
    myth_block_bytes(0)
{
    alsa_device = device.right(device.size()-5).toLatin1();
}

bool AudioInputALSA::Open(uint sample_bits, uint sample_rate, uint channels)
{
    if (alsa_device.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("invalid alsa device name, %1")
                          .arg(alsa_device.constData()));
        return false;
    }
    (void)AlsaBad(snd_config_update_free_global(), "failed to update snd config");
    m_audio_sample_rate = sample_rate;
    m_audio_channels = channels;
    m_audio_sample_bits = sample_bits;
    if (AlsaBad(snd_pcm_open(&pcm_handle, alsa_device.constData(),
                             SND_PCM_STREAM_CAPTURE, 0), // blocking mode
                "pcm open failed"))
    {
        pcm_handle = NULL;
        return false;
    }
    if (!(PrepHwParams() && PrepSwParams()))
    {
        (void)snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
        return false;
    }
    LOG(VB_AUDIO, LOG_INFO, LOC_DEV + "pcm open");
    return true;
}

void AudioInputALSA::Close(void)
{
    if (pcm_handle != NULL)
    {
        Stop();
        (void)AlsaBad(snd_pcm_close(pcm_handle), "Close close failed");
    }
    pcm_handle = NULL;
}

bool AudioInputALSA::Stop(void)
{
    bool stopped = false;
    if (pcm_handle != NULL &&
        !AlsaBad(snd_pcm_drop(pcm_handle), "Stop drop failed"))
    {
        stopped = true;
        LOG(VB_AUDIO, LOG_INFO, LOC_DEV + "capture stopped");
    }
    return stopped;
}

int AudioInputALSA::GetSamples(void* buf, uint nbytes)
{
    if (!pcm_handle)
       return 0;
    int bytes_read = 0;
    int pcm_state = snd_pcm_state(pcm_handle);
    switch (pcm_state)
    {
        case SND_PCM_STATE_XRUN:
        case SND_PCM_STATE_SUSPENDED:
        {
            bool recov = Stop() && Start();
            LOG(VB_AUDIO, LOG_INFO, LOC_DEV + "xrun recovery " +
                                              (recov ? "good" : "not good"));
            if (!recov)
                break;
        }
        case SND_PCM_STATE_PREPARED:
            if (AlsaBad(snd_pcm_start(pcm_handle), "pcm start failed"))
                 break;
            // else fall through
        case SND_PCM_STATE_RUNNING:
            bytes_read = PcmRead(buf, nbytes);
            break;
        default:
            LOG(VB_AUDIO, LOG_ERR, LOC_DEV +
                QString("weird pcm state through GetSamples, %1")
                    .arg(pcm_state));
            break;
    }
    return bytes_read;
}

int AudioInputALSA::GetNumReadyBytes(void)
{
    int bytes_avail = 0;
    if (pcm_handle != NULL)
    {
        snd_pcm_sframes_t frames_avail;
        int pcm_state = snd_pcm_state(pcm_handle);
        switch (pcm_state)
        {
            case SND_PCM_STATE_PREPARED:
            case SND_PCM_STATE_RUNNING:
                if (!AlsaBad((frames_avail = snd_pcm_avail_update(pcm_handle)),
                             "GetNumReadyBytes, available update failed"))
                    bytes_avail = snd_pcm_frames_to_bytes(pcm_handle,
                                                          frames_avail);
        }
    }
    return bytes_avail;
}

bool AudioInputALSA::PrepHwParams(void)
{
    snd_pcm_hw_params_t* hwparams;
    snd_pcm_hw_params_alloca(&hwparams);
    if (AlsaBad(snd_pcm_hw_params_any(pcm_handle, hwparams),
                "failed to init hw params"))
        return false;
    snd_pcm_access_t axs = SND_PCM_ACCESS_RW_INTERLEAVED; //always?
    if (AlsaBad(snd_pcm_hw_params_set_access(pcm_handle, hwparams, axs),
                "failed to set interleaved rw io"))
        return false;
    snd_pcm_format_t format =
        (m_audio_sample_bits > 8) ? SND_PCM_FORMAT_S16 : SND_PCM_FORMAT_U8;
    if (AlsaBad(snd_pcm_hw_params_set_format(pcm_handle, hwparams, format),
                QString("failed to set sample format %1")
                        .arg(snd_pcm_format_description(format))))
        return false;
    if (VERBOSE_LEVEL_CHECK(VB_AUDIO, LOG_DEBUG))
    {
        uint min_chans, max_chans;
        if(AlsaBad(snd_pcm_hw_params_get_channels_min(hwparams, &min_chans),
                    QString("unable to get min channel count")))
            min_chans = 0;
        if(AlsaBad(snd_pcm_hw_params_get_channels_max(hwparams, &max_chans),
                    QString("unable to get max channel count")))
            max_chans = 0;
        LOG(VB_AUDIO, LOG_DEBUG, LOC_DEV +
            QString("min channels %1, max channels %2, myth requests %3")
                          .arg(min_chans).arg(max_chans).arg(m_audio_channels));
    }
    if (AlsaBad(snd_pcm_hw_params_set_channels(pcm_handle, hwparams,
                m_audio_channels), QString("failed to set channels to %1")
                                           .arg(m_audio_channels)))
    {
        return false;
    }
    if (AlsaBad(snd_pcm_hw_params_set_rate(pcm_handle, hwparams,
                m_audio_sample_rate, 0), QString("failed to set sample rate %1")
                                                 .arg(m_audio_sample_rate)))
    {
        uint rate_num = 0;
        uint rate_den = 0;
        if (!AlsaBad(snd_pcm_hw_params_get_rate_numden(hwparams, &rate_num,
                     &rate_den), "snd_pcm_hw_params_get_rate_numden failed"))
            if (m_audio_sample_rate != (int)(rate_num / rate_den))
                LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                    QString("device reports sample rate as %1")
                        .arg(rate_num / rate_den));
        return false;
    }
    uint buffer_time = 64000; // 64 msec
    uint period_time = buffer_time / 4;
    if (AlsaBad(snd_pcm_hw_params_set_period_time_near(pcm_handle, hwparams, &period_time, NULL),
                "failed to set period time"))
        return false;
    if (AlsaBad(snd_pcm_hw_params_set_buffer_time_near(pcm_handle, hwparams, &buffer_time, NULL),
                "failed to set buffer time"))
        return false;
    if (AlsaBad(snd_pcm_hw_params_get_period_size(hwparams, &period_size, NULL),
                "failed to get period size"))
        return false;

    if (AlsaBad(snd_pcm_hw_params (pcm_handle, hwparams),
                "failed to set hwparams"))
        return false;

    myth_block_bytes = snd_pcm_frames_to_bytes(pcm_handle, period_size);
    LOG(VB_AUDIO, LOG_INFO, LOC_DEV +
            QString("channels %1, sample rate %2, buffer_time %3 msec, period "
                    "size %4").arg(m_audio_channels)
                .arg(m_audio_sample_rate).arg(buffer_time / 1000.0, -1, 'f', 1)
                .arg(period_size));
    LOG(VB_AUDIO, LOG_DEBUG, LOC_DEV + QString("myth block size %1")
                                           .arg(myth_block_bytes));
    return true;
}

bool AudioInputALSA::PrepSwParams(void)
{
    snd_pcm_sw_params_t* swparams;
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_uframes_t boundary;
    if (AlsaBad(snd_pcm_sw_params_current(pcm_handle, swparams),
               "failed to get swparams"))
        return false;
   if (AlsaBad(snd_pcm_sw_params_get_boundary(swparams, &boundary),
               "failed to get boundary"))
        return false;
    // explicit start, not auto start
    if (AlsaBad(snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams,
                boundary), "failed to set start threshold"))
        return false;
    if (AlsaBad(snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams,
                boundary), "failed to set stop threshold"))
        return false;
    if (AlsaBad(snd_pcm_sw_params(pcm_handle, swparams),
                "failed to set software parameters"))
        return false;

    return true;
}

int AudioInputALSA::PcmRead(void* buf, uint nbytes)
{
    unsigned char* bufptr = (unsigned char*)buf;
    snd_pcm_uframes_t to_read = snd_pcm_bytes_to_frames(pcm_handle, nbytes);
    snd_pcm_uframes_t nframes = to_read;
    snd_pcm_sframes_t nread, avail;
    int retries = 0;
    while (nframes > 0 && retries < 3)
    {
        if (AlsaBad((avail = snd_pcm_avail_update(pcm_handle)),
                    "available update failed"))
        {
            if (!Recovery(avail))
            {
                ++retries;
                continue;
            }
        }
        if ((nread = snd_pcm_readi(pcm_handle, bufptr, nframes)) < 0)
        {
            switch (nread)
            {
                case -EAGAIN:
                    break;
                case -EBADFD:
                    LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                        QString("in a state unfit to read (%1): %2")
                            .arg(nread).arg(snd_strerror(nread)));
                    break;
                case -EINTR:
                case -EPIPE:
                case -ESTRPIPE:
                    Recovery(nread);
                    break;
                default:
                    LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                        QString("weird return from snd_pcm_readi: %1")
                            .arg(snd_strerror(nread)));
                    break;
            }
        }
        else
        {
            nframes -= nread;
            bufptr += snd_pcm_frames_to_bytes(pcm_handle, nread);
        }
        ++retries;
    }
    if (nframes > 0)
        LOG(VB_AUDIO, LOG_ERR, LOC_DEV +
            QString("short pcm read, %1 of %2 frames, retries %1")
                .arg(to_read - nframes).arg(to_read).arg(retries));
    return snd_pcm_frames_to_bytes(pcm_handle, to_read - nframes);
}

bool AudioInputALSA::Recovery(int err)
{
    if (err > 0)
        err = -err;
    bool isgood = false;
    bool suspense = false;
    switch (err)
    {
        case -EINTR:
            isgood = true; // nothin' to see here
            break;
        case -ESTRPIPE:
            suspense = true;
        case -EPIPE:
        {
            int ret = snd_pcm_prepare(pcm_handle);
            if (ret < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
                    QString("failed to recover from %1. %2")
                        .arg(suspense ? "suspend" : "underrun")
                        .arg(snd_strerror(ret)));
                return false;
            }
            isgood = true;
            break;
        }
        default:
            break;
    }
    return isgood;
}

bool AudioInputALSA::AlsaBad(int op_result, QString errmsg)
{   // (op_result < 0) => return true
    bool bad = (op_result < 0);
    if (bad)
        LOG(VB_GENERAL, LOG_ERR, LOC_DEV +
            errmsg + ": " + snd_strerror(op_result));
    return bad;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
