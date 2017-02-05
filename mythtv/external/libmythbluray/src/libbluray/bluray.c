/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2010       hpi1
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

#include "bluray-version.h"
#include "bluray.h"
#include "bluray_internal.h"
#include "keys.h"
#include "register.h"
#include "util/array.h"
#include "util/event_queue.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"
#include "util/mutex.h"
#include "bdnav/bdid_parse.h"
#include "bdnav/navigation.h"
#include "bdnav/index_parse.h"
#include "bdnav/meta_parse.h"
#include "bdnav/meta_data.h"
#include "bdnav/clpi_parse.h"
#include "bdnav/sound_parse.h"
#include "hdmv/hdmv_vm.h"
#include "hdmv/mobj_parse.h"
#include "decoders/graphics_controller.h"
#include "decoders/hdmv_pids.h"
#include "decoders/m2ts_filter.h"
#include "decoders/overlay.h"
#include "disc/disc.h"
#include "disc/enc_info.h"
#include "file/file.h"
#ifdef USING_BDJAVA
#include "bdj/bdj.h"
#include "bdj/bdjo_parse.h"
#endif

#include <stdio.h> // SEEK_
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>


typedef enum {
    title_undef = 0,
    title_hdmv,
    title_bdj,
} BD_TITLE_TYPE;

typedef struct {
    /* current clip */
    NAV_CLIP       *clip;
    BD_FILE_H      *fp;
    uint64_t       clip_size;
    uint64_t       clip_block_pos;
    uint64_t       clip_pos;

    /* current aligned unit */
    uint16_t       int_buf_off;

    /* current stream UO mask (combined from playlist and current clip UO masks) */
    BD_UO_MASK     uo_mask;

    /* internally handled pids */
    uint16_t        ig_pid; /* pid of currently selected IG stream */
    uint16_t        pg_pid; /* pid of currently selected PG stream */

    /* */
    uint8_t         eof_hit;
    uint8_t         encrypted_block_cnt;
    uint8_t         seek_flag;  /* used to fine-tune first read after seek */

    M2TS_FILTER    *m2ts_filter;
} BD_STREAM;

typedef struct {
    NAV_CLIP *clip;
    size_t    clip_size;
    uint8_t  *buf;
} BD_PRELOAD;

struct bluray {

    BD_MUTEX          mutex;  /* protect API function access to internal data */

    /* current disc */
    BD_DISC          *disc;
    BLURAY_DISC_INFO  disc_info;
    BLURAY_TITLE    **titles;  /* titles from disc index */
    META_ROOT        *meta;
    NAV_TITLE_LIST   *title_list;

    /* current playlist */
    NAV_TITLE      *title;
    uint32_t       title_idx;
    uint64_t       s_pos;

    /* streams */
    BD_STREAM      st0;       /* main path */
    BD_PRELOAD     st_ig;     /* preloaded IG stream sub path */
    BD_PRELOAD     st_textst; /* preloaded TextST sub path */

    /* buffer for bd_read(): current aligned unit of main stream (st0) */
    uint8_t        int_buf[6144];

    /* seamless angle change request */
    int            seamless_angle_change;
    uint32_t       angle_change_pkt;
    uint32_t       angle_change_time;
    unsigned       request_angle;

    /* mark tracking */
    uint64_t       next_mark_pos;
    int            next_mark;

    /* player state */
    BD_REGISTERS   *regs;            /* player registers */
    BD_EVENT_QUEUE *event_queue;     /* navigation mode event queue */
    BD_UO_MASK      uo_mask;         /* Current UO mask */
    BD_UO_MASK      title_uo_mask;   /* UO mask from current .bdjo file or Movie Object */
    BD_TITLE_TYPE   title_type;      /* type of current title (in navigation mode) */
    /* Pending action after playlist end
     * BD-J: delayed sending of BDJ_EVENT_END_OF_PLAYLIST
     *       1 - message pending. 3 - message sent.
     */
    uint8_t         end_of_playlist; /* 1 - reached. 3 - processed . */
    uint8_t         app_scr;         /* 1 if application provides presentation timetamps */

    /* HDMV */
    HDMV_VM        *hdmv_vm;
    uint8_t         hdmv_suspended;

    /* BD-J */
#ifdef USING_BDJAVA
    BDJAVA         *bdjava;
    BDJ_STORAGE     bdjstorage;
    uint8_t         bdj_wait_start;  /* BD-J has selected playlist (prefetch) but not yet started playback */
#endif

    /* HDMV graphics */
    GRAPHICS_CONTROLLER *graphics_controller;
    SOUND_DATA          *sound_effects;
    BD_UO_MASK           gc_uo_mask;      /* UO mask from current menu page */
    uint32_t             gc_status;
    uint8_t              decode_pg;

    /* TextST */
    uint32_t gc_wakeup_time;  /* stream timestamp of next subtitle */
    uint64_t gc_wakeup_pos;   /* stream position of gc_wakeup_time */

    /* ARGB overlay output */
#ifdef USING_BDJAVA
    void                *argb_overlay_proc_handle;
    bd_argb_overlay_proc_f argb_overlay_proc;
    BD_ARGB_BUFFER      *argb_buffer;
    BD_MUTEX             argb_buffer_mutex;
#endif
};

/* Stream Packet Number = byte offset / 192. Avoid 64-bit division. */
#define SPN(pos) (((uint32_t)((pos) >> 6)) / 3)


/*
 * Library version
 */
void bd_get_version(int *major, int *minor, int *micro)
{
    *major = BLURAY_VERSION_MAJOR;
    *minor = BLURAY_VERSION_MINOR;
    *micro = BLURAY_VERSION_MICRO;
}

/*
 * Navigation mode event queue
 */

static int _get_event(BLURAY *bd, BD_EVENT *ev)
{
    int result = event_queue_get(bd->event_queue, ev);
    if (!result) {
        ev->event = BD_EVENT_NONE;
    }
    return result;
}

static int _queue_event(BLURAY *bd, uint32_t event, uint32_t param)
{
    int result = 0;
    if (bd->event_queue) {
        BD_EVENT ev = { event, param };
        result = event_queue_put(bd->event_queue, &ev);
        if (!result) {
            BD_DEBUG(DBG_BLURAY|DBG_CRIT, "_queue_event(%d, %d): queue overflow !\n", event, param);
        }
    }
    return result;
}

/*
 * PSR utils
 */

static void _update_time_psr(BLURAY *bd, uint32_t time)
{
    /*
     * Update PSR8: Presentation Time
     * The PSR8 represents presentation time in the playing interval from IN_time until OUT_time of
     * the current PlayItem, measured in units of a 45 kHz clock.
     */

    if (!bd->title || !bd->st0.clip) {
        return;
    }
    if (time < bd->st0.clip->in_time) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_update_time_psr(): timestamp before clip start\n");
        return;
    }
    if (time > bd->st0.clip->out_time) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_update_time_psr(): timestamp after clip end\n");
        return;
    }

    bd_psr_write(bd->regs, PSR_TIME, time);
}

static uint32_t _update_time_psr_from_stream(BLURAY *bd)
{
    /* update PSR_TIME from stream. Not real presentation time (except when seeking), but near enough. */
    NAV_CLIP *clip = bd->st0.clip;

    if (bd->title && clip) {

        uint32_t clip_pkt, clip_time;
        nav_clip_packet_search(bd->st0.clip, SPN(bd->st0.clip_pos), &clip_pkt, &clip_time);
        if (clip_time >= clip->in_time && clip_time <= clip->out_time) {
            _update_time_psr(bd, clip_time);
            return clip_time;
        } else {
            BD_DEBUG(DBG_BLURAY|DBG_CRIT, "%s: no timestamp for SPN %u (got %u). clip %u-%u.\n",
                     clip->name, SPN(bd->st0.clip_pos), clip_time, clip->in_time, clip->out_time);
        }
    }

    return 0;
}

static void _update_stream_psr_by_lang(BD_REGISTERS *regs,
                                       uint32_t psr_lang, uint32_t psr_stream,
                                       uint32_t enable_flag,
                                       MPLS_STREAM *streams, unsigned num_streams,
                                       uint32_t *lang, uint32_t blacklist)
{
    uint32_t preferred_lang;
    int      stream_idx = -1;
    unsigned ii;
    uint32_t stream_lang = 0;

    /* get preferred language */
    preferred_lang = bd_psr_read(regs, psr_lang);

    /* find stream */
    for (ii = 0; ii < num_streams; ii++) {
        if (preferred_lang == str_to_uint32((const char *)streams[ii].lang, 3)) {
            stream_idx = ii;
            break;
        }
    }

    /* requested language not found ? */
    if (stream_idx < 0) {
        BD_DEBUG(DBG_BLURAY, "Stream with preferred language not found\n");
        /* select first stream */
        stream_idx = 0;
        /* no subtitles if preferred language not found */
        enable_flag = 0;
    }

    stream_lang = str_to_uint32((const char *)streams[stream_idx].lang, 3);

    /* avoid enabling subtitles if audio is in the same language */
    if (blacklist && blacklist == stream_lang) {
        enable_flag = 0;
        BD_DEBUG(DBG_BLURAY, "Subtitles disabled (audio is in the same language)\n");
    }

    if (lang) {
        *lang = stream_lang;
    }

    /* update PSR */

    BD_DEBUG(DBG_BLURAY, "Selected stream %d (language %s)\n", stream_idx, streams[stream_idx].lang);

    bd_psr_write_bits(regs, psr_stream,
                      (stream_idx + 1) | enable_flag,
                      0x80000fff);
}

static void _update_clip_psrs(BLURAY *bd, NAV_CLIP *clip)
{
    MPLS_STN *stn = &clip->title->pl->play_item[clip->ref].stn;
    uint32_t audio_lang = 0;

    bd_psr_write(bd->regs, PSR_PLAYITEM, clip->ref);
    bd_psr_write(bd->regs, PSR_TIME,     clip->in_time);

    /* Update selected audio and subtitle stream PSRs when not using menus.
     * Selection is based on language setting PSRs and clip STN.
     */
    if (bd->title_type == title_undef) {

        if (stn->num_audio) {
            _update_stream_psr_by_lang(bd->regs,
                                       PSR_AUDIO_LANG, PSR_PRIMARY_AUDIO_ID, 0,
                                       stn->audio, stn->num_audio,
                                       &audio_lang, 0);
        }

        if (stn->num_pg) {
            _update_stream_psr_by_lang(bd->regs,
                                       PSR_PG_AND_SUB_LANG, PSR_PG_STREAM, 0x80000000,
                                       stn->pg, stn->num_pg,
                                       NULL, audio_lang);
        }

    /* Validate selected audio, subtitle and IG stream PSRs when using menus */
    } else {
        uint32_t psr_val;

        if (stn->num_audio) {
            bd_psr_lock(bd->regs);
            psr_val = bd_psr_read(bd->regs, PSR_PRIMARY_AUDIO_ID);
            if (psr_val == 0 || psr_val > stn->num_audio) {
                _update_stream_psr_by_lang(bd->regs,
                                           PSR_AUDIO_LANG, PSR_PRIMARY_AUDIO_ID, 0,
                                           stn->audio, stn->num_audio,
                                           &audio_lang, 0);
            } else {
                audio_lang = str_to_uint32((const char *)stn->audio[psr_val - 1].lang, 3);
            }
            bd_psr_unlock(bd->regs);
        }
        if (stn->num_pg) {
            bd_psr_lock(bd->regs);
            psr_val = bd_psr_read(bd->regs, PSR_PG_STREAM) & 0xfff;
            if ((psr_val == 0) || (psr_val > stn->num_pg)) {
                _update_stream_psr_by_lang(bd->regs,
                                           PSR_PG_AND_SUB_LANG, PSR_PG_STREAM, 0x80000000,
                                           stn->pg, stn->num_pg,
                                           NULL, audio_lang);
            }
            bd_psr_unlock(bd->regs);
        }
        if (stn->num_ig) {
            bd_psr_lock(bd->regs);
            psr_val = bd_psr_read(bd->regs, PSR_IG_STREAM_ID);
            if ((psr_val == 0) || (psr_val > stn->num_ig)) {
                bd_psr_write(bd->regs, PSR_IG_STREAM_ID, 1);
                BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Selected IG stream 1 (stream %d not available)\n", psr_val);
            }
            bd_psr_unlock(bd->regs);
        }
    }
}

static int _is_interactive_title(BLURAY *bd)
{
    if (bd->titles && bd->title_type != title_undef) {
        unsigned title = bd_psr_read(bd->regs, PSR_TITLE_NUMBER);
        if (title == 0xffff && bd->disc_info.first_play->interactive) {
            return 1;
        }
        if (title <= bd->disc_info.num_titles && bd->titles[title]) {
            return bd->titles[title]->interactive;
        }
    }
    return 0;
}

static void _update_chapter_psr(BLURAY *bd)
{
    if (!_is_interactive_title(bd) && bd->title->chap_list.count > 0) {
        uint32_t current_chapter = bd_get_current_chapter(bd);
        bd_psr_write(bd->regs, PSR_CHAPTER,  current_chapter + 1);
    }
}

/*
 * PG
 */

static int _find_pg_stream(BLURAY *bd, uint16_t *pid, int *sub_path_idx, unsigned *sub_clip_idx, uint8_t *char_code)
{
    unsigned  main_clip_idx = bd->st0.clip ? bd->st0.clip->ref : 0;
    MPLS_PI  *pi        = &bd->title->pl->play_item[main_clip_idx];
    unsigned  pg_stream = bd_psr_read(bd->regs, PSR_PG_STREAM);

#if 0
    /* Enable decoder unconditionally (required for forced subtitles).
       Display flag is checked in graphics controller. */
    /* check PG display flag from PSR */
    if (!(pg_stream & 0x80000000)) {
      return 0;
    }
#endif

    pg_stream &= 0xfff;

    if (pg_stream > 0 && pg_stream <= pi->stn.num_pg) {
        pg_stream--; /* stream number to table index */
        if (pi->stn.pg[pg_stream].stream_type == 2) {
            *sub_path_idx = pi->stn.pg[pg_stream].subpath_id;
            *sub_clip_idx = pi->stn.pg[pg_stream].subclip_id;
        }
        *pid = pi->stn.pg[pg_stream].pid;

        if (char_code && pi->stn.pg[pg_stream].coding_type == BLURAY_STREAM_TYPE_SUB_TEXT) {
            *char_code = pi->stn.pg[pg_stream].char_code;
        }

        BD_DEBUG(DBG_BLURAY, "_find_pg_stream(): current PG stream pid 0x%04x sub-path %d\n",
              *pid, *sub_path_idx);
        return 1;
    }

    return 0;
}

