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

static int _rle_ensure_size(RLE_ENC *p)
{
    if (BD_UNLIKELY(!p->free_elem)) {
        BD_PG_RLE_ELEM *start = rle_get(p);
        if (p->error) {
            return -1;
        }
        /* realloc to 2x */
        void *tmp = refcnt_realloc(start, p->num_elem * 2 * sizeof(BD_PG_RLE_ELEM));
        if (!tmp) {
            p->error = 1;
            return -1;
        }
        start = tmp;
        p->elem = start + p->num_elem;
        p->free_elem = p->num_elem;
        p->num_elem *= 2;
    }

    return 0;
}

/*
 * crop encoded image
 */

static int _enc_elem(RLE_ENC *p, uint16_t color, uint16_t len)
{
    if (BD_UNLIKELY(_rle_ensure_size(p) < 0)) {
        return -1;
    }

    p->elem->color = color;
    p->elem->len = len;

    p->free_elem--;
    p->elem++;

    return 0;
}

static int _enc_eol(RLE_ENC *p)
{
    return _enc_elem(p, 0, 0);
}

BD_PG_RLE_ELEM *rle_crop_object(const BD_PG_RLE_ELEM *orig, int width,
                                int crop_x, int crop_y, int crop_w, int crop_h)
{
    RLE_ENC  rle;
    int      x0 = crop_x;
    int      x1 = crop_x + crop_w; /* first pixel outside of cropped region */
    int      x, y;

    if (rle_begin(&rle) < 0) {
        return NULL;
    }

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

            if (BD_UNLIKELY(_enc_elem(&rle, bite.color, bite.len) < 0)) {
                goto out;
            }
        }

        if (BD_LIKELY(!orig->len)) {
            /* skip eol marker */
            orig++;
        } else {
            BD_DEBUG(DBG_GC | DBG_CRIT, "rle eol marker missing\n");
        }

        if (BD_UNLIKELY(_enc_eol(&rle) < 0)) {
            goto out;
        }
    }

 out:
    return rle_get(&rle);
}

/*
 * compression
 */

static int _rle_grow(RLE_ENC *p)
{
    p->free_elem--;
    p->elem++;

    if (BD_UNLIKELY(_rle_ensure_size(p) < 0)) {
        return -1;
    }

    p->elem->len = 0;

    return 0;
}

int rle_add_eol(RLE_ENC *p)
{
    if (BD_LIKELY(p->elem->len)) {
        if (BD_UNLIKELY(_rle_grow(p) < 0)) {
            return -1;
        }
    }
    p->elem->color = 0;

    if (BD_UNLIKELY(_rle_grow(p) < 0)) {
        return -1;
    }
    p->elem->color = 0xffff;

    return 0;
}

int rle_add_bite(RLE_ENC *p, uint8_t color, int len)
{
    if (BD_LIKELY(color == p->elem->color)) {
        p->elem->len += len;
    } else {
        if (BD_LIKELY(p->elem->len)) {
            if (BD_UNLIKELY(_rle_grow(p) < 0)) {
                return -1;
            }
        }
        p->elem->color = color;
        p->elem->len = len;
    }

    return 0;
}

int rle_compress_chunk(RLE_ENC *p, const uint8_t *mem, unsigned width)
{
    unsigned ii;
    for (ii = 0; ii < width; ii++) {
        if (BD_UNLIKELY(rle_add_bite(p, mem[ii], 1) < 0)) {
              return -1;
        }
    }
    return 0;
}
