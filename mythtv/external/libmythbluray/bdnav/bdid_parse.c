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

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"
#include "bdid_parse.h"

#include <stdlib.h>
#include <string.h>

#define BDID_SIG1  ('B' << 24 | 'D' << 16 | 'I' << 8 | 'D')
#define BDID_SIG2A ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define BDID_SIG2B ('0' << 24 | '1' << 16 | '0' << 8 | '0')

static int _parse_header(BITSTREAM *bs, uint32_t *data_start, uint32_t *extension_data_start)
{
    uint32_t sig1, sig2;

    bs_seek_byte(bs, 0);

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

static BDID_DATA *_bdid_parse(const char *file_name)
{
    BITSTREAM  bs;
    BD_FILE_H *fp;
    BDID_DATA *bdid = NULL;

    uint32_t   data_start, extension_data_start;
    uint8_t    tmp[16];

    fp = file_open(file_name, "rb");
    if (!fp) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "bdid_parse(): error opening %s\n", file_name);
        return NULL;
    }

    bs_init(&bs, fp);

    if (!_parse_header(&bs, &data_start, &extension_data_start)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "id.bdmv: invalid header\n");
        goto error;
    }

    bdid = calloc(1, sizeof(BDID_DATA));

    bs_seek_byte(&bs, 40);

    bs_read_bytes(&bs, tmp, 4);
    print_hex(bdid->org_id, tmp, 4);

    bs_read_bytes(&bs, tmp, 16);
    print_hex(bdid->disc_id, tmp, 16);

    file_close(fp);
    return bdid;

 error:
    X_FREE(bdid);
    file_close(fp);
    return NULL;
}

BDID_DATA *bdid_parse(const char *file_name)
{
    BDID_DATA *bdid = _bdid_parse(file_name);

    /* if failed, try backup file */
    if (!bdid) {
        size_t len   = strlen(file_name);
        char *backup = malloc(len + 8);

        strcpy(backup, file_name);
        strcpy(backup + len - 7, "BACKUP/id.bdmv");

        bdid = _bdid_parse(backup);

        X_FREE(backup);
    }

    return bdid;
}

void bdid_free(BDID_DATA **p)
{
    if (p && *p) {
        X_FREE(*p);
    }
}
