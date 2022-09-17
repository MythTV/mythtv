/*
 * Various muxing utility functions
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/dict.h"
#include "libavutil/internal.h"
#include "libavutil/log.h"
#include "libavutil/mem.h"
#include "libavutil/parseutils.h"
#include "avformat.h"
#include "avio.h"
#include "internal.h"
#include "mux.h"

int64_t av_stream_get_end_pts(const AVStream *st)
{
    if (cffstream(st)->priv_pts) {
        return cffstream(st)->priv_pts->val;
    } else
        return AV_NOPTS_VALUE;
}

int avformat_query_codec(const AVOutputFormat *ofmt, enum AVCodecID codec_id,
                         int std_compliance)
{
    if (ofmt) {
        unsigned int codec_tag;
        if (ofmt->query_codec)
            return ofmt->query_codec(codec_id, std_compliance);
        else if (ofmt->codec_tag)
            return !!av_codec_get_tag2(ofmt->codec_tag, codec_id, &codec_tag);
        else if (codec_id == ofmt->video_codec ||
                 codec_id == ofmt->audio_codec ||
                 codec_id == ofmt->subtitle_codec ||
                 codec_id == ofmt->data_codec)
            return 1;
    }
    return AVERROR_PATCHWELCOME;
}

int ff_format_shift_data(AVFormatContext *s, int64_t read_start, int shift_size)
{
    int ret;
    int64_t pos, pos_end;
    uint8_t *buf, *read_buf[2];
    int read_buf_id = 0;
    int read_size[2];
    AVIOContext *read_pb;

    buf = av_malloc_array(shift_size, 2);
    if (!buf)
        return AVERROR(ENOMEM);
    read_buf[0] = buf;
    read_buf[1] = buf + shift_size;

    /* Shift the data: the AVIO context of the output can only be used for
     * writing, so we re-open the same output, but for reading. It also avoids
     * a read/seek/write/seek back and forth. */
    avio_flush(s->pb);
    ret = s->io_open(s, &read_pb, s->url, AVIO_FLAG_READ, NULL);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Unable to re-open %s output file for shifting data\n", s->url);
        goto end;
    }

    /* mark the end of the shift to up to the last data we wrote, and get ready
     * for writing */
    pos_end = avio_tell(s->pb);
    avio_seek(s->pb, read_start + shift_size, SEEK_SET);

    avio_seek(read_pb, read_start, SEEK_SET);
    pos = avio_tell(read_pb);

#define READ_BLOCK do {                                                             \
    read_size[read_buf_id] = avio_read(read_pb, read_buf[read_buf_id], shift_size);  \
    read_buf_id ^= 1;                                                               \
} while (0)

    /* shift data by chunk of at most shift_size */
    READ_BLOCK;
    do {
        int n;
        READ_BLOCK;
        n = read_size[read_buf_id];
        if (n <= 0)
            break;
        avio_write(s->pb, read_buf[read_buf_id], n);
        pos += n;
    } while (pos < pos_end);
    ret = ff_format_io_close(s, &read_pb);

end:
    av_free(buf);
    return ret;
}

int ff_format_output_open(AVFormatContext *s, const char *url, AVDictionary **options)
{
    if (!s->oformat)
        return AVERROR(EINVAL);

    if (!(s->oformat->flags & AVFMT_NOFILE))
        return s->io_open(s, &s->pb, url, AVIO_FLAG_WRITE, options);
    return 0;
}

int ff_stream_encode_params_copy(AVStream *dst, const AVStream *src)
{
    int ret;

    dst->id                  = src->id;
    dst->time_base           = src->time_base;
    dst->nb_frames           = src->nb_frames;
    dst->disposition         = src->disposition;
    dst->sample_aspect_ratio = src->sample_aspect_ratio;
    dst->avg_frame_rate      = src->avg_frame_rate;
    dst->r_frame_rate        = src->r_frame_rate;

    av_dict_free(&dst->metadata);
    ret = av_dict_copy(&dst->metadata, src->metadata, 0);
    if (ret < 0)
        return ret;

    ret = avcodec_parameters_copy(dst->codecpar, src->codecpar);
    if (ret < 0)
        return ret;

    ret = ff_stream_side_data_copy(dst, src);
    if (ret < 0)
        return ret;

    return 0;
}

int ff_parse_creation_time_metadata(AVFormatContext *s, int64_t *timestamp, int return_seconds)
{
    AVDictionaryEntry *entry;
    int64_t parsed_timestamp;
    int ret;
    if ((entry = av_dict_get(s->metadata, "creation_time", NULL, 0))) {
        if ((ret = av_parse_time(&parsed_timestamp, entry->value, 0)) >= 0) {
            *timestamp = return_seconds ? parsed_timestamp / 1000000 : parsed_timestamp;
            return 1;
        } else {
            av_log(s, AV_LOG_WARNING, "Failed to parse creation_time %s\n", entry->value);
            return ret;
        }
    }
    return 0;
}

int ff_standardize_creation_time(AVFormatContext *s)
{
    int64_t timestamp;
    int ret = ff_parse_creation_time_metadata(s, &timestamp, 0);
    if (ret == 1)
        return avpriv_dict_set_timestamp(&s->metadata, "creation_time", timestamp);
    return ret;
}