static int _init_pg_stream(BLURAY *bd)
{
    int      pg_subpath = -1;
    unsigned pg_subclip = 0;
    uint16_t pg_pid     = 0;

    bd->st0.pg_pid = 0;

    if (!bd->graphics_controller) {
        return 0;
    }

    /* reset PG decoder and controller */
    gc_run(bd->graphics_controller, GC_CTRL_PG_RESET, 0, NULL);

    if (!bd->decode_pg || !bd->title) {
        return 0;
    }

    _find_pg_stream(bd, &pg_pid, &pg_subpath, &pg_subclip, NULL);

    /* store PID of main path embedded PG stream */
    if (pg_subpath < 0) {
        bd->st0.pg_pid = pg_pid;
        return !!pg_pid;
    }

    return 0;
}

static void _update_textst_timer(BLURAY *bd)
{
    if (bd->st_textst.clip) {
        if (bd->st0.clip_block_pos >= bd->gc_wakeup_pos) {
            GC_NAV_CMDS cmds = {-1, NULL, -1, 0, 0, EMPTY_UO_MASK};

            gc_run(bd->graphics_controller, GC_CTRL_PG_UPDATE, bd->gc_wakeup_time, &cmds);

            bd->gc_wakeup_time = cmds.wakeup_time;
            bd->gc_wakeup_pos = (uint64_t)(int64_t)-1; /* no wakeup */

            /* next event in this clip ? */
            if (cmds.wakeup_time >= bd->st0.clip->in_time && cmds.wakeup_time < bd->st0.clip->out_time) {
                /* find event position in main path clip */
                NAV_CLIP *clip = bd->st0.clip;
                if (clip->cl) {
                    uint32_t spn = clpi_lookup_spn(clip->cl, cmds.wakeup_time, /*before=*/1,
                                                   bd->title->pl->play_item[clip->ref].clip[clip->angle].stc_id);
                    if (spn) {
                        bd->gc_wakeup_pos = (uint64_t)spn * 192L;
                  }
                }
            }
        }
    }
}

static void _init_textst_timer(BLURAY *bd)
{
    if (bd->st_textst.clip && bd->st0.clip->cl) {
        uint32_t clip_time;
        clpi_access_point(bd->st0.clip->cl, SPN(bd->st0.clip_block_pos), /*next=*/0, /*angle_change=*/0, &clip_time);
        bd->gc_wakeup_time = clip_time;
        bd->gc_wakeup_pos = 0;
        _update_textst_timer(bd);
    }
}

/*
 * UO mask
 */

static uint32_t _compressed_mask(BD_UO_MASK mask)
{
    return mask.menu_call | (mask.title_search << 1);
}

static void _update_uo_mask(BLURAY *bd)
{
    BD_UO_MASK old_mask = bd->uo_mask;
    BD_UO_MASK new_mask;

    new_mask = bd_uo_mask_combine(bd->title_uo_mask, bd->st0.uo_mask);
    new_mask = bd_uo_mask_combine(bd->gc_uo_mask,    new_mask);
    if (_compressed_mask(old_mask) != _compressed_mask(new_mask)) {
        _queue_event(bd, BD_EVENT_UO_MASK_CHANGED, _compressed_mask(new_mask));
    }
    bd->uo_mask = new_mask;
}

#ifdef USING_BDJAVA
void bd_set_bdj_uo_mask(BLURAY *bd, unsigned mask)
{
    bd->title_uo_mask.title_search = !!(mask & BDJ_TITLE_SEARCH_MASK);
    bd->title_uo_mask.menu_call    = !!(mask & BDJ_MENU_CALL_MASK);

    _update_uo_mask(bd);
}
#endif

static void _update_hdmv_uo_mask(BLURAY *bd)
{
    uint32_t mask = hdmv_vm_get_uo_mask(bd->hdmv_vm);
    bd->title_uo_mask.title_search = !!(mask & HDMV_TITLE_SEARCH_MASK);
    bd->title_uo_mask.menu_call    = !!(mask & HDMV_MENU_CALL_MASK);

    _update_uo_mask(bd);
}


/*
 * clip access (BD_STREAM)
 */

static void _close_m2ts(BD_STREAM *st)
{
    if (st->fp != NULL) {
        file_close(st->fp);
        st->fp = NULL;
    }

    m2ts_filter_close(&st->m2ts_filter);
}

static int _open_m2ts(BLURAY *bd, BD_STREAM *st)
{
    _close_m2ts(st);

    if (!st->clip) {
        return 0;
    }

    st->fp = disc_open_stream(bd->disc, st->clip->name);

    st->clip_size = 0;
    st->clip_pos = (uint64_t)st->clip->start_pkt * 192;
    st->clip_block_pos = (st->clip_pos / 6144) * 6144;
    st->eof_hit = 0;
    st->encrypted_block_cnt = 0;

    if (st->fp) {
        int64_t clip_size = file_size(st->fp);
        if (clip_size > 0) {

            if (file_seek(st->fp, st->clip_block_pos, SEEK_SET) < 0) {
                BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to seek clip %s!\n", st->clip->name);
                _close_m2ts(st);
                return 0;
            }

            st->clip_size   = clip_size;
            st->int_buf_off = 6144;

            if (st == &bd->st0) {
                MPLS_PL *pl = st->clip->title->pl;
                MPLS_STN *stn = &pl->play_item[st->clip->ref].stn;

                st->uo_mask = bd_uo_mask_combine(pl->app_info.uo_mask,
                                                 pl->play_item[st->clip->ref].uo_mask);
                _update_uo_mask(bd);

                st->m2ts_filter = m2ts_filter_init((int64_t)st->clip->in_time << 1,
                                                   (int64_t)st->clip->out_time << 1,
                                                   stn->num_video, stn->num_audio,
                                                   stn->num_ig, stn->num_pg);

                _update_clip_psrs(bd, st->clip);

                _init_pg_stream(bd);

                _init_textst_timer(bd);
            }

            return 1;
        }

        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Clip %s empty!\n", st->clip->name);
        _close_m2ts(st);
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open clip %s!\n", st->clip->name);

    return 0;
}

static int _validate_unit(BLURAY *bd, BD_STREAM *st, uint8_t *buf)
{
    /* Check TP_extra_header Copy_permission_indicator. If != 0, unit may be encrypted. */
    /* Check first sync byte. It should never be encrypted. */
    if (BD_UNLIKELY(buf[0] & 0xc0 || buf[4] != 0x47)) {

        /* Check first sync bytes. If not OK, drop unit. */
        if (buf[4] != 0x47 || buf [4 + 192] != 0x47 || buf[4 + 2*192] != 0x47 || buf[4 + 3*192] != 0x47) {

            /* Some streams have Copy_permission_indicator incorrectly set. */
            /* Check first TS sync byte. If unit is encrypted, first 16 bytes are plain, rest not. */
            /* not 100% accurate (can be random data too). But the unit is broken anyway ... */
            if (buf[4] == 0x47) {

                /* most likely encrypted stream. Check couple of blocks before erroring out. */
                st->encrypted_block_cnt++;

                if (st->encrypted_block_cnt > 10) {
                    /* error out */
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "TP header copy permission indicator != 0. Stream seems to be encrypted.\n");
                    _queue_event(bd, BD_EVENT_ENCRYPTED, BD_ERROR_AACS);
                    return -1;
                }
            }

            /* broken block, ignore it */
            _queue_event(bd, BD_EVENT_READ_ERROR, 1);
            return 0;
        }
    }

    st->eof_hit = 0;
    st->encrypted_block_cnt = 0;
    return 1;
}

static int _skip_unit(BLURAY *bd, BD_STREAM *st)
{
    const size_t len = 6144;

    /* skip broken unit */
    st->clip_block_pos += len;
    st->clip_pos += len;

    _queue_event(bd, BD_EVENT_READ_ERROR, 0);

    /* seek to next unit start */
    if (file_seek(st->fp, st->clip_block_pos, SEEK_SET) < 0) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to seek clip %s!\n", st->clip->name);
        return -1;
    }

    return 0;
}

static int _read_block(BLURAY *bd, BD_STREAM *st, uint8_t *buf)
{
    const size_t len = 6144;

    if (st->fp) {
        BD_DEBUG(DBG_STREAM, "Reading unit at %"PRIu64"...\n", st->clip_block_pos);

        if (len + st->clip_block_pos <= st->clip_size) {
            size_t read_len;

            if ((read_len = file_read(st->fp, buf, len))) {
                int error;

                if (read_len != len) {
                    BD_DEBUG(DBG_STREAM | DBG_CRIT, "Read %d bytes at %"PRIu64" ; requested %d !\n", (int)read_len, st->clip_block_pos, (int)len);
                    return _skip_unit(bd, st);
                }
                st->clip_block_pos += len;

                if ((error = _validate_unit(bd, st, buf)) <= 0) {
                    /* skip broken unit */
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Skipping broken unit at %"PRId64"\n", st->clip_block_pos - len);
                    st->clip_pos += len;
                    return error;
                }

                if (st->m2ts_filter) {
                    int result = m2ts_filter(st->m2ts_filter, buf);
                    if (result < 0) {
                        m2ts_filter_close(&st->m2ts_filter);
                        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "m2ts filter error\n");
                    }
                }

                BD_DEBUG(DBG_STREAM, "Read unit OK!\n");

#ifdef BLURAY_READ_ERROR_TEST
                /* simulate broken blocks */
                if (random() % 1000)
#else
                return 1;
#endif
            }

            BD_DEBUG(DBG_STREAM | DBG_CRIT, "Read unit at %"PRIu64" failed !\n", st->clip_block_pos);

            return _skip_unit(bd, st);
        }

        /* This is caused by truncated .m2ts file or invalid clip length.
         *
         * Increase position to avoid infinite loops.
         * Next clip won't be selected until all packets of this clip have been read.
         */
        st->clip_block_pos += len;
        st->clip_pos += len;

        if (!st->eof_hit) {
            BD_DEBUG(DBG_STREAM | DBG_CRIT, "Read past EOF !\n");
            st->eof_hit = 1;
        }

        return 0;
    }

    BD_DEBUG(DBG_BLURAY, "No valid title selected!\n");

    return -1;
}

/*
 * clip preload (BD_PRELOAD)
 */

static void _close_preload(BD_PRELOAD *p)
{
    X_FREE(p->buf);
    memset(p, 0, sizeof(*p));
}

#define PRELOAD_SIZE_LIMIT  (512*1024*1024)  /* do not preload clips larger than 512M */

static int _preload_m2ts(BLURAY *bd, BD_PRELOAD *p)
{
    /* setup and open BD_STREAM */

    BD_STREAM st;

    memset(&st, 0, sizeof(st));
    st.clip = p->clip;

    if (st.clip_size > PRELOAD_SIZE_LIMIT) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "_preload_m2ts(): too large clip (%"PRId64")\n", st.clip_size);
        return 0;
    }

    if (!_open_m2ts(bd, &st)) {
        return 0;
    }

    /* allocate buffer */
    p->clip_size = (size_t)st.clip_size;
    uint8_t* tmp = (uint8_t*)realloc(p->buf, p->clip_size);
    if (!tmp) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_preload_m2ts(): out of memory\n");
        _close_m2ts(&st);
        _close_preload(p);
        return 0;
    }

    p->buf = tmp;

    /* read clip to buffer */

    uint8_t *buf = p->buf;
    uint8_t *end = p->buf + p->clip_size;

    for (; buf < end; buf += 6144) {
        if (_read_block(bd, &st, buf) <= 0) {
            BD_DEBUG(DBG_BLURAY|DBG_CRIT, "_preload_m2ts(): error loading %s at %"PRIu64"\n",
                  st.clip->name, (uint64_t)(buf - p->buf));
            _close_m2ts(&st);
            _close_preload(p);
            return 0;
        }
    }

    /* */

    BD_DEBUG(DBG_BLURAY, "_preload_m2ts(): loaded %"PRIu64" bytes from %s\n",
          st.clip_size, st.clip->name);

    _close_m2ts(&st);

    return 1;
}

static int64_t _seek_stream(BLURAY *bd, BD_STREAM *st,
                            NAV_CLIP *clip, uint32_t clip_pkt)
{
    if (!clip)
        return -1;

    if (!st->fp || !st->clip || clip->ref != st->clip->ref) {
        // The position is in a new clip
        st->clip = clip;
        if (!_open_m2ts(bd, st)) {
            return -1;
        }
    }

    if (st->m2ts_filter) {
        m2ts_filter_seek(st->m2ts_filter, 0, (int64_t)st->clip->in_time << 1);
    }

    st->clip_pos = (uint64_t)clip_pkt * 192;
    st->clip_block_pos = (st->clip_pos / 6144) * 6144;

    if (file_seek(st->fp, st->clip_block_pos, SEEK_SET) < 0) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to seek clip %s!\n", st->clip->name);
    }

    st->int_buf_off = 6144;
    st->seek_flag = 1;

    return st->clip_pos;
}

/*
 * Graphics controller interface
 */

static int _run_gc(BLURAY *bd, gc_ctrl_e msg, uint32_t param)
{
    int result = -1;

    if (!bd) {
        return -1;
    }

    if (bd->graphics_controller && bd->hdmv_vm) {
        GC_NAV_CMDS cmds = {-1, NULL, -1, 0, 0, EMPTY_UO_MASK};

        result = gc_run(bd->graphics_controller, msg, param, &cmds);

        if (cmds.num_nav_cmds > 0) {
            hdmv_vm_set_object(bd->hdmv_vm, cmds.num_nav_cmds, cmds.nav_cmds);
            bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
        }

        if (cmds.status != bd->gc_status) {
            uint32_t changed_flags = cmds.status ^ bd->gc_status;
            bd->gc_status = cmds.status;
            if (changed_flags & GC_STATUS_MENU_OPEN) {
                _queue_event(bd, BD_EVENT_MENU, !!(bd->gc_status & GC_STATUS_MENU_OPEN));
            }
            if (changed_flags & GC_STATUS_POPUP) {
                _queue_event(bd, BD_EVENT_POPUP, !!(bd->gc_status & GC_STATUS_POPUP));
            }
        }

        if (cmds.sound_id_ref >= 0 && cmds.sound_id_ref < 0xff) {
            _queue_event(bd, BD_EVENT_SOUND_EFFECT, cmds.sound_id_ref);
        }

        bd->gc_uo_mask = cmds.page_uo_mask;
        _update_uo_mask(bd);

    } else {
        if (bd->gc_status & GC_STATUS_MENU_OPEN) {
            _queue_event(bd, BD_EVENT_MENU, 0);
        }
        if (bd->gc_status & GC_STATUS_POPUP) {
            _queue_event(bd, BD_EVENT_POPUP, 0);
        }
        bd->gc_status = GC_STATUS_NONE;
    }

    return result;
}

/*
 * meta open
 */

static int _meta_open(BLURAY *bd)
{
    if (!bd->meta) {
        bd->meta = meta_parse(bd->disc);
    }

    return !!bd->meta;
}

