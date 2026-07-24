/*
 * This file is part of libdvdread
 * Copyright (C) 2026 VideoLAN
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

#ifndef DVDREAD_FILESYSTEM_H_
#define DVDREAD_FILESYSTEM_H_

#include <stdint.h>
#include <sys/types.h>

#if defined(_MSC_VER)
#include <basetsd.h>
#if !defined(ssize_t)
typedef SSIZE_T ssize_t;
#endif
#endif

#define DVDREAD_NAME_MAX 255
/* directory entry, only d_name is used by the lib */
typedef struct
{
    char    d_name[DVDREAD_NAME_MAX + 1];
} dvd_dirent_t;

#define DVD_S_IFMT         0170000     /* These bits determine file type. */
#define DVD_S_IFCHR        0020000     /* character special */
#define DVD_S_IFDIR        0040000     /* directory */
#define DVD_S_IFBLK        0060000     /* block special */
#define DVD_S_IFREG        0100000     /* regular */

/* This is fixed width so it does not depend on the caller off_t. */
typedef int64_t dvd_off_t;
typedef struct
{
    dvd_off_t size;
    unsigned int st_mode;
} dvdstat_t;

/*
 * the struct an application fills in to provide its own filesystem
 *
 * dir_open and file_open return a handle the application defines, the lib only
 * hands it back to the matching read, seek and close calls
 *
 * stat returns 0 on success or a negative value on error
 * dir_read returns 0 on success, a positive value at end of directory, a negative value on error
 * file_read returns the bytes read, 0 at end of file, a negative value on error
 * file_seek returns the offset from the start of the file or a negative value on error
 */
typedef struct dvd_reader_filesystem_s dvd_reader_filesystem_h;
struct dvd_reader_filesystem_s
{
    void *internal;
    void    (*close)     (dvd_reader_filesystem_h *fs);
    int     (*stat)      (dvd_reader_filesystem_h *fs, const char *path, dvdstat_t *statbuf);
    void   *(*dir_open)  (dvd_reader_filesystem_h *fs, const char *dirname);
    int     (*dir_read)  (void *dir, dvd_dirent_t *entry);
    void    (*dir_close) (void *dir);
    void   *(*file_open) (dvd_reader_filesystem_h *fs, const char *filename);
    ssize_t (*file_read) (void *file, char *buf, size_t size);
    dvd_off_t (*file_seek) (void *file, dvd_off_t offset, int whence);
    int     (*file_close)(void *file);
};

#endif /* DVDREAD_FILESYSTEM_H_ */
