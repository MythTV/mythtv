/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2012  Petri Hintukainen <phintuka@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "mpls_parse.h"

#include "extdata_parse.h"

#include "disc/disc.h"

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"

#include <stdlib.h>
#include <string.h>

#define MPLS_SIG1  ('M' << 24 | 'P' << 16 | 'L' << 8 | 'S')
#define MPLS_SIG2A ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define MPLS_SIG2B ('0' << 24 | '1' << 16 | '0' << 8 | '0')

static void
_human_readable_sig(char *sig, uint32_t s1, uint32_t s2)
{
    sig[0] = (s1 >> 24) & 0xFF;
    sig[1] = (s1 >> 16) & 0xFF;
    sig[2] = (s1 >>  8) & 0xFF;
    sig[3] = (s1      ) & 0xFF;
    sig[4] = (s2 >> 24) & 0xFF;
    sig[5] = (s2 >> 16) & 0xFF;
    sig[6] = (s2 >>  8) & 0xFF;
    sig[7] = (s2      ) & 0xFF;
    sig[8] = 0;
}

int
mpls_parse_uo(uint8_t *buf, BD_UO_MASK *uo)
{
    BITBUFFER bb;
    bb_init(&bb, buf, 8);

    memset(uo, 0, sizeof(BD_UO_MASK));

    uo->menu_call                       = bb_read(&bb, 1);
    uo->title_search                    = bb_read(&bb, 1);
    uo->chapter_search                  = bb_read(&bb, 1);
    uo->time_search                     = bb_read(&bb, 1);
    uo->skip_to_next_point              = bb_read(&bb, 1);
    uo->skip_to_prev_point              = bb_read(&bb, 1);
    uo->play_firstplay                  = bb_read(&bb, 1);
    uo->stop                            = bb_read(&bb, 1);
    uo->pause_on                        = bb_read(&bb, 1);
    uo->pause_off                       = bb_read(&bb, 1);
    uo->still_off                       = bb_read(&bb, 1);
    uo->forward                         = bb_read(&bb, 1);
    uo->backward                        = bb_read(&bb, 1);
    uo->resume                          = bb_read(&bb, 1);
    uo->move_up                         = bb_read(&bb, 1);
    uo->move_down                       = bb_read(&bb, 1);
    uo->move_left                       = bb_read(&bb, 1);
    uo->move_right                      = bb_read(&bb, 1);
    uo->select                          = bb_read(&bb, 1);
    uo->activate                        = bb_read(&bb, 1);
    uo->select_and_activate             = bb_read(&bb, 1);
    uo->primary_audio_change            = bb_read(&bb, 1);
    bb_skip(&bb, 1);
    uo->angle_change                    = bb_read(&bb, 1);
    uo->popup_on                        = bb_read(&bb, 1);
    uo->popup_off                       = bb_read(&bb, 1);
    uo->pg_enable_disable               = bb_read(&bb, 1);
    uo->pg_change                       = bb_read(&bb, 1);
    uo->secondary_video_enable_disable  = bb_read(&bb, 1);
    uo->secondary_video_change          = bb_read(&bb, 1);
    uo->secondary_audio_enable_disable  = bb_read(&bb, 1);
    uo->secondary_audio_change          = bb_read(&bb, 1);
    bb_skip(&bb, 1);
    uo->pip_pg_change                   = bb_read(&bb, 1);
    bb_skip(&bb, 30);
    return 1;
}

static int
_parse_uo(BITSTREAM *bits, BD_UO_MASK *uo)
{
    uint8_t buf[8];
    bs_read_bytes(bits, buf, 8);
    return mpls_parse_uo(buf, uo);
}

