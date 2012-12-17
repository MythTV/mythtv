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
#include "register.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"
#include "util/mutex.h"
#include "bdnav/navigation.h"
#include "bdnav/index_parse.h"
#include "bdnav/meta_parse.h"
#include "bdnav/clpi_parse.h"
#include "bdnav/sound_parse.h"
#include "hdmv/hdmv_vm.h"
#include "decoders/graphics_controller.h"
#include "file/file.h"
#include "file/dl.h"
#ifdef USING_BDJAVA
#include "bdj/bdj.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#ifdef HAVE_MNTENT_H
#include <sys/stat.h>
#include <mntent.h>
#endif

#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#endif

#ifdef __cplusplus
typedef int     (*fptr_int)(...);
typedef int32_t (*fptr_int32)(...);
typedef void*   (*fptr_p_void)(...);
#else
typedef int     (*fptr_int)();
typedef int32_t (*fptr_int32)();
typedef void*   (*fptr_p_void)();
#endif

#define MAX_EVENTS 31  /* 2^n - 1 */
typedef struct bd_event_queue_s {
    BD_MUTEX mutex;
    unsigned in;  /* next free slot */
    unsigned out; /* next event */
    BD_EVENT ev[MAX_EVENTS+1];
} BD_EVENT_QUEUE;

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

    BD_UO_MASK     uo_mask;

} BD_STREAM;

typedef struct {
    NAV_CLIP *clip;
    uint64_t  clip_size;
    uint8_t  *buf;
} BD_PRELOAD;

struct bluray {

    BD_MUTEX          mutex;  /* protect API function access to internal data */

    /* current disc */
    char             *device_path;
    BLURAY_DISC_INFO  disc_info;
    INDX_ROOT        *index;
    META_ROOT        *meta;
    NAV_TITLE_LIST   *title_list;

    /* current playlist */
    NAV_TITLE      *title;
    uint32_t       title_idx;
    uint64_t       s_pos;

    /* streams */
    BD_STREAM      st0; /* main path */
    BD_PRELOAD     st_ig; /* preloaded IG stream sub path */
    int            ig_pid; /* pid of currently selected IG stream in main path */

    /* buffer for bd_read(): current aligned unit of main stream (st0) */
    uint8_t        int_buf[6144];

    /* seamless angle change request */
    int            seamless_angle_change;
    uint32_t       angle_change_pkt;
    uint32_t       angle_change_time;
    unsigned       request_angle;

    /* chapter tracking */
    uint64_t       next_chapter_start;

    /* aacs */
    void           *h_libaacs;   // library handle
    void           *aacs;
    fptr_p_void    libaacs_open;
    fptr_int       libaacs_decrypt_unit;

    /* BD+ */
    void           *h_libbdplus; // library handle
    void           *bdplus;
    fptr_p_void    bdplus_init;
    fptr_int32     bdplus_seek;
    fptr_int32     bdplus_fixup;

    /* player state */
    BD_REGISTERS   *regs;       // player registers
    BD_EVENT_QUEUE *event_queue; // navigation mode event queue
    BD_TITLE_TYPE  title_type;  // type of current title (in navigation mode)

    HDMV_VM        *hdmv_vm;
    uint8_t        hdmv_suspended;
#ifdef USING_BDJAVA
    BDJAVA         *bdjava;
#endif

    /* graphics */
    GRAPHICS_CONTROLLER *graphics_controller;
    SOUND_DATA          *sound_effects;
    uint32_t             gc_status;
};

#define DL_CALL(lib,func,param,...)             \
     do {                                           \
          fptr_p_void fptr = (fptr_p_void)dl_dlsym(lib, #func);  \
          if (fptr) {                               \
              fptr(param, ##__VA_ARGS__);           \
          }                                         \
      } while (0)

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

static void _init_event_queue(BLURAY *bd)
{
    if (!bd->event_queue) {
        bd->event_queue = calloc(1, sizeof(struct bd_event_queue_s));
        bd_mutex_init(&bd->event_queue->mutex);
    } else {
        bd_mutex_lock(&bd->event_queue->mutex);
        bd->event_queue->in  = 0;
        bd->event_queue->out = 0;
        memset(bd->event_queue->ev, 0, sizeof(bd->event_queue->ev));
        bd_mutex_unlock(&bd->event_queue->mutex);
    }
}

static void _free_event_queue(BLURAY *bd)
{
    if (bd->event_queue) {
        bd_mutex_destroy(&bd->event_queue->mutex);
        X_FREE(bd->event_queue);
    }
}

static int _get_event(BLURAY *bd, BD_EVENT *ev)
{
    struct bd_event_queue_s *eq = bd->event_queue;

    if (eq) {
        bd_mutex_lock(&eq->mutex);

        if (eq->in != eq->out) {

            *ev = eq->ev[eq->out];
            eq->out = (eq->out + 1) & MAX_EVENTS;

            bd_mutex_unlock(&eq->mutex);
            return 1;
        }

        bd_mutex_unlock(&eq->mutex);
    }

    ev->event = BD_EVENT_NONE;

    return 0;
}

static int _queue_event(BLURAY *bd, uint32_t event, uint32_t param)
{
    struct bd_event_queue_s *eq = bd->event_queue;

    if (eq) {
        bd_mutex_lock(&eq->mutex);

        unsigned new_in = (eq->in + 1) & MAX_EVENTS;

        if (new_in != eq->out) {
            eq->ev[eq->in].event = event;
            eq->ev[eq->in].param = param;
            eq->in = new_in;

            bd_mutex_unlock(&eq->mutex);
            return 1;
        }

        bd_mutex_unlock(&eq->mutex);

        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "_queue_event(%d, %d): queue overflow !\n", event, param);
    }

    return 0;
}

/*
 * PSR utils
 */

static void _update_stream_psr_by_lang(BD_REGISTERS *regs,
                                       uint32_t psr_lang, uint32_t psr_stream,
                                       uint32_t enable_flag,
                                       MPLS_STREAM *streams, unsigned num_streams,
                                       uint32_t *lang, uint32_t blacklist)
{
    uint32_t psr_val;
    int      stream_idx = -1;
    unsigned ii;

    /* get preferred language */
    psr_val = bd_psr_read(regs, psr_lang);
    if (psr_val == 0xffffff) {
        /* language setting not initialized */
        return;
    }

    /* find stream */

    for (ii = 0; ii < num_streams; ii++) {
        if (psr_val == str_to_uint32((const char *)streams[ii].lang, 3)) {
            stream_idx = ii;
            break;
        }
    }

    if (stream_idx < 0) {
        /* requested language not found */
        stream_idx = 0;
        enable_flag = 0;

    } else {
        if (lang) {
            *lang = psr_val;
        }
        if (blacklist == psr_val) {
            enable_flag = 0;
        }
    }

    /* update PSR */

    BD_DEBUG(DBG_BLURAY, "Selected stream %d (language %s)\n", stream_idx, streams[stream_idx].lang);

    bd_psr_lock(regs);

    psr_val = bd_psr_read(regs, psr_stream) & 0xffff0000;
    psr_val |= (stream_idx + 1) | enable_flag;
    bd_psr_write(regs, psr_stream, psr_val);

    bd_psr_unlock(regs);
}

static void _update_clip_psrs(BLURAY *bd, NAV_CLIP *clip)
{
    bd_psr_write(bd->regs, PSR_PLAYITEM, clip->ref);
    bd_psr_write(bd->regs, PSR_TIME,     clip->in_time);

    /* Update selected audio and subtitle stream PSRs when not using menus.
     * Selection is based on language setting PSRs and clip STN.
     */
    if (bd->title_type == title_undef) {
        MPLS_STN *stn = &clip->title->pl->play_item[clip->ref].stn;
        uint32_t audio_lang = 0;

        _update_stream_psr_by_lang(bd->regs,
                                   PSR_AUDIO_LANG, PSR_PRIMARY_AUDIO_ID, 0,
                                   stn->audio, stn->num_audio,
                                   &audio_lang, 0);
        _update_stream_psr_by_lang(bd->regs,
                                   PSR_PG_AND_SUB_LANG, PSR_PG_STREAM, 0x80000000,
                                   stn->pg, stn->num_pg,
                                   NULL, audio_lang);
    }
}


static void _update_chapter_psr(BLURAY *bd)
{
    uint32_t current_chapter = bd_get_current_chapter(bd);
    bd->next_chapter_start = bd_chapter_pos(bd, current_chapter + 1);
    bd_psr_write(bd->regs, PSR_CHAPTER,  current_chapter + 1);
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

    /* reset UO mask */
    memset(&st->uo_mask, 0, sizeof(st->uo_mask));
}

static int _open_m2ts(BLURAY *bd, BD_STREAM *st)
{
    char *f_name;

    _close_m2ts(st);

    f_name = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "STREAM" DIR_SEP "%s",
                        bd->device_path, st->clip->name);

    st->clip_pos = (uint64_t)st->clip->start_pkt * 192;
    st->clip_block_pos = (st->clip_pos / 6144) * 6144;

    if ((st->fp = file_open(f_name, "rb"))) {
        file_seek(st->fp, 0, SEEK_END);
        if ((st->clip_size = file_tell(st->fp))) {
            file_seek(st->fp, st->clip_block_pos, SEEK_SET);
            st->int_buf_off = 6144;
            X_FREE(f_name);

            if (bd->bdplus) {
                DL_CALL(bd->h_libbdplus, bdplus_set_title,
                        bd->bdplus, st->clip->clip_id);
            }

            if (bd->aacs) {
                uint32_t title = bd_psr_read(bd->regs, PSR_TITLE_NUMBER);
                DL_CALL(bd->h_libaacs, aacs_select_title,
                        bd->aacs, title);
            }

            if (st == &bd->st0) {
                MPLS_PL *pl = st->clip->title->pl;
                st->uo_mask = bd_uo_mask_combine(pl->app_info.uo_mask,
                                                 pl->play_item[st->clip->ref].uo_mask);

                _update_clip_psrs(bd, st->clip);
            }

            return 1;
        }

        BD_DEBUG(DBG_BLURAY, "Clip %s empty! (%p)\n", f_name, bd);
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open clip %s! (%p)\n",
          f_name, bd);

    X_FREE(f_name);
    return 0;
}

