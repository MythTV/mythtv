/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "textst_decode.h"

#include "pg_decode.h"           // pg_decode_*()

#include "util/macro.h"
#include "util/bits.h"
#include "util/logging.h"

#include <string.h>
#include <stdlib.h>


static int8_t _decode_int8(BITBUFFER *bb)
{
  unsigned sign = bb_read(bb, 1);
  int8_t   val  = bb_read(bb, 7);
  return sign ? -val : val;
}

static int16_t _decode_int16(BITBUFFER *bb)
{
  unsigned sign = bb_read(bb, 1);
  int16_t  val  = bb_read(bb, 15);
  return sign ? -val : val;
}

static int64_t _decode_pts(BITBUFFER *bb)
{
    return ((int64_t)bb_read(bb, 1)) << 32 | bb_read(bb, 32);
}

static void _decode_rect(BITBUFFER *bb, BD_TEXTST_RECT *p)
{
    p->xpos = bb_read(bb, 16);;
    p->ypos = bb_read(bb, 16);;
    p->width = bb_read(bb, 16);;
    p->height = bb_read(bb, 16);;
}

static void _decode_region_info(BITBUFFER *bb, BD_TEXTST_REGION_INFO *p)
{
  _decode_rect(bb, &p->region);
  p->background_color = bb_read(bb, 8);
  bb_skip(bb, 8);
}

static void _decode_font_style(BITBUFFER *bb, BD_TEXTST_FONT_STYLE *p)
{
    uint8_t font_style = bb_read(bb, 8);
    p->bold            = !!(font_style & 1);
    p->italic          = !!(font_style & 2);
    p->outline_border  = !!(font_style & 4);
}

static void _decode_region_style(BITBUFFER *bb, BD_TEXTST_REGION_STYLE *p)
{
    p->region_style_id = bb_read(bb, 8);

    _decode_region_info(bb, &p->region_info);
    _decode_rect(bb, &p->text_box);

    p->text_flow   = bb_read(bb, 8);
    p->text_halign = bb_read(bb, 8);
    p->text_valign = bb_read(bb, 8);
    p->line_space  = bb_read(bb, 8);
    p->font_id_ref = bb_read(bb, 8);

    _decode_font_style(bb, &p->font_style);

    p->font_size         = bb_read(bb, 8);
    p->font_color        = bb_read(bb, 8);
    p->outline_color     = bb_read(bb, 8);
    p->outline_thickness = bb_read(bb, 8);
}

static void _decode_user_style(BITBUFFER *bb, BD_TEXTST_USER_STYLE *p)
{
    p->user_style_id         = bb_read(bb, 8);
    p->region_hpos_delta     = _decode_int16(bb);
    p->region_vpos_delta     = _decode_int16(bb);
    p->text_box_hpos_delta   = _decode_int16(bb);
    p->text_box_vpos_delta   = _decode_int16(bb);
    p->text_box_width_delta  = _decode_int16(bb);
    p->text_box_height_delta = _decode_int16(bb);
    p->font_size_delta       = _decode_int8(bb);
    p->line_space_delta      = _decode_int8(bb);
}

static int _decode_dialog_region(BITBUFFER *bb, BD_TEXTST_DIALOG_REGION *p)
{
    p->continous_present_flag = bb_read(bb, 1);
    p->forced_on_flag         = bb_read(bb, 1);
    bb_skip(bb, 6);
    p->region_style_id_ref    = bb_read(bb, 8);

    uint16_t data_length      = bb_read(bb, 16);
    int      bytes_allocated  = data_length;
    uint16_t bytes_read       = 0;

    p->elem       = malloc(bytes_allocated);
    p->elem_count = 0;
    p->line_count = 1;
    if (!p->elem) {
        BD_DEBUG(DBG_DECODE | DBG_CRIT, "out of memory\n");
        return 0;
    }

    uint8_t *ptr = (uint8_t *)p->elem;

    while (bytes_read < data_length) {

        /* parse header */

        uint8_t code = bb_read(bb, 8);
        bytes_read++;
        if (code != 0x1b) {
            BD_DEBUG(DBG_DECODE, "_decode_dialog_region(): missing escape\n");
            continue;
        }

        uint8_t type   = bb_read(bb, 8);
        uint8_t length = bb_read(bb, 8);
        bytes_read += 2 + length;

        /* realloc */

        int bytes_used = ((intptr_t)ptr - (intptr_t)p->elem);
        int need = bytes_used + length + sizeof(BD_TEXTST_DATA);
        if (bytes_allocated < need) {
            bytes_allocated = need * 2;
            BD_TEXTST_DATA *tmp = realloc(p->elem, bytes_allocated);
            if (!tmp) {
                BD_DEBUG(DBG_DECODE | DBG_CRIT, "out of memory\n");
                return 0;
            }
            p->elem = tmp;
            ptr = ((uint8_t *)p->elem) + bytes_used;
        }

        /* parse content */

        BD_TEXTST_DATA *data = (BD_TEXTST_DATA *)ptr;
        memset(data, 0, sizeof(*data));

        data->type = type;
        switch (data->type) {
            case BD_TEXTST_DATA_STRING:
                bb_read_bytes(bb, data->data.text.string, length);
                data->data.text.length = length;
                ptr += length;
                break;
            case BD_TEXTST_DATA_FONT_ID:
                data->data.font_id_ref = bb_read(bb, 8);
                break;
            case BD_TEXTST_DATA_FONT_STYLE:
                _decode_font_style(bb, &data->data.style.style);
                data->data.style.outline_color = bb_read(bb, 8);
                data->data.style.outline_thickness = bb_read(bb, 8);
                break;
            case BD_TEXTST_DATA_FONT_SIZE:
                data->data.font_size = bb_read(bb, 8);
                break;
            case BD_TEXTST_DATA_FONT_COLOR:
                data->data.font_color = bb_read(bb, 8);
                break;
            case BD_TEXTST_DATA_NEWLINE:
                p->line_count++;
                break;
            case BD_TEXTST_DATA_RESET_STYLE:
                break;
            default:
                BD_DEBUG(DBG_DECODE, "_decode_dialog_region(): unknown marker %d (len %d)\n", type, length);
                bb_skip(bb, 8 * length);
                continue;
        }
        ptr += sizeof(BD_TEXTST_DATA);
        p->elem_count++;
    }

    return 1;
}