/*
 * disc info
 */

const BLURAY_DISC_INFO *bd_get_disc_info(BLURAY *bd)
{
    return &bd->disc_info;
}

static void _fill_disc_info(BLURAY *bd, BD_ENC_INFO *enc_info)
{
    bd->disc_info.aacs_detected      = enc_info->aacs_detected;
    bd->disc_info.libaacs_detected   = enc_info->libaacs_detected;
    bd->disc_info.aacs_error_code    = enc_info->aacs_error_code;
    bd->disc_info.aacs_handled       = enc_info->aacs_handled;
    bd->disc_info.aacs_mkbv          = enc_info->aacs_mkbv;
    memcpy(bd->disc_info.disc_id, enc_info->disc_id, 20);
    bd->disc_info.bdplus_detected    = enc_info->bdplus_detected;
    bd->disc_info.libbdplus_detected = enc_info->libbdplus_detected;
    bd->disc_info.bdplus_handled     = enc_info->bdplus_handled;
    bd->disc_info.bdplus_gen         = enc_info->bdplus_gen;
    bd->disc_info.bdplus_date        = enc_info->bdplus_date;
    bd->disc_info.no_menu_support    = enc_info->no_menu_support;

    bd->disc_info.udf_volume_id      = disc_volume_id(bd->disc);

    bd->disc_info.bluray_detected        = 0;
    bd->disc_info.top_menu_supported     = 0;
    bd->disc_info.first_play_supported   = 0;
    bd->disc_info.num_hdmv_titles        = 0;
    bd->disc_info.num_bdj_titles         = 0;
    bd->disc_info.num_unsupported_titles = 0;

    bd->disc_info.bdj_detected    = 0;
    bd->disc_info.bdj_supported   = 0;
    bd->disc_info.libjvm_detected = 0;
    bd->disc_info.bdj_handled     = 0;

    bd->disc_info.num_titles  = 0;
    bd->disc_info.titles      = NULL;
    bd->disc_info.top_menu    = NULL;
    bd->disc_info.first_play  = NULL;

    array_free((void**)&bd->titles);

    memset(bd->disc_info.bdj_org_id,  0, sizeof(bd->disc_info.bdj_org_id));
    memset(bd->disc_info.bdj_disc_id, 0, sizeof(bd->disc_info.bdj_disc_id));

    INDX_ROOT *index = indx_get(bd->disc);
    if (index) {
        INDX_PLAY_ITEM *pi;
        unsigned        ii;

        bd->disc_info.bluray_detected = 1;

        /* application info */
        bd->disc_info.video_format     = index->app_info.video_format;
        bd->disc_info.frame_rate       = index->app_info.frame_rate;
        bd->disc_info.content_exist_3D = index->app_info.content_exist_flag;
        bd->disc_info.initial_output_mode_preference = index->app_info.initial_output_mode_preference;
        memcpy(bd->disc_info.provider_data, index->app_info.user_data, sizeof(bd->disc_info.provider_data));

        /* allocate array for title info */
        BLURAY_TITLE **titles = (BLURAY_TITLE**)array_alloc(index->num_titles + 2, sizeof(BLURAY_TITLE));
        if (!titles) {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't allocate memory\n");
            return;
        }
        bd->titles = titles;
        bd->disc_info.titles = (const BLURAY_TITLE * const *)titles;
        bd->disc_info.num_titles = index->num_titles;

        /* count titles and fill title info */

        for (ii = 0; ii < index->num_titles; ii++) {
            if (index->titles[ii].object_type == indx_object_type_hdmv) {
                bd->disc_info.num_hdmv_titles++;
                titles[ii + 1]->interactive = (index->titles[ii].hdmv.playback_type == indx_hdmv_playback_type_interactive);
                titles[ii + 1]->id_ref = index->titles[ii].hdmv.id_ref;
            }
            if (index->titles[ii].object_type == indx_object_type_bdj) {
                bd->disc_info.num_bdj_titles++;
                bd->disc_info.bdj_detected = 1;
                titles[ii + 1]->bdj = 1;
                titles[ii + 1]->interactive = (index->titles[ii].bdj.playback_type == indx_bdj_playback_type_interactive);
                titles[ii + 1]->id_ref = atoi(index->titles[ii].bdj.name);
            }

            titles[ii + 1]->accessible =  !(index->titles[ii].access_type & INDX_ACCESS_PROHIBITED_MASK);
            titles[ii + 1]->hidden     = !!(index->titles[ii].access_type & INDX_ACCESS_HIDDEN_MASK);
        }

        pi = &index->first_play;
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.bdj_detected = 1;
            titles[index->num_titles + 1]->bdj = 1;
            titles[index->num_titles + 1]->interactive = (pi->bdj.playback_type == indx_bdj_playback_type_interactive);
            titles[index->num_titles + 1]->id_ref = atoi(pi->bdj.name);
        }
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            titles[index->num_titles + 1]->interactive = (pi->hdmv.playback_type == indx_hdmv_playback_type_interactive);
            titles[index->num_titles + 1]->id_ref = pi->hdmv.id_ref;
        }

        pi = &index->top_menu;
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.bdj_detected = 1;
            titles[0]->bdj = 1;
            titles[0]->interactive = (pi->bdj.playback_type == indx_bdj_playback_type_interactive);
            titles[0]->id_ref = atoi(pi->bdj.name);
        }
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            titles[0]->interactive = (pi->hdmv.playback_type == indx_hdmv_playback_type_interactive);
            titles[0]->id_ref = pi->hdmv.id_ref;
        }

        /* check for BD-J capability */

#ifdef USING_BDJAVA
        if (bd->disc_info.bdj_detected) {
            bd->disc_info.bdj_supported = 1;
            /* BD-J titles found. Check if jvm + jar can be loaded ? */
            switch (bdj_jvm_available(&bd->bdjstorage)) {
                case 2: bd->disc_info.bdj_handled     = 1;
                case 1: bd->disc_info.libjvm_detected = 1;
                default:;
            }
        }
#endif /* USING_BDJAVA */

        /* mark supported titles */

        if (bd->disc_info.bdj_detected && !bd->disc_info.bdj_handled) {
            bd->disc_info.num_unsupported_titles = bd->disc_info.num_bdj_titles;
        }

        pi = &index->first_play;
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            bd->disc_info.first_play_supported = 1;
        }
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.first_play_supported = bd->disc_info.bdj_handled;
        }

        pi = &index->top_menu;
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            bd->disc_info.top_menu_supported = 1;
        }
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.top_menu_supported = bd->disc_info.bdj_handled;
        }

        /* */

        if (bd->disc_info.first_play_supported) {
            titles[index->num_titles + 1]->accessible = 1;
            bd->disc_info.first_play = titles[index->num_titles + 1];
        }
        if (bd->disc_info.top_menu_supported) {
            titles[0]->accessible = 1;
            bd->disc_info.top_menu = titles[0];
        }

        /* populate title names */
        bd_get_meta(bd);

        indx_free(&index);
    }

#if 0
    if (!bd->disc_info.first_play_supported || !bd->disc_info.top_menu_supported) {
        bd->disc_info.no_menu_support = 1;
    }
#endif

    if (bd->disc_info.bdj_detected) {
        BDID_DATA *bdid = bdid_get(bd->disc); /* parse id.bdmv */
        if (bdid) {
            memcpy(bd->disc_info.bdj_org_id,  bdid->org_id,  sizeof(bd->disc_info.bdj_org_id));
            memcpy(bd->disc_info.bdj_disc_id, bdid->disc_id, sizeof(bd->disc_info.bdj_disc_id));
            bdid_free(&bdid);
        }
    }
}

/*
 * bdj
 */

#ifdef USING_BDJAVA
const uint8_t *bd_get_aacs_data(BLURAY *bd, int type)
{
    return disc_get_data(bd->disc, type);
}
#endif

#ifdef USING_BDJAVA
uint64_t bd_get_uo_mask(BLURAY *bd)
{
    /* internal function. Used by BD-J. */
    union {
      uint64_t u64;
      BD_UO_MASK mask;
    } mask = {0};

    //bd_mutex_lock(&bd->mutex);
    memcpy(&mask.mask, &bd->uo_mask, sizeof(BD_UO_MASK));
    //bd_mutex_unlock(&bd->mutex);

    return mask.u64;
}
#endif

#ifdef USING_BDJAVA
void bd_set_bdj_kit(BLURAY *bd, int mask)
{
    _queue_event(bd, BD_EVENT_KEY_INTEREST_TABLE, mask);
}
#endif

#ifdef USING_BDJAVA
void bd_select_rate(BLURAY *bd, float rate, int reason)
{
    if (reason == BDJ_PLAYBACK_STOP) {
        /* playback stop. Might want to wait for buffers empty here. */
        return;
    }

    if (reason == BDJ_PLAYBACK_START) {
        /* playback is triggered by bd_select_rate() */
        bd->bdj_wait_start = 0;
    }

    if (rate < 0.5) {
        _queue_event(bd, BD_EVENT_STILL, 1);
    } else {
        _queue_event(bd, BD_EVENT_STILL, 0);
    }
}
#endif

#ifdef USING_BDJAVA
int bd_set_virtual_package(BLURAY *bd, const char *vp_path, int psr_init_backup)
{
    bd_mutex_lock(&bd->mutex);

    if (bd->title) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_set_virtual_package() failed: playlist is playing\n");
        return -1;
    }
    if (bd->title_type != title_bdj) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_set_virtual_package() failed: HDMV title\n");
        return -1;
    }

    if (psr_init_backup) {
        bd_psr_reset_backup_registers(bd->regs);
    }

    disc_update(bd->disc, vp_path);

    /* TODO: reload all cached information, update disc info, notify app */

    bd_mutex_unlock(&bd->mutex);

    return 0;
}
#endif

#ifdef USING_BDJAVA
BD_DISC *bd_get_disc(BLURAY *bd)
{
    return bd ? bd->disc : NULL;
}
#endif

#ifdef USING_BDJAVA
uint32_t bd_reg_read(BLURAY *bd, int psr, int reg)
{
    if (psr) {
        return bd_psr_read(bd->regs, reg);
    } else {
        return bd_gpr_read(bd->regs, reg);
    }
}
#endif

#ifdef USING_BDJAVA
int bd_reg_write(BLURAY *bd, int psr, int reg, uint32_t value, uint32_t psr_value_mask)
{
    if (psr) {
        if (psr < 102) {
            /* avoid deadlocks (psr_write triggers callbacks that may lock this mutex) */
            bd_mutex_lock(&bd->mutex);
        }
        int res = bd_psr_write_bits(bd->regs, reg, value, psr_value_mask);
        if (psr < 102) {
            bd_mutex_unlock(&bd->mutex);
        }
        return res;
    } else {
        return bd_gpr_write(bd->regs, reg, value);
    }
}
#endif

#ifdef USING_BDJAVA
BD_ARGB_BUFFER *bd_lock_osd_buffer(BLURAY *bd)
{
    bd_mutex_lock(&bd->argb_buffer_mutex);
    return bd->argb_buffer;
}

void bd_unlock_osd_buffer(BLURAY *bd)
{
    bd_mutex_unlock(&bd->argb_buffer_mutex);
}

/*
 * handle graphics updates from BD-J layer
 */
void bd_bdj_osd_cb(BLURAY *bd, const unsigned *img, int w, int h,
                   int x0, int y0, int x1, int y1)
{
    BD_ARGB_OVERLAY aov;

    if (!bd->argb_overlay_proc) {
        _queue_event(bd, BD_EVENT_MENU, 0);
        return;
    }

    memset(&aov, 0, sizeof(aov));
    aov.pts   = -1;
    aov.plane = BD_OVERLAY_IG;

    /* no image data -> init or close */
    if (!img) {
        if (w > 0 && h > 0) {
            aov.cmd = BD_ARGB_OVERLAY_INIT;
            aov.w   = w;
            aov.h   = h;
            _queue_event(bd, BD_EVENT_MENU, 1);
        } else {
            aov.cmd = BD_ARGB_OVERLAY_CLOSE;
            _queue_event(bd, BD_EVENT_MENU, 0);
        }

        bd->argb_overlay_proc(bd->argb_overlay_proc_handle, &aov);
        return;
    }

    /* no changed pixels ? */
    if (x1 < x0 || y1 < y0) {
        return;
    }

    /* pass only changed region */
    if (bd->argb_buffer && (bd->argb_buffer->width < w || bd->argb_buffer->height < h)) {
        aov.argb   = img;
    } else {
        aov.argb   = img + x0 + y0 * w;
    }
    aov.stride = w;
    aov.x      = x0;
    aov.y      = y0;
    aov.w      = x1 - x0 + 1;
    aov.h      = y1 - y0 + 1;

    if (bd->argb_buffer) {
        /* set dirty region */
        bd->argb_buffer->dirty[BD_OVERLAY_IG].x0 = x0;
        bd->argb_buffer->dirty[BD_OVERLAY_IG].x1 = x1;
        bd->argb_buffer->dirty[BD_OVERLAY_IG].y0 = y0;
        bd->argb_buffer->dirty[BD_OVERLAY_IG].y1 = y1;
    }

    /* draw */
    aov.cmd = BD_ARGB_OVERLAY_DRAW;
    bd->argb_overlay_proc(bd->argb_overlay_proc_handle, &aov);

    /* commit changes */
    aov.cmd = BD_ARGB_OVERLAY_FLUSH;
    bd->argb_overlay_proc(bd->argb_overlay_proc_handle, &aov);

    if (bd->argb_buffer) {
        /* reset dirty region */
        bd->argb_buffer->dirty[BD_OVERLAY_IG].x0 = bd->argb_buffer->width;
        bd->argb_buffer->dirty[BD_OVERLAY_IG].x1 = bd->argb_buffer->height;
        bd->argb_buffer->dirty[BD_OVERLAY_IG].y0 = 0;
        bd->argb_buffer->dirty[BD_OVERLAY_IG].y1 = 0;
    }
}
#endif

static int _start_bdj(BLURAY *bd, unsigned title)
{
#ifdef USING_BDJAVA
    if (bd->bdjava == NULL) {
        const char *root = disc_root(bd->disc);
        bd->bdjava = bdj_open(root, bd, bd->disc_info.bdj_disc_id, &bd->bdjstorage);
        if (!bd->bdjava) {
            return 0;
        }
    }

    return !bdj_process_event(bd->bdjava, BDJ_EVENT_START, title);
#else
    (void)bd;
    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Title %d: BD-J not compiled in\n", title);
    return 0;
#endif
}

#ifdef USING_BDJAVA
static int _bdj_event(BLURAY *bd, unsigned ev, unsigned param)
{
    if (bd->bdjava != NULL) {
        return bdj_process_event(bd->bdjava, ev, param);
    }
    return -1;
}
#else
#define _bdj_event(bd, ev, param) do{}while(0)
#endif

