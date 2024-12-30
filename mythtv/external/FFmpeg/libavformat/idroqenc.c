/*
 * id RoQ (.roq) File muxer
 * Copyright (c) 2007 Vitor Sessak
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

#include "avformat.h"
#include "mux.h"
#include "rawenc.h"


static int roq_write_header(struct AVFormatContext *s)
{
    uint8_t header[] = {
        0x84, 0x10, 0xFF, 0xFF, 0xFF, 0xFF, /* fps: */ 0x1E, 0x00
    };
    int n;

// set the actual fps
    for(n=0;n<s->nb_streams;n++) {
        if (s->streams[n]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            unsigned int fps;

            if (s->streams[n]->avg_frame_rate.den != 1) {
                av_log(s, AV_LOG_ERROR, "Frame rate must be integer\n");
                return AVERROR(EINVAL);
            }

            if ((fps=s->streams[n]->avg_frame_rate.num) > 255) {
                av_log(s, AV_LOG_ERROR, "Frame rate may not exceed 255fps\n");
                return AVERROR(EINVAL);
            }

            if (fps != 30) {
                av_log(s, AV_LOG_WARNING, "For vintage compatibility fps must be 30\n");
            }

            header[6] = fps;
            break;
        }
    }

    avio_write(s->pb, header, 8);

    return 0;
}

const FFOutputFormat ff_roq_muxer = {
    .p.name        = "roq",
    .p.long_name   = NULL_IF_CONFIG_SMALL("raw id RoQ"),
    .p.extensions  = "roq",
    .p.audio_codec = AV_CODEC_ID_ROQ_DPCM,
    .p.video_codec = AV_CODEC_ID_ROQ,
    .p.subtitle_codec = AV_CODEC_ID_NONE,
    .flags_internal   = FF_OFMT_FLAG_MAX_ONE_OF_EACH |
                        FF_OFMT_FLAG_ONLY_DEFAULT_CODECS,
    .write_header = roq_write_header,
    .write_packet = ff_raw_write_packet,
};