static int
_parse_appinfo(BITSTREAM *bits, MPLS_AI *ai)
{
    int64_t /*pos,*/ len;

    if (!bs_is_align(bits, 0x07)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_appinfo: alignment error\n");
    }
    //pos = bs_pos(bits) >> 3;
    len = bs_read(bits, 32);

    if (bs_avail(bits) < len * 8) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_appinfo: unexpected end of file\n");
        return 0;
    }

    // Reserved
    bs_skip(bits, 8);
    ai->playback_type = bs_read(bits, 8);
    if (ai->playback_type == 2 || ai->playback_type == 3) {
        ai->playback_count = bs_read(bits, 16);
    } else {
        // Reserved
        bs_skip(bits, 16);
    }
    _parse_uo(bits, &ai->uo_mask);
    ai->random_access_flag = bs_read(bits, 1);
    ai->audio_mix_flag = bs_read(bits, 1);
    ai->lossless_bypass_flag = bs_read(bits, 1);
#if 0
    // Reserved
    bs_skip(bits, 13);
    bs_seek_byte(bits, pos + len);
#endif
    return 1;
}

static int
_parse_header(BITSTREAM *bits, MPLS_PL *pl)
{
    if (bs_avail(bits) < 5 * 32 + 160) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_header: unexpected end of file\n");
        return 0;
    }

    pl->type_indicator  = bs_read(bits, 32);
    pl->type_indicator2 = bs_read(bits, 32);
    if (pl->type_indicator != MPLS_SIG1 || 
        (pl->type_indicator2 != MPLS_SIG2A && 
         pl->type_indicator2 != MPLS_SIG2B)) {

        char sig[9];
        char expect[9];

        _human_readable_sig(sig, pl->type_indicator, pl->type_indicator2);
        _human_readable_sig(expect, MPLS_SIG1, MPLS_SIG2A);
        BD_DEBUG(DBG_NAV | DBG_CRIT, "failed signature match, expected (%s) got (%s)\n",
                expect, sig);
        return 0;
    }
    pl->list_pos = bs_read(bits, 32);
    pl->mark_pos = bs_read(bits, 32);
    pl->ext_pos  = bs_read(bits, 32);

    // Skip 160 reserved bits
    bs_skip(bits, 160);

    _parse_appinfo(bits, &pl->app_info);
    return 1;
}

static int
_parse_stream(BITSTREAM *bits, MPLS_STREAM *s)
{
    int len;
    int64_t pos;

    if (!bs_is_align(bits, 0x07)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_stream: Stream alignment error\n");
    }
    len = bs_read(bits, 8);
    pos = bs_pos(bits) >> 3;

    s->stream_type = bs_read(bits, 8);
    switch (s->stream_type) {
        case 1:
            s->pid = bs_read(bits, 16);
            break;

        case 2:
        case 4:
            s->subpath_id = bs_read(bits, 8);
            s->subclip_id = bs_read(bits, 8);
            s->pid        = bs_read(bits, 16);
            break;

        case 3:
            s->subpath_id = bs_read(bits, 8);
            s->pid        = bs_read(bits, 16);
            break;

        default:
            BD_DEBUG(DBG_NAV | DBG_CRIT, "unrecognized stream type %02x\n", s->stream_type);
            break;
    };

    if (bs_seek_byte(bits, pos + len) < 0) {
        return 0;
    }

    len = bs_read(bits, 8);
    pos = bs_pos(bits) >> 3;

    s->lang[0] = '\0';
    s->coding_type = bs_read(bits, 8);
    switch (s->coding_type) {
        case 0x01:
        case 0x02:
        case 0xea:
        case 0x1b:
            s->format = bs_read(bits, 4);
            s->rate   = bs_read(bits, 4);
            break;

        case 0x03:
        case 0x04:
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0xa1:
        case 0xa2:
            s->format = bs_read(bits, 4);
            s->rate   = bs_read(bits, 4);
            bs_read_string(bits, s->lang, 3);
            break;

        case 0x90:
        case 0x91:
            bs_read_string(bits, s->lang, 3);
            break;

        case 0x92:
            s->char_code = bs_read(bits, 8);
            bs_read_string(bits, s->lang, 3);
            break;

        default:
            BD_DEBUG(DBG_NAV | DBG_CRIT, "unrecognized coding type %02x\n", s->coding_type);
            break;
    };
    s->lang[3] = '\0';

    if (bs_seek_byte(bits, pos + len) < 0) {
        return 0;
    }

    return 1;
}

