/*
 * This file is part of libbluray
 * Copyright (C) 2010-2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "mutex.h"

#include "logging.h"
#include "macro.h"

#if defined(_WIN32)
#   include <windows.h>
#elif defined(HAVE_PTHREAD_H)
#   include <pthread.h>
#else
#   error no mutex support found
#endif


#if defined(_WIN32)

typedef struct {
    CRITICAL_SECTION cs;
} MUTEX_IMPL;

static int _mutex_lock(MUTEX_IMPL *p)
{
    EnterCriticalSection(&p->cs);
    return 0;
}

static int _mutex_unlock(MUTEX_IMPL *p)
{
    LeaveCriticalSection(&p->cs);
    return 0;
}

static int _mutex_init(MUTEX_IMPL *p)
{
    InitializeCriticalSection(&p->cs);
    return 0;
}

static int _mutex_destroy(MUTEX_IMPL *p)
{
    DeleteCriticalSection(&p->cs);
    return 0;
}


#elif defined(HAVE_PTHREAD_H)

typedef struct {
    int             lock_count;
    pthread_t       owner;
    pthread_mutex_t mutex;
} MUTEX_IMPL;

static int _mutex_init(MUTEX_IMPL *p)
{
    p->owner      = (pthread_t)-1;
    p->lock_count = 0;

    if (pthread_mutex_init(&p->mutex, NULL)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "pthread_mutex_init() failed !\n");
        return -1;
    }

    return 0;
}

static int _mutex_lock(MUTEX_IMPL *p)
{
    if (pthread_equal(p->owner, pthread_self())) {
        /* recursive lock */
        p->lock_count++;
        return 0;
    }

    if (pthread_mutex_lock(&p->mutex)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "pthread_mutex_lock() failed !\n");
        return -1;
    }

    p->owner      = pthread_self();
    p->lock_count = 1;

    return 0;
}

static int _mutex_unlock(MUTEX_IMPL *p)
{
    if (!pthread_equal(p->owner, pthread_self())) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_unlock(): not owner !\n");
        return -1;
    }

    p->lock_count--;
    if (p->lock_count > 0) {
        return 0;
    }

    /* unlock */

    p->owner = (pthread_t)-1;

    if (pthread_mutex_unlock(&p->mutex)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "pthread_mutex_unlock() failed !\n");
        return -1;
    }

    return 0;
}

static int _mutex_destroy(MUTEX_IMPL *p)
{
    _mutex_lock(p);
    _mutex_unlock(p);

    if (pthread_mutex_destroy(&p->mutex)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "pthread_mutex_destroy() failed !\n");
        return -1;
    }

    return 0;
}

#endif /* HAVE_PTHREAD_H */

int bd_mutex_lock(BD_MUTEX *p)
{
    if (!p->impl) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_lock() failed !\n");
        return -1;
    }
    return _mutex_lock((MUTEX_IMPL*)p->impl);
}

int bd_mutex_unlock(BD_MUTEX *p)
{
    if (!p->impl) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_unlock() failed !\n");
        return -1;
    }
    return _mutex_unlock((MUTEX_IMPL*)p->impl);
}

int bd_mutex_init(BD_MUTEX *p)
{
    p->impl = calloc(1, sizeof(MUTEX_IMPL));
    if (!p->impl) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_init() failed !\n");
        return -1;
    }

    if (_mutex_init((MUTEX_IMPL*)p->impl) < 0) {
        X_FREE(p->impl);
        return -1;
    }

    return 0;
}

int bd_mutex_destroy(BD_MUTEX *p)
{
    if (!p->impl) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_destroy() failed !\n");
        return -1;
    }

    if (_mutex_destroy((MUTEX_IMPL*)p->impl) < 0) {
        return -1;
    }

    X_FREE(p->impl);
    return 0;
}

