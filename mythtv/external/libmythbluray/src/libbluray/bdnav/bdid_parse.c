/*
 * This file is part of libbluray
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

#include "bdid_parse.h"

#include "disc/disc.h"

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#include <stdlib.h>

#define BDID_SIG1  ('B' << 24 | 'D' << 16 | 'I' << 8 | 'D')
#define BDID_SIG2A ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define BDID_SIG2B ('0' << 24 | '1' << 16 | '0' << 8 | '0')

static int _parse_header(BITSTREAM *bs, uint32_t *data_start, uint32_t *extension_data_start)
{
    uint32_t sig1, sig2;

    if (bs_seek_byte(bs, 0) < 0) {
        return 0;
    }

    sig1 = bs_read(bs, 32);
    sig2 = bs_read(bs, 32);

    if (sig1 != BDID_SIG1 ||
       (sig2 != BDID_SIG2A &&
        sig2 != BDID_SIG2B)) {
       BD_DEBUG(DBG_NAV | DBG_CRIT, "id.bdmv failed signature match: expected BDID0100 got %8.8s\n", bs->buf);
       return 0;
    }

    *data_start           = bs_read(bs, 32);
    *extension_data_start = bs_read(bs, 32);

    return 1;
}

static BDID_DATA *_bdid_parse(BD_FILE_H *fp)
{
    BITSTREAM  bs;
    BDID_DATA *bdid = NULL;

    uint32_t   data_start, extension_data_start;
    uint8_t    tmp[16];

    if (bs_init(&bs, fp) < 0) {
        BD_DEBUG(DBG_NAV, "id.bdmv: read error\n");
        return NULL;
    }

    if (!_parse_header(&bs, &data_start, &extension_data_start)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "id.bdmv: invalid header\n");
        return NULL;
    }

    if (bs_seek_byte(&bs, 40) < 0) {
        BD_DEBUG(DBG_NAV, "id.bdmv: read error\n");
        return NULL;
    }

    bdid = calloc(1, sizeof(BDID_DATA));
    if (!bdid) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return NULL;
    }

    bs_read_bytes(&bs, tmp, 4);
    str_print_hex(bdid->org_id, tmp, 4);

    bs_read_bytes(&bs, tmp, 16);
    str_print_hex(bdid->disc_id, tmp, 16);

    return bdid;
}

static BDID_DATA *_bdid_get(BD_DISC *disc, const char *path)
{
    BD_FILE_H *fp;
    BDID_DATA *bdid;

    fp = disc_open_path(disc, path);
    if (!fp) {
        return NULL;
    }

    bdid = _bdid_parse(fp);
    file_close(fp);
    return bdid;
}

BDID_DATA *bdid_get(BD_DISC *disc)
{
    BDID_DATA *bdid;

    bdid = _bdid_get(disc, "CERTIFICATE" DIR_SEP "id.bdmv");

    /* if failed, try backup file */
    if (!bdid) {
        bdid = _bdid_get(disc, "CERTIFICATE" DIR_SEP "BACKUP" DIR_SEP "id.bdmv");
    }

    return bdid;
}

void bdid_free(BDID_DATA **p)
{
    if (p && *p) {
        X_FREE(*p);
    }
}
