/*
 * This file is part of libbluray
 * Copyright (C) 2013-2015  VideoLAN
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

#include "bdplus.h"

#include "file/dl.h"
#include "file/file.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#include <stdlib.h>
#include <string.h>


struct bd_bdplus {
    void           *h_libbdplus; /* library handle from dlopen */

    void           *bdplus;      /* bdplus handle from bdplus_open() */

    /* functions */
    fptr_int32     event;
    fptr_p_void    m2ts;
    fptr_int32     m2ts_close;
    fptr_int32     seek;
    fptr_int32     fixup;

    /* old API */
    fptr_p_void    title;

    int impl_id;
};


static void _libbdplus_close(BD_BDPLUS *p)
{
    if (p->bdplus) {
        DL_CALL(p->h_libbdplus, bdplus_free, p->bdplus);
        p->bdplus = NULL;
    }
}

static void _unload(BD_BDPLUS *p)
{
    _libbdplus_close(p);

    if (p->h_libbdplus) {
        dl_dlclose(p->h_libbdplus);
    }
}

void libbdplus_unload(BD_BDPLUS **p)
{
    if (p && *p) {
        _unload(*p);
        X_FREE(*p);
    }
}

int libbdplus_required(void *have_file_handle, int (*have_file)(void *, const char *, const char *))
{
    if (have_file(have_file_handle, "BDSVM", "00000.svm")) {
        BD_DEBUG(DBG_BLURAY, "BDSVM" DIR_SEP "00000.svm found. Disc seems to be BD+ protected.\n");
        return 1;
    }

    BD_DEBUG(DBG_BLURAY, "BDSVM" DIR_SEP "00000.svm not found. No BD+ protection.\n");
    return 0;
}

#define IMPL_USER       0
#define IMPL_LIBBDPLUS  1
#define IMPL_LIBMMBD    2

static void *_libbdplus_open(int *impl_id)
{
    const char * const libbdplus[] = {
      getenv("LIBBDPLUS_PATH"),
      "libbdplus",
      "libmmbd",
    };
    unsigned ii;

    for (ii = *impl_id; ii < sizeof(libbdplus) / sizeof(libbdplus[0]); ii++) {
        if (libbdplus[ii]) {
            void *handle = dl_dlopen(libbdplus[ii], "0");
            if (handle) {
                *impl_id = ii;
                BD_DEBUG(DBG_BLURAY, "Using %s for BD+\n", libbdplus[ii]);
                return handle;
            }
        }
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No usable BD+ libraries found!\n");
    return NULL;
}

int libbdplus_is_mmbd(BD_BDPLUS *p)
{
    return p && (p->impl_id == IMPL_LIBMMBD);
}

static BD_BDPLUS *_load(int impl_id)
{
    BD_BDPLUS *p = calloc(1, sizeof(BD_BDPLUS));
    if (!p) {
        return NULL;
    }
    p->impl_id = impl_id;

    BD_DEBUG(DBG_BDPLUS, "attempting to load libbdplus\n");

    p->h_libbdplus = _libbdplus_open(&p->impl_id);
    if (!p->h_libbdplus) {
        X_FREE(p);
        return NULL;
    }

    BD_DEBUG(DBG_BLURAY, "Loading libbdplus (%p)\n", p->h_libbdplus);

    *(void **)(&p->event)      = dl_dlsym(p->h_libbdplus, "bdplus_event");
    *(void **)(&p->m2ts)       = dl_dlsym(p->h_libbdplus, "bdplus_m2ts");
    *(void **)(&p->seek)       = dl_dlsym(p->h_libbdplus, "bdplus_seek");
    *(void **)(&p->fixup)      = dl_dlsym(p->h_libbdplus, "bdplus_fixup");
    *(void **)(&p->m2ts_close) = dl_dlsym(p->h_libbdplus, "bdplus_m2ts_close");
    if (!p->m2ts) {
        /* Old API */
        *(void **)(&p->title)  = dl_dlsym(p->h_libbdplus, "bdplus_set_title");
        if (!p->title) {
            *(void **)(&p->title)  = dl_dlsym(p->h_libbdplus, "bdplus_set_m2ts");
        }
    }

    if (!p->seek || !p->fixup || !((p->m2ts && p->m2ts_close) || p->title)) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "libbdplus dlsym failed! (%p)\n", p->h_libbdplus);
        libbdplus_unload(&p);
        return NULL;
    }

    BD_DEBUG(DBG_BLURAY, "Loaded libbdplus (%p)\n", p->h_libbdplus);
    return p;
}

BD_BDPLUS *libbdplus_load()
{
    return _load(0);
}

