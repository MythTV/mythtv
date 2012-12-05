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

#include "pg_decode.h"

#include "util/macro.h"
#include "util/logging.h"
#include "util/bits.h"

#include <string.h>
#include <stdlib.h>


void pg_decode_video_descriptor(BITBUFFER *bb, BD_PG_VIDEO_DESCRIPTOR *p)
{
    p->video_width  = bb_read(bb, 16);
    p->video_height = bb_read(bb, 16);
    p->frame_rate   = bb_read(bb, 4);
    bb_skip(bb, 4);
}

void pg_decode_composition_descriptor(BITBUFFER *bb, BD_PG_COMPOSITION_DESCRIPTOR *p)
{
    p->number = bb_read(bb, 16);
    p->state  = bb_read(bb, 2);
    bb_skip(bb, 6);
}

void pg_decode_sequence_descriptor(BITBUFFER *bb, BD_PG_SEQUENCE_DESCRIPTOR *p)
{
    p->first_in_seq = bb_read(bb, 1);
    p->last_in_seq  = bb_read(bb, 1);
    bb_skip(bb, 6);
}

void pg_decode_window(BITBUFFER *bb, BD_PG_WINDOW *p)
{
    p->id     = bb_read(bb, 8);
    p->x      = bb_read(bb, 16);
    p->y      = bb_read(bb, 16);
    p->width  = bb_read(bb, 16);
    p->height = bb_read(bb, 16);
}

void pg_decode_composition_object(BITBUFFER *bb, BD_PG_COMPOSITION_OBJECT *p)
{
    p->object_id_ref = bb_read(bb, 16);
    p->window_id_ref = bb_read(bb, 8);

    p->crop_flag      = bb_read(bb, 1);
    p->forced_on_flag = bb_read(bb, 1);
    bb_skip(bb, 6);

    p->x = bb_read(bb, 16);
    p->y = bb_read(bb, 16);

    if (p->crop_flag) {
        p->crop_x = bb_read(bb, 16);
        p->crop_y = bb_read(bb, 16);
        p->crop_w = bb_read(bb, 16);
        p->crop_h = bb_read(bb, 16);
    }
}

/*
 * segments
 */

int pg_decode_palette_update(BITBUFFER *bb, BD_PG_PALETTE *p)
{
    p->id      = bb_read(bb, 8);
    p->version = bb_read(bb, 8);

    while (!bb_eof(bb)) {
        uint8_t entry_id = bb_read(bb, 8);

        p->entry[entry_id].Y  = bb_read(bb, 8);
        p->entry[entry_id].Cr = bb_read(bb, 8);
        p->entry[entry_id].Cb = bb_read(bb, 8);
        p->entry[entry_id].T  = bb_read(bb, 8);
    }

    return 1;
}

int pg_decode_palette(BITBUFFER *bb, BD_PG_PALETTE *p)
{
    memset(p->entry, 0, sizeof(p->entry));

    return pg_decode_palette_update(bb, p);
}

static int _decode_rle(BITBUFFER *bb, BD_PG_OBJECT *p)
{
    int pixels_left = p->width * p->height;
    int num_rle     = 0;
    int rle_size    = p->width * p->height / 4;

    if (rle_size < 1)
        rle_size = 1;

    p->img = realloc(p->img, rle_size * sizeof(BD_PG_RLE_ELEM));
    if (!p->img) {
        BD_DEBUG(DBG_DECODE | DBG_CRIT, "pg_decode_object(): relloc(%zu) failed\n",
                 rle_size * sizeof(BD_PG_RLE_ELEM));
        return 0;
    }

    while (!bb_eof(bb)) {
        uint32_t len   = 1;
        uint8_t  color = 0;

        if (!(color = bb_read(bb, 8))) {
            if (!bb_read(bb, 1)) {
                if (!bb_read(bb, 1)) {
                    len = bb_read(bb, 6);
                } else {
                    len = bb_read(bb, 14);
                }
            } else {
                if (!bb_read(bb, 1)) {
                    len = bb_read(bb, 6);
                } else {
                    len = bb_read(bb, 14);
                }
                color = bb_read(bb, 8);
            }
        }

        p->img[num_rle].len   = len;
        p->img[num_rle].color = color;

        pixels_left -= len;

        if (pixels_left < 0) {
            BD_DEBUG(DBG_DECODE, "pg_decode_object(): too many pixels (%d)\n", -pixels_left);
            return 0;
        }

        num_rle++;
        if (num_rle >= rle_size) {
            void *tmp = p->img;
            rle_size *= 2;
            p->img = realloc(p->img, rle_size * sizeof(BD_PG_RLE_ELEM));
            if (!p->img) {
                BD_DEBUG(DBG_DECODE | DBG_CRIT, "pg_decode_object(): relloc(%zu) failed\n",
                         rle_size * sizeof(BD_PG_RLE_ELEM));
                X_FREE(tmp);
                return 0;
            }
        }
    }

    if (pixels_left > 0) {
        BD_DEBUG(DBG_DECODE, "pg_decode_object(): missing %d pixels\n", pixels_left);
        return 0;
    }

    return 1;
}

