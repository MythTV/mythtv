/*
 * Copyright (c) 2012 Justin Ruggles <justin.ruggles@gmail.com>
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/dict.h"
// #include "libavutil/error.h"
#include "libavutil/log.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"

#include "avresample.h"
#include "audio_data.h"
#include "internal.h"

int avresample_open(AVAudioResampleContext *avr)
{
    int ret;

    /* set channel mixing parameters */
    avr->in_channels = av_get_channel_layout_nb_channels(avr->in_channel_layout);
    if (avr->in_channels <= 0 || avr->in_channels > AVRESAMPLE_MAX_CHANNELS) {
        av_log(avr, AV_LOG_ERROR, "Invalid input channel layout: %"PRIu64"\n",
               avr->in_channel_layout);
        return AVERROR(EINVAL);
    }
    avr->out_channels = av_get_channel_layout_nb_channels(avr->out_channel_layout);
    if (avr->out_channels <= 0 || avr->out_channels > AVRESAMPLE_MAX_CHANNELS) {
        av_log(avr, AV_LOG_ERROR, "Invalid output channel layout: %"PRIu64"\n",
               avr->out_channel_layout);
        return AVERROR(EINVAL);
    }
    avr->resample_channels = FFMIN(avr->in_channels, avr->out_channels);
    avr->downmix_needed    = avr->in_channels  > avr->out_channels;
    avr->upmix_needed      = avr->out_channels > avr->in_channels ||
                             avr->am->matrix                      ||
                             (avr->out_channels == avr->in_channels &&
                              avr->in_channel_layout != avr->out_channel_layout);
    avr->mixing_needed     = avr->downmix_needed || avr->upmix_needed;

    /* set resampling parameters */
    avr->resample_needed   = avr->in_sample_rate != avr->out_sample_rate ||
                             avr->force_resampling;

    /* set sample format conversion parameters */
    /* override user-requested internal format to avoid unexpected failures
       TODO: support more internal formats */
    if (avr->resample_needed && avr->internal_sample_fmt != AV_SAMPLE_FMT_S16P) {
        av_log(avr, AV_LOG_WARNING, "Using s16p as internal sample format\n");
        avr->internal_sample_fmt = AV_SAMPLE_FMT_S16P;
    } else if (avr->mixing_needed &&
               avr->internal_sample_fmt != AV_SAMPLE_FMT_S16P &&
               avr->internal_sample_fmt != AV_SAMPLE_FMT_FLTP) {
        av_log(avr, AV_LOG_WARNING, "Using fltp as internal sample format\n");
        avr->internal_sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    if (avr->in_channels == 1)
        avr->in_sample_fmt = av_get_planar_sample_fmt(avr->in_sample_fmt);
    if (avr->out_channels == 1)
        avr->out_sample_fmt = av_get_planar_sample_fmt(avr->out_sample_fmt);
    avr->in_convert_needed = (avr->resample_needed || avr->mixing_needed) &&
                              avr->in_sample_fmt != avr->internal_sample_fmt;
    if (avr->resample_needed || avr->mixing_needed)
        avr->out_convert_needed = avr->internal_sample_fmt != avr->out_sample_fmt;
    else
        avr->out_convert_needed = avr->in_sample_fmt != avr->out_sample_fmt;

    /* allocate buffers */
    if (avr->mixing_needed || avr->in_convert_needed) {
        avr->in_buffer = ff_audio_data_alloc(FFMAX(avr->in_channels, avr->out_channels),
                                             0, avr->internal_sample_fmt,
                                             "in_buffer");
        if (!avr->in_buffer) {
            ret = AVERROR(EINVAL);
            goto error;
        }
    }
    if (avr->resample_needed) {
        avr->resample_out_buffer = ff_audio_data_alloc(avr->out_channels,
                                                       0, avr->internal_sample_fmt,
                                                       "resample_out_buffer");
        if (!avr->resample_out_buffer) {
            ret = AVERROR(EINVAL);
            goto error;
        }
    }
    if (avr->out_convert_needed) {
        avr->out_buffer = ff_audio_data_alloc(avr->out_channels, 0,
                                              avr->out_sample_fmt, "out_buffer");
        if (!avr->out_buffer) {
            ret = AVERROR(EINVAL);
            goto error;
        }
    }
    avr->out_fifo = av_audio_fifo_alloc(avr->out_sample_fmt, avr->out_channels,
                                        1024);
    if (!avr->out_fifo) {
        ret = AVERROR(ENOMEM);
        goto error;
    }

    /* setup contexts */
    if (avr->in_convert_needed) {
        avr->ac_in = ff_audio_convert_alloc(avr, avr->internal_sample_fmt,
                                            avr->in_sample_fmt, avr->in_channels);
        if (!avr->ac_in) {
            ret = AVERROR(ENOMEM);
            goto error;
        }
    }
    if (avr->out_convert_needed) {
        enum AVSampleFormat src_fmt;
        if (avr->in_convert_needed)
            src_fmt = avr->internal_sample_fmt;
        else
            src_fmt = avr->in_sample_fmt;
        avr->ac_out = ff_audio_convert_alloc(avr, avr->out_sample_fmt, src_fmt,
                                             avr->out_channels);
        if (!avr->ac_out) {
            ret = AVERROR(ENOMEM);
            goto error;
        }
    }
    if (avr->resample_needed) {
        avr->resample = ff_audio_resample_init(avr);
        if (!avr->resample) {
            ret = AVERROR(ENOMEM);
            goto error;
        }
    }
    if (avr->mixing_needed) {
        ret = ff_audio_mix_init(avr);
        if (ret < 0)
            goto error;
    }

    return 0;

error:
    avresample_close(avr);
    return ret;
}

