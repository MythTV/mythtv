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

#ifndef LIBBLURAY_MUTEX_H_
#define LIBBLURAY_MUTEX_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(_WIN32)
#   include <windows.h>
#elif defined(HAVE_PTHREAD_H)
#   include <pthread.h>
#else
#   error no mutex support found
#endif


#if defined(_WIN32)

#include <errno.h>

typedef CRITICAL_SECTION BD_MUTEX;

#define bd_mutex_lock(m) \
  (EnterCriticalSection(m), 0)

#define bd_mutex_unlock(m) \
  (LeaveCriticalSection(m), 0)

#define bd_mutex_trylock(m) \
  (TryEnterCriticalSection(m) ? 0 : EBUSY)

#define bd_mutex_init(m) \
  (InitializeCriticalSection(m), 0)

#define bd_mutex_destroy(m) \
  (DeleteCriticalSection(m), 0)


#elif defined(HAVE_PTHREAD_H)

/*
 * recursive mutex
 */

typedef struct bd_mutex_s BD_MUTEX;
struct bd_mutex_s {
    int             lock_count;
    pthread_t       owner;
    pthread_mutex_t mutex;
};

static inline int bd_mutex_init(BD_MUTEX *p)
{
    p->owner      = (pthread_t)-1;
    p->lock_count = 0;

    if (pthread_mutex_init(&p->mutex, NULL)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_init() failed !");
        return -1;
    }

    return 0;
}

static inline int bd_mutex_destroy(BD_MUTEX *p)
{
    if (pthread_mutex_destroy(&p->mutex)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_destroy() failed !");
        return -1;
    }
    return 0;
}

static int bd_mutex_lock(BD_MUTEX *p)
{
    if (pthread_equal(p->owner, pthread_self())) {
        /* recursive lock */
        p->lock_count++;
        return 0;
    }

    if (pthread_mutex_lock(&p->mutex)) {
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_lock() failed !\n");
        return -1;
    }

    p->owner      = pthread_self();
    p->lock_count = 1;

    return 0;
}

static int bd_mutex_unlock(BD_MUTEX *p)
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
        BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_mutex_unlock() failed !\n");
        return -1;
    }

    return 0;
}

#endif // HAVE_PTHREAD_H


#endif // LIBBLURAY_MUTEX_H_
