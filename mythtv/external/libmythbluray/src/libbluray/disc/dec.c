/*
 * This file is part of libbluray
 * Copyright (C) 2014  VideoLAN
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

#include "dec.h"

#include "enc_info.h"
#include "aacs.h"
#include "bdplus.h"

#include "file/file.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#include <string.h>

struct bd_dec {
    int        use_menus;
    BD_AACS   *aacs;
    BD_BDPLUS *bdplus;
};

/*
 * stream
 */

typedef struct {
    BD_FILE_H    *fp;
    BD_AACS      *aacs;
    BD_BDPLUS_ST *bdplus;
} DEC_STREAM;

static int64_t _stream_read(BD_FILE_H *fp, uint8_t *buf, int64_t size)
{
    DEC_STREAM *st = (DEC_STREAM *)fp->internal;
    int64_t     result;

    if (size != 6144) {
        BD_DEBUG(DBG_CRIT, "read size != unit size\n");
        return 0;
    }

    result = st->fp->read(st->fp, buf, size);
    if (result <= 0) {
        return result;
    }

    if (st->aacs) {
        if (libaacs_decrypt_unit(st->aacs, buf)) {
            /* failure is detected from TP header */
        }
    }

    if (st->bdplus) {
        if (libbdplus_fixup(st->bdplus, buf, (int)size) < 0) {
          /* there's no way to verify if the stream was decoded correctly */
        }
    }

    return result;
}

static int64_t _stream_seek(BD_FILE_H *fp, int64_t offset, int32_t origin)
{
    DEC_STREAM *st = (DEC_STREAM *)fp->internal;
    int64_t result = st->fp->seek(st->fp, offset, origin);
    if (result >= 0 && st->bdplus) {
        libbdplus_seek(st->bdplus, st->fp->tell(st->fp));
    }
    return result;
}

static int64_t _stream_tell(BD_FILE_H *fp)
{
    DEC_STREAM *st = (DEC_STREAM *)fp->internal;
    return st->fp->tell(st->fp);
}

static void _stream_close(BD_FILE_H *fp)
{
    DEC_STREAM *st = (DEC_STREAM *)fp->internal;
    if (st->bdplus) {
        libbdplus_m2ts_close(&st->bdplus);
    }
    st->fp->close(st->fp);
    X_FREE(fp->internal);
    X_FREE(fp);
}

BD_FILE_H *dec_open_stream(BD_DEC *dec, BD_FILE_H *fp, uint32_t clip_id)
{
    DEC_STREAM *st;
    BD_FILE_H  *p = calloc(1, sizeof(BD_FILE_H));
    if (!p) {
        return NULL;
    }

    st = calloc(1, sizeof(DEC_STREAM));
    if (!st) {
        X_FREE(p);
        return NULL;
    }
    st->fp = fp;

    if (dec->bdplus) {
        st->bdplus = libbdplus_m2ts(dec->bdplus, clip_id, 0);
    }

    if (dec->aacs) {
        st->aacs = dec->aacs;
        if (!dec->use_menus) {
            /* There won't be title events --> need to manually reset AACS CPS */
            libaacs_select_title(dec->aacs, 0xffff);
        }
    }

    p->internal = st;
    p->read  = _stream_read;
    p->seek  = _stream_seek;
    p->tell  = _stream_tell;
    p->close = _stream_close;

    return p;
}

/*
 *
 */

/*
 *
 */

static int _bdrom_have_file(void *p, const char *dir, const char *file)
{
    struct dec_dev *dev = (struct dec_dev *)p;
    BD_FILE_H *fp;
    char *path;

    path = str_printf("%s" DIR_SEP "%s", dir, file);
    if (!path) {
        return 0;
    }

    fp = dev->pf_file_open_bdrom(dev->file_open_bdrom_handle, path);
    X_FREE(path);

    if (fp) {
        file_close(fp);
        return 1;
    }

    return 0;
}

static int _libaacs_init(BD_DEC *dec, struct dec_dev *dev,
                         BD_ENC_INFO *i, const char *keyfile_path)
{
    int result;
    const uint8_t *disc_id;

    if (!dec->aacs) {
        return 0;
    }

    result = libaacs_open(dec->aacs, dev->device, dev->file_open_vfs_handle, (void*)dev->pf_file_open_vfs, keyfile_path);

    i->aacs_error_code = result;
    i->aacs_handled    = !result;
    i->aacs_mkbv       = libaacs_get_mkbv(dec->aacs);
    disc_id = libaacs_get_aacs_data(dec->aacs, BD_AACS_DISC_ID);
    if (disc_id) {
        memcpy(i->disc_id, disc_id, 20);
    }

    if (result) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "aacs_open() failed: %d!\n", result);
        libaacs_unload(&dec->aacs);
        return 0;
    }

    BD_DEBUG(DBG_BLURAY, "Opened libaacs\n");
    return 1;
}