#ifdef USING_BDJAVA
static void _stop_bdj(BLURAY *bd)
{
    if (bd->bdjava != NULL) {
        bdj_process_event(bd->bdjava, BDJ_EVENT_STOP, 0);
        _queue_event(bd, BD_EVENT_STILL, 0);
        _queue_event(bd, BD_EVENT_KEY_INTEREST_TABLE, 0);
    }
}
#else
#define _stop_bdj(bd) do{}while(0)
#endif

#ifdef USING_BDJAVA
static void _close_bdj(BLURAY *bd)
{
    if (bd->bdjava != NULL) {
        bdj_close(bd->bdjava);
        bd->bdjava = NULL;
    }
}
#else
#define _close_bdj(bd) do{}while(0)
#endif

#ifdef USING_BDJAVA
static void _storage_free(BLURAY *bd)
{
    X_FREE(bd->bdjstorage.cache_root);
    X_FREE(bd->bdjstorage.persistent_root);
    X_FREE(bd->bdjstorage.classpath);
}
#else
#define _storage_free(bd) do{}while(0)
#endif

/*
 * open / close
 */

BLURAY *bd_init(void)
{
    char *env;

    BD_DEBUG(DBG_BLURAY, "libbluray version "BLURAY_VERSION_STRING"\n");

    BLURAY *bd = calloc(1, sizeof(BLURAY));

    if (!bd) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't allocate memory\n");
        return NULL;
    }

    bd->regs = bd_registers_init();
    if (!bd->regs) {
        BD_DEBUG(DBG_BLURAY, "bd_registers_init() failed\n");
        X_FREE(bd);
        return NULL;
    }

    bd_mutex_init(&bd->mutex);
#ifdef USING_BDJAVA
    bd_mutex_init(&bd->argb_buffer_mutex);

    env = getenv("LIBBLURAY_PERSISTENT_STORAGE");
    if (env) {
        int v = (!strcmp(env, "yes")) ? 1 : (!strcmp(env, "no")) ? 0 : atoi(env);
        bd->bdjstorage.no_persistent_storage = !v;
    }
#endif

    BD_DEBUG(DBG_BLURAY, "BLURAY initialized!\n");

    return bd;
}

static int _bd_open(BLURAY *bd,
                    const char *device_path, const char *keyfile_path,
                    fs_access *p_fs)
{
    BD_ENC_INFO enc_info;

    if (!bd) {
        return 0;
    }
    if (bd->disc) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Disc already open\n");
        return 0;
    }

    bd->disc = disc_open(device_path, p_fs,
                         &enc_info, keyfile_path,
                         (void*)bd->regs, (void*)bd_psr_read, (void*)bd_psr_write);

    if (!bd->disc) {
        return 0;
    }

    _fill_disc_info(bd, &enc_info);

    return bd->disc_info.bluray_detected;
}

int bd_open_disc(BLURAY *bd, const char *device_path, const char *keyfile_path)
{
    if (!device_path) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No device path provided!\n");
        return 0;
    }

    return _bd_open(bd, device_path, keyfile_path, NULL);
}

int bd_open_stream(BLURAY *bd,
                   void *read_blocks_handle,
                   int (*read_blocks)(void *handle, void *buf, int lba, int num_blocks))
{
    if (!read_blocks) {
        return 0;
    }

    fs_access fs = { read_blocks_handle, read_blocks, NULL, NULL };
    return _bd_open(bd, NULL, NULL, &fs);
}

int bd_open_files(BLURAY *bd,
                  void *handle,
                  struct bd_dir_s *(*open_dir)(void *handle, const char *rel_path),
                  struct bd_file_s *(*open_file)(void *handle, const char *rel_path))
{
    if (!open_dir || !open_file) {
        return 0;
    }

    fs_access fs = { handle, NULL, open_dir, open_file };
    return _bd_open(bd, NULL, NULL, &fs);
}

BLURAY *bd_open(const char *device_path, const char *keyfile_path)
{
    BLURAY *bd;

    bd = bd_init();
    if (!bd) {
        return NULL;
    }

    if (!bd_open_disc(bd, device_path, keyfile_path)) {
        bd_close(bd);
        return NULL;
    }

    return bd;
}

void bd_close(BLURAY *bd)
{
    if (!bd) {
        return;
    }

    _close_bdj(bd);

    _close_m2ts(&bd->st0);
    _close_preload(&bd->st_ig);
    _close_preload(&bd->st_textst);

    if (bd->title_list != NULL) {
        nav_free_title_list(bd->title_list);
    }
    if (bd->title != NULL) {
        nav_title_close(bd->title);
    }

    hdmv_vm_free(&bd->hdmv_vm);

    gc_free(&bd->graphics_controller);
    meta_free(&bd->meta);
    sound_free(&bd->sound_effects);
    bd_registers_free(bd->regs);

    event_queue_destroy(&bd->event_queue);
    array_free((void**)&bd->titles);
    _storage_free(bd);

    disc_close(&bd->disc);

    bd_mutex_destroy(&bd->mutex);
#ifdef USING_BDJAVA
    bd_mutex_destroy(&bd->argb_buffer_mutex);
#endif

    BD_DEBUG(DBG_BLURAY, "BLURAY destroyed!\n");

    X_FREE(bd);
}

/*
 * PlayMark tracking
 */

static void _find_next_playmark(BLURAY *bd)
{
    unsigned ii;

    bd->next_mark = -1;
    bd->next_mark_pos = (uint64_t)-1;
    for (ii = 0; ii < bd->title->mark_list.count; ii++) {
        uint64_t pos = (uint64_t)bd->title->mark_list.mark[ii].title_pkt * 192L;
        if (pos > bd->s_pos) {
            bd->next_mark = ii;
            bd->next_mark_pos = pos;
            break;
        }
    }

    _update_chapter_psr(bd);
}

static void _playmark_reached(BLURAY *bd)
{
    BD_DEBUG(DBG_BLURAY, "PlayMark %d reached (%"PRIu64")\n", bd->next_mark, bd->next_mark_pos);

    _queue_event(bd, BD_EVENT_PLAYMARK, bd->next_mark);
    _bdj_event(bd, BDJ_EVENT_MARK, bd->next_mark);

    /* update next mark */
    bd->next_mark++;
    if ((unsigned)bd->next_mark < bd->title->mark_list.count) {
        bd->next_mark_pos = (uint64_t)bd->title->mark_list.mark[bd->next_mark].title_pkt * 192L;
    } else {
        bd->next_mark = -1;
        bd->next_mark_pos = (uint64_t)-1;
    }

    /* chapter tracking */
    _update_chapter_psr(bd);
}

/*
 * seeking and current position
 */

static void _seek_internal(BLURAY *bd,
                           NAV_CLIP *clip, uint32_t title_pkt, uint32_t clip_pkt)
{
    if (_seek_stream(bd, &bd->st0, clip, clip_pkt) >= 0) {
        uint32_t media_time;

        /* update title position */
        bd->s_pos = (uint64_t)title_pkt * 192;

        /* Update PSR_TIME */
        media_time = _update_time_psr_from_stream(bd);

        /* emit notification events */
        if (media_time >= clip->in_time) {
            media_time = media_time - clip->in_time + clip->title_time;
        }
        _queue_event(bd, BD_EVENT_SEEK, media_time);
        _bdj_event(bd, BDJ_EVENT_SEEK, media_time);

        /* playmark tracking */
        _find_next_playmark(bd);

        /* reset PG decoder and controller */
        if (bd->graphics_controller) {
            gc_run(bd->graphics_controller, GC_CTRL_PG_RESET, 0, NULL);

            _init_textst_timer(bd);
        }

        BD_DEBUG(DBG_BLURAY, "Seek to %"PRIu64"\n", bd->s_pos);
    }
}

/* _change_angle() should be used only before call to _seek_internal() ! */
static void _change_angle(BLURAY *bd)
{
    if (bd->seamless_angle_change) {
        bd->st0.clip = nav_set_angle(bd->title, bd->st0.clip, bd->request_angle);
        bd->seamless_angle_change = 0;
        bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);

        /* force re-opening .m2ts file in _seek_internal() */
        _close_m2ts(&bd->st0);
    }
}

int64_t bd_seek_time(BLURAY *bd, uint64_t tick)
{
    uint32_t clip_pkt, out_pkt;
    NAV_CLIP *clip;

    if (tick >> 33) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_time(%"PRIu64") failed: invalid timestamp\n", tick);
        return bd->s_pos;
    }

    tick /= 2;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        tick < bd->title->duration) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_time_search(bd->title, (uint32_t)tick, &clip_pkt, &out_pkt);

        _seek_internal(bd, clip, out_pkt, clip_pkt);

    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_time(%u) failed\n", (unsigned int)tick);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

uint64_t bd_tell_time(BLURAY *bd)
{
    uint32_t clip_pkt = 0, out_pkt = 0, out_time = 0;
    NAV_CLIP *clip;

    if (!bd) {
        return 0;
    }

    bd_mutex_lock(&bd->mutex);

    if (bd->title) {
        clip = nav_packet_search(bd->title, SPN(bd->s_pos), &clip_pkt, &out_pkt, &out_time);
        if (clip) {
            out_time += clip->title_time;
        }
    }

    bd_mutex_unlock(&bd->mutex);

    return ((uint64_t)out_time) * 2;
}

int64_t bd_seek_chapter(BLURAY *bd, unsigned chapter)
{
    uint32_t clip_pkt, out_pkt;
    NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        chapter < bd->title->chap_list.count) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_chapter_search(bd->title, chapter, &clip_pkt, &out_pkt);

        _seek_internal(bd, clip, out_pkt, clip_pkt);

    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_chapter(%u) failed\n", chapter);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

