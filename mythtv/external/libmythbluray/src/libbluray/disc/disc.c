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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "disc.h"

#include "dec.h"

#include "util/logging.h"
#include "util/macro.h"
#include "util/mutex.h"
#include "util/strutl.h"
#include "file/file.h"
#include "file/mount.h"

#include <ctype.h>
#include <string.h>

#ifdef ENABLE_UDF
#include "udf_fs.h"
#endif

struct bd_disc {
    BD_MUTEX  ovl_mutex;     /* protect access to overlay root */

    char     *disc_root;     /* disc filesystem root (if disc is mounted) */
    char     *overlay_root;  /* overlay filesystem root (if set) */

    BD_DEC   *dec;

    void         *fs_handle;
    BD_FILE_H * (*pf_file_open_bdrom)(void *, const char *);
    BD_DIR_H *  (*pf_dir_open_bdrom)(void *, const char *);
    void        (*pf_fs_close)(void *);

    const char   *udf_volid;

    int8_t        avchd;  /* -1 - unknown. 0 - no. 1 - yes */
};

/*
 * BD-ROM filesystem
 */

static BD_FILE_H *_bdrom_open_path(void *p, const char *rel_path)
{
    BD_DISC *disc = (BD_DISC *)p;
    BD_FILE_H *fp;
    char *abs_path;

    abs_path = str_printf("%s%s", disc->disc_root, rel_path);
    if (!abs_path) {
        return NULL;
    }

    fp = file_open(abs_path, "rb");
    X_FREE(abs_path);

    return fp;
}

static BD_DIR_H *_bdrom_open_dir(void *p, const char *dir)
{
    BD_DISC *disc = (BD_DISC *)p;
    BD_DIR_H *dp;
    char *path;

    path = str_printf("%s%s", disc->disc_root, dir);
    if (!path) {
        return NULL;
    }

    dp = dir_open(path);
    X_FREE(path);

    return dp;
}

/*
 * AVCHD 8.3 filenames
 */

static char *_avchd_file_name(const char *rel_path)
{
    static const char map[][2][6] = {
        { ".mpls", ".MPL" },
        { ".clpi", ".CPI" },
        { ".m2ts", ".MTS" },
        { ".bdmv", ".BDM" },
    };
    char *avchd_path = str_dup(rel_path);
    char *name = avchd_path ? strrchr(avchd_path, DIR_SEP_CHAR) : NULL;
    char *dot = name ? strrchr(name, '.') : NULL;
    size_t i;

    if (dot) {

        /* take up to 8 chars from file name */
        for (i = 0; *name && name < dot && i < 9; i++, name++) {
            *name = toupper(*name);
        }

        /* convert extension */
        for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
            if (!strcmp(dot, map[i][0])) {
                strcpy(name, map[i][1]);
                return avchd_path;
            }
        }
    }

    /* failed */
    X_FREE(avchd_path);
    return NULL;
}

/*
 * overlay filesystem
 */

static BD_FILE_H *_overlay_open_path(BD_DISC *p, const char *rel_path)
{
    BD_FILE_H *fp = NULL;

    bd_mutex_lock(&p->ovl_mutex);

    if (p->overlay_root) {
        char *abs_path = str_printf("%s%s", p->overlay_root, rel_path);
        if (abs_path) {
            fp = file_open(abs_path, "rb");
            X_FREE(abs_path);
        }
    }

    bd_mutex_unlock(&p->ovl_mutex);

    return fp;
}

static BD_DIR_H *_overlay_open_dir(BD_DISC *p, const char *dir)
{
    BD_DIR_H *dp = NULL;

    bd_mutex_lock(&p->ovl_mutex);

    if (p->overlay_root) {
        char *abs_path = str_printf("%s%s", p->disc_root, dir);
        if (abs_path) {
            dp = dir_open_default()(abs_path);
            X_FREE(abs_path);
        }
    }

    bd_mutex_unlock(&p->ovl_mutex);

    return dp;
}

/*
 * directory combining
 */

typedef struct {
    unsigned int count;
    unsigned int pos;
    BD_DIRENT    entry[1]; /* VLA */
} COMB_DIR;

static void _comb_dir_close(BD_DIR_H *dp)
{
    X_FREE(dp->internal);
    X_FREE(dp);
}

static int _comb_dir_read(BD_DIR_H *dp, BD_DIRENT *entry)
{
    COMB_DIR *priv = (COMB_DIR *)dp->internal;
    if (priv->pos < priv->count) {
        strcpy(entry->d_name, priv->entry[priv->pos++].d_name);
        return 0;
    }
    return 1;
}

