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

#include "refcnt.h"

#include "logging.h"
#include "mutex.h"

#include <stdlib.h>
#include <string.h>

/*
 *
 */

typedef struct bd_refcnt {
  struct bd_refcnt *me;
  void    (*cleanup)(void *);
  BD_MUTEX mutex;   /* initialized only if counted == 1 */
  int      count;   /* reference count */
  unsigned counted; /* 1 if this object is ref-counted */
} BD_REFCNT;

/*
 *
 */

void refcnt_inc(const void *obj)
{
    BD_REFCNT *ref;

    if (!obj) {
        return;
    }

    ref = ((const BD_REFCNT *)obj)[-1].me;
    if (obj != (const void *)&ref[1]) {
        BD_DEBUG(DBG_CRIT, "refcnt_inc(): invalid object\n");
        return;
    }

    if (!ref->counted) {
        bd_mutex_init(&ref->mutex);
        ref->counted = 1;
        ref->count = 2;
        return;
    }

    bd_mutex_lock(&ref->mutex);
    ++ref->count;
    bd_mutex_unlock(&ref->mutex);
}

void refcnt_dec(const void *obj)
{
    BD_REFCNT *ref;

    if (!obj) {
        return;
    }

    ref = ((const BD_REFCNT *)obj)[-1].me;
    if (obj != (const void *)&ref[1]) {
        BD_DEBUG(DBG_CRIT, "refcnt_dec(): invalid object\n");
        return;
    }

    if (ref->counted) {
        int count;

        bd_mutex_lock(&ref->mutex);
        count = --ref->count;
        bd_mutex_unlock(&ref->mutex);

        if (count > 0) {
            return;
        }

        bd_mutex_destroy(&ref->mutex);
    }

    if (ref->cleanup)
        ref->cleanup(&ref[1]);

    memset(ref, 0, sizeof(*ref));
    free(ref);
}

void *refcnt_realloc(void *obj, size_t sz, void (*cleanup)(void *))
{
    sz += sizeof(BD_REFCNT);

    if (obj) {
        const BD_REFCNT *ref = ((const BD_REFCNT *)obj)[-1].me;
        if (obj != (const void *)&ref[1]) {
            BD_DEBUG(DBG_CRIT, "refcnt_realloc(): invalid object\n");
            return NULL;
        }

        if (ref->counted) {
            BD_DEBUG(DBG_CRIT, "refcnt_realloc(): realloc locked object !\n");
            return NULL;
        }

        obj = realloc(((BD_REFCNT *)obj)[-1].me, sz);
        if (!obj) {
            /* do not call cleanup() - nothing is free'd here */
            return NULL;
        }
    } else {
        obj = realloc(NULL, sz);
        if (!obj) {
            return NULL;
        }
        memset(obj, 0, sizeof(BD_REFCNT));
    }

    ((BD_REFCNT *)obj)->cleanup = cleanup;
    ((BD_REFCNT *)obj)->me = obj;
    return &((BD_REFCNT *)obj)[1];
}



