/*
 * This file is part of libbluray
 * Copyright (C) 2010  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "mobj_parse.h"

#include "mobj_data.h"

#include "disc/disc.h"

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#include <stdlib.h>
#include <string.h>

#define MOBJ_SIG1  ('M' << 24 | 'O' << 16 | 'B' << 8 | 'J')
#define MOBJ_SIG2A ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define MOBJ_SIG2B ('0' << 24 | '1' << 16 | '0' << 8 | '0')

static int _mobj_parse_header(BITSTREAM *bs, int *extension_data_start)
{
    uint32_t sig1, sig2;

    if (bs_seek_byte(bs, 0) < 0) {
        return 0;
    }

    sig1 = bs_read(bs, 32);
    sig2 = bs_read(bs, 32);

    if (sig1 != MOBJ_SIG1 ||
       (sig2 != MOBJ_SIG2A &&
        sig2 != MOBJ_SIG2B)) {
     BD_DEBUG(DBG_NAV, "MovieObject.bdmv failed signature match: expected MOBJ0100 got %8.8s\n", bs->buf);
     return 0;
    }

    *extension_data_start = bs_read(bs, 32);

    return 1;
}

void mobj_parse_cmd(uint8_t *buf, MOBJ_CMD *cmd)
{
    BITBUFFER bb;
    bb_init(&bb, buf, 12);

    cmd->insn.op_cnt     = bb_read(&bb, 3);
    cmd->insn.grp        = bb_read(&bb, 2);
    cmd->insn.sub_grp    = bb_read(&bb, 3);

    cmd->insn.imm_op1    = bb_read(&bb, 1);
    cmd->insn.imm_op2    = bb_read(&bb, 1);
    bb_skip(&bb, 2);    /* reserved */
    cmd->insn.branch_opt = bb_read(&bb, 4);

    bb_skip(&bb, 4);    /* reserved */
    cmd->insn.cmp_opt    = bb_read(&bb, 4);

    bb_skip(&bb, 3);    /* reserved */
    cmd->insn.set_opt    = bb_read(&bb, 5);

    cmd->dst = bb_read(&bb, 32);
    cmd->src = bb_read(&bb, 32);
}

static int _mobj_parse_object(BITSTREAM *bs, MOBJ_OBJECT *obj)
{
    int i;

    obj->resume_intention_flag = bs_read(bs, 1);
    obj->menu_call_mask = bs_read(bs, 1);
    obj->title_search_mask = bs_read(bs, 1);

    bs_skip(bs, 13); /* padding */

    obj->num_cmds = bs_read(bs, 16);
    obj->cmds     = calloc(obj->num_cmds, sizeof(MOBJ_CMD));
    if (!obj->cmds) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return 0;
    }

    for (i = 0; i < obj->num_cmds; i++) {
        uint8_t buf[12];
        bs_read_bytes(bs, buf, 12);
        mobj_parse_cmd(buf, &obj->cmds[i]);
    }

    return 1;
}

void mobj_free(MOBJ_OBJECTS **p)
{
    if (p && *p) {

        if ((*p)->objects) {
            int i;
            for (i = 0 ; i < (*p)->num_objects; i++) {
                X_FREE((*p)->objects[i].cmds);
            }

            X_FREE((*p)->objects);
        }

        X_FREE(*p);
    }
}

static MOBJ_OBJECTS *_mobj_parse(BD_FILE_H *fp)
{
    BITSTREAM     bs;
    MOBJ_OBJECTS *objects = NULL;
    uint16_t      num_objects;
    uint32_t      data_len;
    int           extension_data_start, i;

    if (bs_init(&bs, fp) < 0) {
        BD_DEBUG(DBG_NAV, "MovieObject.bdmv: read error\n");
        goto error;
    }

    if (!_mobj_parse_header(&bs, &extension_data_start)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "MovieObject.bdmv: invalid header\n");
        goto error;
    }

    if (extension_data_start) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "MovieObject.bdmv: unknown extension data at %d\n", extension_data_start);
    }

    if (bs_seek_byte(&bs, 40) < 0) {
        BD_DEBUG(DBG_NAV, "MovieObject.bdmv: read error\n");
        goto error;
    }

    data_len = bs_read(&bs, 32);

    if ((bs_end(&bs) - bs_pos(&bs))/8 < (int64_t)data_len) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "MovieObject.bdmv: invalid data_len %d !\n", data_len);
        goto error;
    }

    objects = calloc(1, sizeof(MOBJ_OBJECTS));
    if (!objects) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        goto error;
    }

    bs_skip(&bs, 32); /* reserved */
    num_objects = bs_read(&bs, 16);

    objects->num_objects = num_objects;
    objects->objects = calloc(num_objects, sizeof(MOBJ_OBJECT));
    if (!objects->objects) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        goto error;
    }

    for (i = 0; i < objects->num_objects; i++) {
        if (!_mobj_parse_object(&bs, &objects->objects[i])) {
            BD_DEBUG(DBG_NAV | DBG_CRIT, "MovieObject.bdmv: error parsing object %d\n", i);
            goto error;
        }
    }

    return objects;

 error:
    mobj_free(&objects);
    return NULL;
}

MOBJ_OBJECTS *mobj_parse(const char *file_name)
{
    BD_FILE_H    *fp;
    MOBJ_OBJECTS *objects;

    fp = file_open(file_name, "rb");
    if (!fp) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "error opening %s\n", file_name);
        return NULL;
    }

    objects = _mobj_parse(fp);
    file_close(fp);
    return objects;
}

static MOBJ_OBJECTS *_mobj_get(BD_DISC *disc, const char *path)
{
    BD_FILE_H    *fp;
    MOBJ_OBJECTS *objects;

    fp = disc_open_path(disc, path);
    if (!fp) {
        return NULL;
    }

    objects = _mobj_parse(fp);
    file_close(fp);
    return objects;
}

MOBJ_OBJECTS *mobj_get(BD_DISC *disc)
{
    MOBJ_OBJECTS *objects;

    objects = _mobj_get(disc, "BDMV" DIR_SEP "MovieObject.bdmv");
    if (objects) {
        return objects;
    }

    /* if failed, try backup file */
    objects = _mobj_get(disc, "BDMV" DIR_SEP "BACKUP" DIR_SEP "MovieObject.bdmv");
    return objects;
}