static void _comb_dir_append(BD_DIR_H *dp, BD_DIRENT *entry)
{
    COMB_DIR *priv = (COMB_DIR *)dp->internal;
    unsigned int i;

    if (!priv) {
        return;
    }

    /* no duplicates */
    for (i = 0; i < priv->count; i++) {
        if (!strcmp(priv->entry[i].d_name, entry->d_name)) {
            return;
        }
    }

    /* append */
    priv = realloc(dp->internal, sizeof(*priv) + priv->count * sizeof(BD_DIRENT));
    if (!priv) {
        return;
    }
    strcpy(priv->entry[priv->count].d_name, entry->d_name);
    priv->count++;
    dp->internal = (void*)priv;
}

static BD_DIR_H *_combine_dirs(BD_DIR_H *ovl, BD_DIR_H *rom)
{
    BD_DIR_H *dp = calloc(1, sizeof(BD_DIR_H));
    BD_DIRENT entry;

    if (dp) {
        dp->read     = _comb_dir_read;
        dp->close    = _comb_dir_close;
        dp->internal = calloc(1, sizeof(COMB_DIR));
        if (!dp->internal) {
            X_FREE(dp);
            goto out;
        }

        while (!dir_read(ovl, &entry)) {
            _comb_dir_append(dp, &entry);
        }
        while (!dir_read(rom, &entry)) {
            _comb_dir_append(dp, &entry);
        }
    }

 out:
    dir_close(ovl);
    dir_close(rom);

    return dp;
}

/*
 * disc open / close
 */

static BD_DISC *_disc_init()
{
    BD_DISC *p = calloc(1, sizeof(BD_DISC));
    if (p) {
        bd_mutex_init(&p->ovl_mutex);

        /* default file access functions */
        p->fs_handle          = (void*)p;
        p->pf_file_open_bdrom = _bdrom_open_path;
        p->pf_dir_open_bdrom  = _bdrom_open_dir;

        p->avchd = -1;
    }
    return p;
}

static void _set_paths(BD_DISC *p, const char *device_path)
{
    if (device_path) {
        char *disc_root = mount_get_mountpoint(device_path);

        /* make sure path ends to slash */
        if (!disc_root || (disc_root[0] && disc_root[strlen(disc_root) - 1] == DIR_SEP_CHAR)) {
            p->disc_root = disc_root;
        } else {
            p->disc_root = str_printf("%s%c", disc_root, DIR_SEP_CHAR);
            X_FREE(disc_root);
        }
    }
}

BD_DISC *disc_open(const char *device_path,
                   fs_access *p_fs,
                   struct bd_enc_info *enc_info,
                   const char *keyfile_path,
                   void *regs, void *psr_read, void *psr_write)
{
    BD_DISC *p = _disc_init();

    if (p) {
        if (p_fs && p_fs->open_dir) {
            p->fs_handle          = p_fs->fs_handle;
            p->pf_file_open_bdrom = p_fs->open_file;
            p->pf_dir_open_bdrom  = p_fs->open_dir;
        }

        _set_paths(p, device_path);

#ifdef ENABLE_UDF
        /* check if disc root directory can be opened. If not, treat it as device/image file. */
        BD_DIR_H *dp_img = device_path ? dir_open(device_path) : NULL;
        if (!dp_img) {
            void *udf = udf_image_open(device_path, p_fs ? p_fs->fs_handle : NULL, p_fs ? p_fs->read_blocks : NULL);
            if (!udf) {
                BD_DEBUG(DBG_FILE | DBG_CRIT, "failed opening UDF image %s\n", device_path);
            } else {
                p->fs_handle          = udf;
                p->pf_fs_close        = udf_image_close;
                p->pf_file_open_bdrom = udf_file_open;
                p->pf_dir_open_bdrom  = udf_dir_open;

                p->udf_volid = udf_volume_id(udf);

                /* root not accessible with stdio */
                X_FREE(p->disc_root);
            }
        } else {
            dir_close(dp_img);
            BD_DEBUG(DBG_FILE, "%s does not seem to be image file or device node\n", device_path);
        }
#endif

        struct dec_dev dev = { p->fs_handle, p->pf_file_open_bdrom, p, (file_openFp)disc_open_path, p->disc_root, device_path };
        p->dec = dec_init(&dev, enc_info, keyfile_path, regs, psr_read, psr_write);
    }

    return p;
}

void disc_close(BD_DISC **pp)
{
    if (pp && *pp) {
        BD_DISC *p = *pp;

        dec_close(&p->dec);

        if (p->pf_fs_close) {
            p->pf_fs_close(p->fs_handle);
        }

        bd_mutex_destroy(&p->ovl_mutex);

        X_FREE(p->disc_root);
        X_FREE(*pp);
    }
}

/*
 *
 */

const char *disc_root(BD_DISC *p)
{
    return p->disc_root;
}