static int _read_block(BLURAY *bd, BD_STREAM *st, uint8_t *buf)
{
    const int len = 6144;

    if (st->fp) {
        BD_DEBUG(DBG_STREAM, "Reading unit [%d bytes] at %"PRIu64"... (%p)\n",
              len, st->clip_block_pos, bd);

        if (len + st->clip_block_pos <= st->clip_size) {
            int read_len;

            if ((read_len = file_read(st->fp, buf, len))) {
                if (read_len != len)
                    BD_DEBUG(DBG_STREAM | DBG_CRIT, "Read %d bytes at %"PRIu64" ; requested %d ! (%p)\n", read_len, st->clip_block_pos, len, bd);

                if (bd->aacs && bd->libaacs_decrypt_unit) {
                    if (!bd->libaacs_decrypt_unit(bd->aacs, buf)) {
                        BD_DEBUG(DBG_AACS | DBG_CRIT, "Unable decrypt unit (AACS)! (%p)\n", bd);

                        return -1;
                    } // decrypt
                } // aacs

                st->clip_block_pos += len;

                // bdplus fixup, if required.
                if (bd->bdplus_fixup && bd->bdplus) {
                    int32_t numFixes;
                    numFixes = bd->bdplus_fixup(bd->bdplus, len, buf);
#if 1
                    if (numFixes) {
                        BD_DEBUG(DBG_BDPLUS,
                              "BDPLUS did %u fixups\n", numFixes);
                    }
#endif

                }

                /* Check TP_extra_header Copy_permission_indicator. If != 0, unit is still encrypted. */
                if (buf[0] & 0xc0) {
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT,
                          "TP header copy permission indicator != 0, unit is still encrypted? (%p)\n", bd);
                    _queue_event(bd, BD_EVENT_ENCRYPTED, 0);
                    return -1;
                }

                BD_DEBUG(DBG_STREAM, "Read unit OK! (%p)\n", bd);

#ifdef BLURAY_READ_ERROR_TEST
                /* simulate broken blocks */
                if (random() % 1000)
#else
                return 1;
#endif
            }

            BD_DEBUG(DBG_STREAM | DBG_CRIT, "Read %d bytes at %"PRIu64" failed ! (%p)\n", len, st->clip_block_pos, bd);

            _queue_event(bd, BD_EVENT_READ_ERROR, 0);

            /* skip broken unit */
            st->clip_block_pos += len;
            st->clip_pos += len;
            file_seek(st->fp, st->clip_block_pos, SEEK_SET);

            return 0;
        }

        BD_DEBUG(DBG_STREAM | DBG_CRIT, "Read past EOF ! (%p)\n", bd);

        /* This is caused by truncated .m2ts file or invalid clip length.
         *
         * Increase position to avoid infinite loops.
         * Next clip won't be selected until all packets of this clip have been read.
         */
        st->clip_block_pos += len;
        st->clip_pos += len;

        return 0;
    }

    BD_DEBUG(DBG_BLURAY, "No valid title selected! (%p)\n", bd);

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

static int _preload_m2ts(BLURAY *bd, BD_PRELOAD *p)
{
    /* setup and open BD_STREAM */

    BD_STREAM st;

    memset(&st, 0, sizeof(st));
    st.clip = p->clip;

    if (!_open_m2ts(bd, &st)) {
        return 0;
    }

    /* allocate buffer */
    p->clip_size = st.clip_size;
    p->buf       = realloc(p->buf, p->clip_size);

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

    st->clip_pos = (uint64_t)clip_pkt * 192;
    st->clip_block_pos = (st->clip_pos / 6144) * 6144;

    file_seek(st->fp, st->clip_block_pos, SEEK_SET);

    st->int_buf_off = 6144;

    return st->clip_pos;
}

/*
 * Graphics controller interface
 */

