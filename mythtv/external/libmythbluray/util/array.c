/*
 * This file is part of libbluray
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

#include "array.h"

#include "macro.h"

#include <stdlib.h>


void *array_alloc(size_t n, size_t sz)
{
    size_t size = sizeof(void *) + sz;
    if (size < sz) {
        return NULL;
    }

    unsigned char *p = (unsigned char *)calloc(n, size);
    if (!p) {
        return NULL;
    }

    void **array = (void **)p;
    p += n * sizeof(void *);
    size_t i;
    for (i = 0; i < n; i++, p += sz) {
        array[i] = p;
    }

    return array;
}

void array_free(void **p)
{
    if (p && *p) {
        X_FREE(*p);
    }
}