static int
_parse_stn(BITSTREAM *bits, MPLS_STN *stn)
{
    int len;
    int64_t pos;
    MPLS_STREAM    *ss;
    int ii,jj;

    if (!bs_is_align(bits, 0x07)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_stream: Stream alignment error\n");
    }
    // Skip STN len
    len = bs_read(bits, 16);
    pos = bs_pos(bits) >> 3;

    // Skip 2 reserved bytes
    bs_skip(bits, 16);

    stn->num_video           = bs_read(bits, 8);
    stn->num_audio           = bs_read(bits, 8);
    stn->num_pg              = bs_read(bits, 8);
    stn->num_ig              = bs_read(bits, 8);
    stn->num_secondary_audio = bs_read(bits, 8);
    stn->num_secondary_video = bs_read(bits, 8);
    stn->num_pip_pg          = bs_read(bits, 8);

    // 5 reserve bytes
    bs_skip(bits, 5 * 8);

    // Primary Video Streams
    ss = NULL;
    if (stn->num_video) {
        ss = calloc(stn->num_video, sizeof(MPLS_STREAM));
        for (ii = 0; ii < stn->num_video; ii++) {
            if (!_parse_stream(bits, &ss[ii])) {
                X_FREE(ss);
                BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing video entry\n");
                return 0;
            }
        }
    }
    stn->video = ss;

    // Primary Audio Streams
    ss = NULL;
    if (stn->num_audio) {
        ss = calloc(stn->num_audio, sizeof(MPLS_STREAM));
        for (ii = 0; ii < stn->num_audio; ii++) {

            if (!_parse_stream(bits, &ss[ii])) {
                X_FREE(ss);
                BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing audio entry\n");
                return 0;
            }
        }
    }
    stn->audio = ss;

    // Presentation Graphic Streams
    ss = NULL;
    if (stn->num_pg  || stn->num_pip_pg) {
        ss = calloc(stn->num_pg + stn->num_pip_pg, sizeof(MPLS_STREAM));
        for (ii = 0; ii < (stn->num_pg + stn->num_pip_pg); ii++) {
            if (!_parse_stream(bits, &ss[ii])) {
                X_FREE(ss);
                BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing pg/pip-pg entry\n");
                return 0;
            }
        }
    }
    stn->pg = ss;

    // Interactive Graphic Streams
    ss = NULL;
    if (stn->num_ig) {
        ss = calloc(stn->num_ig, sizeof(MPLS_STREAM));
        for (ii = 0; ii < stn->num_ig; ii++) {
            if (!_parse_stream(bits, &ss[ii])) {
                X_FREE(ss);
                BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing ig entry\n");
                return 0;
            }
        }
    }
    stn->ig = ss;

    // Secondary Audio Streams
    ss = NULL;
    if (stn->num_secondary_audio) {
        ss = calloc(stn->num_secondary_audio, sizeof(MPLS_STREAM));
        for (ii = 0; ii < stn->num_secondary_audio; ii++) {
            if (!_parse_stream(bits, &ss[ii])) {
                X_FREE(ss);
                BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing secondary audio entry\n");
                return 0;
            }
            // Read Secondary Audio Extra Attributes
            ss->sa_num_primary_audio_ref = bs_read(bits, 8);
            bs_skip(bits, 8);
            if (ss->sa_num_primary_audio_ref) {
                ss->sa_primary_audio_ref = calloc(ss->sa_num_primary_audio_ref, sizeof(uint8_t));
                for (jj = 0; jj < ss->sa_num_primary_audio_ref; jj++) {
                   ss->sa_primary_audio_ref[jj] = bs_read(bits, 8);
                }
                if (ss->sa_num_primary_audio_ref % 2) {
                    bs_skip(bits, 8);
                }
            }
        }
    }
    stn->secondary_audio = ss;

    // Secondary Video Streams
    ss = NULL;
    if (stn->num_secondary_video) {
        ss = calloc(stn->num_secondary_video, sizeof(MPLS_STREAM));
        for (ii = 0; ii < stn->num_secondary_video; ii++) {
            if (!_parse_stream(bits, &ss[ii])) {
                X_FREE(ss);
                BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing secondary video entry\n");
                return 0;
            }
            // Read Secondary Video Extra Attributes
            ss->sv_num_secondary_audio_ref = bs_read(bits, 8);
           bs_skip(bits, 8);
            if (ss->sv_num_secondary_audio_ref) {
                ss->sv_secondary_audio_ref = calloc(ss->sv_num_secondary_audio_ref, sizeof(uint8_t));
                for (jj = 0; jj < ss->sv_num_secondary_audio_ref; jj++) {
                    ss->sv_secondary_audio_ref[jj] = bs_read(bits, 8);
                }
                if (ss->sv_num_secondary_audio_ref % 2) {
                    bs_skip(bits, 8);
                }
            }
            ss->sv_num_pip_pg_ref = bs_read(bits, 8);
            bs_skip(bits, 8);
            if (ss->sv_num_pip_pg_ref) {
                ss->sv_pip_pg_ref = calloc(ss->sv_num_pip_pg_ref, sizeof(uint8_t));
                for (jj = 0; jj < ss->sv_num_pip_pg_ref; jj++) {
                    ss->sv_pip_pg_ref[jj] = bs_read(bits, 8);
                }
                if (ss->sv_num_pip_pg_ref % 2) {
                    bs_skip(bits, 8);
                }
            }

        }
    }
    stn->secondary_video = ss;

    if (bs_seek_byte(bits, pos + len) < 0) {
        return 0;
    }

    return 1;
}

