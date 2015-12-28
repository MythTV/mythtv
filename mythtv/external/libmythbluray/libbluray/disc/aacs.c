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

#include "aacs.h"

#include "file/dl.h"
#include "file/file.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#include <stdlib.h>


struct bd_aacs {
    void           *h_libaacs;   /* library handle from dlopen */
    void           *aacs;        /* aacs handle from aacs_open() */

    const uint8_t *disc_id;
    uint32_t       mkbv;

    /* function pointers */
    fptr_int       decrypt_unit;

    fptr_p_void    get_vid;
    fptr_p_void    get_pmsn;
    fptr_p_void    get_device_binding_id;
    fptr_p_void    get_device_nonce;
    fptr_p_void    get_media_key;
};


static void _libaacs_close(BD_AACS *p)
{
    if (p->aacs) {
        DL_CALL(p->h_libaacs, aacs_close, p->aacs);
        p->aacs = NULL;
    }
}

void libaacs_unload(BD_AACS **p)
{
    if (p && *p) {
        _libaacs_close(*p);

        if ((*p)->h_libaacs) {
            dl_dlclose((*p)->h_libaacs);
        }

        X_FREE(*p);
    }
}

int libaacs_required(void *have_file_handle, int (*have_file)(void *, const char *, const char *))
{
    if (have_file(have_file_handle, "AACS", "Unit_Key_RO.inf")) {
        BD_DEBUG(DBG_BLURAY, "AACS" DIR_SEP "Unit_Key_RO.inf found. Disc seems to be AACS protected.\n");
        return 1;
    }

    BD_DEBUG(DBG_BLURAY, "AACS" DIR_SEP "Unit_Key_RO.inf not found. No AACS protection.\n");
    return 0;
}

static void *_open_libaacs(void)
{
    const char * const libaacs[] = {
      getenv("LIBAACS_PATH"),
      "libaacs",
      "libmmbd",
    };
    unsigned ii;

    for (ii = 0; ii < sizeof(libaacs) / sizeof(libaacs[0]); ii++) {
        if (libaacs[ii]) {
            void *handle = dl_dlopen(libaacs[ii], "0");
            if (handle) {
                BD_DEBUG(DBG_BLURAY, "Using %s for AACS\n", libaacs[ii]);
                return handle;
            }
        }
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No usable AACS libraries found!\n");
    return NULL;
}

BD_AACS *libaacs_load(void)
{
    BD_AACS *p = calloc(1, sizeof(BD_AACS));
    if (!p) {
        return NULL;
    }

    p->h_libaacs = _open_libaacs();
    if (!p->h_libaacs) {
        X_FREE(p);
        return NULL;
    }

    BD_DEBUG(DBG_BLURAY, "Loading aacs library (%p)\n", p->h_libaacs);

    *(void **)(&p->decrypt_unit) = dl_dlsym(p->h_libaacs, "aacs_decrypt_unit");
    *(void **)(&p->get_vid)      = dl_dlsym(p->h_libaacs, "aacs_get_vid");
    *(void **)(&p->get_pmsn)     = dl_dlsym(p->h_libaacs, "aacs_get_pmsn");
    *(void **)(&p->get_device_binding_id) = dl_dlsym(p->h_libaacs, "aacs_get_device_binding_id");
    *(void **)(&p->get_device_nonce)      = dl_dlsym(p->h_libaacs, "aacs_get_device_nonce");
    *(void **)(&p->get_media_key)         = dl_dlsym(p->h_libaacs, "aacs_get_mk");

    if (!p->decrypt_unit) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "libaacs dlsym failed! (%p)\n", p->h_libaacs);
        libaacs_unload(&p);
        return NULL;
    }

    BD_DEBUG(DBG_BLURAY, "Loaded libaacs (%p)\n", p->h_libaacs);

    if (file_open != file_open_default()) {
        BD_DEBUG(DBG_BLURAY, "Registering libaacs filesystem handler %p (%p)\n", (void *)(intptr_t)file_open, p->h_libaacs);
        DL_CALL(p->h_libaacs, aacs_register_file, file_open);
    }

    return p;
}

int libaacs_open(BD_AACS *p, const char *device,
                   void *file_open_handle, void *file_open_fp,
                   const char *keyfile_path)

