/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "bdjo_parse.h"

#include "bdjo_data.h"

#include "disc/disc.h"

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"

#include <stdlib.h>
#include <string.h>

static char *_read_string(BITSTREAM* bs, uint32_t length)
{
    char *out = malloc(length + 1);
    if (out) {
        bs_read_string(bs, out, length);
    } else {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
        bs_skip(bs, length * 8);
    }
    return out;
}

/* BDJO_TERMINAL_INFO */

static int _parse_terminal_info(BITSTREAM* bs, BDJO_TERMINAL_INFO *p)
{
    bs_skip(bs, 32); // skip length

    bs_read_string(bs, p->default_font, 5);
    p->initial_havi_config_id = bs_read(bs, 4);
    p->menu_call_mask         = bs_read(bs, 1);
    p->title_search_mask      = bs_read(bs, 1);

    bs_skip(bs, 34); // skip padding

    return 1;
}

/* BDJO_APP_CACHE_ITEM */

static void _parse_app_cache_item(BITSTREAM* bs, BDJO_APP_CACHE_ITEM *p)
{
    p->type = bs_read(bs, 8);

    bs_read_string(bs, p->ref_to_name, 5);
    bs_read_string(bs, p->lang_code,   3);

    bs_skip(bs, 24); // skip padding
}

/* BDJO_APP_CACHE_INFO */

static void _clean_app_cache_info(BDJO_APP_CACHE_INFO *p)
{
    if (p) {
        X_FREE(p->item);
    }
}

static int _parse_app_cache_info(BITSTREAM* bs, BDJO_APP_CACHE_INFO *p)
{
    unsigned ii;

    bs_skip(bs, 32); // skip length

    p->num_item = bs_read(bs, 8);
    bs_skip(bs, 8); // skip padding

    p->item = calloc(p->num_item, sizeof(BDJO_APP_CACHE_ITEM));
    if (!p->item) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
        return -1;
    }

    for (ii = 0; ii < p->num_item; ii++) {
        _parse_app_cache_item(bs, &p->item[ii]);
    }

    return 1;
}

/* BDJO_ACCESSIBLE_PLAYLISTS */

static void _clean_accessible_playlists(BDJO_ACCESSIBLE_PLAYLISTS *p)
{
    if (p) {
        X_FREE(p->pl);
    }
}

static int _parse_accessible_playlists(BITSTREAM* bs, BDJO_ACCESSIBLE_PLAYLISTS *p)
{
    unsigned ii;

    bs_skip(bs, 32); // skip length

    p->num_pl                        = bs_read(bs, 11);
    p->access_to_all_flag            = bs_read(bs, 1);
    p->autostart_first_playlist_flag = bs_read(bs, 1);
    bs_skip(bs, 19); // skip padding

    p->pl = calloc(p->num_pl, sizeof(BDJO_PLAYLIST));
    if (!p->pl) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
        return -1;
    }

    for (ii = 0; ii < p->num_pl; ii++) {
        bs_read_string(bs, p->pl[ii].name, 5);
        bs_skip(bs, 8); // skip padding
    }

    return 1;
}

/* BDJO_APP_PROFILE */

static void _parse_app_profile(BITSTREAM *bs, BDJO_APP_PROFILE *p)
{
    p->profile_number = bs_read(bs, 16);
    p->major_version  = bs_read(bs, 8);
    p->minor_version  = bs_read(bs, 8);
    p->micro_version  = bs_read(bs, 8);

    bs_skip(bs, 8);
}

/* BDJO_APP_NAME */

static void _clean_app_name(BDJO_APP_NAME *p)
{
    if (p) {
        X_FREE(p->name);
    }
}

static int _count_app_strings(BITSTREAM *bs, uint16_t data_length, uint16_t prefix_bytes, const char *type)
{
    int      count = 0;
    uint32_t bytes_read = 0;
    int64_t  pos = bs_pos(bs) >> 3;

    while (bytes_read < data_length) {
        bs_skip(bs, prefix_bytes * 8);
        uint8_t length = bs_read(bs, 8);
        bs_skip(bs, 8 * length);
        bytes_read += prefix_bytes + 1 + length;
        count++;
    }

    // seek back
    if (bytes_read) {
        if (bs_seek_byte(bs, pos) < 0) {
            return -1;
        }
    }

    if (bytes_read != data_length) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "data size mismatch (%d/%d), skipping %s\n", bytes_read, data_length, type);
        count = 0;
    }

    return count;
}

static int _parse_app_name(BITSTREAM *bs, BDJO_APP_NAME *p)
{
    bs_read_string(bs, p->lang, 3);
    uint32_t length = bs_read(bs, 8);
    p->name = _read_string(bs, length);
    return p->name ? 1 : -1;
}

/* BDJO_APP_PARAM */

static void _clean_app_param(BDJO_APP_PARAM *p)
{
    if (p) {
        X_FREE(p->param);
    }
}

static int _parse_app_param(BITSTREAM *bs, BDJO_APP_PARAM *p)
{
    uint32_t length = bs_read(bs, 8);
    p->param = _read_string(bs, length);
    return p->param ? 1 : -1;
}