static void
_clean_stn(MPLS_STN *stn)
{
    if(stn->secondary_audio) {
        X_FREE(stn->secondary_audio->sa_primary_audio_ref);
    }
    if(stn->secondary_video) {
        X_FREE(stn->secondary_video->sv_secondary_audio_ref);
        X_FREE(stn->secondary_video->sv_pip_pg_ref);
    }

    X_FREE(stn->video);
    X_FREE(stn->audio);
    X_FREE(stn->pg);
    X_FREE(stn->ig);
    X_FREE(stn->secondary_audio);
    X_FREE(stn->secondary_video);
}

static int
_parse_playitem(BITSTREAM *bits, MPLS_PI *pi)
{
    int len, ii;
    int64_t pos;
    char clip_id[6], codec_id[5];
    uint8_t stc_id;

    if (!bs_is_align(bits, 0x07)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_playitem: Stream alignment error\n");
    }

    // PlayItem Length
    len = bs_read(bits, 16);
    pos = bs_pos(bits) >> 3;

    // Primary Clip identifer
    bs_read_string(bits, clip_id, 5);

    bs_read_string(bits, codec_id, 4);
    if (memcmp(codec_id, "M2TS", 4) != 0) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "Incorrect CodecIdentifier (%s)\n", codec_id);
    }

    // Skip reserved 11 bits
    bs_skip(bits, 11);

    pi->is_multi_angle = bs_read(bits, 1);

    pi->connection_condition = bs_read(bits, 4);
    if (pi->connection_condition != 0x01 && 
        pi->connection_condition != 0x05 &&
        pi->connection_condition != 0x06) {

        BD_DEBUG(DBG_NAV | DBG_CRIT, "Unexpected connection condition %02x\n",
                pi->connection_condition);
    }

    stc_id   = bs_read(bits, 8);
    pi->in_time  = bs_read(bits, 32);
    pi->out_time = bs_read(bits, 32);

    _parse_uo(bits, &pi->uo_mask);
    pi->random_access_flag = bs_read(bits, 1);
    bs_skip(bits, 7);
    pi->still_mode = bs_read(bits, 8);
    if (pi->still_mode == 0x01) {
        pi->still_time = bs_read(bits, 16);
    } else {
        bs_skip(bits, 16);
    }

    pi->angle_count = 1;
    if (pi->is_multi_angle) {
        pi->angle_count = bs_read(bits, 8);
        if (pi->angle_count < 1) {
            pi->angle_count = 1;
        }
        bs_skip(bits, 6);
        pi->is_different_audio = bs_read(bits, 1);
        pi->is_seamless_angle = bs_read(bits, 1);
    }
    pi->clip = calloc(pi->angle_count, sizeof(MPLS_CLIP));
    strcpy(pi->clip[0].clip_id, clip_id);
    strcpy(pi->clip[0].codec_id, codec_id);
    pi->clip[0].stc_id = stc_id;
    for (ii = 1; ii < pi->angle_count; ii++) {
        bs_read_string(bits, pi->clip[ii].clip_id, 5);

        bs_read_string(bits, pi->clip[ii].codec_id, 4);
        if (memcmp(pi->clip[ii].codec_id, "M2TS", 4) != 0) {
            BD_DEBUG(DBG_NAV | DBG_CRIT, "Incorrect CodecIdentifier (%s)\n", pi->clip[ii].codec_id);
        }
        pi->clip[ii].stc_id   = bs_read(bits, 8);
    }
    if (!_parse_stn(bits, &pi->stn)) {
        return 0;
    }

    // Seek past any unused items
    if (bs_seek_byte(bits, pos + len) < 0) {
        return 0;
    }

    return 1;
}