void avresample_close(AVAudioResampleContext *avr)
{
    ff_audio_data_free(&avr->in_buffer);
    ff_audio_data_free(&avr->resample_out_buffer);
    ff_audio_data_free(&avr->out_buffer);
    av_audio_fifo_free(avr->out_fifo);
    avr->out_fifo = NULL;
    av_freep(&avr->ac_in);
    av_freep(&avr->ac_out);
    ff_audio_resample_free(&avr->resample);
    ff_audio_mix_close(avr->am);
    return;
}

void avresample_free(AVAudioResampleContext **avr)
{
    if (!*avr)
        return;
    avresample_close(*avr);
    av_freep(&(*avr)->am);
    av_opt_free(*avr);
    av_freep(avr);
}

static int handle_buffered_output(AVAudioResampleContext *avr,
                                  AudioData *output, AudioData *converted)
{
    int ret;

    if (!output || av_audio_fifo_size(avr->out_fifo) > 0 ||
        (converted && output->allocated_samples < converted->nb_samples)) {
        if (converted) {
            /* if there are any samples in the output FIFO or if the
               user-supplied output buffer is not large enough for all samples,
               we add to the output FIFO */
            av_dlog(avr, "[FIFO] add %s to out_fifo\n", converted->name);
            ret = ff_audio_data_add_to_fifo(avr->out_fifo, converted, 0,
                                            converted->nb_samples);
            if (ret < 0)
                return ret;
        }

        /* if the user specified an output buffer, read samples from the output
           FIFO to the user output */
        if (output && output->allocated_samples > 0) {
            av_dlog(avr, "[FIFO] read from out_fifo to output\n");
            av_dlog(avr, "[end conversion]\n");
            return ff_audio_data_read_from_fifo(avr->out_fifo, output,
                                                output->allocated_samples);
        }
    } else if (converted) {
        /* copy directly to output if it is large enough or there is not any
           data in the output FIFO */
        av_dlog(avr, "[copy] %s to output\n", converted->name);
        output->nb_samples = 0;
        ret = ff_audio_data_copy(output, converted);
        if (ret < 0)
            return ret;
        av_dlog(avr, "[end conversion]\n");
        return output->nb_samples;
    }
    av_dlog(avr, "[end conversion]\n");
    return 0;
}