/* BDJO_APP */

static void _clean_bdjo_app(BDJO_APP *p)
{
    if (p) {
        unsigned ii;
        for (ii = 0; ii < p->num_name; ii++) {
            _clean_app_name(&p->name[ii]);
        }
        for (ii = 0; ii < p->num_param; ii++) {
            _clean_app_param(&p->param[ii]);
        }
        X_FREE(p->profile);
        X_FREE(p->name);
        X_FREE(p->icon_locator);
        X_FREE(p->base_dir);
        X_FREE(p->classpath_extension);
        X_FREE(p->initial_class);
        X_FREE(p->param);
    }
}

static char *_read_app_string(BITSTREAM *bs)
{
    char *result;
    uint8_t length = bs_read(bs, 8);

    result = _read_string(bs, length);

    // word align
    if (!(length & 1))
        bs_skip(bs, 8);

    return result;
}

static int _parse_app_names(BITSTREAM *bs, BDJO_APP *p)
{
    unsigned ii;
    int r;

    uint32_t data_length = bs_read(bs, 16);
    r = _count_app_strings(bs, data_length, 3, "names");
    if (r < 0) {
        return -1;
    }
    p->num_name = r;

    if (data_length == 0) return 1;

    if (p->num_name) {
        p->name = calloc(p->num_name, sizeof(BDJO_APP_NAME));
        if (!p->name) {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
            return -1;
        }

        for (ii = 0; ii < p->num_name; ii++) {
            if (_parse_app_name(bs, &p->name[ii]) < 0) {
                return -1;
            }
        }

    } else {
        // ignore invalid data (seek over chunk)
        bs_skip(bs, data_length*8);
    }

    // word align
    bs_skip(bs, 8 * (data_length & 1));

    return 1;
}


static int _parse_app_params(BITSTREAM *bs, BDJO_APP *p)
{
    unsigned ii;
    int r;

    uint32_t data_length = bs_read(bs, 8);
    r = _count_app_strings(bs, data_length, 0, "params");
    if (r < 0) {
        return -1;
    }
    p->num_param = r;

    if (p->num_param) {
        p->param = calloc(p->num_param, sizeof(BDJO_APP_PARAM));
        if (!p->param) {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
            return -1;
        }

        for (ii = 0; ii < p->num_param; ii++) {
            if (_parse_app_param(bs, &p->param[ii]) < 0) {
                return -1;
            }
        }

    } else {
        // ignore invalid data (seek over chunk)
        bs_skip(bs, data_length*8);
    }

    // word align
    if (!(data_length & 1))
        bs_skip(bs, 8);

    return 1;
}

static int _parse_bdjo_app(BITSTREAM *bs, BDJO_APP *p)
{
    unsigned ii;

    p->control_code = bs_read(bs, 8);
    p->type         = bs_read(bs, 4);
    bs_skip(bs, 4);
    p->org_id       = bs_read(bs, 32);
    p->app_id       = bs_read(bs, 16);

    bs_skip(bs, 80); // skip sescriptor tag and length

    /* application descriptor */

    p->num_profile = bs_read(bs, 4);
    bs_skip(bs, 12); // skip padding

    p->profile = calloc(p->num_profile, sizeof(BDJO_APP_PROFILE));
    if (!p->profile) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
        return -1;
    }

    for (ii = 0; ii < p->num_profile; ii++) {
        _parse_app_profile(bs, &p->profile[ii]);
    }

    p->priority   = bs_read(bs, 8);
    p->binding    = bs_read(bs, 2);
    p->visibility = bs_read(bs, 2);
    bs_skip(bs, 4);

    if (_parse_app_names(bs, p) < 0) {
        return -1;
    }

    p->icon_locator        = _read_app_string(bs);
    if (!p->icon_locator) {
        return -1;
    }
    p->icon_flags          = bs_read(bs, 16);

    p->base_dir            = _read_app_string(bs);
    if (!p->base_dir) {
        return -1;
    }

    p->classpath_extension = _read_app_string(bs);
    if (!p->classpath_extension) {
        return -1;
    }

    p->initial_class       = _read_app_string(bs);
    if (!p->initial_class) {
        return -1;
    }

    if (_parse_app_params(bs, p) < 0) {
        return -1;
    }

    return 1;
}

/* BDJO_APP_MANAGEMENT_TABLE */

static void _clean_app_management_table(BDJO_APP_MANAGEMENT_TABLE *p)
{
    if (p) {
        unsigned ii;
        for (ii = 0; ii < p->num_app; ii++) {
            _clean_bdjo_app(&p->app[ii]);
        }
        X_FREE(p->app);
    }
}

static int _parse_app_management_table(BITSTREAM *bs, BDJO_APP_MANAGEMENT_TABLE *p)
{
    unsigned ii;

    bs_skip(bs, 32);  // length

    p->num_app = bs_read(bs, 8);
    bs_skip(bs, 8);  // skip padding

    p->app = calloc(p->num_app, sizeof(BDJO_APP));
    if (!p->app) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
        return -1;
    }

    for (ii = 0; ii < p->num_app; ii++) {
        /* TODO: if parsing of application data fails, ignore that app but parse others */
        if (_parse_bdjo_app(bs, &p->app[ii]) < 0) {
            return -1;
        }
    }

    return 1;
}

