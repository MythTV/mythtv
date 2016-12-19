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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "sound_parse.h"

#include "disc/disc.h"

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"

#include <stdlib.h>

#define BCLK_SIG1  ('B' << 24 | 'C' << 16 | 'L' << 8 | 'K')
#define BCLK_SIG2A ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define BCLK_SIG2B ('0' << 24 | '1' << 16 | '0' << 8 | '0')


static int _bclk_parse_header(BITSTREAM *bs, uint32_t *data_start, uint32_t *extension_data_start)
{
    uint32_t sig1, sig2;

    if (bs_seek_byte(bs, 0) < 0) {
        return 0;
    }

    sig1 = bs_read(bs, 32);
    sig2 = bs_read(bs, 32);

    if (sig1 != BCLK_SIG1 ||
       (sig2 != BCLK_SIG2A &&
        sig2 != BCLK_SIG2B)) {
     BD_DEBUG(DBG_NAV, "sound.bdmv failed signature match: expected BCLK0100 got %8.8s\n", bs->buf);
     return 0;
    }

    *data_start           = bs_read(bs, 32);
    *extension_data_start = bs_read(bs, 32);

    return 1;
}

static int _sound_parse_attributes(BITSTREAM *bs, SOUND_OBJECT *obj)
{
    int i;

    switch (i = bs_read(bs, 4)) {
        default: BD_DEBUG(DBG_NAV, "unknown channel configuration code %d\n", i);
                 /* fall thru */
        case 1:  obj->num_channels = 1;
                 break;
        case 3:  obj->num_channels = 2;
                 break;
    };
    switch (i = bs_read(bs, 4)) {
        default: BD_DEBUG(DBG_NAV, "unknown sample rate code %d\n", i);
                 /* fall thru */
        case 1:  obj->sample_rate = 48000;
                 break;
    };
    switch (i = bs_read(bs, 2)) {
        default: BD_DEBUG(DBG_NAV, "unknown bits per sample code %d\n", i);
                 /* fall thru */
        case 1:  obj->bits_per_sample = 16;
                 break;
    };

    bs_skip(bs, 6); /* padding */

    return 1;
}

static int _sound_parse_index(BITSTREAM *bs, uint32_t *sound_data_index, SOUND_OBJECT *obj)
{
    if (!_sound_parse_attributes(bs, obj))
      return 0;

    *sound_data_index = bs_read(bs, 32);
    obj->num_frames   = bs_read(bs, 32);
    obj->num_frames  /= (obj->bits_per_sample / 8) * obj->num_channels;

    return 1;
}

static int _sound_read_samples(BITSTREAM *bs, SOUND_OBJECT *obj)
{
    uint32_t n;
    uint32_t num_samples = obj->num_frames * obj->num_channels;

    if (!num_samples) {
        return 1;
    }

    obj->samples = calloc(num_samples, sizeof(uint16_t));
    if (!obj->samples) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return 0;
    }

    for (n = 0; n < num_samples; n++) {
        obj->samples[n] = bs_read(bs, 16);
    }

    return 1;
}

void sound_free(SOUND_DATA **p)
{
    if (p && *p) {

        if ((*p)->sounds) {
            unsigned i;
            for (i = 0 ; i < (*p)->num_sounds; i++) {
                X_FREE((*p)->sounds[i].samples);
            }

            X_FREE((*p)->sounds);
        }
        X_FREE(*p);
    }
}

static SOUND_DATA *_sound_parse(BD_FILE_H *fp)
{
    BITSTREAM     bs;
    SOUND_DATA   *data = NULL;
    uint16_t      num_sounds;
    uint32_t      data_len;
    int           i;
    uint32_t      data_start, extension_data_start;
    uint32_t     *data_offsets = NULL;

    if (bs_init(&bs, fp) < 0) {
        BD_DEBUG(DBG_NAV, "sound.bdmv: read error\n");
        goto error;
    }

    if (!_bclk_parse_header(&bs, &data_start, &extension_data_start)) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "invalid header\n");
        goto error;
    }

    if (bs_seek_byte(&bs, 40) < 0) {
        goto error;
    }

    data_len = bs_read(&bs, 32);
    bs_skip(&bs, 8); /* reserved */
    num_sounds = bs_read(&bs, 8);

    if (data_len < 1 || num_sounds < 1) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "empty database\n");
        goto error;
    }

    data_offsets = calloc(num_sounds, sizeof(uint32_t));
    data = calloc(1, sizeof(SOUND_DATA));
    if (!data_offsets || !data) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        goto error;
    }
    data->num_sounds = num_sounds;
    data->sounds = calloc(num_sounds, sizeof(SOUND_OBJECT));
    if (!data->sounds) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        goto error;
    }

    /* parse headers */

    for (i = 0; i < data->num_sounds; i++) {
        if (!_sound_parse_index(&bs, data_offsets + i, &data->sounds[i])) {
            BD_DEBUG(DBG_NAV | DBG_CRIT, "error parsing sound %d attributes\n", i);
            goto error;
        }
    }

    /* read samples */

    for (i = 0; i < data->num_sounds; i++) {

        if (bs_seek_byte(&bs, data_start + data_offsets[i]) < 0) {
            BD_DEBUG(DBG_NAV | DBG_CRIT, "error reading samples for sound %d\n", i);
            data->sounds[i].num_frames = 0;
            continue;
        }

        if (!_sound_read_samples(&bs, &data->sounds[i])) {
            BD_DEBUG(DBG_NAV | DBG_CRIT, "error reading samples for sound %d\n", i);
            goto error;
        }
    }

    X_FREE(data_offsets);
    return data;

 error:
    sound_free(&data);
    X_FREE(data_offsets);
    return NULL;
}

SOUND_DATA *sound_get(BD_DISC *disc)
{
    BD_FILE_H  *fp;
    SOUND_DATA *p;

    /* there's no no backup copy for sound.bdmv */

    fp = disc_open_path(disc, "BDMV" DIR_SEP "AUXDATA" DIR_SEP "sound.bdmv");
    if (!fp) {
        return NULL;
    }

    p = _sound_parse(fp);
    file_close(fp);
    return p;
}
