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

#if !defined(_BD_DISC_H_)
#define _BD_DISC_H_

#include "util/attributes.h"

#include <stdint.h>
#include <stddef.h> /* size_t */

struct bd_file_s;
struct bd_dir_s;
struct bd_enc_info;

/* application provided file system access (optional) */
typedef struct fs_access {
    void *fs_handle;

    /* method 1: block (device) access */
    int (*read_blocks)(void *fs_handle, void *buf, int lba, int num_blocks);

    /* method 2: file access */
    struct bd_dir_s  *(*open_dir) (void *fs_handle, const char *rel_path);
    struct bd_file_s *(*open_file)(void *fs_handle, const char *rel_path);
} fs_access;

/*
 * BluRay Virtual File System
 *
 * Map file access to BD-ROM file system or binding unit data area
 */

typedef struct bd_disc BD_DISC;

BD_PRIVATE BD_DISC *disc_open(const char *device_path,
                              fs_access *p_fs,
                              struct bd_enc_info *enc_info,
                              const char *keyfile_path,
                              void *regs, void *psr_read, void *psr_write);

BD_PRIVATE void     disc_close(BD_DISC **);

/* Get BD-ROM root path */
BD_PRIVATE const char *disc_root(BD_DISC *disc);

/* Get UDF volume ID */
BD_PRIVATE const char *disc_volume_id(BD_DISC *);

/* Generate pseudo disc ID */
BD_PRIVATE void disc_pseudo_id(BD_DISC *, uint8_t *id/*[20]*/);

/* Open VFS file (relative to disc root) */
BD_PRIVATE struct bd_file_s *disc_open_file(BD_DISC *disc, const char *dir, const char *file);
BD_PRIVATE struct bd_file_s *disc_open_path(BD_DISC *disc, const char *path);

/* Open VFS directory (relative to disc root) */
BD_PRIVATE struct bd_dir_s *disc_open_dir (BD_DISC *disc, const char *dir);

/* Read VFS file */
BD_PRIVATE size_t disc_read_file(BD_DISC *disc, const char *dir, const char *file,
                                  uint8_t **data);

/* Update virtual package */
BD_PRIVATE void disc_update(BD_DISC *disc, const char *overlay_root);

BD_PRIVATE int  disc_cache_bdrom_file(BD_DISC *p, const char *rel_path, const char *cache_path);

/* open BD-ROM directory (relative to disc root) */
BD_PRIVATE struct bd_dir_s  *disc_open_bdrom_dir(BD_DISC *disc, const char *path);

/*
 * m2ts stream interface
 */

BD_PRIVATE struct bd_file_s *disc_open_stream(BD_DISC *disc, const char *file);

/*
 *
 */

BD_PRIVATE const uint8_t *disc_get_data(BD_DISC *, int type);

enum {
    DISC_EVENT_START,       /* param: number of titles, 0 if playing with menus */
    DISC_EVENT_TITLE,       /* param: title number */
    DISC_EVENT_APPLICATION, /* param: app data */
};

BD_PRIVATE void disc_event(BD_DISC *, uint32_t event, uint32_t param);


#endif /* _BD_DISC_H_ */