static int _run_gc(BLURAY *bd, gc_ctrl_e msg, uint32_t param)
{
    int result = -1;

    if (bd && bd->graphics_controller && bd->hdmv_vm) {
        GC_NAV_CMDS cmds = {-1, NULL, -1, 0};

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
 * libaacs and libbdplus open / close
 */

static void _libaacs_close(BLURAY *bd)
{
    if (bd->aacs) {
        DL_CALL(bd->h_libaacs, aacs_close, bd->aacs);
        bd->aacs = NULL;
    }
}

static void _libaacs_unload(BLURAY *bd)
{
    _libaacs_close(bd);

    if (bd->h_libaacs) {
        dl_dlclose(bd->h_libaacs);
        bd->h_libaacs = NULL;
    }

    bd->libaacs_open         = NULL;
    bd->libaacs_decrypt_unit = NULL;
}

static int _libaacs_required(BLURAY *bd)
{
    BD_FILE_H *fd;
    char      *tmp;

    tmp = str_printf("%s/AACS/Unit_Key_RO.inf", bd->device_path);
    fd = file_open(tmp, "rb");
    X_FREE(tmp);

    if (fd) {
        file_close(fd);

        BD_DEBUG(DBG_BLURAY, "AACS/Unit_Key_RO.inf found. Disc seems to be AACS protected (%p)\n", bd);
        bd->disc_info.aacs_detected = 1;
        return 1;
    }

    BD_DEBUG(DBG_BLURAY, "AACS/Unit_Key_RO.inf not found. No AACS protection (%p)\n", bd);
    bd->disc_info.aacs_detected = 0;
    return 0;
}

static int _libaacs_load(BLURAY *bd)
{
    if (bd->h_libaacs) {
        return 1;
    }

    bd->disc_info.libaacs_detected = 0;
    if ((bd->h_libaacs = dl_dlopen("libaacs", "0"))) {

        BD_DEBUG(DBG_BLURAY, "Loading libaacs (%p)\n", bd->h_libaacs);

        bd->libaacs_open         = (fptr_p_void)dl_dlsym(bd->h_libaacs, "aacs_open");
        bd->libaacs_decrypt_unit = (fptr_int)dl_dlsym(bd->h_libaacs, "aacs_decrypt_unit");

        if (bd->libaacs_open && bd->libaacs_decrypt_unit) {
            BD_DEBUG(DBG_BLURAY, "Loaded libaacs (%p)\n", bd->h_libaacs);
            bd->disc_info.libaacs_detected = 1;

            if (file_open != file_open_default()) {
                BD_DEBUG(DBG_BLURAY, "Registering libaacs filesystem handler %p (%p)\n", file_open, bd->h_libaacs);
                DL_CALL(bd->h_libaacs, aacs_register_file, file_open);
            }

            return 1;

        } else {
            BD_DEBUG(DBG_BLURAY, "libaacs dlsym failed! (%p)\n", bd->h_libaacs);
        }

    } else {
        BD_DEBUG(DBG_BLURAY, "libaacs not found! (%p)\n", bd);
    }

    _libaacs_unload(bd);

    return 0;
}

static int _libaacs_open(BLURAY *bd, const char *keyfile_path)
{
    _libaacs_close(bd);

    if (!_libaacs_required(bd)) {
        /* no AACS */
        return 1; /* no error if libaacs is not needed */
    }

    if (!_libaacs_load(bd)) {
        /* no libaacs */
        return 0;
    }

    int error_code = 0;
    fptr_p_void aacs_open2 = (fptr_p_void)dl_dlsym(bd->h_libaacs, "aacs_open2");
    if (!aacs_open2) {
        BD_DEBUG(DBG_BLURAY, "Using old aacs_open(), no verbose error reporting available (%p)\n", bd->aacs);
        bd->aacs = bd->libaacs_open(bd->device_path, keyfile_path);
    } else {
        bd->aacs = aacs_open2(bd->device_path, keyfile_path, &error_code);
    }

    if (bd->aacs) {
        fptr_int    aacs_get_mkb_version = (fptr_int)    dl_dlsym(bd->h_libaacs, "aacs_get_mkb_version");
        fptr_p_void aacs_get_disc_id     = (fptr_p_void) dl_dlsym(bd->h_libaacs, "aacs_get_disc_id");
        if (aacs_get_mkb_version) {
            bd->disc_info.aacs_mkbv = aacs_get_mkb_version(bd->aacs);
        }
        if (aacs_get_disc_id) {
            memcpy(bd->disc_info.disc_id, aacs_get_disc_id(bd->aacs), 20);
        }

        if (!error_code) {
            BD_DEBUG(DBG_BLURAY, "Opened libaacs (%p)\n", bd->aacs);
            bd->disc_info.aacs_handled = 1;
            return 1;
        }
    }

    BD_DEBUG(DBG_BLURAY, "aacs_open() failed!\n");
    bd->disc_info.aacs_handled = 0;

    switch (error_code) {
        case 0: /* AACS_SUCCESS */
            break;
        case -1: /* AACS_ERROR_CORRUPTED_DISC */
            bd->disc_info.aacs_error_code = BD_AACS_CORRUPTED_DISC;
            break;
        case -2: /* AACS_ERROR_NO_CONFIG */
            bd->disc_info.aacs_error_code = BD_AACS_NO_CONFIG;
            break;
        case -3: /* AACS_ERROR_NO_PK */
            bd->disc_info.aacs_error_code = BD_AACS_NO_PK;
            break;
        case -4: /* AACS_ERROR_NO_CERT */
            bd->disc_info.aacs_error_code = BD_AACS_NO_CERT;
            break;
        case -5: /* AACS_ERROR_CERT_REVOKED */
            bd->disc_info.aacs_error_code = BD_AACS_CERT_REVOKED;
            break;
        case -6: /* AACS_ERROR_MMC_OPEN */
        case -7: /* AACS_ERROR_MMC_FAILURE */
            bd->disc_info.aacs_error_code = BD_AACS_MMC_FAILED;
            break;
    }

    _libaacs_unload(bd);
    return 0;
}

static uint8_t *_libaacs_get_vid(BLURAY *bd)
{
    if (bd->aacs) {
        fptr_p_void fptr = (fptr_p_void)dl_dlsym(bd->h_libaacs, "aacs_get_vid");
        if (fptr) {
            return (uint8_t*)fptr(bd->aacs);
        }
        BD_DEBUG(DBG_BLURAY, "aacs_get_vid() dlsym failed! (%p)", bd);
        return NULL;
    }

    BD_DEBUG(DBG_BLURAY, "_libaacs_get_vid(): libaacs not initialized! (%p)", bd);
    return NULL;
}

static void _libbdplus_close(BLURAY *bd)
{
    if (bd->bdplus) {
        DL_CALL(bd->h_libbdplus, bdplus_free, bd->bdplus);
        bd->bdplus = NULL;
    }
}

static void _libbdplus_unload(BLURAY *bd)
{
    _libbdplus_close(bd);

    if (bd->h_libbdplus) {
        dl_dlclose(bd->h_libbdplus);
        bd->h_libbdplus = NULL;
    }

    bd->bdplus_init  = NULL;
    bd->bdplus_seek  = NULL;
    bd->bdplus_fixup = NULL;
}

static int _libbdplus_required(BLURAY *bd)
{
    BD_FILE_H *fd;
    char      *tmp;

    tmp = str_printf("%s/BDSVM/00000.svm", bd->device_path);
    fd = file_open(tmp, "rb");
    X_FREE(tmp);

    if (fd) {
        file_close(fd);

        BD_DEBUG(DBG_BLURAY, "BDSVM/00000.svm found. Disc seems to be BD+ protected (%p)\n", bd);
        bd->disc_info.bdplus_detected = 1;
        return 1;
    }

    BD_DEBUG(DBG_BLURAY, "BDSVM/00000.svm not found. No BD+ protection (%p)\n", bd);
    bd->disc_info.bdplus_detected = 0;
    return 0;
}

static int _libbdplus_load(BLURAY *bd)
{
    BD_DEBUG(DBG_BDPLUS, "attempting to load libbdplus\n");

    if (bd->h_libbdplus) {
        return 1;
    }

    bd->disc_info.libbdplus_detected = 0;
    if ((bd->h_libbdplus = dl_dlopen("libbdplus", "0"))) {

        BD_DEBUG(DBG_BLURAY, "Loading libbdplus (%p)\n", bd->h_libbdplus);

        bd->bdplus_init  = (fptr_p_void)dl_dlsym(bd->h_libbdplus, "bdplus_init");
        bd->bdplus_seek  = (fptr_int32)dl_dlsym(bd->h_libbdplus, "bdplus_seek");
        bd->bdplus_fixup = (fptr_int32)dl_dlsym(bd->h_libbdplus, "bdplus_fixup");

        if (bd->bdplus_init && bd->bdplus_seek && bd->bdplus_fixup) {
            BD_DEBUG(DBG_BLURAY, "Loaded libbdplus (%p)\n", bd->h_libbdplus);
            bd->disc_info.libbdplus_detected = 1;
            return 1;
        }

        BD_DEBUG(DBG_BLURAY, "libbdplus dlsym failed! (%p)\n", bd->h_libbdplus);

    } else {
        BD_DEBUG(DBG_BLURAY, "libbdplus not found! (%p)\n", bd);
    }

    _libbdplus_unload(bd);

    return 0;
}

static int _libbdplus_open(BLURAY *bd, const char *keyfile_path)
{
    // Take a quick stab to see if we want/need bdplus
    // we should fix this, and add various string functions.
    uint8_t vid[16] = {
        0xC5,0x43,0xEF,0x2A,0x15,0x0E,0x50,0xC4,0xE2,0xCA,
        0x71,0x65,0xB1,0x7C,0xA7,0xCB}; // FIXME

    _libbdplus_close(bd);

    if (!_libbdplus_required(bd)) {
        /* no BD+ */
        return 1; /* no error if libbdplus is not needed */
    }

    if (!_libbdplus_load(bd)) {
        /* no libbdplus */
        return 0;
    }

    const uint8_t *aacs_vid = (const uint8_t *)_libaacs_get_vid(bd);
    bd->bdplus = bd->bdplus_init(bd->device_path, keyfile_path, aacs_vid ? aacs_vid : vid);

    if (bd->bdplus) {
        BD_DEBUG(DBG_BLURAY,"libbdplus initialized\n");
        bd->disc_info.bdplus_handled = 1;
        return 1;
    }

    BD_DEBUG(DBG_BLURAY,"bdplus_init() failed\n");
    bd->disc_info.bdplus_handled = 0;

    _libbdplus_unload(bd);
    return 0;
}

/*
 * index open
 */

static int _index_open(BLURAY *bd)
{
    if (!bd->index) {
        char *file;

        file = str_printf("%s/BDMV/index.bdmv", bd->device_path);
        bd->index = indx_parse(file);
        X_FREE(file);
    }

    return !!bd->index;
}

/*
 * meta open
 */

static int _meta_open(BLURAY *bd)
{
    if (!bd->meta) {
        bd->meta = meta_parse(bd->device_path);
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

static void _fill_disc_info(BLURAY *bd)
{
    bd->disc_info.bluray_detected        = 0;
    bd->disc_info.top_menu_supported     = 0;
    bd->disc_info.first_play_supported   = 0;
    bd->disc_info.num_hdmv_titles        = 0;
    bd->disc_info.num_bdj_titles         = 0;
    bd->disc_info.num_unsupported_titles = 0;

    if (bd->index) {
        INDX_PLAY_ITEM *pi;
        unsigned        ii;

        bd->disc_info.bluray_detected = 1;

        pi = &bd->index->first_play;
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            bd->disc_info.first_play_supported = 1;
        }

        pi = &bd->index->top_menu;
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            bd->disc_info.top_menu_supported = 1;
        }

        for (ii = 0; ii < bd->index->num_titles; ii++) {
            if (bd->index->titles[ii].object_type == indx_object_type_hdmv) {
                bd->disc_info.num_hdmv_titles++;
            }
            if (bd->index->titles[ii].object_type == indx_object_type_bdj) {
                bd->disc_info.num_bdj_titles++;
                bd->disc_info.num_unsupported_titles++;
            }
        }
    }
}

/*
 * bdj
 */

static int _start_bdj(BLURAY *bd, unsigned title)
{
#ifdef USING_BDJAVA
    if (bd->bdjava == NULL) {
        bd->bdjava = bdj_open(bd->device_path, bd, bd->regs, bd->index);
        if (!bd->bdjava) {
            return 0;
        }
    }
    return bdj_start(bd->bdjava, title);
#else
    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Title %d: BD-J not compiled in (%p)\n", title, bd);
    return 0;
#endif
}

#ifdef USING_BDJAVA
static void _bdj_event(BLURAY *bd, unsigned ev, unsigned param)
{
    if (bd->bdjava != NULL) {
        bdj_process_event(bd->bdjava, ev, param);
    }
}
#else
#define _bdj_event(bd, ev, param) do{}while(0)
#endif

static void _stop_bdj(BLURAY *bd)
{
#ifdef USING_BDJAVA
    if (bd->bdjava != NULL) {
        bdj_stop(bd->bdjava);
    }
#endif
}

static void _close_bdj(BLURAY *bd)
{
#ifdef USING_BDJAVA
    if (bd->bdjava != NULL) {
        bdj_close(bd->bdjava);
        bd->bdjava = NULL;
    }
#endif
}

#ifdef HAVE_MNTENT_H
/*
 * Replace device node (/dev/sr0) by mount point
 * Implemented on Linux and MacOSX
 */
static void get_mount_point(BLURAY *bd)
{
    struct stat st;
    if (stat (bd->device_path, &st) )
        return;

    /* If it's a directory, all is good */
    if (S_ISDIR(st.st_mode))
        return;

    FILE *f = setmntent ("/proc/self/mounts", "r");
    if (!f)
        return;

    struct mntent* m;
#ifdef HAVE_GETMNTENT_R
    struct mntent mbuf;
    char buf [8192];
    while ((m = getmntent_r (f, &mbuf, buf, sizeof(buf))) != NULL) {
#else
    while ((m = getmntent (f)) != NULL) {
#endif
        if (!strcmp (m->mnt_fsname, bd->device_path)) {
            free(bd->device_path);
            bd->device_path = str_dup (m->mnt_dir);
            break;
        }
    }
    endmntent (f);
}
#endif
#ifdef __APPLE__
static void get_mount_point(BLURAY *bd)
{
    struct stat st;
    if (stat (bd->device_path, &st) )
        return;

    /* If it's a directory, all is good */
    if (S_ISDIR(st.st_mode))
        return;

    struct statfs mbuf[128];
    int fs_count;

    if ( (fs_count = getfsstat (NULL, 0, MNT_NOWAIT)) == -1 ) {
        return;
    }

    getfsstat (mbuf, fs_count * sizeof(mbuf[0]), MNT_NOWAIT);

    for ( int i = 0; i < fs_count; ++i) {
        if (!strcmp (mbuf[i].f_mntfromname, bd->device_path)) {
            free(bd->device_path);
            bd->device_path = str_dup (mbuf[i].f_mntonname);
        }
    }
}
#endif

/*
 * open / close
 */

BLURAY *bd_open(const char* device_path, const char* keyfile_path)
{
    BD_DEBUG(DBG_BLURAY, "libbluray version "BLURAY_VERSION_STRING"\n");

    if (!device_path) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No device path provided!\n");
        return NULL;
    }

    BLURAY *bd = calloc(1, sizeof(BLURAY));

    if (!bd) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't allocate memory\n");
        return NULL;
    }

    bd->device_path = str_dup(device_path);

#if (defined HAVE_MNTENT_H || defined __APPLE__)
    get_mount_point(bd);
#endif

    _libaacs_open(bd, keyfile_path);

    _libbdplus_open(bd, keyfile_path);

    _index_open(bd);

    bd->regs = bd_registers_init();

    _fill_disc_info(bd);

    bd_mutex_init(&bd->mutex);

    BD_DEBUG(DBG_BLURAY, "BLURAY initialized! (%p)\n", bd);

    return bd;
}

void bd_close(BLURAY *bd)
{
    _close_bdj(bd);

    _libaacs_unload(bd);

    _libbdplus_unload(bd);

    _close_m2ts(&bd->st0);
    _close_preload(&bd->st_ig);

    if (bd->title_list != NULL) {
        nav_free_title_list(bd->title_list);
    }
    if (bd->title != NULL) {
        nav_title_close(bd->title);
    }

    hdmv_vm_free(&bd->hdmv_vm);

    gc_free(&bd->graphics_controller);
    indx_free(&bd->index);
    meta_free(&bd->meta);
    sound_free(&bd->sound_effects);
    bd_registers_free(bd->regs);

    _free_event_queue(bd);
    X_FREE(bd->device_path);

    bd_mutex_destroy(&bd->mutex);

    BD_DEBUG(DBG_BLURAY, "BLURAY destroyed! (%p)\n", bd);

    X_FREE(bd);
}

/*
 * seeking and current position
 */

static void _seek_internal(BLURAY *bd,
                           NAV_CLIP *clip, uint32_t title_pkt, uint32_t clip_pkt)
{
    if (_seek_stream(bd, &bd->st0, clip, clip_pkt) >= 0) {

        /* update title position */
        bd->s_pos = (uint64_t)title_pkt * 192;

        /* chapter tracking */
        _update_chapter_psr(bd);

        BD_DEBUG(DBG_BLURAY, "Seek to %"PRIu64" (%p)\n",
              bd->s_pos, bd);

        if (bd->bdplus_seek && bd->bdplus) {
            bd->bdplus_seek(bd->bdplus, bd->st0.clip_block_pos);
        }
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

    tick /= 2;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        tick < bd->title->duration) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_time_search(bd->title, tick, &clip_pkt, &out_pkt);

        _seek_internal(bd, clip, out_pkt, clip_pkt);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

uint64_t bd_tell_time(BLURAY *bd)
{
    uint32_t clip_pkt = 0, out_pkt = 0, out_time = 0;
    NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd && bd->title) {
        clip = nav_packet_search(bd->title, SPN(bd->s_pos), &clip_pkt, &out_pkt, &out_time);
        if (clip) {
            out_time += clip->start_time;
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
        ret = nav_chapter_get_current(bd->st0.clip, SPN(bd->st0.clip_pos));
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
      out_pkt  = clip->pos;

      _seek_internal(bd, clip, out_pkt, clip_pkt);
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

    bd_mutex_lock(&bd->mutex);

    if (bd && bd->title) {
        ret = (uint64_t)bd->title->packets * 192;
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

uint64_t bd_tell(BLURAY *bd)
{
    uint64_t ret = 0;

    bd_mutex_lock(&bd->mutex);

    if (bd) {
        ret = bd->s_pos;
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

/*
 * read
 */

static int64_t _clip_seek_time(BLURAY *bd, uint32_t tick)
{
    uint32_t clip_pkt, out_pkt;

    if (tick < bd->st0.clip->out_time) {

        // Find the closest access unit to the requested position
        nav_clip_time_search(bd->st0.clip, tick, &clip_pkt, &out_pkt);

        _seek_internal(bd, bd->st0.clip, out_pkt, clip_pkt);
    }

    return bd->s_pos;
}

int bd_read(BLURAY *bd, unsigned char *buf, int len)
{
    BD_STREAM *st = &bd->st0;
    int out_len;

    if (st->fp) {
        out_len = 0;
        BD_DEBUG(DBG_STREAM, "Reading [%d bytes] at %"PRIu64"... (%p)\n", len, bd->s_pos, bd);

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
                        bd->s_pos = st->clip->pos;
                    } else {
                        _change_angle(bd);
                        _clip_seek_time(bd, bd->angle_change_time);
                    }
                    bd->seamless_angle_change = 0;
                } else {
                    uint64_t angle_pos;

                    angle_pos = bd->angle_change_pkt * 192;
                    if (angle_pos - st->clip_pos < size)
                    {
                        size = angle_pos - st->clip_pos;
                    }
                }
            }
            if (st->clip == NULL) {
                // We previously reached the last clip.  Nothing
                // else to read.
                _queue_event(bd, BD_EVENT_END_OF_TITLE, 0);
                _bdj_event(bd, BDJ_EVENT_END_OF_PLAYLIST, 0);
                return 0;
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
                        BD_DEBUG(DBG_BLURAY|DBG_STREAM, "End of title (%p)\n", bd);
                        _queue_event(bd, BD_EVENT_END_OF_TITLE, 0);
                        _bdj_event(bd, BDJ_EVENT_END_OF_PLAYLIST, 0);
                        return 0;
                    }
                    if (!_open_m2ts(bd, st)) {
                        return -1;
                    }
                }

                int r = _read_block(bd, st, bd->int_buf);
                if (r > 0) {

                    if (bd->ig_pid > 0) {
                        if (gc_decode_ts(bd->graphics_controller, bd->ig_pid, bd->int_buf, 1, -1) > 0) {
                            /* initialize menus */
                            _run_gc(bd, GC_CTRL_INIT_MENU, 0);
                        }
                    }

                    st->int_buf_off = st->clip_pos % 6144;

                } else if (r == 0) {
                    /* recoverable error (EOF, broken block) */
                    return out_len;
                } else {
                    /* fatal error */
                    return -1;
                }
            }
            if (size > (unsigned int)6144 - st->int_buf_off) {
                size = 6144 - st->int_buf_off;
            }
            memcpy(buf, bd->int_buf + st->int_buf_off, size);
            buf += size;
            len -= size;
            out_len += size;
            st->clip_pos += size;
            st->int_buf_off += size;
            bd->s_pos += size;
        }

        /* chapter tracking */
        if (bd->s_pos > bd->next_chapter_start) {
            _update_chapter_psr(bd);
        }

        BD_DEBUG(DBG_STREAM, "%d bytes read OK! (%p)\n", out_len, bd);

        return out_len;
    }

    BD_DEBUG(DBG_STREAM | DBG_CRIT, "bd_read(): no valid title selected! (%p)\n", bd);

    return -1;
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
                return _open_m2ts(bd, st);
            }
        }
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