const char *disc_volume_id(BD_DISC *p)
{
    return p ? p->udf_volid : NULL;
}

BD_DIR_H *disc_open_bdrom_dir(BD_DISC *p, const char *rel_path)
{
    return p->pf_dir_open_bdrom(p->fs_handle, rel_path);
}

/*
 * VFS
 */

BD_FILE_H *disc_open_path(BD_DISC *p, const char *rel_path)
{
    BD_FILE_H *fp;

    if (p->avchd > 0) {
        char *avchd_path = _avchd_file_name(rel_path);
        if (avchd_path) {
            BD_DEBUG(DBG_FILE, "AVCHD: %s -> %s\n", rel_path, avchd_path);
            fp = p->pf_file_open_bdrom(p->fs_handle, avchd_path);
            X_FREE(avchd_path);
            if (fp) {
                return fp;
            }
        }
    }

    /* search file from overlay */
    fp = _overlay_open_path(p, rel_path);

    /* if not found, try BD-ROM */
    if (!fp) {
        fp = p->pf_file_open_bdrom(p->fs_handle, rel_path);

        if (!fp) {

            /* AVCHD short filenames detection */
            if (p->avchd < 0 && !strcmp(rel_path, "BDMV" DIR_SEP "index.bdmv")) {
                fp = p->pf_file_open_bdrom(p->fs_handle, "BDMV" DIR_SEP "INDEX.BDM");
                if (fp) {
                    BD_DEBUG(DBG_FILE | DBG_CRIT, "detected AVCHD 8.3 filenames\n");
                }
                p->avchd = !!fp;
            }

            if (!fp) {
                BD_DEBUG(DBG_FILE | DBG_CRIT, "error opening file %s\n", rel_path);
            }
        }
    }

    return fp;
}

BD_FILE_H *disc_open_file(BD_DISC *p, const char *dir, const char *file)
{
    BD_FILE_H *fp;
    char *path;

    path = str_printf("%s" DIR_SEP "%s", dir, file);
    if (!path) {
        return NULL;
    }

    fp = disc_open_path(p, path);
    X_FREE(path);

    return fp;
}

BD_DIR_H *disc_open_dir(BD_DISC *p, const char *dir)
{
    BD_DIR_H *dp_rom;
    BD_DIR_H *dp_ovl;

    dp_rom = p->pf_dir_open_bdrom(p->fs_handle, dir);
    dp_ovl = _overlay_open_dir(p, dir);

    if (!dp_ovl) {
        if (!dp_rom) {
            BD_DEBUG(DBG_FILE, "error opening dir %s\n", dir);
        }
        return dp_rom;
    }
    if (!dp_rom) {
        return dp_ovl;
    }

    return _combine_dirs(dp_ovl, dp_rom);
}

size_t disc_read_file(BD_DISC *disc, const char *dir, const char *file,
                       uint8_t **data)
{
    BD_FILE_H *fp;
    int64_t    size;

    *data = NULL;

    if (dir) {
        fp = disc_open_file(disc, dir, file);
    } else {
        fp = disc_open_path(disc, file);
    }
    if (!fp) {
        return 0;
    }

    size = file_size(fp);
    if (size > 0 && size < BD_MAX_SSIZE) {
        *data = malloc((size_t)size);
        if (*data) {
            int64_t got = file_read(fp, *data, size);
            if (got != size) {
                BD_DEBUG(DBG_FILE | DBG_CRIT, "Error reading file %s from %s\n", file, dir);
                X_FREE(*data);
                size = 0;
            }
        } else {
          size = 0;
        }
    }
    else {
      size = 0;
    }

    file_close(fp);
    return (size_t)size;
}

/*
 * filesystem update
 */

void disc_update(BD_DISC *p, const char *overlay_root)
{
    bd_mutex_lock(&p->ovl_mutex);

    X_FREE(p->overlay_root);
    if (overlay_root) {
        p->overlay_root = str_dup(overlay_root);
    }

    bd_mutex_unlock(&p->ovl_mutex);
}