{
    int error_code = 0;

    fptr_p_void open;
    fptr_p_void open2;
    fptr_p_void init;
    fptr_int    open_device;
    fptr_int    aacs_get_mkb_version;
    fptr_p_void aacs_get_disc_id;

    _libaacs_close(p);

    *(void **)(&open)  = dl_dlsym(p->h_libaacs, "aacs_open");
    *(void **)(&open2) = dl_dlsym(p->h_libaacs, "aacs_open2");
    *(void **)(&init)  = dl_dlsym(p->h_libaacs, "aacs_init");
    *(void **)(&aacs_get_mkb_version) = dl_dlsym(p->h_libaacs, "aacs_get_mkb_version");
    *(void **)(&aacs_get_disc_id)     = dl_dlsym(p->h_libaacs, "aacs_get_disc_id");
    *(void **)(&open_device)          = dl_dlsym(p->h_libaacs, "aacs_open_device");

    if (init && open_device) {
        p->aacs = init();
        DL_CALL(p->h_libaacs, aacs_set_fopen, p->aacs, file_open_handle, file_open_fp);
        error_code = open_device(p->aacs, device, keyfile_path);
    } else if (open2) {
        BD_DEBUG(DBG_BLURAY, "Using old aacs_open2(), no UDF support available\n");
        p->aacs = open2(device, keyfile_path, &error_code);
    } else if (open) {
        BD_DEBUG(DBG_BLURAY, "Using old aacs_open(), no verbose error reporting available\n");
        p->aacs = open(device, keyfile_path);
    } else {
        BD_DEBUG(DBG_BLURAY, "aacs_open() not found\n");
    }

    if (p->aacs) {
        if (aacs_get_mkb_version) {
            p->mkbv = aacs_get_mkb_version(p->aacs);
        }
        if (aacs_get_disc_id) {
            p->disc_id = (const uint8_t *)aacs_get_disc_id(p->aacs);
        }
        return error_code;
    }

    return error_code ? error_code : 1;
}

/*
 *
 */

void libaacs_select_title(BD_AACS *p, uint32_t title)
{
    if (p && p->aacs) {
        DL_CALL(p->h_libaacs, aacs_select_title, p->aacs, title);
    }
}

int libaacs_decrypt_unit(BD_AACS *p, uint8_t *buf)
{
    if (p && p->aacs) {
        if (!p->decrypt_unit(p->aacs, buf)) {
            BD_DEBUG(DBG_AACS | DBG_CRIT, "Unable decrypt unit (AACS)!\n");

            return -1;
        } // decrypt
    } // aacs

    return 0;
}

/*
 *
 */

static const uint8_t *_get_vid(BD_AACS *p)
{
    if (!p->get_vid) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "aacs_get_vid() dlsym failed!\n");
        return NULL;
    }

    return (const uint8_t*)p->get_vid(p->aacs);
}

static const uint8_t *_get_pmsn(BD_AACS *p)
{
    if (!p->get_pmsn) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "aacs_get_pmsn() dlsym failed!\n");
        return NULL;
    }

    return (const uint8_t*)p->get_pmsn(p->aacs);
}

static const uint8_t *_get_device_binding_id(BD_AACS *p)
{
    if (!p->get_device_binding_id) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "aacs_get_device_binding_id() dlsym failed!\n");
        return NULL;
    }

    return (const uint8_t*)p->get_device_binding_id(p->aacs);
}

static const uint8_t *_get_device_nonce(BD_AACS *p)
{
    if (!p->get_device_nonce) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "aacs_get_device_nonce() dlsym failed!\n");
        return NULL;
    }

    return (const uint8_t*)p->get_device_nonce(p->aacs);
}

static const uint8_t *_get_media_key(BD_AACS *p)
{
    if (!p->get_media_key) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "aacs_get_mk() dlsym failed!\n");
        return NULL;
    }

    return (const uint8_t*)p->get_media_key(p->aacs);
}

uint32_t libaacs_get_mkbv(BD_AACS *p)
{
    return p ? p->mkbv : 0;
}

static const char *_type2str(int type)
{
    switch (type) {
    case BD_AACS_DISC_ID:            return "DISC_ID";
    case BD_AACS_MEDIA_VID:          return "MEDIA_VID";
    case BD_AACS_MEDIA_PMSN:         return "MEDIA_PMSN";
    case BD_AACS_DEVICE_BINDING_ID:  return "DEVICE_BINDING_ID";
    case BD_AACS_DEVICE_NONCE:       return "DEVICE_NONCE";
    case BD_AACS_MEDIA_KEY:          return "MEDIA_KEY";
    default: return "???";
    }
}

BD_PRIVATE const uint8_t *libaacs_get_aacs_data(BD_AACS *p, int type)
{
    if (!p || !p->aacs) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "get_aacs_data(%s): libaacs not initialized!\n", _type2str(type));
        return NULL;
    }

    switch (type) {
        case BD_AACS_DISC_ID:
            return p->disc_id;

        case BD_AACS_MEDIA_VID:
            return _get_vid(p);

        case BD_AACS_MEDIA_PMSN:
            return _get_pmsn(p);

        case BD_AACS_DEVICE_BINDING_ID:
            return _get_device_binding_id(p);

        case BD_AACS_DEVICE_NONCE:
            return _get_device_nonce(p);

        case BD_AACS_MEDIA_KEY:
            return _get_media_key(p);
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "get_aacs_data(): unknown query %d\n", type);
    return NULL;
}