int64_t bd_chapter_pos(BLURAY *bd, unsigned chapter)
{
    uint32_t clip_pkt, out_pkt;
    int64_t ret = -1;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        chapter < bd->title->chap_list.count) {

        // Find the closest access unit to the requested position
        nav_chapter_search(bd->title, chapter, &clip_pkt, &out_pkt);
        ret = (int64_t)out_pkt * 192;
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

uint32_t bd_get_current_chapter(BLURAY *bd)
{
    uint32_t ret = 0;

    bd_mutex_lock(&bd->mutex);

    if (bd->title) {
        ret = nav_chapter_get_current(bd->title, SPN(bd->s_pos));
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

int64_t bd_seek_playitem(BLURAY *bd, unsigned clip_ref)
{
    uint32_t clip_pkt, out_pkt;
    NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        clip_ref < bd->title->clip_list.count) {

      _change_angle(bd);

      clip     = &bd->title->clip_list.clip[clip_ref];
      clip_pkt = clip->start_pkt;
      out_pkt  = clip->title_pkt;

      _seek_internal(bd, clip, out_pkt, clip_pkt);

    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_playitem(%u) failed\n", clip_ref);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

int64_t bd_seek_mark(BLURAY *bd, unsigned mark)
{
    uint32_t clip_pkt, out_pkt;
    NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        mark < bd->title->mark_list.count) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_mark_search(bd->title, mark, &clip_pkt, &out_pkt);

        _seek_internal(bd, clip, out_pkt, clip_pkt);

    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_mark(%u) failed\n", mark);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

int64_t bd_seek(BLURAY *bd, uint64_t pos)
{
    uint32_t pkt, clip_pkt, out_pkt, out_time;
    NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        pos < (uint64_t)bd->title->packets * 192) {

        pkt = SPN(pos);

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_packet_search(bd->title, pkt, &clip_pkt, &out_pkt, &out_time);

        _seek_internal(bd, clip, out_pkt, clip_pkt);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

uint64_t bd_get_title_size(BLURAY *bd)
{
    uint64_t ret = 0;

    if (!bd) {
        return 0;
    }

    bd_mutex_lock(&bd->mutex);

    if (bd->title) {
        ret = (uint64_t)bd->title->packets * 192;
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

uint64_t bd_tell(BLURAY *bd)
{
    uint64_t ret = 0;

    if (!bd) {
        return 0;
    }

    bd_mutex_lock(&bd->mutex);

    ret = bd->s_pos;

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

/*
 * read
 */

static int64_t _clip_seek_time(BLURAY *bd, uint32_t tick)
{
    uint32_t clip_pkt, out_pkt;

    if (!bd->title || !bd->st0.clip) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_clip_seek_time(): no playlist playing\n");
        return -1;
    }

    if (tick >= bd->st0.clip->out_time) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_clip_seek_time(): timestamp after clip end (%u < %u)\n",
                 bd->st0.clip->out_time, tick);
        return -1;
    }

    // Find the closest access unit to the requested position
    nav_clip_time_search(bd->st0.clip, tick, &clip_pkt, &out_pkt);

    _seek_internal(bd, bd->st0.clip, out_pkt, clip_pkt);

    return bd->s_pos;
}

static int _bd_read(BLURAY *bd, unsigned char *buf, int len)
{
    BD_STREAM *st = &bd->st0;
    int out_len;

    if (st->fp) {
        out_len = 0;
        BD_DEBUG(DBG_STREAM, "Reading [%d bytes] at %"PRIu64"...\n", len, bd->s_pos);

        if (st->clip == NULL) {
            // We previously reached the last clip.  Nothing
            // else to read.
            _queue_event(bd, BD_EVENT_END_OF_TITLE, 0);
            bd->end_of_playlist |= 1;
            return 0;
        }

        while (len > 0) {
            uint32_t clip_pkt;

            unsigned int size = len;
            // Do we need to read more data?
            clip_pkt = SPN(st->clip_pos);
            if (bd->seamless_angle_change) {
                if (clip_pkt >= bd->angle_change_pkt) {
                    if (clip_pkt >= st->clip->end_pkt) {
                        st->clip = nav_next_clip(bd->title, st->clip);
                        if (!_open_m2ts(bd, st)) {
                            return -1;
                        }
                        bd->s_pos = (uint64_t)st->clip->title_pkt * 192L;
                    } else {
                        _change_angle(bd);
                        _clip_seek_time(bd, bd->angle_change_time);
                    }
                    bd->seamless_angle_change = 0;
                } else {
                    uint64_t angle_pos;

                    angle_pos = (uint64_t)bd->angle_change_pkt * 192L;
                    if (angle_pos - st->clip_pos < size) {
                        size = (unsigned int)(angle_pos - st->clip_pos);
                    }
                }
            }
            if (st->int_buf_off == 6144 || clip_pkt >= st->clip->end_pkt) {

                // Do we need to get the next clip?
                if (clip_pkt >= st->clip->end_pkt) {

                    // split read()'s at clip boundary
                    if (out_len) {
                        return out_len;
                    }

                    MPLS_PI *pi = &st->clip->title->pl->play_item[st->clip->ref];

                    // handle still mode clips
                    if (pi->still_mode == BLURAY_STILL_INFINITE) {
                        _queue_event(bd, BD_EVENT_STILL_TIME, 0);
                        return 0;
                    }
                    if (pi->still_mode == BLURAY_STILL_TIME) {
                        if (bd->event_queue) {
                            _queue_event(bd, BD_EVENT_STILL_TIME, pi->still_time);
                            return 0;
                        }
                    }

                    // find next clip
                    st->clip = nav_next_clip(bd->title, st->clip);
                    if (st->clip == NULL) {
                        BD_DEBUG(DBG_BLURAY | DBG_STREAM, "End of title\n");
                        _queue_event(bd, BD_EVENT_END_OF_TITLE, 0);
                        bd->end_of_playlist |= 1;
                        return 0;
                    }
                    if (!_open_m2ts(bd, st)) {
                        return -1;
                    }

                    if (st->clip->connection == CONNECT_NON_SEAMLESS) {
                        /* application layer demuxer buffers must be reset here */
                        _queue_event(bd, BD_EVENT_DISCONTINUITY, st->clip->in_time);
                    }

                }

                int r = _read_block(bd, st, bd->int_buf);
                if (r > 0) {

                    if (st->ig_pid > 0) {
                        if (gc_decode_ts(bd->graphics_controller, st->ig_pid, bd->int_buf, 1, -1) > 0) {
                            /* initialize menus */
                            _run_gc(bd, GC_CTRL_INIT_MENU, 0);
                        }
                    }
                    if (st->pg_pid > 0) {
                        if (gc_decode_ts(bd->graphics_controller, st->pg_pid, bd->int_buf, 1, -1) > 0) {
                            /* render subtitles */
                            gc_run(bd->graphics_controller, GC_CTRL_PG_UPDATE, 0, NULL);
                        }
                    }
                    if (bd->st_textst.clip) {
                        _update_textst_timer(bd);
                    }

                    st->int_buf_off = st->clip_pos % 6144;

                } else if (r == 0) {
                    /* recoverable error (EOF, broken block) */
                    return out_len;
                } else {
                    /* fatal error */
                    return -1;
                }

                /* finetune seek point (avoid skipping PAT/PMT/PCR) */
                if (BD_UNLIKELY(st->seek_flag)) {
                    st->seek_flag = 0;

                    /* rewind if previous packets contain PAT/PMT/PCR */
                    while (st->int_buf_off >= 192 && TS_PID(bd->int_buf + st->int_buf_off - 192) <= HDMV_PID_PCR) {
                        st->clip_pos -= 192;
                        st->int_buf_off -= 192;
                        bd->s_pos -= 192;
                    }
                }

            }
            if (size > (unsigned int)6144 - st->int_buf_off) {
                size = 6144 - st->int_buf_off;
            }

            /* cut read at clip end packet */
            uint32_t new_clip_pkt = SPN(st->clip_pos + size);
            if (new_clip_pkt > st->clip->end_pkt) {
                BD_DEBUG(DBG_STREAM, "cut %d bytes at end of block\n", (new_clip_pkt - st->clip->end_pkt) * 192);
                size -= (new_clip_pkt - st->clip->end_pkt) * 192;
            }

            /* copy chunk */
            memcpy(buf, bd->int_buf + st->int_buf_off, size);
            buf += size;
            len -= size;
            out_len += size;
            st->clip_pos += size;
            st->int_buf_off += size;
            bd->s_pos += size;
        }

        /* mark tracking */
        if (bd->next_mark >= 0 && bd->s_pos > bd->next_mark_pos) {
            _playmark_reached(bd);
        }

        BD_DEBUG(DBG_STREAM, "%d bytes read OK!\n", out_len);
        return out_len;
    }

    BD_DEBUG(DBG_STREAM | DBG_CRIT, "bd_read(): no valid title selected!\n");

    return -1;
}

int bd_read(BLURAY *bd, unsigned char *buf, int len)
{
    int result;

    bd_mutex_lock(&bd->mutex);
    result = _bd_read(bd, buf, len);
    bd_mutex_unlock(&bd->mutex);

    return result;
}

int bd_read_skip_still(BLURAY *bd)
{
    BD_STREAM *st = &bd->st0;
    int ret = 0;

    bd_mutex_lock(&bd->mutex);

    if (st->clip) {
        MPLS_PI *pi = &st->clip->title->pl->play_item[st->clip->ref];

        if (pi->still_mode == BLURAY_STILL_TIME) {
            st->clip = nav_next_clip(bd->title, st->clip);
            if (st->clip) {
                ret = _open_m2ts(bd, st);
            }
        }
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

/*
 * synchronous sub paths
 */

static int _preload_textst_subpath(BLURAY *bd)
{
    uint8_t        char_code      = BLURAY_TEXT_CHAR_CODE_UTF8;
    int            textst_subpath = -1;
    unsigned       textst_subclip = 0;
    uint16_t       textst_pid     = 0;
    unsigned       ii;

    if (!bd->graphics_controller) {
        return 0;
    }

    if (!bd->decode_pg || !bd->title) {
        return 0;
    }

    _find_pg_stream(bd, &textst_pid, &textst_subpath, &textst_subclip, &char_code);
    if (textst_subpath < 0) {
        return 0;
    }

    if (textst_subclip >= bd->title->sub_path[textst_subpath].clip_list.count) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_preload_textst_subpath(): invalid subclip id\n");
        return -1;
    }

    if (bd->st_textst.clip == &bd->title->sub_path[textst_subpath].clip_list.clip[textst_subclip]) {
        BD_DEBUG(DBG_BLURAY, "_preload_textst_subpath(): subpath already loaded");
        return 1;
    }

    gc_run(bd->graphics_controller, GC_CTRL_PG_RESET, 0, NULL);

    bd->st_textst.clip = &bd->title->sub_path[textst_subpath].clip_list.clip[textst_subclip];
    if (!bd->st0.clip->cl) {
        /* required for fonts */
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_preload_textst_subpath(): missing clip data\n");
        return -1;
    }

    if (!_preload_m2ts(bd, &bd->st_textst)) {
        _close_preload(&bd->st_textst);
        return 0;
    }

    gc_decode_ts(bd->graphics_controller, 0x1800, bd->st_textst.buf, SPN(bd->st_textst.clip_size) / 32, -1);

    /* set fonts and encoding from clip info */
    gc_add_font(bd->graphics_controller, NULL, -1);
    for (ii = 0; ii < bd->st_textst.clip->cl->font_info.font_count; ii++) {
        char *file = str_printf("%s.otf", bd->st_textst.clip->cl->font_info.font[ii].file_id);
        if (file) {
            uint8_t *data = NULL;
            size_t size = disc_read_file(bd->disc, "BDMV" DIR_SEP "AUXDATA", file, &data);
            if (data && size > 0 && gc_add_font(bd->graphics_controller, data, size) < 0) {
                X_FREE(data);
            }
            X_FREE(file);
        }
    }
    gc_run(bd->graphics_controller, GC_CTRL_PG_CHARCODE, char_code, NULL);

    /* start presentation timer */
    _init_textst_timer(bd);

    return 1;
}

/*
 * preloader for asynchronous sub paths
 */

static int _find_ig_stream(BLURAY *bd, uint16_t *pid, int *sub_path_idx, unsigned *sub_clip_idx)
{
    unsigned  main_clip_idx = bd->st0.clip ? bd->st0.clip->ref : 0;
    MPLS_PI  *pi        = &bd->title->pl->play_item[main_clip_idx];
    unsigned  ig_stream = bd_psr_read(bd->regs, PSR_IG_STREAM_ID);

    if (ig_stream > 0 && ig_stream <= pi->stn.num_ig) {
        ig_stream--; /* stream number to table index */
        if (pi->stn.ig[ig_stream].stream_type == 2) {
            *sub_path_idx = pi->stn.ig[ig_stream].subpath_id;
            *sub_clip_idx = pi->stn.ig[ig_stream].subclip_id;
        }
        *pid = pi->stn.ig[ig_stream].pid;

        BD_DEBUG(DBG_BLURAY, "_find_ig_stream(): current IG stream pid 0x%04x sub-path %d\n",
              *pid, *sub_path_idx);
        return 1;
    }

    return 0;
}

static int _preload_ig_subpath(BLURAY *bd)
{
    int      ig_subpath = -1;
    unsigned ig_subclip = 0;
    uint16_t ig_pid     = 0;

    if (!bd->graphics_controller) {
        return 0;
    }

    _find_ig_stream(bd, &ig_pid, &ig_subpath, &ig_subclip);

    if (ig_subpath < 0) {
        return 0;
    }

    if (ig_subclip >= bd->title->sub_path[ig_subpath].clip_list.count) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_preload_ig_subpath(): invalid subclip id\n");
        return -1;
    }

    if (bd->st_ig.clip == &bd->title->sub_path[ig_subpath].clip_list.clip[ig_subclip]) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_preload_ig_subpath(): subpath already loaded");
        //return 1;
    }

    bd->st_ig.clip = &bd->title->sub_path[ig_subpath].clip_list.clip[ig_subclip];

    if (bd->title->sub_path[ig_subpath].clip_list.count > 1) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_preload_ig_subpath(): multi-clip sub paths not supported\n");
    }

    if (!_preload_m2ts(bd, &bd->st_ig)) {
        _close_preload(&bd->st_ig);
        return 0;
    }

    return 1;
}

static int _preload_subpaths(BLURAY *bd)
{
    _close_preload(&bd->st_ig);
    _close_preload(&bd->st_textst);

    if (bd->title->pl->sub_count <= 0) {
        return 0;
    }

    return _preload_ig_subpath(bd) | _preload_textst_subpath(bd);
}

static int _init_ig_stream(BLURAY *bd)
{
    int      ig_subpath = -1;
    unsigned ig_subclip = 0;
    uint16_t ig_pid     = 0;

    bd->st0.ig_pid = 0;

    if (!bd->graphics_controller) {
        return 0;
    }

    _find_ig_stream(bd, &ig_pid, &ig_subpath, &ig_subclip);

    /* decode already preloaded IG sub-path */
    if (bd->st_ig.clip) {
        gc_decode_ts(bd->graphics_controller, ig_pid, bd->st_ig.buf, SPN(bd->st_ig.clip_size) / 32, -1);
        return 1;
    }

    /* store PID of main path embedded IG stream */
    if (ig_subpath < 0) {
        bd->st0.ig_pid = ig_pid;
        return 1;
    }

    return 0;
}

/*
 * select title / angle
 */

static void _close_playlist(BLURAY *bd)
{
    if (bd->graphics_controller) {
        gc_run(bd->graphics_controller, GC_CTRL_RESET, 0, NULL);
    }

    /* stopping playback in middle of playlist ? */
    if (bd->title && bd->st0.clip) {
        if (bd->st0.clip->ref < bd->title->clip_list.count - 1) {
            /* not last clip of playlist */
            BD_DEBUG(DBG_BLURAY, "close playlist (not last clip)\n");
            _queue_event(bd, BD_EVENT_PLAYLIST_STOP, 0);
        } else {
            /* last clip of playlist */
            int clip_pkt = SPN(bd->st0.clip_pos);
            int skip = bd->st0.clip->end_pkt - clip_pkt;
            BD_DEBUG(DBG_BLURAY, "close playlist (last clip), packets skipped %d\n", skip);
            if (skip > 100) {
                _queue_event(bd, BD_EVENT_PLAYLIST_STOP, 0);
            }
        }
    }

    _close_m2ts(&bd->st0);
    _close_preload(&bd->st_ig);
    _close_preload(&bd->st_textst);

    if (bd->title) {
        nav_title_close(bd->title);
        bd->title = NULL;
    }

    /* reset UO mask */
    memset(&bd->st0.uo_mask, 0, sizeof(BD_UO_MASK));
    memset(&bd->gc_uo_mask,  0, sizeof(BD_UO_MASK));
    _update_uo_mask(bd);
}

static int _open_playlist(BLURAY *bd, const char *f_name, unsigned angle)
{
    _close_playlist(bd);

    bd->title = nav_title_open(bd->disc, f_name, angle);
    if (bd->title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s!\n", f_name);
        return 0;
    }

    bd->seamless_angle_change = 0;
    bd->s_pos = 0;
    bd->end_of_playlist = 0;
    bd->st0.ig_pid = 0;

    bd_psr_write(bd->regs, PSR_PLAYLIST, atoi(bd->title->name));
    bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);
    bd_psr_write(bd->regs, PSR_CHAPTER, 0xffff);

    // Get the initial clip of the playlist
    bd->st0.clip = nav_next_clip(bd->title, NULL);
    if (_open_m2ts(bd, &bd->st0)) {
        BD_DEBUG(DBG_BLURAY, "Title %s selected\n", f_name);

        _find_next_playmark(bd);

        _preload_subpaths(bd);

        bd->st0.seek_flag = 1;

        return 1;
    }
    return 0;
}

int bd_select_playlist(BLURAY *bd, uint32_t playlist)
{
    char *f_name;
    int result;

    f_name = str_printf("%05d.mpls", playlist);
    if (!f_name) {
        return 0;
    }

    bd_mutex_lock(&bd->mutex);

    if (bd->title_list) {
        /* update current title */
        unsigned i;
        for (i = 0; i < bd->title_list->count; i++) {
            if (playlist == bd->title_list->title_info[i].mpls_id) {
                bd->title_idx = i;
                break;
            }
        }
    }

    result = _open_playlist(bd, f_name, 0);

    bd_mutex_unlock(&bd->mutex);

    X_FREE(f_name);
    return result;
}

#ifdef USING_BDJAVA
int bd_bdj_seek(BLURAY *bd, int playitem, int playmark, int64_t time)
{
    bd_mutex_lock(&bd->mutex);

    if (playitem > 0) {
        bd_seek_playitem(bd, playitem);
    }
    if (playmark >= 0) {
        bd_seek_mark(bd, playmark);
    }
    if (time >= 0) {
        bd_seek_time(bd, time);
    }

    bd_mutex_unlock(&bd->mutex);

    return 1;
}

static int _play_playlist_at(BLURAY *bd, int playlist, int playitem, int playmark, int64_t time)
{
    if (playlist < 0) {
        _close_playlist(bd);
        return 1;
    }

    if (!bd_select_playlist(bd, playlist)) {
        return 0;
    }

#ifdef USING_BDJAVA
    bd->bdj_wait_start = 1;  /* playback is triggered by bd_select_rate() */
#endif

    bd_bdj_seek(bd, playitem, playmark, time);

    return 1;
}

int bd_play_playlist_at(BLURAY *bd, int playlist, int playitem, int playmark, int64_t time)
{
    int result;

    /* select + seek should be atomic (= player can't read data between select and seek to start position) */
    bd_mutex_lock(&bd->mutex);
    result = _play_playlist_at(bd, playlist, playitem, playmark, time);
    bd_mutex_unlock(&bd->mutex);

    return result;
}

int bd_bdj_sound_effect(BLURAY *bd, int id)
{
    if (bd->sound_effects && id >= bd->sound_effects->num_sounds) {
        return -1;
    }
    if (id < 0 || id > 0xff) {
        return -1;
    }

    _queue_event(bd, BD_EVENT_SOUND_EFFECT, id);
    return 0;
}

#endif /* USING_BDJAVA */

// Select a title for playback
// The title index is an index into the list
// established by bd_get_titles()
int bd_select_title(BLURAY *bd, uint32_t title_idx)
{
    const char *f_name;
    int result;

    // Open the playlist
    if (bd->title_list == NULL) {
        BD_DEBUG(DBG_CRIT | DBG_BLURAY, "Title list not yet read!\n");
        return 0;
    }
    if (bd->title_list->count <= title_idx) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Invalid title index %d!\n", title_idx);
        return 0;
    }

    bd_mutex_lock(&bd->mutex);

    bd->title_idx = title_idx;
    f_name = bd->title_list->title_info[title_idx].name;

    result = _open_playlist(bd, f_name, 0);

    bd_mutex_unlock(&bd->mutex);

    return result;
}

uint32_t bd_get_current_title(BLURAY *bd)
{
    return bd->title_idx;
}

static int _bd_select_angle(BLURAY *bd, unsigned angle)
{
    unsigned orig_angle;

    if (bd->title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't select angle: title not yet selected!\n");
        return 0;
    }

    orig_angle = bd->title->angle;

    bd->st0.clip = nav_set_angle(bd->title, bd->st0.clip, angle);

    if (orig_angle == bd->title->angle) {
        return 1;
    }

    bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);

    if (!_open_m2ts(bd, &bd->st0)) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Error selecting angle %d !\n", angle);
        return 0;
    }

    return 1;
}