int avresample_convert(AVAudioResampleContext *avr, void **output,
                       int out_plane_size, int out_samples, void **input,
                       int in_plane_size, int in_samples)
{
    AudioData input_buffer;
    AudioData output_buffer;
    AudioData *current_buffer;
    int ret;

    /* reset internal buffers */
    if (avr->in_buffer) {
        avr->in_buffer->nb_samples = 0;
        ff_audio_data_set_channels(avr->in_buffer,
                                   avr->in_buffer->allocated_channels);
    }
    if (avr->resample_out_buffer) {
        avr->resample_out_buffer->nb_samples = 0;
        ff_audio_data_set_channels(avr->resample_out_buffer,
                                   avr->resample_out_buffer->allocated_channels);
    }
    if (avr->out_buffer) {
        avr->out_buffer->nb_samples = 0;
        ff_audio_data_set_channels(avr->out_buffer,
                                   avr->out_buffer->allocated_channels);
    }

    av_dlog(avr, "[start conversion]\n");

    /* initialize output_buffer with output data */
    if (output) {
        ret = ff_audio_data_init(&output_buffer, output, out_plane_size,
                                 avr->out_channels, out_samples,
                                 avr->out_sample_fmt, 0, "output");
        if (ret < 0)
            return ret;
        output_buffer.nb_samples = 0;
    }

    if (input) {
        /* initialize input_buffer with input data */
        ret = ff_audio_data_init(&input_buffer, input, in_plane_size,
                                 avr->in_channels, in_samples,
                                 avr->in_sample_fmt, 1, "input");
        if (ret < 0)
            return ret;
        current_buffer = &input_buffer;

        if (avr->upmix_needed && !avr->in_convert_needed && !avr->resample_needed &&
            !avr->out_convert_needed && output && out_samples >= in_samples) {
            /* in some rare cases we can copy input to output and upmix
               directly in the output buffer */
            av_dlog(avr, "[copy] %s to output\n", current_buffer->name);
            ret = ff_audio_data_copy(&output_buffer, current_buffer);
            if (ret < 0)
                return ret;
            current_buffer = &output_buffer;
        } else if (avr->mixing_needed || avr->in_convert_needed) {
            /* if needed, copy or convert input to in_buffer, and downmix if
               applicable */
            if (avr->in_convert_needed) {
                ret = ff_audio_data_realloc(avr->in_buffer,
                                            current_buffer->nb_samples);
                if (ret < 0)
                    return ret;
                av_dlog(avr, "[convert] %s to in_buffer\n", current_buffer->name);
                ret = ff_audio_convert(avr->ac_in, avr->in_buffer, current_buffer,
                                       current_buffer->nb_samples);
                if (ret < 0)
                    return ret;
            } else {
                av_dlog(avr, "[copy] %s to in_buffer\n", current_buffer->name);
                ret = ff_audio_data_copy(avr->in_buffer, current_buffer);
                if (ret < 0)
                    return ret;
            }
            ff_audio_data_set_channels(avr->in_buffer, avr->in_channels);
            if (avr->downmix_needed) {
                av_dlog(avr, "[downmix] in_buffer\n");
                ret = ff_audio_mix(avr->am, avr->in_buffer);
                if (ret < 0)
                    return ret;
            }
            current_buffer = avr->in_buffer;
        }
    } else {
        /* flush resampling buffer and/or output FIFO if input is NULL */
        if (!avr->resample_needed)
            return handle_buffered_output(avr, output ? &output_buffer : NULL,
                                          NULL);
        current_buffer = NULL;
    }

    if (avr->resample_needed) {
        AudioData *resample_out;
        int consumed = 0;

        if (!avr->out_convert_needed && output && out_samples > 0)
            resample_out = &output_buffer;
        else
            resample_out = avr->resample_out_buffer;
        av_dlog(avr, "[resample] %s to %s\n", current_buffer->name,
                resample_out->name);
        ret = ff_audio_resample(avr->resample, resample_out,
                                current_buffer, &consumed);
        if (ret < 0)
            return ret;

        /* if resampling did not produce any samples, just return 0 */
        if (resample_out->nb_samples == 0) {
            av_dlog(avr, "[end conversion]\n");
            return 0;
        }

        current_buffer = resample_out;
    }

    if (avr->upmix_needed) {
        av_dlog(avr, "[upmix] %s\n", current_buffer->name);
        ret = ff_audio_mix(avr->am, current_buffer);
        if (ret < 0)
            return ret;
    }

    /* if we resampled or upmixed directly to output, return here */
    if (current_buffer == &output_buffer) {
        av_dlog(avr, "[end conversion]\n");
        return current_buffer->nb_samples;
    }

    if (avr->out_convert_needed) {
        if (output && out_samples >= current_buffer->nb_samples) {
            /* convert directly to output */
            av_dlog(avr, "[convert] %s to output\n", current_buffer->name);
            ret = ff_audio_convert(avr->ac_out, &output_buffer, current_buffer,
                                   current_buffer->nb_samples);
            if (ret < 0)
                return ret;

            av_dlog(avr, "[end conversion]\n");
            return output_buffer.nb_samples;
        } else {
            ret = ff_audio_data_realloc(avr->out_buffer,
                                        current_buffer->nb_samples);
            if (ret < 0)
                return ret;
            av_dlog(avr, "[convert] %s to out_buffer\n", current_buffer->name);
            ret = ff_audio_convert(avr->ac_out, avr->out_buffer,
                                   current_buffer, current_buffer->nb_samples);
            if (ret < 0)
                return ret;
            current_buffer = avr->out_buffer;
        }
    }

    return handle_buffered_output(avr, output ? &output_buffer : NULL,
                                  current_buffer);
}

int avresample_available(AVAudioResampleContext *avr)
{
    return av_audio_fifo_size(avr->out_fifo);
}

int avresample_read(AVAudioResampleContext *avr, void **output, int nb_samples)
{
    if (!output)
        return av_audio_fifo_drain(avr->out_fifo, nb_samples);
    return av_audio_fifo_read(avr->out_fifo, output, nb_samples);
}

unsigned avresample_version(void)
{
    return LIBAVRESAMPLE_VERSION_INT;
}

const char *avresample_license(void)
{
#define LICENSE_PREFIX "libavresample license: "
    return LICENSE_PREFIX FFMPEG_LICENSE + sizeof(LICENSE_PREFIX) - 1;
}

const char *avresample_configuration(void)
{
    return FFMPEG_CONFIGURATION;
}