/*
 * preloader for asynchronous sub paths
 */

static int _find_ig_stream(BLURAY *bd, uint16_t *pid, int *sub_path_idx)
{
    MPLS_PI  *pi        = &bd->title->pl->play_item[0];
    unsigned  ig_stream = bd_psr_read(bd->regs, PSR_IG_STREAM_ID);

    if (ig_stream > 0 && ig_stream <= pi->stn.num_ig) {
        ig_stream--; /* stream number to table index */
        if (pi->stn.ig[ig_stream].stream_type == 2) {
            *sub_path_idx = pi->stn.ig[ig_stream].subpath_id;
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
    uint16_t ig_pid     = 0;

    if (!bd->graphics_controller) {
        return 0;
    }

    _find_ig_stream(bd, &ig_pid, &ig_subpath);

    if (ig_subpath < 0) {
        return 0;
    }

    bd->st_ig.clip = &bd->title->sub_path[ig_subpath].clip_list.clip[0];

    if (!_preload_m2ts(bd, &bd->st_ig)) {
        _close_preload(&bd->st_ig);
        return 0;
    }

    return 1;
}

static int _preload_subpaths(BLURAY *bd)
{
    _close_preload(&bd->st_ig);

    if (bd->title->pl->sub_count <= 0) {
        return 0;
    }

    return _preload_ig_subpath(bd);
}

static int _init_ig_stream(BLURAY *bd)
{
    int      ig_subpath = -1;
    uint16_t ig_pid     = 0;

    bd->ig_pid = 0;

    if (!bd->graphics_controller) {
        return 0;
    }

    _find_ig_stream(bd, &ig_pid, &ig_subpath);

    /* decode already preloaded IG sub-path */
    if (bd->st_ig.clip) {
        gc_decode_ts(bd->graphics_controller, ig_pid, bd->st_ig.buf, SPN(bd->st_ig.clip_size) / 32, -1);
        return 1;
    }

    /* store PID of main path embedded IG stream */
    if (ig_subpath < 0) {
        bd->ig_pid = ig_pid;
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

    _close_m2ts(&bd->st0);
    _close_preload(&bd->st_ig);

    if (bd->title) {
        nav_title_close(bd->title);
        bd->title = NULL;
    }
}

static int _open_playlist(BLURAY *bd, const char *f_name, unsigned angle)
{
    _close_playlist(bd);

    bd->title = nav_title_open(bd->device_path, f_name, angle);
    if (bd->title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s! (%p)\n",
              f_name, bd);
        return 0;
    }

    bd->seamless_angle_change = 0;
    bd->s_pos = 0;

    bd->next_chapter_start = bd_chapter_pos(bd, 1);

    bd_psr_write(bd->regs, PSR_PLAYLIST, atoi(bd->title->name));
    bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);
    bd_psr_write(bd->regs, PSR_CHAPTER, 1);

    // Get the initial clip of the playlist
    bd->st0.clip = nav_next_clip(bd->title, NULL);
    if (_open_m2ts(bd, &bd->st0)) {
        BD_DEBUG(DBG_BLURAY, "Title %s selected! (%p)\n", f_name, bd);

        _preload_subpaths(bd);

        return 1;
    }
    return 0;
}

int bd_select_playlist(BLURAY *bd, uint32_t playlist)
{
    char *f_name = str_printf("%05d.mpls", playlist);
    int result;

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

// Select a title for playback
// The title index is an index into the list
// established by bd_get_titles()
int bd_select_title(BLURAY *bd, uint32_t title_idx)
{
    const char *f_name;

    // Open the playlist
    if (bd->title_list == NULL) {
        BD_DEBUG(DBG_BLURAY, "Title list not yet read! (%p)\n", bd);
        return 0;
    }
    if (bd->title_list->count <= title_idx) {
        BD_DEBUG(DBG_BLURAY, "Invalid title index %d! (%p)\n", title_idx, bd);
        return 0;
    }

    bd->title_idx = title_idx;
    f_name = bd->title_list->title_info[title_idx].name;

    return _open_playlist(bd, f_name, 0);
}

uint32_t bd_get_current_title(BLURAY *bd)
{
    return bd->title_idx;
}

int bd_select_angle(BLURAY *bd, unsigned angle)
{
    unsigned orig_angle;

    if (bd->title == NULL) {
        BD_DEBUG(DBG_BLURAY, "Title not yet selected! (%p)\n", bd);
        return 0;
    }

    orig_angle = bd->title->angle;

    bd->st0.clip = nav_set_angle(bd->title, bd->st0.clip, angle);

    if (orig_angle == bd->title->angle) {
        return 1;
    }

    bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);

    if (!_open_m2ts(bd, &bd->st0)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "Error selecting angle %d ! (%p)\n", angle, bd);
        return 0;
    }

    return 1;
}

