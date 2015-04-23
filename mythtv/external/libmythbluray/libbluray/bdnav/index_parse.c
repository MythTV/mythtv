/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
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
#include "index_parse.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int _parse_hdmv_obj(BITSTREAM *bs, INDX_HDMV_OBJ *hdmv)
{
    hdmv->playback_type = bs_read(bs, 2);
    bs_skip(bs, 14);
    hdmv->id_ref = bs_read(bs, 16);
    bs_skip(bs, 32);

    return 1;
}

static int _parse_bdj_obj(BITSTREAM *bs, INDX_BDJ_OBJ *bdj)
{
    bdj->playback_type = bs_read(bs, 2);
    bs_skip(bs, 14);
    bs_read_bytes(bs, (uint8_t*)bdj->name, 5);
    bdj->name[5] = 0;
    bs_skip(bs, 8);

    return 1;
}

static int _parse_playback_obj(BITSTREAM *bs, INDX_PLAY_ITEM *obj)
{
    obj->object_type = bs_read(bs, 2);
    bs_skip(bs, 30);

    if (obj->object_type == 1) {
        return _parse_hdmv_obj(bs, &obj->hdmv);
    } else {
        return _parse_bdj_obj(bs, &obj->bdj);
    }
}

static int _parse_index(BITSTREAM *bs, INDX_ROOT *index)
{
    uint32_t index_len, i;

    index_len = bs_read(bs, 32);

    /* TODO: check if goes to extension data area */

    if ((bs_end(bs) - bs_pos(bs))/8 < (off_t)index_len) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "index.bdmv: invalid index_len %d !\n", index_len);
        return 0;
    }

    if (!_parse_playback_obj(bs, &index->first_play) ||
        !_parse_playback_obj(bs, &index->top_menu)) {
        return 0;
    }

    index->num_titles = bs_read(bs, 16);

    index->titles = calloc(index->num_titles, sizeof(INDX_TITLE));

    for (i = 0; i < index->num_titles; i++) {

        index->titles[i].object_type = bs_read(bs, 2);
        index->titles[i].access_type = bs_read(bs, 2);
        bs_skip(bs, 28);

        if (index->titles[i].object_type == 1) {
            _parse_hdmv_obj(bs, &index->titles[i].hdmv);
        } else {
            _parse_bdj_obj(bs, &index->titles[i].bdj);
        }
    }

    return 1;
}

static int _parse_app_info(BITSTREAM *bs, INDX_APP_INFO *app_info)
{
    uint32_t len;

    bs_seek_byte(bs, 40);

    len = bs_read(bs, 32);

    if (len != 34) {
        BD_DEBUG(DBG_NAV, "index.bdmv app_info length is %d, expected 34 !\n", len);
    }

    bs_skip(bs, 1);
    app_info->initial_output_mode_preference = bs_read(bs, 1);
    app_info->content_exist_flag             = bs_read(bs, 1);
    bs_skip(bs, 5);

    app_info->video_format = bs_read(bs, 4);
    app_info->frame_rate   = bs_read(bs, 4);

    bs_read_bytes(bs, app_info->user_data, 32);

    return 1;
}

#define INDX_SIG1  ('I' << 24 | 'N' << 16 | 'D' << 8 | 'X')
#define INDX_SIG2A ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define INDX_SIG2B ('0' << 24 | '1' << 16 | '0' << 8 | '0')

static int _parse_header(BITSTREAM *bs, int *index_start, int *extension_data_start)
{
    uint32_t sig1, sig2;

    bs_seek_byte(bs, 0);

    sig1 = bs_read(bs, 32);
    sig2 = bs_read(bs, 32);

    if (sig1 != INDX_SIG1 ||
       (sig2 != INDX_SIG2A &&
        sig2 != INDX_SIG2B)) {
     BD_DEBUG(DBG_NAV | DBG_CRIT, "index.bdmv failed signature match: expected INDX0100 got %8.8s\n", bs->buf);
     return 0;
    }

    *index_start          = bs_read(bs, 32);
    *extension_data_start = bs_read(bs, 32);

    return 1;
}

static INDX_ROOT *_indx_parse(const char *file_name)
{
    BITSTREAM  bs;
    BD_FILE_H *fp;
    INDX_ROOT *index = calloc(1, sizeof(INDX_ROOT));

    int        indexes_start, extension_data_start;

    fp = file_open(file_name, "rb");
    if (!fp) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "indx_parse(): error opening %s\n", file_name);
        X_FREE(index);
        return NULL;
    }

    bs_init(&bs, fp);

    if (!_parse_header(&bs, &indexes_start, &extension_data_start)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "index.bdmv: invalid header\n");
        goto error;
    }

    if (!_parse_app_info(&bs, &index->app_info)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "index.bdmv: error parsing app info\n");
        goto error;
    }

    bs_seek_byte(&bs, indexes_start);

    if (!_parse_index(&bs, index)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "index.bdmv: error parsing indexes\n");
        goto error;
    }

    file_close(fp);

    return index;

 error:
    X_FREE(index);
    file_close(fp);
    return NULL;
}

INDX_ROOT *indx_parse(const char *file_name)
{
    INDX_ROOT *indx = _indx_parse(file_name);

    /* if failed, try backup file */
    if (!indx) {
        size_t len   = strlen(file_name);
        char *backup = malloc(len + 8);

        strcpy(backup, file_name);
        strcpy(backup + len - 10, "BACKUP/index.bdmv");

        indx = _indx_parse(backup);

        X_FREE(backup);
    }

    return indx;
}

void indx_free(INDX_ROOT **p)
{
    if (p && *p) {
        X_FREE((*p)->titles);
        X_FREE(*p);
    }
}