static void _decode_palette(BITBUFFER *bb, BD_PG_PALETTE_ENTRY *p)
{
    uint16_t entries = bb_read(bb, 16) / 5;
    unsigned ii;

    memset(p, 0, 256 * sizeof(*p));
    for (ii = 0; ii < entries; ii++) {
        pg_decode_palette_entry(bb, p);
    }
}

/*
 * segments
 */


BD_PRIVATE int textst_decode_dialog_style(BITBUFFER *bb, BD_TEXTST_DIALOG_STYLE *p)
{
    unsigned ii;

    p->player_style_flag  = bb_read(bb, 1);
    bb_skip(bb, 15);
    p->region_style_count = bb_read(bb, 8);
    p->user_style_count   = bb_read(bb, 8);

    if (p->region_style_count) {
        p->region_style = calloc(p->region_style_count, sizeof(BD_TEXTST_REGION_STYLE));
        if (!p->region_style) {
            BD_DEBUG(DBG_DECODE | DBG_CRIT, "out of memory\n");
            return 0;
        }
        for (ii = 0; ii < p->region_style_count; ii++) {
            _decode_region_style(bb, &p->region_style[ii]);
        }
    }

    if (p->user_style_count) {
        p->user_style = calloc(p->user_style_count, sizeof(BD_TEXTST_USER_STYLE));
        if (!p->user_style) {
            BD_DEBUG(DBG_DECODE | DBG_CRIT, "out of memory\n");
            return 0;
        }
        for (ii = 0; ii < p->user_style_count; ii++) {
            _decode_user_style(bb, &p->user_style[ii]);
        }
    }

    _decode_palette(bb, p->palette);

    return 1;
}

BD_PRIVATE int textst_decode_dialog_presentation(BITBUFFER *bb, BD_TEXTST_DIALOG_PRESENTATION *p)
{
    unsigned ii, palette_update_flag;

    bb_skip(bb, 7);
    p->start_pts = _decode_pts(bb);
    bb_skip(bb, 7);
    p->end_pts   = _decode_pts(bb);

    palette_update_flag = bb_read(bb, 1);
    bb_skip(bb, 7);

    if (palette_update_flag) {
        p->palette_update = calloc(256, sizeof(BD_PG_PALETTE_ENTRY));
        if (!p->palette_update) {
            BD_DEBUG(DBG_DECODE | DBG_CRIT, "out of memory\n");
            return 0;
        }
        _decode_palette(bb, p->palette_update);
    }

    p->region_count = bb_read(bb, 8);
    if (p->region_count) {
        if (p->region_count > 2) {
            BD_DEBUG(DBG_DECODE | DBG_CRIT, "too many regions (%d)\n", p->region_count);
            return 0;
        }
        for (ii = 0; ii < p->region_count; ii++) {
            if (!_decode_dialog_region(bb, &p->region[ii])) {
                return 0;
            }
        }
    }

    return 1;
}

/*
 * cleanup
 */

void textst_clean_dialog_presentation(BD_TEXTST_DIALOG_PRESENTATION *p)
{
     if (p) {
         X_FREE(p->palette_update);
         X_FREE(p->region[0].elem);
         X_FREE(p->region[1].elem);
     }
}

static void _clean_style(BD_TEXTST_DIALOG_STYLE *p)
{
    if (p) {
        X_FREE(p->region_style);
        X_FREE(p->user_style);
    }
}

void textst_free_dialog_style(BD_TEXTST_DIALOG_STYLE **p)
{
    if (p && *p) {
        _clean_style(*p);
        X_FREE(*p);
    }
}