unsigned bd_get_current_angle(BLURAY *bd)
{
    if (bd->title) {
        return bd->title->angle;
    }
    return 0;
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
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_get_titles(NULL) failed (%p)\n", bd);
        return 0;
    }

    if (bd->title_list != NULL) {
        nav_free_title_list(bd->title_list);
    }
    bd->title_list = nav_get_title_list(bd->device_path, flags, min_title_length);

    if (!bd->title_list) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "nav_get_title_list(%s) failed (%p)\n", bd->device_path, bd);
        return 0;
    }

    return bd->title_list->count;
}

static void _copy_streams(NAV_CLIP *clip, BLURAY_STREAM_INFO *streams, MPLS_STREAM *si, int count)
{
    int ii;

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
}

static BLURAY_TITLE_INFO* _fill_title_info(NAV_TITLE* title, uint32_t title_idx, uint32_t playlist)
{
    BLURAY_TITLE_INFO *title_info;
    unsigned int ii;

    title_info = calloc(1, sizeof(BLURAY_TITLE_INFO));
    title_info->idx = title_idx;
    title_info->playlist = playlist;
    title_info->duration = (uint64_t)title->duration * 2;
    title_info->angle_count = title->angle_count;
    title_info->chapter_count = title->chap_list.count;
    title_info->chapters = calloc(title_info->chapter_count, sizeof(BLURAY_TITLE_CHAPTER));
    for (ii = 0; ii < title_info->chapter_count; ii++) {
        title_info->chapters[ii].idx = ii;
        title_info->chapters[ii].start = (uint64_t)title->chap_list.mark[ii].title_time * 2;
        title_info->chapters[ii].duration = (uint64_t)title->chap_list.mark[ii].duration * 2;
        title_info->chapters[ii].offset = (uint64_t)title->chap_list.mark[ii].title_pkt * 192L;
        title_info->chapters[ii].clip_ref = title->chap_list.mark[ii].clip_ref;
    }
    title_info->mark_count = title->mark_list.count;
    title_info->marks = calloc(title_info->mark_count, sizeof(BLURAY_TITLE_MARK));
    for (ii = 0; ii < title_info->mark_count; ii++) {
        title_info->marks[ii].idx = ii;
		title_info->marks[ii].type = title->mark_list.mark[ii].mark_type;
        title_info->marks[ii].start = (uint64_t)title->mark_list.mark[ii].title_time * 2;
        title_info->marks[ii].duration = (uint64_t)title->mark_list.mark[ii].duration * 2;
        title_info->marks[ii].offset = (uint64_t)title->mark_list.mark[ii].title_pkt * 192L;
		title_info->marks[ii].clip_ref = title->mark_list.mark[ii].clip_ref;
    }
    title_info->clip_count = title->clip_list.count;
    title_info->clips = calloc(title_info->clip_count, sizeof(BLURAY_CLIP_INFO));
    for (ii = 0; ii < title_info->clip_count; ii++) {
        MPLS_PI *pi = &title->pl->play_item[ii];
        BLURAY_CLIP_INFO *ci = &title_info->clips[ii];
        NAV_CLIP *nc = &title->clip_list.clip[ii];

        ci->pkt_count = nc->end_pkt - nc->start_pkt;
        ci->start_time = (uint64_t)nc->start_time * 2;
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
        ci->video_streams = calloc(ci->video_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->audio_streams = calloc(ci->audio_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->pg_streams = calloc(ci->pg_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->ig_streams = calloc(ci->ig_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->sec_video_streams = calloc(ci->sec_video_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->sec_audio_streams = calloc(ci->sec_audio_stream_count, sizeof(BLURAY_STREAM_INFO));
        _copy_streams(nc, ci->video_streams, pi->stn.video, ci->video_stream_count);
        _copy_streams(nc, ci->audio_streams, pi->stn.audio, ci->audio_stream_count);
        _copy_streams(nc, ci->pg_streams, pi->stn.pg, ci->pg_stream_count);
        _copy_streams(nc, ci->ig_streams, pi->stn.ig, ci->ig_stream_count);
        _copy_streams(nc, ci->sec_video_streams, pi->stn.secondary_video, ci->sec_video_stream_count);
        _copy_streams(nc, ci->sec_audio_streams, pi->stn.secondary_audio, ci->sec_audio_stream_count);
    }

    return title_info;
}

static BLURAY_TITLE_INFO *_get_title_info(BLURAY *bd, uint32_t title_idx, uint32_t playlist, const char *mpls_name,
                                          unsigned angle)
{
    NAV_TITLE *title;
    BLURAY_TITLE_INFO *title_info;

    title = nav_title_open(bd->device_path, mpls_name, angle);
    if (title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s! (%p)\n",
              mpls_name, bd);
        return NULL;
    }

    title_info = _fill_title_info(title, title_idx, playlist);

    nav_title_close(title);
    return title_info;
}

BLURAY_TITLE_INFO* bd_get_title_info(BLURAY *bd, uint32_t title_idx, unsigned angle)
{
    if (bd->title_list == NULL) {
        BD_DEBUG(DBG_BLURAY, "Title list not yet read! (%p)\n", bd);
        return NULL;
    }
    if (bd->title_list->count <= title_idx) {
        BD_DEBUG(DBG_BLURAY, "Invalid title index %d! (%p)\n", title_idx, bd);
        return NULL;
    }

    return _get_title_info(bd,
                           title_idx, bd->title_list->title_info[title_idx].mpls_id,
                           bd->title_list->title_info[title_idx].name,
                           angle);
}

BLURAY_TITLE_INFO* bd_get_playlist_info(BLURAY *bd, uint32_t playlist, unsigned angle)
{
    char *f_name = str_printf("%05d.mpls", playlist);
    BLURAY_TITLE_INFO *title_info;

    title_info = _get_title_info(bd, 0, playlist, f_name, angle);

    X_FREE(f_name);

    return title_info;
}

void bd_free_title_info(BLURAY_TITLE_INFO *title_info)
{
    unsigned int ii;

    X_FREE(title_info->chapters);
    X_FREE(title_info->marks);
    for (ii = 0; ii < title_info->clip_count; ii++) {
        X_FREE(title_info->clips[ii].video_streams);
        X_FREE(title_info->clips[ii].audio_streams);
        X_FREE(title_info->clips[ii].pg_streams);
        X_FREE(title_info->clips[ii].ig_streams);
        X_FREE(title_info->clips[ii].sec_video_streams);
        X_FREE(title_info->clips[ii].sec_audio_streams);
    }
    X_FREE(title_info->clips);
    X_FREE(title_info);
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

    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (idx == map[i].idx) {
            return !bd_psr_setting_write(bd->regs, idx, value);
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

        default:
            return 0;
    }
}

/*
 * BD-J testing
 */

int bd_start_bdj(BLURAY *bd, const char *start_object)
{
    unsigned ii;

    if (!bd || !bd->index) {
        return 0;
    }

    /* first play object ? */
    if (bd->index->first_play.object_type == indx_object_type_bdj &&
        !strcmp(start_object, bd->index->first_play.bdj.name)) {
        return _start_bdj(bd, BLURAY_TITLE_FIRST_PLAY);
    }

    /* top menu ? */
    if (bd->index->first_play.object_type == indx_object_type_bdj &&
        !strcmp(start_object, bd->index->top_menu.bdj.name)) {
        return _start_bdj(bd, BLURAY_TITLE_TOP_MENU);
    }

    /* valid BD-J title from disc index ? */
    for (ii = 0; ii < bd->index->num_titles; ii++) {
        INDX_TITLE *t = &bd->index->titles[ii];

        if (t->object_type == indx_object_type_bdj &&
            !strcmp(start_object, t->bdj.name)) {
            return _start_bdj(bd, ii + 1);
        }
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No %s.bdjo in disc index\n", start_object);
    return 0;
 }

void bd_stop_bdj(BLURAY *bd)
{
    _close_bdj(bd);
}

/*
 * Navigation mode interface
 */

static void _process_psr_restore_event(BLURAY *bd, BD_PSR_EVENT *ev)
{
    /* PSR restore events are handled internally.
     * Restore stored playback position.
     */

    BD_DEBUG(DBG_BLURAY, "PSR restore: psr%u = %u (%p)\n", ev->psr_idx, ev->new_val, bd);

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
            bd_seek_time(bd, ((int64_t)ev->new_val) << 1);
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
        BD_DEBUG(DBG_BLURAY, "PSR write: psr%u = %u (%p)\n", ev->psr_idx, ev->new_val, bd);
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
            _queue_event(bd, BD_EVENT_PLAYLIST, ev->new_val);
            break;
        case PSR_PLAYITEM:
            _bdj_event  (bd, BDJ_EVENT_PLAYITEM,ev->new_val);
            _queue_event(bd, BD_EVENT_PLAYITEM, ev->new_val);
            break;
        case PSR_CHAPTER:
            _bdj_event  (bd, BDJ_EVENT_CHAPTER, ev->new_val);
            _queue_event(bd, BD_EVENT_CHAPTER,  ev->new_val);
            break;
        case PSR_TIME:
            _bdj_event  (bd, BDJ_EVENT_PTS,     ev->new_val);
            break;

        default:;
    }
}

static void _process_psr_change_event(BLURAY *bd, BD_PSR_EVENT *ev)
{
    BD_DEBUG(DBG_BLURAY, "PSR change: psr%u = %u (%p)\n", ev->psr_idx, ev->new_val, bd);

    _process_psr_write_event(bd, ev);

    switch (ev->psr_idx) {

        /* stream selection */

        case PSR_IG_STREAM_ID:
            _queue_event(bd, BD_EVENT_IG_STREAM, ev->new_val);
            break;

        case PSR_PRIMARY_AUDIO_ID:
            _queue_event(bd, BD_EVENT_AUDIO_STREAM, ev->new_val);
            break;

        case PSR_PG_STREAM:
            _bdj_event(bd, BDJ_EVENT_SUBTITLE, ev->new_val);
            if ((ev->new_val & 0x80000fff) != (ev->old_val & 0x80000fff)) {
                _queue_event(bd, BD_EVENT_PG_TEXTST,        !!(ev->new_val & 0x80000000));
                _queue_event(bd, BD_EVENT_PG_TEXTST_STREAM,    ev->new_val & 0xfff);
            }
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
            BD_DEBUG(DBG_BLURAY, "PSR save event (%p)\n", bd);
            break;
        default:
            BD_DEBUG(DBG_BLURAY, "PSR event %d: psr%u = %u (%p)\n", ev->ev_type, ev->psr_idx, ev->new_val, bd);
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
    bd->title_type = title_bdj;

    return _start_bdj(bd, title);
}

static int _play_hdmv(BLURAY *bd, unsigned id_ref)
{
    int result = 1;

    _stop_bdj(bd);

    bd->title_type = title_hdmv;

    if (!bd->hdmv_vm) {
        bd->hdmv_vm = hdmv_vm_init(bd->device_path, bd->regs, bd->index);
    }

    if (hdmv_vm_select_object(bd->hdmv_vm, id_ref)) {
        result = 0;
    }

    bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);

    return result;
}

static int _play_title(BLURAY *bd, unsigned title)
{
    /* first play object ? */
    if (title == BLURAY_TITLE_FIRST_PLAY) {
        INDX_PLAY_ITEM *p = &bd->index->first_play;

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, 0xffff); /* 5.2.3.3 */

        if (p->object_type == indx_object_type_hdmv) {
            if (p->hdmv.id_ref == 0xffff) {
                /* no first play title (5.2.3.3) */
                bd->title_type = title_hdmv;
                return 1;
            }
            return _play_hdmv(bd, p->hdmv.id_ref);
        }

        if (p->object_type == indx_object_type_bdj) {
            return _play_bdj(bd, title);
        }

        return 0;
    }

    /* bd_play not called ? */
    if (bd->title_type == title_undef) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_call_title(): bd_play() not called !\n");
        return 0;
    }

    /* top menu ? */
    if (title == BLURAY_TITLE_TOP_MENU) {
        INDX_PLAY_ITEM *p = &bd->index->top_menu;

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, 0); /* 5.2.3.3 */

        if (p->object_type == indx_object_type_hdmv) {
            if (p->hdmv.id_ref == 0xffff) {
                /* no top menu (5.2.3.3) */
                bd->title_type = title_hdmv;
                return 0;
            }
            return _play_hdmv(bd, p->hdmv.id_ref);
        }

        if (p->object_type == indx_object_type_bdj) {
            return _play_bdj(bd, title);
        }

        return 0;
    }

    /* valid title from disc index ? */
    if (title > 0 && title <= bd->index->num_titles) {
        INDX_TITLE *t = &bd->index->titles[title-1];

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, title); /* 5.2.3.3 */

        if (t->object_type == indx_object_type_hdmv) {
            return _play_hdmv(bd, t->hdmv.id_ref);
        } else {
            return _play_bdj(bd, title);
        }
    }

    return 0;
}

