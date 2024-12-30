/*
 * AVR demuxer
 * Copyright (c) 2012 Paul B Mahol
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

#include "libavutil/intreadwrite.h"
#include "avformat.h"
#include "demux.h"
#include "internal.h"
#include "pcm.h"

static int avr_probe(const AVProbeData *p)
{
    if (AV_RL32(p->buf) != MKTAG('2', 'B', 'I', 'T'))
        return 0;

    if (!AV_RB16(p->buf+12) || AV_RB16(p->buf+12) > 256) // channels
        return AVPROBE_SCORE_EXTENSION/2;
    if (AV_RB16(p->buf+14) > 256) // bps
        return AVPROBE_SCORE_EXTENSION/2;

    return AVPROBE_SCORE_EXTENSION;
}

static int avr_read_header(AVFormatContext *s)
{
    uint16_t chan, sign, bps;
    AVStream *st;

    st = avformat_new_stream(s, NULL);
    if (!st)
        return AVERROR(ENOMEM);

    st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;

    avio_skip(s->pb, 4 /* magic */ + 8 /* sample_name */);

    chan = avio_rb16(s->pb);
    if (!chan) {
        st->codecpar->ch_layout.nb_channels = 1;
    } else if (chan == 0xFFFFu) {
        st->codecpar->ch_layout.nb_channels = 2;
    } else {
        avpriv_request_sample(s, "chan %d", chan);
        return AVERROR_PATCHWELCOME;
    }

    st->codecpar->bits_per_coded_sample = bps = avio_rb16(s->pb);

    sign = avio_rb16(s->pb);

    avio_skip(s->pb, 2 /* loop */ + 2 /* midi */ + 1 /* replay speed */);

    st->codecpar->sample_rate = avio_rb24(s->pb);
    if (st->codecpar->sample_rate == 0)
        return AVERROR_INVALIDDATA;

    avio_skip(s->pb, 4 * 3 + 2 * 3 + 20 + 64);

    st->codecpar->codec_id = ff_get_pcm_codec_id(bps, 0, 1, sign);
    if (st->codecpar->codec_id == AV_CODEC_ID_NONE) {
        avpriv_request_sample(s, "Bps %d and sign %d", bps, sign);
        return AVERROR_PATCHWELCOME;
    }

    st->codecpar->block_align = bps * st->codecpar->ch_layout.nb_channels / 8;

    avpriv_set_pts_info(st, 64, 1, st->codecpar->sample_rate);
    return 0;
}

const FFInputFormat ff_avr_demuxer = {
    .p.name         = "avr",
    .p.long_name    = NULL_IF_CONFIG_SMALL("AVR (Audio Visual Research)"),
    .p.extensions   = "avr",
    .p.flags        = AVFMT_GENERIC_INDEX,
    .read_probe     = avr_probe,
    .read_header    = avr_read_header,
    .read_packet    = ff_pcm_read_packet,
    .read_seek      = ff_pcm_read_seek,
};