static void
_clean_playitem(MPLS_PI *pi)
{
    X_FREE(pi->clip);
    _clean_stn(&pi->stn);
}

static int
_parse_subplayitem(BITSTREAM *bits, MPLS_SUB_PI *spi)
{
    int len, ii;
    int64_t pos;
    char clip_id[6], codec_id[5];
    uint8_t stc_id;

    if (!bs_is_align(bits, 0x07)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_subplayitem: alignment error\n");
    }

    // PlayItem Length
    len = bs_read(bits, 16);
    pos = bs_pos(bits) >> 3;

    // Primary Clip identifer
    bs_read_string(bits, clip_id, 5);

    bs_read_string(bits, codec_id, 4);
    if (memcmp(codec_id, "M2TS", 4) != 0) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "Incorrect CodecIdentifier (%s)\n", codec_id);
    }

    bs_skip(bits, 27);

    spi->connection_condition = bs_read(bits, 4);

    if (spi->connection_condition != 0x01 && 
        spi->connection_condition != 0x05 &&
        spi->connection_condition != 0x06) {

        BD_DEBUG(DBG_NAV | DBG_CRIT, "Unexpected connection condition %02x\n",
                spi->connection_condition);
    }
    spi->is_multi_clip     = bs_read(bits, 1);
    stc_id                 = bs_read(bits, 8);
    spi->in_time           = bs_read(bits, 32);
    spi->out_time          = bs_read(bits, 32);
    spi->sync_play_item_id = bs_read(bits, 16);
    spi->sync_pts          = bs_read(bits, 32);
    spi->clip_count = 1;
    if (spi->is_multi_clip) {
        spi->clip_count    = bs_read(bits, 8);
        if (spi->clip_count < 1) {
            spi->clip_count = 1;
        }
    }
    spi->clip = calloc(spi->clip_count, sizeof(MPLS_CLIP));
    strcpy(spi->clip[0].clip_id, clip_id);
    strcpy(spi->clip[0].codec_id, codec_id);
    spi->clip[0].stc_id = stc_id;
    for (ii = 1; ii < spi->clip_count; ii++) {
        // Primary Clip identifer
        bs_read_string(bits, spi->clip[ii].clip_id, 5);

        bs_read_string(bits, spi->clip[ii].codec_id, 4);
        if (memcmp(spi->clip[ii].codec_id, "M2TS", 4) != 0) {
            BD_DEBUG(DBG_NAV | DBG_CRIT, "Incorrect CodecIdentifier (%s)\n", spi->clip[ii].codec_id);
        }
        spi->clip[ii].stc_id = bs_read(bits, 8);
    }


    // Seek to end of subpath
    if (bs_seek_byte(bits, pos + len) < 0) {
        return 0;
    }

    return 1;
}