int bd_play(BLURAY *bd)
{
    /* reset player state */

    bd->title_type = title_undef;

    if (bd->hdmv_vm) {
        hdmv_vm_free(&bd->hdmv_vm);
    }

    _init_event_queue(bd);

    bd_psr_lock(bd->regs);
    bd_psr_register_cb(bd->regs, _process_psr_event, bd);
    _queue_initial_psr_events(bd);
    bd_psr_unlock(bd->regs);

    return _play_title(bd, BLURAY_TITLE_FIRST_PLAY);
}

static int _try_play_title(BLURAY *bd, unsigned title)
{
    if (bd->title_type == title_undef && title != BLURAY_TITLE_FIRST_PLAY) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_play_title(): bd_play() not called\n");
        return 0;
    }

    if (bd->st0.uo_mask.title_search) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "title search masked by stream\n");
        return 0;
    }

    if (bd->title_type == title_hdmv) {
        if (hdmv_vm_get_uo_mask(bd->hdmv_vm) & HDMV_TITLE_SEARCH_MASK) {
            BD_DEBUG(DBG_BLURAY|DBG_CRIT, "title search masked by movie object\n");
            return 0;
        }
    }

    return _play_title(bd, title);
}

int bd_play_title(BLURAY *bd, unsigned title)
{
    int ret;
    bd_mutex_lock(&bd->mutex);
    ret = _try_play_title(bd, title);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

static int _try_menu_call(BLURAY *bd, int64_t pts)
{
    if (pts >= 0) {
        bd_psr_write(bd->regs, PSR_TIME, (uint32_t)(((uint64_t)pts) >> 1));
    }

    if (bd->title_type == title_undef) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_menu_call(): bd_play() not called\n");
        return 0;
    }

    if (bd->st0.uo_mask.menu_call) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "menu call masked by stream\n");
        return 0;
    }

    if (bd->title_type == title_hdmv) {
        if (hdmv_vm_get_uo_mask(bd->hdmv_vm) & HDMV_MENU_CALL_MASK) {
            BD_DEBUG(DBG_BLURAY|DBG_CRIT, "menu call masked by movie object\n");
            return 0;
        }

        if (hdmv_vm_suspend_pl(bd->hdmv_vm) < 0) {
            BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_menu_call(): error storing playback location\n");
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
            _queue_event(bd, BD_EVENT_SEEK, 0);
            bd_seek_playitem(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_PM:
            _queue_event(bd, BD_EVENT_SEEK, 0);
            bd_seek_mark(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_STOP:
            // stop current playlist
            _close_playlist(bd);
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
        _queue_event(bd, BD_EVENT_ERROR, 0);
        bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
        return -1;
    }

    /* process all events */
    do {
        _process_hdmv_vm_event(bd, &hdmv_ev);

    } while (!hdmv_vm_get_event(bd->hdmv_vm, &hdmv_ev));

    /* update VM state */
    bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);

    return 0;
}

static int _read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event)
{
    if (_get_event(bd, event)) {
        return 0;
    }

    /* run HDMV VM ? */
    if (!bd->hdmv_suspended && bd->title_type == title_hdmv) {

        while (!bd->hdmv_suspended) {

            if (_run_hdmv(bd) < 0) {
                BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_read_ext(): HDMV VM error\n");
                bd->title_type = title_undef;
                return -1;
            }
            if (_get_event(bd, event)) {
                return 0;
            }
        }
    }

    if (len < 1) {
        /* just polled events ? */
        return 0;
    }

    int bytes = bd_read(bd, buf, len);

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
        _init_event_queue(bd);

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
    if (pts >= 0) {
        bd_psr_write(bd->regs, PSR_TIME, (uint32_t)(((uint64_t)pts) >> 1));
    }
}