/* BDJO_KEY_INTEREST_TABLE */

static int _parse_key_interest_table(BITSTREAM *bs, BDJO_KEY_INTEREST_TABLE *p)
{
    p->vk_play              = bs_read(bs, 1);
    p->vk_stop              = bs_read(bs, 1);
    p->vk_ffw               = bs_read(bs, 1);
    p->vk_rew               = bs_read(bs, 1);
    p->vk_track_next        = bs_read(bs, 1);
    p->vk_track_prev        = bs_read(bs, 1);
    p->vk_pause             = bs_read(bs, 1);
    p->vk_still_off         = bs_read(bs, 1);
    p->vk_sec_audio_ena_dis = bs_read(bs, 1);
    p->vk_sec_video_ena_dis = bs_read(bs, 1);
    p->pg_textst_ena_dis    = bs_read(bs, 1);

    bs_skip(bs, 21);

    return 1;
}

/* BDJO_FILE_ACCESS_INFO */

static void _clean_file_access_info(BDJO_FILE_ACCESS_INFO *p)
{
    if (p) {
        X_FREE(p->path);
    }
}

static int _parse_file_access_info(BITSTREAM *bs, BDJO_FILE_ACCESS_INFO *p)
{
    uint16_t file_access_length = bs_read(bs, 16);
    p->path = _read_string(bs, file_access_length);
    return p->path ? 1 : -1;
}

/* BDJO */

static void _clean_bdjo(BDJO *p)
{
    if (p) {
        _clean_app_cache_info(&p->app_cache_info);
        _clean_accessible_playlists(&p->accessible_playlists);
        _clean_app_management_table(&p->app_table);
        _clean_file_access_info(&p->file_access_info);
    }
}

static int _get_version(const uint8_t* str)
{
    if (memcmp(str, "0100", 4) != 0)
        return 100;
    else if (memcmp(str, "0200", 4) != 0)
        return 200;
    else
        return 0;
}

static int _check_version(BITSTREAM *bs)
{
    // first check magic number
    uint8_t magic[4];
    bs_read_bytes(bs, magic, 4);

    if (memcmp(magic, "BDJO", 4) != 0) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Invalid magic number in BDJO.\n");
        return -1;
    }

    // get version string
    uint8_t version[4];
    bs_read_bytes(bs, version, 4);
    if (!_get_version(version)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Invalid version of BDJO.\n");
        return -1;
    }
    BD_DEBUG(DBG_BDJ, "[bdj] BDJO > Version: %.4s\n", version);

    // skip address table
    bs_skip(bs, 8*40);

    return 1;
}

static BDJO *_bdjo_parse(BD_FILE_H *fp)
{
    BITSTREAM   bs;
    BDJO       *p;

    if (bs_init(&bs, fp) < 0) {
        BD_DEBUG(DBG_BDJ, "?????.bdjo: read error\n");
        return NULL;
    }

    p = calloc(1, sizeof(BDJO));
    if (!p) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Out of memory\n");
        return NULL;
    }

    if (_check_version(&bs) < 0 ||
        _parse_terminal_info(&bs, &p->terminal_info) < 0 ||
        _parse_app_cache_info(&bs, &p->app_cache_info) < 0 ||
        _parse_accessible_playlists(&bs, &p->accessible_playlists) < 0 ||
        _parse_app_management_table(&bs, &p->app_table) < 0 ||
        _parse_key_interest_table(&bs, &p->key_interest_table) < 0 ||
        _parse_file_access_info(&bs, &p->file_access_info) < 0) {

        bdjo_free(&p);
    }

    return p;
}

/*
 *
 */

void bdjo_free(BDJO **pp)
{
    if (pp && *pp) {
        _clean_bdjo(*pp);
        X_FREE(*pp);
    }
}

BDJO *bdjo_parse(const char *path)
{
    BD_FILE_H *fp;
    BDJO      *bdjo;

    fp = file_open(path, "rb");
    if (!fp) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to open bdjo file (%s)\n", path);
        return NULL;
    }

    bdjo = _bdjo_parse(fp);
    file_close(fp);
    return bdjo;
}

static BDJO *_bdjo_get(BD_DISC *disc, const char *dir, const char *file)
{
    BD_FILE_H *fp;
    BDJO      *bdjo;

    fp = disc_open_file(disc, dir, file);
    if (!fp) {
        return NULL;
    }

    bdjo = _bdjo_parse(fp);
    file_close(fp);

    return bdjo;
}

BDJO *bdjo_get(BD_DISC *disc, const char *file)
{
    BDJO *bdjo;

    bdjo = _bdjo_get(disc, "BDMV" DIR_SEP "BDJO", file);
    if (bdjo) {
        return bdjo;
    }

    /* if failed, try backup file */
    bdjo = _bdjo_get(disc, "BDMV" DIR_SEP "BACKUP" DIR_SEP "BDJO", file);
    return bdjo;
}
