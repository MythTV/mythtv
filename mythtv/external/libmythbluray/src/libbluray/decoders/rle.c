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

#include "rle.h"

#include "util/logging.h"

/*
 * util
 */

static void _rle_ensure_size(RLE_ENC *p)
{
    if (BD_UNLIKELY(!p->free_elem)) {
        BD_PG_RLE_ELEM *start = rle_get(p);
        /* realloc to 2x */
        p->free_elem = p->num_elem;
        start = refcnt_realloc(start, p->num_elem * 2 * sizeof(BD_PG_RLE_ELEM));
        p->elem = start + p->num_elem;
        p->num_elem *= 2;
    }
}

/*
 * encoding
 */

static void _enc_elem(RLE_ENC *p, uint16_t color, uint16_t len)
{
    _rle_ensure_size(p);

    p->elem->color = color;
    p->elem->len = len;

    p->free_elem--;
    p->elem++;
}

static void _enc_eol(RLE_ENC *p)
{
    _enc_elem(p, 0, 0);
}

BD_PG_RLE_ELEM *rle_crop_object(const BD_PG_RLE_ELEM *orig, int width,
                                       int crop_x, int crop_y, int crop_w, int crop_h)
{
    RLE_ENC  rle;
    int      x0 = crop_x;
    int      x1 = crop_x + crop_w; /* first pixel outside of cropped region */
    int      x, y;

    rle_begin(&rle);

    /* skip crop_y */
    for (y = 0; y < crop_y; y++) {
        for (x = 0; x < width; x += orig->len, orig++) ;
    }

    /* crop lines */

    for (y = 0; y < crop_h; y++) {
        for (x = 0; x < width; ) {
          BD_PG_RLE_ELEM bite = *(orig++);

            if (BD_UNLIKELY(bite.len < 1)) {
                BD_DEBUG(DBG_GC | DBG_CRIT, "rle eol marker in middle of line (x=%d/%d)\n", x, width);
                continue;
            }

            /* starts outside, ends outside */
            if (x + bite.len < x0 || x >= x1) {
                x += bite.len;
                continue;
            }

            /* starts before ? */
            if (BD_UNLIKELY(x < x0)) {
                bite.len -= x0 - x;
                x = x0;
            }

            x += bite.len;

            /* ends after ? */
            if (BD_UNLIKELY(x >= x1)) {
                bite.len -= x - x1;
            }

            _enc_elem(&rle, bite.color, bite.len);
        }

        if (BD_LIKELY(!orig->len)) {
            /* skip eol marker */
            orig++;
        } else {
            BD_DEBUG(DBG_GC | DBG_CRIT, "rle eol marker missing\n");
        }

        _enc_eol(&rle);
    }

    return rle_get(&rle);
}

/*
 * compression
 */

static void _rle_grow(RLE_ENC *p)
{
    p->free_elem--;
    p->elem++;

    _rle_ensure_size(p);

    p->elem->len = 0;
}

void rle_add_eol(RLE_ENC *p)
{
    if (BD_LIKELY(p->elem->len)) {
        _rle_grow(p);
    }
    p->elem->color = 0;

    _rle_grow(p);
    p->elem->color = 0xffff;
}

void rle_add_bite(RLE_ENC *p, uint8_t color, int len)
{
    if (BD_LIKELY(color == p->elem->color)) {
        p->elem->len += len;
    } else {
        if (BD_LIKELY(p->elem->len)) {
            _rle_grow(p);
        }
        p->elem->color = color;
        p->elem->len = len;
    }
}

void rle_compress_chunk(RLE_ENC *p, const uint8_t *mem, unsigned width)
{
    unsigned ii;
    for (ii = 0; ii < width; ii++) {
        rle_add_bite(p, mem[ii], 1);
    }
}
