/*
 * This file is part of libbluray
 * Copyright (C) 2010  VideoLAN
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

#include "time.h"

#include <stdint.h>

#if defined(_WIN32)
#   include <windows.h>
#elif defined(HAVE_SYS_TIME_H)
#   include <sys/time.h>
#else
#   error no time support found
#endif

#if defined(_WIN32)

static uint64_t _bd_get_scr_impl(void)
{
    HANDLE thread;
    DWORD_PTR mask;
    LARGE_INTEGER frequency, counter;

    thread = GetCurrentThread();
    mask = SetThreadAffinityMask(thread, 1);
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    SetThreadAffinityMask(thread, mask);

    return (uint64_t)(counter.QuadPart * 1000.0 / frequency.QuadPart) * 90;
}

#elif defined(HAVE_SYS_TIME_H)

static uint64_t _bd_get_scr_impl(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000) * 90;
}

#endif

uint64_t bd_get_scr(void)
{
    static uint64_t t0 = (uint64_t)-1;

    uint64_t now = _bd_get_scr_impl();

    if (t0 > now) {
        t0 = now;
    }

    return now - t0;
}