static void
_clean_subplayitem(MPLS_SUB_PI *spi)
{
    X_FREE(spi->clip);
}

static int
_parse_subpath(BITSTREAM *bits, MPLS_SUB *sp)
{
    int len, ii;
    int64_t pos;
    MPLS_SUB_PI *spi = NULL;

    if (!bs_is_align(bits, 0x07)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_subpath: alignment error\n");
    }

    // PlayItem Length
    len = bs_read(bits, 32);
    pos = bs_pos(bits) >> 3;

    bs_skip(bits, 8);
    sp->type = bs_read(bits, 8);
    bs_skip(bits, 15);
    sp->is_repeat = bs_read(bits, 1);
    bs_skip(bits, 8);
    sp->sub_playitem_count = bs_read(bits, 8);

    spi = calloc(sp->sub_playitem_count,  sizeof(MPLS_SUB_PI));
    for (ii = 0; ii < sp->sub_playitem_count; ii++) {
        if (!_parse_subplayitem(bits, &spi[ii])) {
            X_FREE(spi);
            BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing sub play item\n");
            return 0;
        }
    }
    sp->sub_play_item = spi;

    // Seek to end of subpath
    if (bs_seek_byte(bits, pos + len) < 0) {
        return 0;
    }

    return 1;
}

static void
_clean_subpath(MPLS_SUB *sp)
{
    int ii;

    for (ii = 0; ii < sp->sub_playitem_count; ii++) {
        _clean_subplayitem(&sp->sub_play_item[ii]);
    }
    X_FREE(sp->sub_play_item);
}

static int
_parse_playlistmark(BITSTREAM *bits, MPLS_PL *pl)
{
    int64_t len;
    int ii;
    MPLS_PLM *plm = NULL;

    if (bs_seek_byte(bits, pl->mark_pos) < 0) {
        return 0;
    }

    // length field
    len = bs_read(bits, 32);

    if (bs_avail(bits) < len * 8) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_playlistmark: unexpected end of file\n");
        return 0;
    }

    // Then get the number of marks
    pl->mark_count = bs_read(bits, 16);

    plm = calloc(pl->mark_count, sizeof(MPLS_PLM));
    for (ii = 0; ii < pl->mark_count; ii++) {
        bs_skip(bits, 8); /* reserved */
        plm[ii].mark_type     = bs_read(bits, 8);
        plm[ii].play_item_ref = bs_read(bits, 16);
        plm[ii].time          = bs_read(bits, 32);
        plm[ii].entry_es_pid  = bs_read(bits, 16);
        plm[ii].duration      = bs_read(bits, 32);
    }
    pl->play_mark = plm;
    return 1;
}

static int
_parse_playlist(BITSTREAM *bits, MPLS_PL *pl)
{
    int64_t len;
    int ii;
    MPLS_PI *pi = NULL;
    MPLS_SUB *sub_path = NULL;

    if (bs_seek_byte(bits, pl->list_pos) < 0) {
        return 0;
    }

    // playlist length
    len = bs_read(bits, 32);

    if (bs_avail(bits) < len * 8) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_playlist: unexpected end of file\n");
        return 0;
    }

    // Skip reserved bytes
    bs_skip(bits, 16);

    pl->list_count = bs_read(bits, 16);
    pl->sub_count = bs_read(bits, 16);

    pi = calloc(pl->list_count,  sizeof(MPLS_PI));
    for (ii = 0; ii < pl->list_count; ii++) {
        if (!_parse_playitem(bits, &pi[ii])) {
            X_FREE(pi);
            BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing play list item\n");
            return 0;
        }
    }
    pl->play_item = pi;

    sub_path = calloc(pl->sub_count,  sizeof(MPLS_SUB));
    for (ii = 0; ii < pl->sub_count; ii++)
    {
        if (!_parse_subpath(bits, &sub_path[ii]))
        {
            X_FREE(sub_path);
            BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing subpath\n");
            return 0;
        }
    }
    pl->sub_path = sub_path;

    return 1;
}