int bd_mouse_select(BLURAY *bd, int64_t pts, uint16_t x, uint16_t y)
{
    bd_set_scr(bd, pts);

    return _run_gc(bd, GC_CTRL_MOUSE_MOVE, (x << 16) | y);
}

int bd_user_input(BLURAY *bd, int64_t pts, uint32_t key)
{
    bd_set_scr(bd, pts);

    if (bd->title_type == title_hdmv) {
        return _run_gc(bd, GC_CTRL_VK_KEY, key);
    } else if (bd->title_type == title_bdj) {
        _bdj_event(bd, BDJ_EVENT_VK_KEY, key);
        return 0;
    }
    return -1;
}

void bd_register_overlay_proc(BLURAY *bd, void *handle, bd_overlay_proc_f func)
{
    if (!bd) {
        return;
    }

    gc_free(&bd->graphics_controller);

    if (func) {
        bd->graphics_controller = gc_init(bd->regs, handle, func);
    }
}

int bd_get_sound_effect(BLURAY *bd, unsigned sound_id, BLURAY_SOUND_EFFECT *effect)
{
    if (!bd || !effect) {
        return -1;
    }

    if (!bd->sound_effects) {

        char *file = str_printf("%s/BDMV/AUXDATA/sound.bdmv", bd->device_path);
        bd->sound_effects = sound_parse(file);
        X_FREE(file);

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
 *
 */

const struct meta_dl *bd_get_meta(BLURAY *bd)
{
    if (!bd) {
        return NULL;
    }

    if (!bd->meta) {
        _meta_open(bd);
    }

    uint32_t psr_menu_lang = bd_psr_read(bd->regs, PSR_MENU_LANG);

    if (psr_menu_lang != 0 && psr_menu_lang != 0xffffff) {
        const char language_code[] = {(psr_menu_lang >> 16) & 0xff, (psr_menu_lang >> 8) & 0xff, psr_menu_lang & 0xff, 0 };
        return meta_get(bd->meta, language_code);
    }
    else {
        return meta_get(bd->meta, NULL);
    }
}

struct clpi_cl *bd_get_clpi(BLURAY *bd, unsigned clip_ref)
{
    if (bd->title && clip_ref < bd->title->clip_list.count) {
        NAV_CLIP *clip = &bd->title->clip_list.clip[clip_ref];
        return clpi_copy(clip->cl);
    }
    return NULL;
}

void bd_free_clpi(struct clpi_cl *cl)
{
    clpi_free(cl);
}
