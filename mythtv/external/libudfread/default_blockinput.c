/*
 * This file is part of libudfread
 * Copyright (C) 2014-2017 VLC authors and VideoLAN
 *
 * Authors: Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "default_blockinput.h"
#include "blockinput.h"

#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef _WIN32
#include <windows.h>
#ifndef HAVE_UNISTD_H
#include <stdio.h>
#endif
#include <io.h>
# undef  lseek
# define lseek _lseeki64
# undef  off_t
# define off_t int64_t
#endif

#ifdef __ANDROID__
# undef  lseek
# define lseek lseek64
# undef  off_t
# define off_t off64_t
#endif

#ifdef _WIN32
static ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
    OVERLAPPED ov;
    DWORD      got;
    HANDLE     handle;

    handle = (HANDLE)(intptr_t)_get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    memset(&ov, 0, sizeof(ov));
    ov.Offset     = (DWORD)offset;
    ov.OffsetHigh = (offset >> 32);
    if (!ReadFile(handle, buf, count, &got, &ov)) {
        return -1;
    }
    return got;
}

#elif defined (NEED_PREAD_IMPL)

#include <pthread.h>
static ssize_t pread_impl(int fd, void *buf, size_t count, off_t offset)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    ssize_t result;

    pthread_mutex_lock(&lock);

    if (lseek(fd, offset, SEEK_SET) != offset) {
        result = -1;
    } else {
        result = read(fd, buf, count);
    }

    pthread_mutex_unlock(&lock);
    return result;
}

#define pread(a,b,c,d) pread_impl(a,b,c,d)

#endif /* _WIN32 || NEED_PREAD_IMPL */


typedef struct default_block_input {
    udfread_block_input input;
    int                 fd;
} default_block_input;


static int _def_close(udfread_block_input *p_gen)
{
    default_block_input *p = (default_block_input *)p_gen;
    int result = -1;

    if (p) {
        if (p->fd >= 0) {
            result = close(p->fd);
        }
        free(p);
    }

    return result;
}

static uint32_t _def_size(udfread_block_input *p_gen)
{
    default_block_input *p = (default_block_input *)p_gen;
    off_t pos;

    pos = lseek(p->fd, 0, SEEK_END);
    if (pos < 0) {
        return 0;
    }

    return (uint32_t)(pos / UDF_BLOCK_SIZE);
}

static int _def_read(udfread_block_input *p_gen, uint32_t lba, void *buf, uint32_t nblocks, int flags)
{
    default_block_input *p = (default_block_input *)p_gen;
    size_t bytes, got;
    off_t  pos;

    (void)flags;

    bytes = (size_t)nblocks * UDF_BLOCK_SIZE;
    got   = 0;
    pos   = (off_t)lba * UDF_BLOCK_SIZE;

    while (got < bytes) {
        ssize_t ret = pread(p->fd, ((char*)buf) + got, bytes - got, pos + (off_t)got);

        if (ret <= 0) {
            if (ret < 0 && errno == EINTR) {
                continue;
            }
            if (got < UDF_BLOCK_SIZE) {
                return ret;
            }
            break;
        }
        got += (size_t)ret;
    }

    return got / UDF_BLOCK_SIZE;
}

udfread_block_input *block_input_new(const char *path)
{
    default_block_input *p = (default_block_input*)calloc(1, sizeof(default_block_input));
    if (!p) {
        return NULL;
    }

#ifdef _WIN32
    p->fd = open(path, O_RDONLY | O_BINARY);
#else
    p->fd = open(path, O_RDONLY);
#endif
    if(p->fd < 0) {
        free(p);
        return NULL;
    }

    p->input.close = _def_close;
    p->input.read  = _def_read;
    p->input.size  = _def_size;

    return &p->input;
}