static void _clean_pip_data(MPLS_PIP_METADATA *p)
{
    X_FREE(p->data);
}

static void
_clean_playlist(MPLS_PL *pl)
{
    int ii;

    if (pl == NULL) {
        return;
    }
    if (pl->play_item != NULL) {
        for (ii = 0; ii < pl->list_count; ii++) {
            _clean_playitem(&pl->play_item[ii]);
        }
        X_FREE(pl->play_item);
    }
    if (pl->sub_path != NULL) {
        for (ii = 0; ii < pl->sub_count; ii++) {
            _clean_subpath(&pl->sub_path[ii]);
        }
        X_FREE(pl->sub_path);
    }
    if (pl->ext_sub_path != NULL) {
        for (ii = 0; ii < pl->ext_sub_count; ii++) {
            _clean_subpath(&pl->ext_sub_path[ii]);
        }
        X_FREE(pl->ext_sub_path);
    }
    if (pl->ext_pip_data != NULL) {
        for (ii = 0; ii < pl->ext_pip_data_count; ii++) {
            _clean_pip_data(&pl->ext_pip_data[ii]);
        }
        X_FREE(pl->ext_pip_data);
    }

    X_FREE(pl->play_mark);
    X_FREE(pl);
}

void
mpls_free(MPLS_PL *pl)
{
    _clean_playlist(pl);
}

static int
_parse_pip_data(BITSTREAM *bits, MPLS_PIP_METADATA *block)
{
    MPLS_PIP_DATA *data;
    unsigned ii;

    uint16_t entries = bs_read(bits, 16);
    if (entries < 1) {
        return 1;
    }

    data = calloc(entries, sizeof(MPLS_PIP_DATA));
    if (!data) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return 0;
    }

    for (ii = 0; ii < entries; ii++) {

        data[ii].time = bs_read(bits, 32);
        data[ii].xpos = bs_read(bits, 12);
        data[ii].ypos = bs_read(bits, 12);
        data[ii].scale_factor = bs_read(bits, 4);
        bs_skip(bits, 4);
    }

    block->data_count = entries;
    block->data = data;

    return 1;
}

static int
_parse_pip_metadata_block(BITSTREAM *bits, uint32_t start_address, MPLS_PIP_METADATA *data)
{
    uint32_t data_address;
    int result;
    int64_t pos;

    data->clip_ref            = bs_read(bits, 16);
    data->secondary_video_ref = bs_read(bits, 8);
    bs_skip(bits, 8);
    data->timeline_type       = bs_read(bits, 4);
    data->luma_key_flag       = bs_read(bits, 1);
    data->trick_play_flag     = bs_read(bits, 1);
    bs_skip(bits, 10);
    if (data->luma_key_flag) {
        bs_skip(bits, 8);
        data->upper_limit_luma_key = bs_read(bits, 8);
    } else {
        bs_skip(bits, 16);
    }
    bs_skip(bits, 16);

    data_address = bs_read(bits, 32);

    pos = bs_pos(bits) / 8;
    if (bs_seek_byte(bits, start_address + data_address) < 0) {
        return 0;
    }
    result = _parse_pip_data(bits, data);
    if (bs_seek_byte(bits, pos) < 0) {
        return 0;
    }

    return result;
}

static int
_parse_pip_metadata_extension(BITSTREAM *bits, MPLS_PL *pl)
{
    MPLS_PIP_METADATA *data;
    int ii;

    uint32_t start_address = (uint32_t)bs_pos(bits) / 8;
    uint32_t len           = bs_read(bits, 32);
    int      entries       = bs_read(bits, 16);

    if (len < 1 || entries < 1) {
        return 0;
    }

    data = calloc(entries, sizeof(MPLS_PIP_METADATA));
    if (!data) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return 0;
    }

    for (ii = 0; ii < entries; ii++) {
        if (!_parse_pip_metadata_block(bits, start_address, &data[ii])) {
            goto error;
        }
    }

    pl->ext_pip_data_count = entries;
    pl->ext_pip_data       = data;

    return 1;

 error:
    BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing pip metadata extension\n");
    for (ii = 0; ii < entries; ii++) {
        _clean_pip_data(&data[ii]);
    }
    X_FREE(data);
    return 0;

}

