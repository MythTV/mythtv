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

#include "config.h"

#if !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#if defined(__GLIBC__) && defined(__GLIBC_MINOR__)
#  if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 24)
#    define USE_READDIR
#  endif
#endif

#include <dvdread/dvd_filesystem.h>

#include "filesystem.h"

// verify the off_t from dvd_filesystem.h has the proper size
static_assert(sizeof(off_t)   <= (64/8), "off_t bigger than 64 bits");
static_assert(sizeof(off_t)   >= (64/8), "off_t smaller than 64 bits");

static void *file_open_default(dvd_reader_filesystem_h *fs, const char* filename)
{
    if (!fs)
        return NULL;

    int *handle;
    int fd;
    int flags = O_RDONLY;
    int mode  = 0;

#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_BINARY
    flags |= O_BINARY;
#endif

    if ((fd = open(filename, flags, mode)) < 0) {
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

    return read(*(int*)file, buf, size);
}

static dvd_off_t file_seek_default(void *file, dvd_off_t offset, int whence)
{
    return lseek(*(int*)file, offset, whence);
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

static void *dir_open_default(dvd_reader_filesystem_h *fs, const char* dirname)
{
    if (!fs)
        return NULL;

    return opendir(dirname);
}

static int dir_read_default(void *dir, dvd_dirent_t *entry)
{
    struct dirent *p_e;

#ifdef USE_READDIR
    errno = 0;
    p_e = readdir((DIR*)dir);
    if (!p_e && errno) {
        return -errno;
    }
#else /* USE_READDIR */
    int result;
    struct dirent e;

    result = readdir_r((DIR*)dir, &e, &p_e);
    if (result) {
        return -result;
    }
#endif /* USE_READDIR */

    if (p_e == NULL) {
        return 1;
    }
    strncpy(entry->d_name, p_e->d_name, sizeof(entry->d_name));
    entry->d_name[sizeof(entry->d_name) - 1] = 0;

    return 0;
}

static void dir_close_default(void *dir)
{
    if (dir)
        closedir((DIR *)dir);
}

static int stat_default(dvd_reader_filesystem_h *fs, const char *path, dvdstat_t* statbuf)
{
    if (!fs)
        return -1;

    struct stat posixstatbuf;
    int ret = stat(path, &posixstatbuf);
    if (ret == 0) {
        statbuf->size = posixstatbuf.st_size;
        statbuf->st_mode = posixstatbuf.st_mode;
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
