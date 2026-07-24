/*
 * This file is part of libdvdread
 * Copyright (C) 2022 VideoLAN
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <io.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/stat.h>
#include <fcntl.h>

/* mingw sys/stat.h defines stat as a macro, keep the struct member usable */
#undef stat

#include <windows.h>
#include "../msvc/contrib/win32_cs.h"

#include <dvdread/dvd_filesystem.h>
#include "filesystem.h"

// verify the off64_t from dvd_filesystem.h has the proper size
static_assert(sizeof(off64_t) <= (64/8), "off64_t bigger than 64 bits");
static_assert(sizeof(off64_t) >= (64/8), "off64_t smaller than 64 bits");
// verify the off_t from dvd_filesystem.h has the proper size
static_assert(sizeof(off_t)   <= (64/8), "off_t bigger than 64 bits");
static_assert(sizeof(off_t)   >= (64/8), "off_t smaller than 64 bits");

static void *file_open_default(dvd_reader_filesystem_h *fs, const char* filename)
{
    if (!fs)
        return NULL;

    int *handle;
    int fd;
    wchar_t *wpath;

    wpath = _utf8_to_wchar(filename);
    if (!wpath) {
        return NULL;
    }

    fd = _wopen(wpath, O_RDONLY | O_BINARY);
    free(wpath);
    if (fd < 0) {
        return NULL;
    }

    handle = malloc(sizeof(*handle));
    if (!handle) {
        close(fd);
        return NULL;
    }
    *handle = fd;

    return handle;
}

static ssize_t file_read_default(void *file, char *buf, size_t size)
{
    if (size == 0) {
        return 0;
    }

    /* Clamp oversized reads, callers must handle partial reads anyway. */
    if (size > INT_MAX)
        size = INT_MAX;

    return read(*(int*)file, buf, (unsigned int)size);
}

static dvd_off_t file_seek_default(void *file, dvd_off_t offset, int whence)
{
    return _lseeki64(*(int*)file, offset, whence);
}

static int file_close_default(void *file)
{
    if (file) {
        int ret = close(*(int*)file);
        free(file);
        return ret;
    }
    return 0;
}

typedef struct {
  intptr_t handle;
  struct _wfinddata_t went;
} win32_dir_t;

static void *dir_open_default(dvd_reader_filesystem_h *fs, const char* dirname)
{
    if (!fs)
        return NULL;

    char    *filespec;
    wchar_t *wfilespec;
    win32_dir_t *d = calloc(1, sizeof(*d));

    if (!d) {
        return NULL;
    }

    filespec = malloc(strlen(dirname) + 3);
    if (!filespec) {
        goto fail;
    }
    sprintf(filespec, "%s\\*", dirname);

    wfilespec = _utf8_to_wchar(filespec);
    free(filespec);
    if (!wfilespec) {
        goto fail;
    }

    d->handle = _wfindfirst(wfilespec, &d->went);
    free(wfilespec);
    if (d->handle != -1) {
        return d;
    }

    fail:
        free(d);
        return NULL;
}

static int dir_read_default(void *dir, dvd_dirent_t *entry)
{
    win32_dir_t *wdir = (win32_dir_t*)dir;
    if (wdir->went.name[0]) {
        if (!WideCharToMultiByte(CP_UTF8, 0, wdir->went.name, -1, entry->d_name, sizeof(entry->d_name), NULL, NULL))
            entry->d_name[0] = 0; /* allow reading next */
        wdir->went.name[0] = 0;
        _wfindnext(wdir->handle, &wdir->went);
        return 0;
    }
    /* end of directory, use a positive value so callers can tell it apart from an error */
    return 1;
}

static void dir_close_default(void *dir)
{
    if (dir) {
        win32_dir_t *wdir = (win32_dir_t*)dir;
        _findclose(wdir->handle);
        free(wdir);
    }
}

static int stat_default(dvd_reader_filesystem_h *fs, const char *path, dvdstat_t* statbuf)
{
    if (!fs)
        return -1;

    struct _stat64 win32statbuf;

    wchar_t *wpath, *it;

    wpath = _utf8_to_wchar(path);
    if (!wpath) {
        return -1;
    }

    /* need to strip possible trailing \\ */
    for (it = wpath; *it; it++)
        if ((*it == '\\' || *it == '/') && *(it+1) == 0)
        *it = 0;

    int ret = _wstat64(wpath, &win32statbuf);
    free(wpath);
    if (ret == 0) {
        statbuf->size = win32statbuf.st_size;
        statbuf->st_mode = win32statbuf.st_mode;
    }
    return ret;
}

static void default_filesystem_close(dvd_reader_filesystem_h *fs) {
  free(fs);
}

dvd_reader_filesystem_h* InitInternalFilesystem(void) {
  dvd_reader_filesystem_h* fs = calloc( 1, sizeof(dvd_reader_filesystem_h));
  if (!fs) {
    return NULL;
  }
  fs->close = default_filesystem_close;
  fs->stat = stat_default;
  fs->dir_open = dir_open_default;
  fs->dir_read = dir_read_default;
  fs->dir_close = dir_close_default;
  fs->file_open = file_open_default;
  fs->file_read = file_read_default;
  fs->file_seek = file_seek_default;
  fs->file_close = file_close_default;
  return fs;
}