static int
_parse_subpath_extension(BITSTREAM *bits, MPLS_PL *pl)
{
    MPLS_SUB *sub_path;
    int ii;

    uint32_t len       = bs_read(bits, 32);
    int      sub_count = bs_read(bits, 16);

    if (len < 1 || sub_count < 1) {
        return 0;
    }

    sub_path = calloc(sub_count,  sizeof(MPLS_SUB));
    if (!sub_path) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return 0;
    }

    for (ii = 0; ii < sub_count; ii++) {
        if (!_parse_subpath(bits, &sub_path[ii])) {
            goto error;
        }
    }
    pl->ext_sub_path  = sub_path;
    pl->ext_sub_count = sub_count;

    return 1;

 error:
    BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing extension subpath\n");
    for (ii = 0; ii < sub_count; ii++) {
        _clean_subpath(&sub_path[ii]);
    }
    X_FREE(sub_path);
    return 0;
}

static int
_parse_mpls_extension(BITSTREAM *bits, int id1, int id2, void *handle)
{
    MPLS_PL *pl = (MPLS_PL*)handle;

    if (id1 == 1) {
        if (id2 == 1) {
            // PiP metadata extension
            return _parse_pip_metadata_extension(bits, pl);
        }
    }

    if (id1 == 2) {
        if (id2 == 1) {
            return 0;
        }
        if (id2 == 2) {
            // SubPath entries extension
            return _parse_subpath_extension(bits, pl);
        }
    }

    BD_DEBUG(DBG_NAV | DBG_CRIT, "_parse_mpls_extension(): unhandled extension %d.%d\n", id1, id2);

    return 0;
}

static MPLS_PL*
_mpls_parse(BD_FILE_H *fp)
{
    BITSTREAM  bits;
    MPLS_PL   *pl = NULL;

    if (bs_init(&bits, fp) < 0) {
        BD_DEBUG(DBG_NAV, "?????.mpls: read error\n");
        return NULL;
    }

    pl = calloc(1, sizeof(MPLS_PL));
    if (pl == NULL) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return NULL;
    }

    if (!_parse_header(&bits, pl)) {
        _clean_playlist(pl);
        return NULL;
    }
    if (!_parse_playlist(&bits, pl)) {
        _clean_playlist(pl);
        return NULL;
    }
    if (!_parse_playlistmark(&bits, pl)) {
        _clean_playlist(pl);
        return NULL;
    }

    if (pl->ext_pos > 0) {
        bdmv_parse_extension_data(&bits,
                                  pl->ext_pos,
                                  _parse_mpls_extension,
                                  pl);
    }

    return pl;
}

MPLS_PL*
mpls_parse(const char *path)
{
    MPLS_PL   *pl;
    BD_FILE_H *fp;

    fp = file_open(path, "rb");
    if (!fp) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "Failed to open %s\n", path);
        return NULL;
    }

    pl = _mpls_parse(fp);
    file_close(fp);
    return pl;
}

static MPLS_PL*
_mpls_get(BD_DISC *disc, const char *dir, const char *file)
{
    MPLS_PL   *pl;
    BD_FILE_H *fp;

    fp = disc_open_file(disc, dir, file);
    if (!fp) {
        return NULL;
    }

    pl = _mpls_parse(fp);
    file_close(fp);
    return pl;
}

MPLS_PL*
mpls_get(BD_DISC *disc, const char *file)
{
    MPLS_PL *pl;

    pl = _mpls_get(disc, "BDMV" DIR_SEP "PLAYLIST", file);
    if (pl) {
        return pl;
    }

    /* if failed, try backup file */
    pl = _mpls_get(disc, "BDMV" DIR_SEP "BACKUP" DIR_SEP "PLAYLIST", file);
    return pl;
}