int disc_cache_bdrom_file(BD_DISC *p, const char *rel_path, const char *cache_path)
{
    BD_FILE_H *fp_in;
    BD_FILE_H *fp_out;
    int64_t    got;
    size_t     size;

    if (!cache_path || !cache_path[0]) {
        return -1;
    }

    /* make sure cache directory exists */
    if (file_mkdirs(cache_path) < 0) {
        return -1;
    }

    /* plain directory ? */
    size = strlen(rel_path);
    if (rel_path[size - 1] == '/' || rel_path[size - 1] == '\\') {
        return 0;
    }

    /* input file from BD-ROM */
    fp_in = p->pf_file_open_bdrom(p->fs_handle, rel_path);
    if (!fp_in) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "error caching file %s (does not exist ?)\n", rel_path);
        return -1;
    }

    /* output file in local filesystem */
    fp_out = file_open(cache_path, "wb");
    if (!fp_out) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "error creating cache file %s\n", cache_path);
        file_close(fp_in);
        return -1;
    }

    do {
        uint8_t buf[16*2048];
        got = file_read(fp_in, buf, sizeof(buf));

        /* we'll call write(fp, buf, 0) after EOF. It is used to check for errors. */
        if (got < 0 || fp_out->write(fp_out, buf, got) != got) {
            BD_DEBUG(DBG_FILE | DBG_CRIT, "error caching file %s\n", rel_path);
            file_close(fp_out);
            file_close(fp_in);
            (void)file_unlink(cache_path);
            return -1;
        }
    } while (got > 0);

    BD_DEBUG(DBG_FILE, "cached %s to %s\n", rel_path, cache_path);

    file_close(fp_out);
    file_close(fp_in);
    return 0;
}

/*
 * streams
 */

BD_FILE_H *disc_open_stream(BD_DISC *disc, const char *file)
{
    BD_FILE_H *fp = disc_open_file(disc, "BDMV" DIR_SEP "STREAM", file);
    if (!fp) {
        return NULL;
    }

    if (disc->dec) {
        BD_FILE_H *st = dec_open_stream(disc->dec, fp, atoi(file));
        if (st) {
            return st;
        }
    }

    return fp;
}

const uint8_t *disc_get_data(BD_DISC *disc, int type)
{
    if (disc->dec) {
        return dec_data(disc->dec, type);
    }
    return NULL;
}

void disc_event(BD_DISC *disc, uint32_t event, uint32_t param)
{
    if (disc->dec) {
        switch (event) {
            case DISC_EVENT_START:
                dec_start(disc->dec, param);
                return;
            case DISC_EVENT_TITLE:
                dec_title(disc->dec, param);
                return;
            case DISC_EVENT_APPLICATION:
                dec_application(disc->dec, param);
                return;
        }
    }
}

/*
 * Pseudo disc ID
 * This is used when AACS disc ID is not available
 */

#define ROTL64(k, n)  (((k) << (n)) | ((k) >> (64 - (n))))

static uint64_t _fmix64(uint64_t k)
{
    k ^= k >> 33;
    k *= UINT64_C(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= UINT64_C(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;
    return k;
}

static void _murmurhash3_128(const uint8_t *in, size_t len, void *out)
{
    // original MurmurHash3 was written by Austin Appleby, and is placed in the public domain.
    // https://code.google.com/p/smhasher/wiki/MurmurHash3
    const uint64_t c1 = UINT64_C(0x87c37b91114253d5);
    const uint64_t c2 = UINT64_C(0x4cf5ad432745937f);
    uint64_t h[2] = {0, 0};
    size_t i;

    /* use only N * 16 bytes, ignore tail */
    len &= ~15;

    for (i = 0; i < len; i += 16) {
        uint64_t k1, k2;
        memcpy(&k1, in + i,     sizeof(uint64_t));
        memcpy(&k2, in + i + 8, sizeof(uint64_t));

        k1 *= c1; k1 = ROTL64(k1, 31); k1 *= c2; h[0] ^= k1;

        h[0] = ROTL64(h[0], 27); h[0] += h[1]; h[0] = h[0] * 5 + 0x52dce729;

        k2 *= c2; k2 = ROTL64(k2, 33); k2 *= c1; h[1] ^= k2;

        h[1] = ROTL64(h[1], 31); h[1] += h[0]; h[1] = h[1] * 5 + 0x38495ab5;
    }

    h[0] ^= len;
    h[1] ^= len;

    h[0] += h[1];
    h[1] += h[0];

    h[0] = _fmix64(h[0]);
    h[1] = _fmix64(h[1]);

    h[0] += h[1];
    h[1] += h[0];

    memcpy(out, h, 2*sizeof(uint64_t));
}

static int _hash_file(BD_DISC *p, const char *dir, const char *file, void *hash)
{
    uint8_t *data = NULL;
    size_t sz;

    sz = disc_read_file(p, dir, file, &data);
    if (sz > 16) {
        _murmurhash3_128(data, sz, hash);
    }

    X_FREE(data);
    return sz > 16;
}

BD_PRIVATE void disc_pseudo_id(BD_DISC *p, uint8_t *id/*[20]*/)
{
    uint8_t h[2][20];
    int i;

    memset(h, 0, sizeof(h));
    _hash_file(p, "BDMV", "MovieObject.bdmv", h[0]);
    _hash_file(p, "BDMV", "index.bdmv", h[1]);

    for (i = 0; i < 20; i++) {
        id[i] = h[0][i] ^ h[1][i];
    }
}