int bd_select_angle(BLURAY *bd, unsigned angle)
{
    int result;
    bd_mutex_lock(&bd->mutex);
    result = _bd_select_angle(bd, angle);
    bd_mutex_unlock(&bd->mutex);
    return result;
}

unsigned bd_get_current_angle(BLURAY *bd)
{
    int angle = 0;

    bd_mutex_lock(&bd->mutex);
    if (bd->title) {
        angle = bd->title->angle;
    }
    bd_mutex_unlock(&bd->mutex);

    return angle;
}


void bd_seamless_angle_change(BLURAY *bd, unsigned angle)
{
    uint32_t clip_pkt;

    bd_mutex_lock(&bd->mutex);

    clip_pkt = SPN(bd->st0.clip_pos + 191);
    bd->angle_change_pkt = nav_angle_change_search(bd->st0.clip, clip_pkt,
                                                   &bd->angle_change_time);
    bd->request_angle = angle;
    bd->seamless_angle_change = 1;

    bd_mutex_unlock(&bd->mutex);
}

/*
 * title lists
 */

uint32_t bd_get_titles(BLURAY *bd, uint8_t flags, uint32_t min_title_length)
{
    if (!bd) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_get_titles(NULL) failed\n");
        return 0;
    }

    if (bd->title_list != NULL) {
        nav_free_title_list(bd->title_list);
    }
    bd->title_list = nav_get_title_list(bd->disc, flags, min_title_length);

    if (!bd->title_list) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "nav_get_title_list(%s) failed\n", disc_root(bd->disc));
        return 0;
    }

    disc_event(bd->disc, DISC_EVENT_START, bd->disc_info.num_titles);

    return bd->title_list->count;
}

int bd_get_main_title(BLURAY *bd)
{
    if (!bd) {
        return -1;
    }
    if (bd->title_type != title_undef) {
        BD_DEBUG(DBG_CRIT | DBG_BLURAY, "bd_get_main_title() can't be used with BluRay menus\n");
    }

    if (bd->title_list == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Title list not yet read!\n");
        return -1;
    }

    return bd->title_list->main_title_idx;
}

static int _copy_streams(NAV_CLIP *clip, BLURAY_STREAM_INFO **pstreams, MPLS_STREAM *si, int count)
{
    BLURAY_STREAM_INFO *streams;
    int ii;

    if (!count) {
        return 1;
    }
    streams = *pstreams = calloc(count, sizeof(BLURAY_STREAM_INFO));
    if (!streams) {
        return 0;
    }

    for (ii = 0; ii < count; ii++) {
        streams[ii].coding_type = si[ii].coding_type;
        streams[ii].format = si[ii].format;
        streams[ii].rate = si[ii].rate;
        streams[ii].char_code = si[ii].char_code;
        memcpy(streams[ii].lang, si[ii].lang, 4);
        streams[ii].pid = si[ii].pid;
        streams[ii].aspect = nav_lookup_aspect(clip, si[ii].pid);
        if ((si->stream_type == 2) || (si->stream_type == 3))
            streams[ii].subpath_id = si->subpath_id;
        else
            streams[ii].subpath_id = -1;
    }

    return 1;
}

static BLURAY_TITLE_INFO* _fill_title_info(NAV_TITLE* title, uint32_t title_idx, uint32_t playlist)
{
    BLURAY_TITLE_INFO *title_info;
    unsigned int ii;

    title_info = calloc(1, sizeof(BLURAY_TITLE_INFO));
    if (!title_info) {
        goto error;
    }
    title_info->idx = title_idx;
    title_info->playlist = playlist;
    title_info->duration = (uint64_t)title->duration * 2;
    title_info->angle_count = title->angle_count;
    title_info->chapter_count = title->chap_list.count;
    if (title_info->chapter_count) {
        title_info->chapters = calloc(title_info->chapter_count, sizeof(BLURAY_TITLE_CHAPTER));
        if (!title_info->chapters) {
            goto error;
        }
        for (ii = 0; ii < title_info->chapter_count; ii++) {
            title_info->chapters[ii].idx = ii;
            title_info->chapters[ii].start = (uint64_t)title->chap_list.mark[ii].title_time * 2;
            title_info->chapters[ii].duration = (uint64_t)title->chap_list.mark[ii].duration * 2;
            title_info->chapters[ii].offset = (uint64_t)title->chap_list.mark[ii].title_pkt * 192L;
            title_info->chapters[ii].clip_ref = title->chap_list.mark[ii].clip_ref;
        }
    }
    title_info->mark_count = title->mark_list.count;
    if (title_info->mark_count) {
        title_info->marks = calloc(title_info->mark_count, sizeof(BLURAY_TITLE_MARK));
        if (!title_info->marks) {
            goto error;
        }
        for (ii = 0; ii < title_info->mark_count; ii++) {
            title_info->marks[ii].idx = ii;
            title_info->marks[ii].type = title->mark_list.mark[ii].mark_type;
            title_info->marks[ii].start = (uint64_t)title->mark_list.mark[ii].title_time * 2;
            title_info->marks[ii].duration = (uint64_t)title->mark_list.mark[ii].duration * 2;
            title_info->marks[ii].offset = (uint64_t)title->mark_list.mark[ii].title_pkt * 192L;
            title_info->marks[ii].clip_ref = title->mark_list.mark[ii].clip_ref;
        }
    }
    title_info->clip_count = title->clip_list.count;
    if (title_info->clip_count) {
        title_info->clips = calloc(title_info->clip_count, sizeof(BLURAY_CLIP_INFO));
        if (!title_info->clips) {
            goto error;
        }
        for (ii = 0; ii < title_info->clip_count; ii++) {
            MPLS_PI *pi = &title->pl->play_item[ii];
            BLURAY_CLIP_INFO *ci = &title_info->clips[ii];
            NAV_CLIP *nc = &title->clip_list.clip[ii];

            ci->pkt_count = nc->end_pkt - nc->start_pkt;
            ci->start_time = (uint64_t)nc->title_time * 2;
            ci->in_time = (uint64_t)pi->in_time * 2;
            ci->out_time = (uint64_t)pi->out_time * 2;
            ci->still_mode = pi->still_mode;
            ci->still_time = pi->still_time;
            ci->video_stream_count = pi->stn.num_video;
            ci->audio_stream_count = pi->stn.num_audio;
            ci->pg_stream_count = pi->stn.num_pg + pi->stn.num_pip_pg;
            ci->ig_stream_count = pi->stn.num_ig;
            ci->sec_video_stream_count = pi->stn.num_secondary_video;
            ci->sec_audio_stream_count = pi->stn.num_secondary_audio;
            if (!_copy_streams(nc, &ci->video_streams, pi->stn.video, ci->video_stream_count) ||
                !_copy_streams(nc, &ci->audio_streams, pi->stn.audio, ci->audio_stream_count) ||
                !_copy_streams(nc, &ci->pg_streams, pi->stn.pg, ci->pg_stream_count) ||
                !_copy_streams(nc, &ci->ig_streams, pi->stn.ig, ci->ig_stream_count) ||
                !_copy_streams(nc, &ci->sec_video_streams, pi->stn.secondary_video, ci->sec_video_stream_count) ||
                !_copy_streams(nc, &ci->sec_audio_streams, pi->stn.secondary_audio, ci->sec_audio_stream_count)) {

                goto error;
            }
        }
    }

    return title_info;

 error:
    BD_DEBUG(DBG_CRIT, "Out of memory\n");
    bd_free_title_info(title_info);
    return NULL;
}

static BLURAY_TITLE_INFO *_get_title_info(BLURAY *bd, uint32_t title_idx, uint32_t playlist, const char *mpls_name,
                                          unsigned angle)
{
    NAV_TITLE *title;
    BLURAY_TITLE_INFO *title_info;

    title = nav_title_open(bd->disc, mpls_name, angle);
    if (title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s!\n", mpls_name);
        return NULL;
    }

    title_info = _fill_title_info(title, title_idx, playlist);

    nav_title_close(title);
    return title_info;
}

BLURAY_TITLE_INFO* bd_get_title_info(BLURAY *bd, uint32_t title_idx, unsigned angle)
{
    if (bd->title_list == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Title list not yet read!\n");
        return NULL;
    }
    if (bd->title_list->count <= title_idx) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Invalid title index %d!\n", title_idx);
        return NULL;
    }

    return _get_title_info(bd,
                           title_idx, bd->title_list->title_info[title_idx].mpls_id,
                           bd->title_list->title_info[title_idx].name,
                           angle);
}

BLURAY_TITLE_INFO* bd_get_playlist_info(BLURAY *bd, uint32_t playlist, unsigned angle)
{
    char *f_name;
    BLURAY_TITLE_INFO *title_info;

    f_name = str_printf("%05d.mpls", playlist);
    if (!f_name) {
        return NULL;
    }

    title_info = _get_title_info(bd, 0, playlist, f_name, angle);

    X_FREE(f_name);

    return title_info;
}

void bd_free_title_info(BLURAY_TITLE_INFO *title_info)
{
    unsigned int ii;

    if (title_info) {
        X_FREE(title_info->chapters);
        X_FREE(title_info->marks);
        if (title_info->clips) {
            for (ii = 0; ii < title_info->clip_count; ii++) {
                X_FREE(title_info->clips[ii].video_streams);
                X_FREE(title_info->clips[ii].audio_streams);
                X_FREE(title_info->clips[ii].pg_streams);
                X_FREE(title_info->clips[ii].ig_streams);
                X_FREE(title_info->clips[ii].sec_video_streams);
                X_FREE(title_info->clips[ii].sec_audio_streams);
            }
            X_FREE(title_info->clips);
        }
        X_FREE(title_info);
    }
}

/*
 * player settings
 */

int bd_set_player_setting(BLURAY *bd, uint32_t idx, uint32_t value)
{
    static const struct { uint32_t idx; uint32_t  psr; } map[] = {
        { BLURAY_PLAYER_SETTING_PARENTAL,       PSR_PARENTAL },
        { BLURAY_PLAYER_SETTING_AUDIO_CAP,      PSR_AUDIO_CAP },
        { BLURAY_PLAYER_SETTING_AUDIO_LANG,     PSR_AUDIO_LANG },
        { BLURAY_PLAYER_SETTING_PG_LANG,        PSR_PG_AND_SUB_LANG },
        { BLURAY_PLAYER_SETTING_MENU_LANG,      PSR_MENU_LANG },
        { BLURAY_PLAYER_SETTING_COUNTRY_CODE,   PSR_COUNTRY },
        { BLURAY_PLAYER_SETTING_REGION_CODE,    PSR_REGION },
        { BLURAY_PLAYER_SETTING_OUTPUT_PREFER,  PSR_OUTPUT_PREFER },
        { BLURAY_PLAYER_SETTING_DISPLAY_CAP,    PSR_DISPLAY_CAP },
        { BLURAY_PLAYER_SETTING_3D_CAP,         PSR_3D_CAP },
        { BLURAY_PLAYER_SETTING_VIDEO_CAP,      PSR_VIDEO_CAP },
        { BLURAY_PLAYER_SETTING_TEXT_CAP,       PSR_TEXT_CAP },
        { BLURAY_PLAYER_SETTING_PLAYER_PROFILE, PSR_PROFILE_VERSION },
    };

    unsigned i;
    int result;

    if (idx == BLURAY_PLAYER_SETTING_DECODE_PG) {
        bd_mutex_lock(&bd->mutex);

        bd->decode_pg = !!value;
        result = !bd_psr_write_bits(bd->regs, PSR_PG_STREAM,
                                    (!!value) << 31,
                                    0x80000000);

        bd_mutex_unlock(&bd->mutex);
        return result;
    }
#ifdef USING_BDJAVA
    if (idx == BLURAY_PLAYER_SETTING_PERSISTENT_STORAGE) {
        if (bd->title_type != title_undef) {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't disable persistent storage during playback\n");
            return 0;
        }
        bd->bdjstorage.no_persistent_storage = !value;
        return 1;
    }
#endif

    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (idx == map[i].idx) {
            bd_mutex_lock(&bd->mutex);
            result = !bd_psr_setting_write(bd->regs, map[i].psr, value);
            bd_mutex_unlock(&bd->mutex);
            return result;
        }
    }

    return 0;
}

