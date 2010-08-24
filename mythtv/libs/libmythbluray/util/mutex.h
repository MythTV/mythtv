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

#if defined(HAVE_PTHREAD_H)
#   include <pthread.h>
#elif defined(_WIN32)
#   include <windows.h>
#else
#   error no mutex support found
#endif

#if defined(_WIN32) && !defined(HAVE_PTHREAD_H)

#include <errno.h>

typedef CRITICAL_SECTION pthread_mutex_t;

#define pthread_mutex_lock(m) \
  (EnterCriticalSection(m), 0)

#define pthread_mutex_unlock(m) \
  (LeaveCriticalSection(m), 0)

#define pthread_mutex_trylock(m) \
  (TryEnterCriticalSection(m) ? 0 : EBUSY)

#define pthread_mutex_init(m, a) \
  (InitializeCriticalSection(m), 0)

#define pthread_mutex_destroy(m) \
  (DeleteCriticalSection(m), 0)

#endif


#endif // LIBBLURAY_MUTEX_H_