int pg_decode_object(BITBUFFER *bb, BD_PG_OBJECT *p)
{
    BD_PG_SEQUENCE_DESCRIPTOR sd;

    p->id      = bb_read(bb, 16);
    p->version = bb_read(bb, 8);

    pg_decode_sequence_descriptor(bb, &sd);

    /* splitted segments should be already joined */
    if (!sd.first_in_seq) {
        BD_DEBUG(DBG_DECODE, "pg_decode_object(): not first in sequence\n");
        return 0;
    }
    if (!sd.last_in_seq) {
        BD_DEBUG(DBG_DECODE, "pg_decode_object(): not last in sequence\n");
        return 0;
    }

    if (!bb_is_align(bb, 0x07)) {
      BD_DEBUG(DBG_DECODE, "pg_decode_object(): alignment error\n");
      return 0;
    }

    uint32_t data_len = bb_read(bb, 24);
    uint32_t buf_len  = bb->p_end - bb->p;
    if (data_len != buf_len) {
        BD_DEBUG(DBG_DECODE, "pg_decode_object(): buffer size mismatch (expected %d, have %d)\n", data_len, buf_len);
        return 0;
    }

    p->width  = bb_read(bb, 16);
    p->height = bb_read(bb, 16);

    return _decode_rle(bb, p);
}

int pg_decode_composition(BITBUFFER *bb, BD_PG_COMPOSITION *p)
{
    unsigned ii;

    pg_decode_video_descriptor(bb, &p->video_descriptor);
    pg_decode_composition_descriptor(bb, &p->composition_descriptor);

    p->palette_update_flag = bb_read(bb, 1);
    bb_skip(bb, 7);

    p->palette_id_ref      = bb_read(bb, 8);

    p->num_composition_objects = bb_read(bb, 8);
    p->composition_object      = calloc(p->num_composition_objects, sizeof(BD_PG_COMPOSITION_OBJECT));

    for (ii = 0; ii < p->num_composition_objects; ii++) {
        pg_decode_composition_object(bb, &p->composition_object[ii]);
    }

    return 1;
}

int pg_decode_windows(BITBUFFER *bb, BD_PG_WINDOWS *p)
{
    unsigned ii;

    p->num_windows = bb_read(bb, 8);
    p->window = calloc(p->num_windows, sizeof(BD_PG_WINDOW));

    for (ii = 0; ii < p->num_windows; ii++) {
        pg_decode_window(bb, &p->window[ii]);
    }

    return 1;
}

/*
 * cleanup
 */

void pg_clean_object(BD_PG_OBJECT *p)
{
    if (p) {
        X_FREE(p->img);
    }
}

void pg_clean_composition(BD_PG_COMPOSITION *p)
{
    if (p) {
        X_FREE(p->composition_object);
    }
}

void pg_clean_windows(BD_PG_WINDOWS *p)
{
    if (p) {
        X_FREE(p->window);
    }
}

void pg_free_palette(BD_PG_PALETTE **p)
{
    if (p && *p) {
        X_FREE(*p);
    }
}

void pg_free_object(BD_PG_OBJECT **p)
{
    if (p && *p) {
        pg_clean_object(*p);
        X_FREE(*p);
    }
}

void pg_free_composition(BD_PG_COMPOSITION **p)
{
    if (p && *p) {
        pg_clean_composition(*p);
        X_FREE(*p);
    }
}

void pg_free_windows(BD_PG_WINDOWS **p)
{
    if (p && *p) {
        pg_clean_windows(*p);
        X_FREE(*p);
    }
}
