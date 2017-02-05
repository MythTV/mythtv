/*
 * This file is part of libbluray
 * Copyright (C) 2015  Petri Hintukainen
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

#include "udf_fs.h"

#include "file/file.h"
#include "util/macro.h"
#include "util/mutex.h"
#include "util/logging.h"

#include "udfread.h"
#include "blockinput.h"

#include <stdlib.h>
#include <stdio.h>   // SEEK_SET
#include <string.h>
#include <inttypes.h>

/*
 * file access
 */

static void _file_close(BD_FILE_H *file)
{
    if (file) {
        udfread_file_close((UDFFILE*)file->internal);
        BD_DEBUG(DBG_FILE, "Closed UDF file (%p)\n", (void*)file);
        X_FREE(file);
    }
}

static int64_t _file_seek(BD_FILE_H *file, int64_t offset, int32_t origin)
{
    return udfread_file_seek((UDFFILE*)file->internal, offset, origin);
}

static int64_t _file_tell(BD_FILE_H *file)
{
    return udfread_file_tell((UDFFILE*)file->internal);
}

static int64_t _file_read(BD_FILE_H *file, uint8_t *buf, int64_t size)
{
    return udfread_file_read((UDFFILE*)file->internal, buf, size);
}

BD_FILE_H *udf_file_open(void *udf, const char *filename)
{
    BD_FILE_H *file = calloc(1, sizeof(BD_FILE_H));
    if (!file) {
        return NULL;
    }

    BD_DEBUG(DBG_FILE, "Opening UDF file %s... (%p)\n", filename, (void*)file);

    file->close = _file_close;
    file->seek  = _file_seek;
    file->read  = _file_read;
    file->write = NULL;
    file->tell  = _file_tell;
    file->eof   = NULL;

    file->internal = udfread_file_open((udfread*)udf, filename);
    if (!file->internal) {
        BD_DEBUG(DBG_FILE, "Error opening file %s!\n", filename);
        X_FREE(file);
    }

    return file;
}

/*
 * directory access
 */

static void _dir_close(BD_DIR_H *dir)
{
    if (dir) {
        udfread_closedir((UDFDIR*)dir->internal);
        BD_DEBUG(DBG_DIR, "Closed UDF dir (%p)\n", (void*)dir);
        X_FREE(dir);
    }
}

static int _dir_read(BD_DIR_H *dir, BD_DIRENT *entry)
{
    struct udfread_dirent e;

    if (!udfread_readdir((UDFDIR*)dir->internal, &e)) {
        return -1;
    }

    strncpy(entry->d_name, e.d_name, sizeof(entry->d_name));
    entry->d_name[sizeof(entry->d_name) - 1] = 0;

    return 0;
}

BD_DIR_H *udf_dir_open(void *udf, const char* dirname)
{
    BD_DIR_H *dir = calloc(1, sizeof(BD_DIR_H));
    if (!dir) {
        return NULL;
    }

    BD_DEBUG(DBG_DIR, "Opening UDF dir %s... (%p)\n", dirname, (void*)dir);

    dir->close = _dir_close;
    dir->read  = _dir_read;

    dir->internal = udfread_opendir((udfread*)udf, dirname);
    if (!dir->internal) {
        BD_DEBUG(DBG_DIR, "Error opening %s\n", dirname);
        X_FREE(dir);
    }

    return dir;
}

/*
 * UDF image access
 */

typedef struct {
    struct udfread_block_input i;
    BD_FILE_H *fp;
    BD_MUTEX mutex;
} UDF_BI;

static int _bi_close(struct udfread_block_input *bi_gen)
{
    UDF_BI *bi = (UDF_BI *)bi_gen;
    file_close(bi->fp);
    bd_mutex_destroy(&bi->mutex);
    X_FREE(bi);
    return 0;
}

static uint32_t _bi_size(struct udfread_block_input *bi_gen)
{
    UDF_BI *bi = (UDF_BI *)bi_gen;
    int64_t size = file_size(bi->fp);
    if (size >= 0) {
        return size / UDF_BLOCK_SIZE;
    }
    return 0;
}

static int _bi_read(struct udfread_block_input *bi_gen, uint32_t lba, void *buf, uint32_t nblocks, int flags)
{
    (void)flags;
    UDF_BI *bi = (UDF_BI *)bi_gen;
    int got = -1;
    int64_t pos = (int64_t)lba * UDF_BLOCK_SIZE;

    /* seek + read must be atomic */
    bd_mutex_lock(&bi->mutex);

    if (file_seek(bi->fp, SEEK_SET, pos) == pos) {
        int64_t bytes = file_read(bi->fp, (uint8_t*)buf, (int64_t)nblocks * UDF_BLOCK_SIZE);
        if (bytes > 0) {
            got = bytes / UDF_BLOCK_SIZE;
        }
    }

    bd_mutex_unlock(&bi->mutex);

    return got;
}

static struct udfread_block_input *_block_input(const char *img)
{
    BD_FILE_H *fp = file_open(img, "rb");
    if (fp) {
        UDF_BI *bi = calloc(1, sizeof(*bi));
        if (bi) {
            bi->fp      = fp;
            bi->i.close = _bi_close;
            bi->i.read  = _bi_read;
            bi->i.size  = _bi_size;
            bd_mutex_init(&bi->mutex);
            return &bi->i;
        }
        file_close(fp);
    }
    return NULL;
}


typedef struct {
    struct udfread_block_input i;
    void *read_block_handle;
    int (*read_blocks)(void *handle, void *buf, int lba, int num_blocks);
} UDF_SI;

static int _si_close(struct udfread_block_input *bi_gen)
{
    X_FREE(bi_gen);
    return 0;
}

static int _si_read(struct udfread_block_input *bi_gen, uint32_t lba, void *buf, uint32_t nblocks, int flags)
{
    (void)flags;
    UDF_SI *si = (UDF_SI *)bi_gen;
    return si->read_blocks(si->read_block_handle, buf, lba, nblocks);
}

static struct udfread_block_input *_stream_input(void *read_block_handle,
                                                 int (*read_blocks)(void *handle, void *buf, int lba, int num_blocks))
{
    UDF_SI *si = calloc(1, sizeof(*si));
    if (si) {
        si->read_block_handle = read_block_handle;
        si->read_blocks = read_blocks;
        si->i.close = _si_close;
        si->i.read  = _si_read;
        return &si->i;
    }
    return NULL;
}


void *udf_image_open(const char *img_path,
                     void *read_block_handle,
                     int (*read_blocks)(void *handle, void *buf, int lba, int num_blocks))
{
    udfread *udf = udfread_init();
    int result = -1;

    if (!udf) {
        return NULL;
    }

    /* stream ? */
    if (read_blocks) {
        struct udfread_block_input *si = _stream_input(read_block_handle, read_blocks);
        if (si) {
            result = udfread_open_input(udf, si);
            if (result < 0) {
                si->close(si);
            }
        }
    } else {

    /* app handles file I/O ? */
    if (result < 0 && file_open != file_open_default()) {
        struct udfread_block_input *bi = _block_input(img_path);
        if (bi) {
            result = udfread_open_input(udf, bi);
            if (result < 0) {
                bi->close(bi);
            }
        }
    }

    if (result < 0) {
        result = udfread_open(udf, img_path);
    }
    }

    if (result < 0) {
        udfread_close(udf);
        return NULL;
    }

    return (void*)udf;
}

const char *udf_volume_id(void *udf)
{
    return udfread_get_volume_id((udfread*)udf);
}

void udf_image_close(void *udf)
{
    udfread_close((udfread*)udf);
}