int libbdplus_init(BD_BDPLUS *p, const char *root, const char *device,
                   void *file_open_handle, void *file_open_fp,
                   const uint8_t *vid, const uint8_t *mk)
{
    fptr_p_void    bdplus_init;
    fptr_void      set_fopen;

    _libbdplus_close(p);

    /* force libmmbd BD+ if no AACS media key:
     * - libbdplus requires media key
     * - libmmbd does not export media key
     *   (=> libbdplus won't work with libmmbd AACS)
     */
    if (mk == NULL && p->impl_id == IMPL_LIBBDPLUS) {
        BD_BDPLUS *p2 = _load(IMPL_LIBMMBD);
        if (p2) {
            if (!libbdplus_init(p2, root, device, file_open_handle, file_open_fp, vid, mk)) {
                /* succeed - swap implementations */
                _unload(p);
                *p = *p2;
                X_FREE(p2);
                return 0;
            }
            /* failed - continue with original bd+ implementation */
            libbdplus_unload(&p2);
        }
    }

    /* */

    *(void **)(&bdplus_init) = dl_dlsym(p->h_libbdplus, "bdplus_init");
    *(void **)(&set_fopen)   = dl_dlsym(p->h_libbdplus, "bdplus_set_fopen");

    if (!bdplus_init) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "libbdplus dlsym(bdplus_init) failed! (%p)\n", p->h_libbdplus);
        return -1;
    }

    if (set_fopen) {
        /* New libbdplus. Use libbluray for file I/O */
        p->bdplus = bdplus_init(NULL, NULL, vid);
        set_fopen(p->bdplus, file_open_handle, file_open_fp);
    } else if (root) {
        /* Old libbdplus or libmmbd. Disc is mounted. */
        p->bdplus = bdplus_init(root, NULL, vid);
    } else if (device) {
        /* Unmounted device */
        if (p->impl_id == IMPL_LIBMMBD && !strncmp(device, "/dev/", 5)) {
            char *tmp = str_printf("dev:%s", device);
            if (tmp) {
                p->bdplus = bdplus_init(tmp, NULL, vid);
                X_FREE(tmp);
            }
        } else {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Too old libbdplus detected. Disc must be mounted first.\n");
        }
    }

    if (!p->bdplus) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bdplus_init() failed! (%p)\n", p->h_libbdplus);
        return -1;
    }

    DL_CALL(p->h_libbdplus, bdplus_set_mk, p->bdplus, mk);

    return 0;
}

static uint32_t _bdplus_get(BD_BDPLUS *p, const char *func)
{
    if (p && p->bdplus) {
        fptr_int32 fp;
        *(void **)(&fp) = dl_dlsym(p->h_libbdplus, func);
        if (fp) {
            return fp(p->bdplus);
        }
    }
    return 0;
}

int libbdplus_get_gen(BD_BDPLUS *p)
{
    return _bdplus_get(p, "bdplus_get_code_gen");
}

int libbdplus_get_date(BD_BDPLUS *p)
{
    return _bdplus_get(p, "bdplus_get_code_date");
}

void libbdplus_event(BD_BDPLUS *p, uint32_t event, uint32_t param1, uint32_t param2)
{
    if (p && p->bdplus && p->event) {
        p->event(p->bdplus, event, param1, param2);
    }
}

void libbdplus_mmap(BD_BDPLUS *p, uint32_t region_id, void *mem)
{
    if (p && p->bdplus) {
        DL_CALL(p->h_libbdplus, bdplus_mmap, p->bdplus, region_id, mem);
    }
}

void libbdplus_psr(BD_BDPLUS *p, void *regs, void *read, void *write)
{
    if (p && p->bdplus) {
        DL_CALL(p->h_libbdplus, bdplus_psr, p->bdplus, regs, read, write);
    }
}

void libbdplus_start(BD_BDPLUS *p)
{
    if (p && p->bdplus) {
        DL_CALL(p->h_libbdplus, bdplus_start, p->bdplus);
    }
}

/*
 *  stream layer
 */

struct bd_bdplus_st {
    BD_BDPLUS *lib;
    void      *st;
};

BD_BDPLUS_ST *libbdplus_m2ts(BD_BDPLUS *p, uint32_t clip_id, uint64_t pos)
{
    if (p && p->bdplus) {

        if (!p->m2ts) {
            /* use old API */
            BD_BDPLUS_ST *ret = calloc(1, sizeof(BD_BDPLUS_ST));
            if (ret) {
                ret->lib = p;
                ret->st  = NULL;
                p->title(p->bdplus, clip_id);
                p->seek(p->bdplus, pos);
            }
            return ret;
        }

        void *st = p->m2ts(p->bdplus, clip_id);

        if (!st) {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "BD+ failed for clip %05d.m2ts\n", clip_id);

        } else if (p->seek(st, pos) < 0) {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "BD+ seek failed for clip %05d.m2ts\n", clip_id);
            p->m2ts_close(st);
        } else {
            BD_BDPLUS_ST *ret = calloc(1, sizeof(BD_BDPLUS_ST));
            if (ret) {
                ret->lib = p;
                ret->st  = st;
                BD_DEBUG(DBG_BLURAY | DBG_CRIT, "BD+ active for clip %05d.m2ts\n", clip_id);
            }
            return ret;
        }
    }

    return NULL;
}

int libbdplus_m2ts_close(BD_BDPLUS_ST **p)
{
    int result = -1;
    if (p && *p) {
        if ((*p)->lib && (*p)->st) {
            result = (*p)->lib->m2ts_close((*p)->st);
        }
        X_FREE(*p);
    }

    return result;
}

int libbdplus_seek(BD_BDPLUS_ST *p, uint64_t pos)
{
    if (p) {
        if (p->st) {
            return p->lib->seek(p->st, pos);
        } else {
            /* use old API */
            return p->lib->seek(p->lib->bdplus, pos);
        }
    }

    return -1;
}

int libbdplus_fixup(BD_BDPLUS_ST *p, uint8_t *buf, int len)
{
    if (p && !p->lib->m2ts) {
        /* use old API */
        return p->lib->fixup(p->lib->bdplus, len, buf);
    }

    if (p && p->st) {
        int32_t numFixes;
        numFixes = p->lib->fixup(p->st, len, buf);
#if 1
        if (numFixes) {
            BD_DEBUG(DBG_BDPLUS, "BD+ did %d fixups\n", numFixes);
        }
#endif
        return numFixes;
    }

    return -1;
}