int bd_set_player_setting_str(BLURAY *bd, uint32_t idx, const char *s)
{
    switch (idx) {
        case BLURAY_PLAYER_SETTING_AUDIO_LANG:
        case BLURAY_PLAYER_SETTING_PG_LANG:
        case BLURAY_PLAYER_SETTING_MENU_LANG:
            return bd_set_player_setting(bd, idx, str_to_uint32(s, 3));

        case BLURAY_PLAYER_SETTING_COUNTRY_CODE:
            return bd_set_player_setting(bd, idx, str_to_uint32(s, 2));

#ifdef USING_BDJAVA
        case BLURAY_PLAYER_CACHE_ROOT:
        case BLURAY_PLAYER_PERSISTENT_ROOT:
            switch (idx) {
                case BLURAY_PLAYER_CACHE_ROOT:
                    bd_mutex_lock(&bd->mutex);
                    X_FREE(bd->bdjstorage.cache_root);
                    bd->bdjstorage.cache_root = str_dup(s);
                    bd_mutex_unlock(&bd->mutex);
                    BD_DEBUG(DBG_BDJ, "Cache root dir set to %s\n", bd->bdjstorage.cache_root);
                    return 1;

                case BLURAY_PLAYER_PERSISTENT_ROOT:
                    bd_mutex_lock(&bd->mutex);
                    X_FREE(bd->bdjstorage.persistent_root);
                    bd->bdjstorage.persistent_root = str_dup(s);
                    bd_mutex_unlock(&bd->mutex);
                    BD_DEBUG(DBG_BDJ, "Persistent root dir set to %s\n", bd->bdjstorage.persistent_root);
                    return 1;
            }
#endif /* USING_BDJAVA */
        default:
            return 0;
    }
}

void bd_select_stream(BLURAY *bd, uint32_t stream_type, uint32_t stream_id, uint32_t enable_flag)
{
    bd_mutex_lock(&bd->mutex);

    switch (stream_type) {
        case BLURAY_AUDIO_STREAM:
            bd_psr_write(bd->regs, PSR_PRIMARY_AUDIO_ID, stream_id & 0xff);
            break;
        case BLURAY_PG_TEXTST_STREAM:
            bd_psr_write_bits(bd->regs, PSR_PG_STREAM,
                              ((!!enable_flag)<<31) | (stream_id & 0xfff),
                              0x80000fff);
            break;
        /*
        case BLURAY_SECONDARY_VIDEO_STREAM:
        case BLURAY_SECONDARY_AUDIO_STREAM:
        */
    }

    bd_mutex_unlock(&bd->mutex);
}

/*
 * BD-J testing
 */

int bd_start_bdj(BLURAY *bd, const char *start_object)
{
    const BLURAY_TITLE *t;
    unsigned int title_num = atoi(start_object);
    unsigned ii;

    if (!bd) {
        return 0;
    }

    /* first play object ? */
    if (bd->disc_info.first_play_supported) {
        t = bd->disc_info.first_play;
        if (t && t->bdj && t->id_ref == title_num) {
            return _start_bdj(bd, BLURAY_TITLE_FIRST_PLAY);
        }
    }

    /* valid BD-J title from disc index ? */
    if (bd->disc_info.titles) {
        for (ii = 0; ii <= bd->disc_info.num_titles; ii++) {
            t = bd->disc_info.titles[ii];
            if (t && t->bdj && t->id_ref == title_num) {
                return _start_bdj(bd, ii);
            }
        }
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No %s.bdjo in disc index\n", start_object);
    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No disc index\n");
    }

    return 0;
 }

void bd_stop_bdj(BLURAY *bd)
{
    bd_mutex_lock(&bd->mutex);
    _close_bdj(bd);
    bd_mutex_unlock(&bd->mutex);
}

/*
 * Navigation mode interface
 */

static void _set_scr(BLURAY *bd, int64_t pts)
{
    if (pts >= 0) {
        uint32_t tick = (uint32_t)(((uint64_t)pts) >> 1);
        _update_time_psr(bd, tick);

    } else if (!bd->app_scr) {
        _update_time_psr_from_stream(bd);
    }
}

static void _process_psr_restore_event(BLURAY *bd, BD_PSR_EVENT *ev)
{
    /* PSR restore events are handled internally.
     * Restore stored playback position.
     */

    BD_DEBUG(DBG_BLURAY, "PSR restore: psr%u = %u\n", ev->psr_idx, ev->new_val);

    switch (ev->psr_idx) {
        case PSR_ANGLE_NUMBER:
            /* can't set angle before playlist is opened */
            return;
        case PSR_TITLE_NUMBER:
            /* pass to the application */
            _queue_event(bd, BD_EVENT_TITLE, ev->new_val);
            return;
        case PSR_CHAPTER:
            /* will be selected automatically */
            return;
        case PSR_PLAYLIST:
            bd_select_playlist(bd, ev->new_val);
            nav_set_angle(bd->title, bd->st0.clip, bd_psr_read(bd->regs, PSR_ANGLE_NUMBER) - 1);
            return;
        case PSR_PLAYITEM:
            bd_seek_playitem(bd, ev->new_val);
            return;
        case PSR_TIME:
            _clip_seek_time(bd, ev->new_val);
            _init_ig_stream(bd);
            _run_gc(bd, GC_CTRL_INIT_MENU, 0);
            return;

        case PSR_SELECTED_BUTTON_ID:
        case PSR_MENU_PAGE_ID:
            /* handled by graphics controller */
            return;

        default:
            /* others: ignore */
            return;
    }
}

/*
 * notification events to APP
 */

static void _process_psr_write_event(BLURAY *bd, BD_PSR_EVENT *ev)
{
    if (ev->ev_type == BD_PSR_WRITE) {
        BD_DEBUG(DBG_BLURAY, "PSR write: psr%u = %u\n", ev->psr_idx, ev->new_val);
    }

    switch (ev->psr_idx) {

        /* current playback position */

        case PSR_ANGLE_NUMBER:
            _bdj_event  (bd, BDJ_EVENT_ANGLE,   ev->new_val);
            _queue_event(bd, BD_EVENT_ANGLE,    ev->new_val);
            break;
        case PSR_TITLE_NUMBER:
            _queue_event(bd, BD_EVENT_TITLE,    ev->new_val);
            break;
        case PSR_PLAYLIST:
            _bdj_event  (bd, BDJ_EVENT_PLAYLIST,ev->new_val);
            _queue_event(bd, BD_EVENT_PLAYLIST, ev->new_val);
            break;
        case PSR_PLAYITEM:
            _bdj_event  (bd, BDJ_EVENT_PLAYITEM,ev->new_val);
            _queue_event(bd, BD_EVENT_PLAYITEM, ev->new_val);
            break;
        case PSR_TIME:
            _bdj_event  (bd, BDJ_EVENT_PTS,     ev->new_val);
            break;

        case 102:
            _bdj_event  (bd, BDJ_EVENT_PSR102,  ev->new_val);
            break;
        case 103:
            disc_event(bd->disc, DISC_EVENT_APPLICATION, ev->new_val);
            break;

        default:;
    }
}

static void _process_psr_change_event(BLURAY *bd, BD_PSR_EVENT *ev)
{
    BD_DEBUG(DBG_BLURAY, "PSR change: psr%u = %u\n", ev->psr_idx, ev->new_val);

    _process_psr_write_event(bd, ev);

    switch (ev->psr_idx) {

        /* current playback position */

        case PSR_TITLE_NUMBER:
            disc_event(bd->disc, DISC_EVENT_TITLE, ev->new_val);
            break;

        case PSR_CHAPTER:
            _bdj_event  (bd, BDJ_EVENT_CHAPTER, ev->new_val);
            if (ev->new_val != 0xffff) {
                _queue_event(bd, BD_EVENT_CHAPTER,  ev->new_val);
            }
            break;

        /* stream selection */

        case PSR_IG_STREAM_ID:
            _queue_event(bd, BD_EVENT_IG_STREAM, ev->new_val);
            break;

        case PSR_PRIMARY_AUDIO_ID:
            _bdj_event(bd, BDJ_EVENT_AUDIO_STREAM, ev->new_val);
            _queue_event(bd, BD_EVENT_AUDIO_STREAM, ev->new_val);
            break;

        case PSR_PG_STREAM:
            _bdj_event(bd, BDJ_EVENT_SUBTITLE, ev->new_val);
            if ((ev->new_val & 0x80000fff) != (ev->old_val & 0x80000fff)) {
                _queue_event(bd, BD_EVENT_PG_TEXTST,        !!(ev->new_val & 0x80000000));
                _queue_event(bd, BD_EVENT_PG_TEXTST_STREAM,    ev->new_val & 0xfff);
            }

            bd_mutex_lock(&bd->mutex);
            if (bd->st0.clip) {
                _init_pg_stream(bd);
                if (bd->st_textst.clip) {
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Changing TextST stream\n");
                    _preload_textst_subpath(bd);
                }
            }
            bd_mutex_unlock(&bd->mutex);

            break;

        case PSR_SECONDARY_AUDIO_VIDEO:
            /* secondary video */
            if ((ev->new_val & 0x8f00ff00) != (ev->old_val & 0x8f00ff00)) {
                _queue_event(bd, BD_EVENT_SECONDARY_VIDEO, !!(ev->new_val & 0x80000000));
                _queue_event(bd, BD_EVENT_SECONDARY_VIDEO_SIZE, (ev->new_val >> 24) & 0xf);
                _queue_event(bd, BD_EVENT_SECONDARY_VIDEO_STREAM, (ev->new_val & 0xff00) >> 8);
            }
            /* secondary audio */
            if ((ev->new_val & 0x400000ff) != (ev->old_val & 0x400000ff)) {
                _queue_event(bd, BD_EVENT_SECONDARY_AUDIO, !!(ev->new_val & 0x40000000));
                _queue_event(bd, BD_EVENT_SECONDARY_AUDIO_STREAM, ev->new_val & 0xff);
            }
            _bdj_event(bd, BDJ_EVENT_SECONDARY_STREAM, ev->new_val);
            break;

        /* 3D status */
        case PSR_3D_STATUS:
            _queue_event(bd, BD_EVENT_STEREOSCOPIC_STATUS, ev->new_val & 1);
            break;

        default:;
    }
}

static void _process_psr_event(void *handle, BD_PSR_EVENT *ev)
{
    BLURAY *bd = (BLURAY*)handle;

    switch(ev->ev_type) {
        case BD_PSR_WRITE:
            _process_psr_write_event(bd, ev);
            break;
        case BD_PSR_CHANGE:
            _process_psr_change_event(bd, ev);
            break;
        case BD_PSR_RESTORE:
            _process_psr_restore_event(bd, ev);
            break;

        case BD_PSR_SAVE:
            BD_DEBUG(DBG_BLURAY, "PSR save event\n");
            break;
        default:
            BD_DEBUG(DBG_BLURAY, "PSR event %d: psr%u = %u\n", ev->ev_type, ev->psr_idx, ev->new_val);
            break;
    }
}

static void _queue_initial_psr_events(BLURAY *bd)
{
    const uint32_t psrs[] = {
        PSR_ANGLE_NUMBER,
        PSR_TITLE_NUMBER,
        PSR_IG_STREAM_ID,
        PSR_PRIMARY_AUDIO_ID,
        PSR_PG_STREAM,
        PSR_SECONDARY_AUDIO_VIDEO,
    };
    unsigned ii;
    BD_PSR_EVENT ev;

    ev.ev_type = BD_PSR_CHANGE;
    ev.old_val = 0;

    for (ii = 0; ii < sizeof(psrs) / sizeof(psrs[0]); ii++) {
        ev.psr_idx = psrs[ii];
        ev.new_val = bd_psr_read(bd->regs, psrs[ii]);

        _process_psr_change_event(bd, &ev);
    }
}

static int _play_bdj(BLURAY *bd, unsigned title)
{
    int result;

    bd->title_type = title_bdj;

    result = _start_bdj(bd, title);
    if (result <= 0) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't play BD-J title %d\n", title);
        bd->title_type = title_undef;
        _queue_event(bd, BD_EVENT_ERROR, BD_ERROR_BDJ);
    }

    return result;
}

static int _play_hdmv(BLURAY *bd, unsigned id_ref)
{
    int result = 1;

    _stop_bdj(bd);

    bd->title_type = title_hdmv;

    if (!bd->hdmv_vm) {
        bd->hdmv_vm = hdmv_vm_init(bd->disc, bd->regs, bd->disc_info.num_titles,
                                   bd->disc_info.first_play_supported, bd->disc_info.top_menu_supported);
    }

    if (hdmv_vm_select_object(bd->hdmv_vm, id_ref)) {
        result = 0;
    }

    bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);

    if (result <= 0) {
        bd->title_type = title_undef;
        _queue_event(bd, BD_EVENT_ERROR, BD_ERROR_HDMV);
    }

    return result;
}

static int _play_title(BLURAY *bd, unsigned title)
{
    if (!bd->disc_info.titles) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_play_title(#%d): No disc index\n", title);
        return 0;
    }

    if (bd->disc_info.no_menu_support) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_play(): no menu support\n");
        return 0;
    }

    /* first play object ? */
    if (title == BLURAY_TITLE_FIRST_PLAY) {

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, 0xffff); /* 5.2.3.3 */

        if (!bd->disc_info.first_play_supported) {
            /* no first play title (5.2.3.3) */
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_play_title(): No first play title\n");
            bd->title_type = title_hdmv;
            return 1;
        }

        if (bd->disc_info.first_play->bdj) {
            return _play_bdj(bd, title);
        } else {
            return _play_hdmv(bd, bd->disc_info.first_play->id_ref);
        }
    }

    /* bd_play not called ? */
    if (bd->title_type == title_undef) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_call_title(): bd_play() not called !\n");
        return 0;
    }

    /* top menu ? */
    if (title == BLURAY_TITLE_TOP_MENU) {

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, 0); /* 5.2.3.3 */

        if (!bd->disc_info.top_menu_supported) {
            /* no top menu (5.2.3.3) */
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_play_title(): No top menu title\n");
            bd->title_type = title_hdmv;
            return 0;
        }

        if (bd->disc_info.top_menu->bdj) {
            return _play_bdj(bd, title);
        } else {
            return _play_hdmv(bd, bd->disc_info.top_menu->id_ref);
        }

        return 0;
    }

    /* valid title from disc index ? */
    if (title > 0 && title <= bd->disc_info.num_titles) {

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, title); /* 5.2.3.3 */
        if (bd->disc_info.titles[title]->bdj) {
            return _play_bdj(bd, title);
        } else {
            return _play_hdmv(bd, bd->disc_info.titles[title]->id_ref);
        }
    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_play_title(#%d): Title not found\n", title);
    }

    return 0;
}

#ifdef USING_BDJAVA
int bd_play_title_internal(BLURAY *bd, unsigned title)
{
    /* used by BD-J. Like bd_play_title() but bypasses UO mask checks. */
    int ret;
    bd_mutex_lock(&bd->mutex);
    ret = _play_title(bd, title);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}
#endif

int bd_play(BLURAY *bd)
{
    int result;

    bd_mutex_lock(&bd->mutex);

    /* reset player state */

    bd->title_type = title_undef;

    if (bd->hdmv_vm) {
        hdmv_vm_free(&bd->hdmv_vm);
    }

    if (!bd->event_queue) {
        bd->event_queue = event_queue_new(sizeof(BD_EVENT));

        bd_psr_lock(bd->regs);
        bd_psr_register_cb(bd->regs, _process_psr_event, bd);
        _queue_initial_psr_events(bd);
        bd_psr_unlock(bd->regs);
    }

    disc_event(bd->disc, DISC_EVENT_START, 0);

    /* start playback from FIRST PLAY title */

    result = _play_title(bd, BLURAY_TITLE_FIRST_PLAY);

    bd_mutex_unlock(&bd->mutex);

    return result;
}

