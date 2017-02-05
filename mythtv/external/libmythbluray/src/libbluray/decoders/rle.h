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

#if !defined(_BD_RLE_H_)
#define _BD_RLE_H_

#include "overlay.h"

#include "util/attributes.h"

#include <stdint.h>

/*
 * encode state
 */

typedef struct {
    BD_PG_RLE_ELEM *elem;     /* current element */
    unsigned int    free_elem;/* unused element count */
    unsigned int    num_elem; /* allocated element count */

    int error;
} RLE_ENC;

/*
 *
 */

#include "util/refcnt.h"
#include "util/macro.h"

BD_PRIVATE BD_PG_RLE_ELEM *rle_crop_object(const BD_PG_RLE_ELEM *orig, int width,
                                           int crop_x, int crop_y, int crop_w, int crop_h);

static inline int rle_begin(RLE_ENC *p)
{
    p->num_elem = 1024;
    p->free_elem = 1024;
    p->elem = refcnt_realloc(NULL, p->num_elem * sizeof(BD_PG_RLE_ELEM));
    if (!p->elem) {
        return -1;
    }
    p->elem->len = 0;
    p->elem->color = 0xffff;

    p->error = 0;

    return 0;
}

static inline BD_PG_RLE_ELEM *rle_get(RLE_ENC *p)
{
    BD_PG_RLE_ELEM *start = (p->elem ? p->elem - (p->num_elem - p->free_elem) : NULL);
    if (p->error) {
        if (start) {
            bd_refcnt_dec(start);
            p->elem = NULL;
        }
        return NULL;
    }
    return start;
}

static inline void rle_end(RLE_ENC *p)
{
    BD_PG_RLE_ELEM *start = rle_get(p);
    if (start) {
        bd_refcnt_dec(start);
    }
    p->elem = NULL;
}

/*
 * compression
 */

BD_PRIVATE int rle_add_eol(RLE_ENC *p);
BD_PRIVATE int rle_add_bite(RLE_ENC *p, uint8_t color, int len);
BD_PRIVATE int rle_compress_chunk(RLE_ENC *p, const uint8_t *mem, unsigned width);

#endif /* _BD_RLE_H_ */
