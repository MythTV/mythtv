/*
 * MODS demuxer
 * Copyright (c) 2015-2016 Florian Nouwt
 * Copyright (c) 2017 Adib Surani
 * Copyright (c) 2020 Paul B Mahol
 * Copyright (c) 2026 Link Mauve
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

typedef struct MODSDemuxContext {
    uint32_t index_pos;
} MODSDemuxContext;

static int mods_probe(const AVProbeData *p)
{
    if (memcmp(p->buf, "MODSN3\x0a\x00", 8))
        return 0;
    if (AV_RB32(p->buf + 8) == 0)
        return 0;
    if (AV_RB32(p->buf + 12) == 0)
        return 0;
    if (AV_RB32(p->buf + 16) == 0)
        return 0;
    return AVPROBE_SCORE_MAX;
}

static int mods_read_header(AVFormatContext *s)
{
    AVIOContext *pb = s->pb;
    MODSDemuxContext *ctx = s->priv_data;
    AVRational fps;
    int64_t pos;
    int64_t timestamp;
    int num_keyframes;
    const AVIndexEntry *e;

    AVStream *st = avformat_new_stream(s, NULL);
    if (!st)
        return AVERROR(ENOMEM);

    avio_skip(pb, 8);

    st->nb_frames            = avio_rl32(pb);
    st->duration             = st->nb_frames;
    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codecpar->codec_id   = AV_CODEC_ID_MOBICLIP;
    st->codecpar->width      = avio_rl32(pb);
    st->codecpar->height     = avio_rl32(pb);

    fps.num = avio_rl32(pb);
    fps.den = 0x1000000;
    avpriv_set_pts_info(st, 64, fps.den, fps.num);

    avio_skip(pb, 16);

    pos = avio_rl32(pb);
    num_keyframes = avio_rl32(pb);
    avio_seek(pb, pos, SEEK_SET);
    ctx->index_pos = pos;

    for (int i = 0; i < num_keyframes; ++i) {
        timestamp = avio_rl32(pb);
        pos = avio_rl32(pb);
        if (avio_feof(pb))
            return AVERROR_INVALIDDATA;

        av_add_index_entry(st, pos, timestamp, 0, 0, AVINDEX_KEYFRAME);
    }

    e = avformat_index_get_entry(st, 0);
    if (!e)
        return AVERROR_INVALIDDATA;

    avio_seek(pb, e->pos, SEEK_SET);

    return 0;
}

static int mods_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    AVIOContext *pb = s->pb;
    MODSDemuxContext *ctx = s->priv_data;
    unsigned size;
    int64_t pos;
    int ret;
    const AVIndexEntry *e;

    if (avio_feof(pb))
        return AVERROR_EOF;

    /* This assumes the keyframes index directly follows the last packet. */
    pos = avio_tell(pb);
    if (pos == ctx->index_pos)
        return AVERROR_EOF;

    size = avio_rl32(pb) >> 14;
    ret = av_get_packet(pb, pkt, size);
    pkt->pos = pos;
    pkt->stream_index = 0;

    e = avformat_index_get_entry_from_timestamp(s->streams[0], pos, 0);
    if (e)
        pkt->flags |= AV_PKT_FLAG_KEY;

    return ret;
}

const FFInputFormat ff_mods_demuxer = {
    .p.name         = "mods",
    .p.long_name    = NULL_IF_CONFIG_SMALL("MobiClip MODS"),
    .p.extensions   = "mods",
    .p.flags        = AVFMT_GENERIC_INDEX,
    .priv_data_size = sizeof(MODSDemuxContext),
    .read_probe     = mods_probe,
    .read_header    = mods_read_header,
    .read_packet    = mods_read_packet,
};
