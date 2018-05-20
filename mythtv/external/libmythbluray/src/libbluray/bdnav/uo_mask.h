/*
 * This file is part of libbluray
 * Copyright (C) 2010-2015  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_BD_UO_MASK_H_)
#define _BD_UO_MASK_H_

#include "uo_mask_table.h"

#include "util/attributes.h"

#include <stdint.h>

static inline BD_UO_MASK uo_mask_combine(BD_UO_MASK a, BD_UO_MASK b)
{
    union {
        uint64_t   u64;
        BD_UO_MASK mask;
    } mask_a = {0}, mask_b = {0}, result;

    mask_a.mask = a;
    mask_b.mask = b;
    result.u64 = mask_a.u64 | mask_b.u64;

    return result.mask;
}

#define EMPTY_UO_MASK  {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0}

static inline BD_UO_MASK uo_mask_get_empty(void)
{
    static const union {
        const uint64_t   u64;
        const BD_UO_MASK mask;
    } empty = {0};

    return empty.mask;
}

BD_PRIVATE int uo_mask_parse(const uint8_t *buf, BD_UO_MASK *uo);

#endif // _BD_UO_MASK_H_