static int _try_play_title(BLURAY *bd, unsigned title)
{
    if (bd->title_type == title_undef && title != BLURAY_TITLE_FIRST_PLAY) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_play_title(): bd_play() not called\n");
        return 0;
    }

    if (bd->uo_mask.title_search) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "title search masked\n");
        _bdj_event(bd, BDJ_EVENT_UO_MASKED, UO_MASK_TITLE_SEARCH_INDEX);
        return 0;
    }

    return _play_title(bd, title);
}

int bd_play_title(BLURAY *bd, unsigned title)
{
    int ret;

    if (title == BLURAY_TITLE_TOP_MENU) {
        /* menu call uses different UO mask */
        return bd_menu_call(bd, -1);
    }

    bd_mutex_lock(&bd->mutex);
    ret = _try_play_title(bd, title);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

static int _try_menu_call(BLURAY *bd, int64_t pts)
{
    _set_scr(bd, pts);

    if (bd->title_type == title_undef) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_menu_call(): bd_play() not called\n");
        return 0;
    }

    if (bd->uo_mask.menu_call) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "menu call masked\n");
        _bdj_event(bd, BDJ_EVENT_UO_MASKED, UO_MASK_MENU_CALL_INDEX);
        return 0;
    }

    if (bd->title_type == title_hdmv) {
        if (hdmv_vm_suspend_pl(bd->hdmv_vm) < 0) {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_menu_call(): error storing playback location\n");
        }
    }

    return _play_title(bd, BLURAY_TITLE_TOP_MENU);
}

int bd_menu_call(BLURAY *bd, int64_t pts)
{
    int ret;
    bd_mutex_lock(&bd->mutex);
    ret = _try_menu_call(bd, pts);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

static void _process_hdmv_vm_event(BLURAY *bd, HDMV_EVENT *hev)
{
    BD_DEBUG(DBG_BLURAY, "HDMV event: %d %d\n", hev->event, hev->param);

    switch (hev->event) {
        case HDMV_EVENT_TITLE:
            _close_playlist(bd);
            _play_title(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_PL:
            bd_select_playlist(bd, hev->param);
            /* initialize menus */
            _init_ig_stream(bd);
            _run_gc(bd, GC_CTRL_INIT_MENU, 0);
            break;

        case HDMV_EVENT_PLAY_PI:
            bd_seek_playitem(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_PM:
            bd_seek_mark(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_STOP:
            // stop current playlist
            _close_playlist(bd);

            bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
            break;

        case HDMV_EVENT_STILL:
            _queue_event(bd, BD_EVENT_STILL, hev->param);
            break;

        case HDMV_EVENT_ENABLE_BUTTON:
            _run_gc(bd, GC_CTRL_ENABLE_BUTTON, hev->param);
            break;

        case HDMV_EVENT_DISABLE_BUTTON:
            _run_gc(bd, GC_CTRL_DISABLE_BUTTON, hev->param);
            break;

        case HDMV_EVENT_SET_BUTTON_PAGE:
            _run_gc(bd, GC_CTRL_SET_BUTTON_PAGE, hev->param);
            break;

        case HDMV_EVENT_POPUP_OFF:
            _run_gc(bd, GC_CTRL_POPUP, 0);
            break;

        case HDMV_EVENT_IG_END:
            _run_gc(bd, GC_CTRL_IG_END, 0);
            break;

        case HDMV_EVENT_END:
        case HDMV_EVENT_NONE:
        default:
            break;
    }
}

static int _run_hdmv(BLURAY *bd)
{
    HDMV_EVENT hdmv_ev;

    /* run VM */
    if (hdmv_vm_run(bd->hdmv_vm, &hdmv_ev) < 0) {
        _queue_event(bd, BD_EVENT_ERROR, BD_ERROR_HDMV);
        bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
        return -1;
    }

    /* process all events */
    do {
        _process_hdmv_vm_event(bd, &hdmv_ev);

    } while (!hdmv_vm_get_event(bd->hdmv_vm, &hdmv_ev));

    /* update VM state */
    bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);

    /* update UO mask */
    _update_hdmv_uo_mask(bd);

    return 0;
}

static int _read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event)
{
    if (_get_event(bd, event)) {
        return 0;
    }

    /* run HDMV VM ? */
    if (bd->title_type == title_hdmv) {

        int loops = 0;
        while (!bd->hdmv_suspended) {

            if (_run_hdmv(bd) < 0) {
                BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_read_ext(): HDMV VM error\n");
                bd->title_type = title_undef;
                return -1;
            }
            if (loops++ > 100) {
                /* Detect infinite loops.
                 * Broken disc may cause infinite loop between graphics controller and HDMV VM.
                 * This happens ex. with "Butterfly on a Wheel":
                 * Triggering unmasked "Menu Call" UO in language selection menu skips
                 * menu system initialization code, resulting in infinite loop in root menu.
                 */
                BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_read_ext(): detected possible HDMV mode live lock (%d loops)\n", loops);
                _queue_event(bd, BD_EVENT_ERROR, BD_ERROR_HDMV);
            }
            if (_get_event(bd, event)) {
                return 0;
            }
        }

        if (bd->gc_status & GC_STATUS_ANIMATE) {
            _run_gc(bd, GC_CTRL_NOP, 0);
        }
    }

    if (len < 1) {
        /* just polled events ? */
        return 0;
    }

#ifdef USING_BDJAVA
    if (bd->title_type == title_bdj) {
        if (bd->end_of_playlist == 1) {
            _bdj_event(bd, BDJ_EVENT_END_OF_PLAYLIST, bd_psr_read(bd->regs, PSR_PLAYLIST));
            bd->end_of_playlist |= 2;
        }

        if (!bd->title) {
            /* BD-J title running but no playlist playing */
            _queue_event(bd, BD_EVENT_IDLE, 0);
            return 0;
        }

        if (bd->bdj_wait_start) {
            /* BD-J playlist prefethed but not yet playing */
            _queue_event(bd, BD_EVENT_IDLE, 1);
            return 0;
        }
    }
#endif

    int bytes = _bd_read(bd, buf, len);

    if (bytes == 0) {

        // if no next clip (=end of title), resume HDMV VM
        if (!bd->st0.clip && bd->title_type == title_hdmv) {
            hdmv_vm_resume(bd->hdmv_vm);
            bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
            BD_DEBUG(DBG_BLURAY, "bd_read_ext(): reached end of playlist. hdmv_suspended=%d\n", bd->hdmv_suspended);
        }
    }

    _get_event(bd, event);

    return bytes;
}

int bd_read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event)
{
    int ret;
    bd_mutex_lock(&bd->mutex);
    ret = _read_ext(bd, buf, len, event);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

int bd_get_event(BLURAY *bd, BD_EVENT *event)
{
    if (!bd->event_queue) {
        bd->event_queue = event_queue_new(sizeof(BD_EVENT));

        bd_psr_register_cb(bd->regs, _process_psr_event, bd);
        _queue_initial_psr_events(bd);
    }

    if (event) {
        return _get_event(bd, event);
    }

    return 0;
}

/*
 * user interaction
 */

void bd_set_scr(BLURAY *bd, int64_t pts)
{
    bd_mutex_lock(&bd->mutex);
    bd->app_scr = 1;
    _set_scr(bd, pts);
    bd_mutex_unlock(&bd->mutex);
}

static int _set_rate(BLURAY *bd, uint32_t rate)
{
    if (!bd->title) {
        return -1;
    }

#ifdef USING_BDJAVA
    if (bd->title_type == title_bdj) {
        return _bdj_event(bd, BDJ_EVENT_RATE, rate);
    }
#endif

    return 0;
}

int bd_set_rate(BLURAY *bd, uint32_t rate)
{
    int result;

    bd_mutex_lock(&bd->mutex);
    result = _set_rate(bd, rate);
    bd_mutex_unlock(&bd->mutex);

    return result;
}

int bd_mouse_select(BLURAY *bd, int64_t pts, uint16_t x, uint16_t y)
{
    uint32_t param = (x << 16) | y;
    int result = -1;

    bd_mutex_lock(&bd->mutex);

    _set_scr(bd, pts);

    if (bd->title_type == title_hdmv) {
        result = _run_gc(bd, GC_CTRL_MOUSE_MOVE, param);
#ifdef USING_BDJAVA
    } else if (bd->title_type == title_bdj) {
        result = _bdj_event(bd, BDJ_EVENT_MOUSE, param);
#endif
    }

    bd_mutex_unlock(&bd->mutex);

    return result;
}

int bd_user_input(BLURAY *bd, int64_t pts, uint32_t key)
{
    int result = -1;

    if (key == BD_VK_ROOT_MENU) {
        return bd_menu_call(bd, pts);
    }

    bd_mutex_lock(&bd->mutex);

    _set_scr(bd, pts);

    if (bd->title_type == title_hdmv) {
        result = _run_gc(bd, GC_CTRL_VK_KEY, key);
#ifdef USING_BDJAVA
    } else if (bd->title_type == title_bdj) {
        result = _bdj_event(bd, BDJ_EVENT_VK_KEY, key);
#endif
    }

    bd_mutex_unlock(&bd->mutex);

    return result;
}

void bd_register_overlay_proc(BLURAY *bd, void *handle, bd_overlay_proc_f func)
{
    if (!bd) {
        return;
    }

    bd_mutex_lock(&bd->mutex);

    gc_free(&bd->graphics_controller);

    if (func) {
        bd->graphics_controller = gc_init(bd->regs, handle, func);
    }

    bd_mutex_unlock(&bd->mutex);
}

void bd_register_argb_overlay_proc(BLURAY *bd, void *handle, bd_argb_overlay_proc_f func, BD_ARGB_BUFFER *buf)
{
#ifdef USING_BDJAVA
    if (!bd) {
        return;
    }

    bd_mutex_lock(&bd->argb_buffer_mutex);

    bd->argb_overlay_proc        = func;
    bd->argb_overlay_proc_handle = handle;
    bd->argb_buffer              = buf;

    bd_mutex_unlock(&bd->argb_buffer_mutex);
#else
    (void)bd;
    (void)handle;
    (void)func;
    (void)buf;
#endif
}

int bd_get_sound_effect(BLURAY *bd, unsigned sound_id, BLURAY_SOUND_EFFECT *effect)
{
    if (!bd || !effect) {
        return -1;
    }

    if (!bd->sound_effects) {

        bd->sound_effects = sound_get(bd->disc);
        if (!bd->sound_effects) {
            return -1;
        }
    }

    if (sound_id < bd->sound_effects->num_sounds) {
        SOUND_OBJECT *o = &bd->sound_effects->sounds[sound_id];

        effect->num_channels = o->num_channels;
        effect->num_frames   = o->num_frames;
        effect->samples      = (const int16_t *)o->samples;

        return 1;
    }

    return 0;
}

/*
 * Direct file access
 */

static int _bd_read_file(BLURAY *bd, const char *dir, const char *file, void **data, int64_t *size)
{
    if (!bd || !bd->disc || !file || !data || !size) {
        BD_DEBUG(DBG_CRIT, "Invalid arguments for bd_read_file()\n");
        return 0;
    }

    *data = NULL;
    *size = (int64_t)disc_read_file(bd->disc, dir, file, (uint8_t**)data);
    if (!*data || *size < 0) {
        BD_DEBUG(DBG_BLURAY, "bd_read_file() failed\n");
        X_FREE(*data);
        return 0;
    }

    BD_DEBUG(DBG_BLURAY, "bd_read_file(): read %"PRId64" bytes from %s"DIR_SEP"%s\n",
             *size, dir, file);
    return 1;
}

int bd_read_file(BLURAY *bd, const char *path, void **data, int64_t *size)
{
    return _bd_read_file(bd, NULL, path, data, size);
}


/*
 * Metadata
 */

const struct meta_dl *bd_get_meta(BLURAY *bd)
{
    const struct meta_dl *meta = NULL;

    if (!bd) {
        return NULL;
    }

    if (!bd->meta) {
        _meta_open(bd);
    }

    uint32_t psr_menu_lang = bd_psr_read(bd->regs, PSR_MENU_LANG);

    if (psr_menu_lang != 0 && psr_menu_lang != 0xffffff) {
        const char language_code[] = {(psr_menu_lang >> 16) & 0xff, (psr_menu_lang >> 8) & 0xff, psr_menu_lang & 0xff, 0 };
        meta = meta_get(bd->meta, language_code);
    } else {
        meta = meta_get(bd->meta, NULL);
    }

    /* assign title names to disc_info */
    if (meta && bd->titles) {
        unsigned ii;
        for (ii = 0; ii < meta->toc_count; ii++) {
            if (meta->toc_entries[ii].title_number > 0 && meta->toc_entries[ii].title_number <= bd->disc_info.num_titles) {
                bd->titles[meta->toc_entries[ii].title_number]->name = meta->toc_entries[ii].title_name;
            }
        }
    }

    return meta;
}

int bd_get_meta_file(BLURAY *bd, const char *name, void **data, int64_t *size)
{
    return _bd_read_file(bd, DIR_SEP "BDMV" DIR_SEP "META" DIR_SEP "DL", name, data, size);
}

/*
 * Database access
 */

struct clpi_cl *bd_get_clpi(BLURAY *bd, unsigned clip_ref)
{
    if (bd->title && clip_ref < bd->title->clip_list.count) {
        NAV_CLIP *clip = &bd->title->clip_list.clip[clip_ref];
        return clpi_copy(clip->cl);
    }
    return NULL;
}

struct clpi_cl *bd_read_clpi(const char *path)
{
    return clpi_parse(path);
}

void bd_free_clpi(struct clpi_cl *cl)
{
    clpi_free(cl);
}

struct mpls_pl *bd_read_mpls(const char *mpls_file)
{
    return mpls_parse(mpls_file);
}

void bd_free_mpls(struct mpls_pl *pl)
{
    mpls_free(pl);
}

struct mobj_objects *bd_read_mobj(const char *mobj_file)
{
    return mobj_parse(mobj_file);
}

void bd_free_mobj(struct mobj_objects *obj)
{
    mobj_free(&obj);
}

struct bdjo_data *bd_read_bdjo(const char *bdjo_file)
{
#ifdef USING_BDJAVA
    return bdjo_parse(bdjo_file);
#else
    (void)bdjo_file;
    return NULL;
#endif
}

void bd_free_bdjo(struct bdjo_data *obj)
{
#ifdef USING_BDJAVA
    bdjo_free(&obj);
#else
    (void)obj;
#endif
}