static int _libbdplus_init(BD_DEC *dec, struct dec_dev *dev,
                           BD_ENC_INFO *i,
                           void *regs, void *psr_read, void *psr_write)
{
    if (!dec->bdplus) {
        return 0;
    }

    const uint8_t *vid = libaacs_get_aacs_data(dec->aacs, BD_AACS_MEDIA_VID);
    const uint8_t *mk  = libaacs_get_aacs_data(dec->aacs, BD_AACS_MEDIA_KEY);
    if (!vid) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "BD+ initialization failed (no AACS ?)\n");
        libbdplus_unload(&dec->bdplus);
        return 0;
    }

    if (libbdplus_init(dec->bdplus, dev->root, dev->device, dev->file_open_bdrom_handle, (void*)dev->pf_file_open_bdrom, vid, mk)) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bdplus_init() failed\n");

        i->bdplus_handled = 0;
        libbdplus_unload(&dec->bdplus);
        return 0;
    }

    BD_DEBUG(DBG_BLURAY, "libbdplus initialized\n");

    /* map player memory regions */
    libbdplus_mmap(dec->bdplus, 0, regs);
    libbdplus_mmap(dec->bdplus, 1, (void*)((uint8_t *)regs + sizeof(uint32_t) * 128));

    /* connect registers */
    libbdplus_psr(dec->bdplus, regs, psr_read, psr_write);

    i->bdplus_gen     = libbdplus_get_gen(dec->bdplus);
    i->bdplus_date    = libbdplus_get_date(dec->bdplus);
    i->bdplus_handled = 1;

    if (i->bdplus_date == 0) {
        // libmmbd -> no menu support
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "WARNING: using libmmbd for BD+. On-disc menus will not work.\n");
        i->no_menu_support = 1;
    }

    return 1;
}

static int _dec_detect(struct dec_dev *dev, BD_ENC_INFO *i)
{
    /* Check for AACS */
    i->aacs_detected = libaacs_required((void*)dev, _bdrom_have_file);
    if (!i->aacs_detected) {
        /* No AACS (=> no BD+) */
        return 0;
    }

    /* check for BD+ */
    i->bdplus_detected = libbdplus_required((void*)dev, _bdrom_have_file);
    return 1;
}

static void _dec_load(BD_DEC *dec, BD_ENC_INFO *i)
{
    int force_mmbd_aacs = 0;

    if (i->bdplus_detected) {
        /* load BD+ library and check BD+ library type. libmmbd doesn't work with libaacs */
        dec->bdplus = libbdplus_load();
        force_mmbd_aacs = dec->bdplus && libbdplus_is_mmbd(dec->bdplus);
    }

    /* load AACS library */
    dec->aacs = libaacs_load(force_mmbd_aacs);

    i->libaacs_detected   = !!dec->aacs;
    i->libbdplus_detected = !!dec->bdplus;
}

/*
 *
 */

BD_DEC *dec_init(struct dec_dev *dev, BD_ENC_INFO *enc_info,
                 const char *keyfile_path,
                 void *regs, void *psr_read, void *psr_write)
{
    BD_DEC *dec = NULL;

    memset(enc_info, 0, sizeof(*enc_info));

    /* detect AACS/BD+ */
    if (!_dec_detect(dev, enc_info)) {
        return NULL;
    }

    dec = calloc(1, sizeof(BD_DEC));
    if (!dec) {
        return NULL;
    }

    /* load compatible libraries */
    _dec_load(dec, enc_info);

    /* init decoding libraries */
    /* BD+ won't help unless AACS works ... */
    if (_libaacs_init(dec, dev, enc_info, keyfile_path)) {
        _libbdplus_init(dec, dev, enc_info, regs, psr_read, psr_write);
    }

    if (!enc_info->aacs_handled) {
        /* AACS failed, clean up */
        dec_close(&dec);
    }

    /* BD+ failure may be non-fatal (not all titles in disc use BD+).
     * Keep working AACS decoder even if BD+ init failed
     */

    return dec;
}

void dec_close(BD_DEC **pp)
{
    if (pp && *pp) {
        BD_DEC *p = *pp;
        libaacs_unload(&p->aacs);
        libbdplus_unload(&p->bdplus);
        X_FREE(*pp);
    }
}

/*
 *
 */

const uint8_t *dec_data(BD_DEC *dec, int type)
{
    if (dec->aacs) {
        return libaacs_get_aacs_data(dec->aacs, type);
    }
    return NULL;
}

void dec_start(BD_DEC *dec, uint32_t num_titles)
{
    if (num_titles == 0) {
        dec->use_menus = 1;
        if (dec->bdplus) {
            libbdplus_start(dec->bdplus);
            libbdplus_event(dec->bdplus, 0x110, 0xffff, 0);
        }
    } else {
        if (dec->bdplus) {
            libbdplus_event(dec->bdplus, 0xffffffff, num_titles, 0);
        }
    }
}

void dec_title(BD_DEC *dec, uint32_t title)
{
    if (dec->aacs) {
        libaacs_select_title(dec->aacs, title);
    }
    if (dec->bdplus) {
        libbdplus_event(dec->bdplus, 0x110, title, 0);
    }
}

void dec_application(BD_DEC *dec, uint32_t data)
{
    if (dec->bdplus) {
        libbdplus_event(dec->bdplus, 0x210, data, 0);
    }
}
