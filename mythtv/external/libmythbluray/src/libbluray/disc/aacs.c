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
#include <string.h>


struct bd_aacs {
    void           *h_libaacs;   /* library handle from dlopen */
    void           *aacs;        /* aacs handle from aacs_open() */

    const uint8_t *disc_id;
    uint32_t       mkbv;

    /* function pointers */
    fptr_int       decrypt_unit;
    fptr_int       decrypt_bus;

    int            impl_id;
};


static void _libaacs_close(BD_AACS *p)
{
    if (p->aacs) {
        DL_CALL(p->h_libaacs, aacs_close, p->aacs);
        p->aacs = NULL;
    }
}

static void _unload(BD_AACS *p)
{
    _libaacs_close(p);

    if (p->h_libaacs) {
        dl_dlclose(p->h_libaacs);
    }
}

void libaacs_unload(BD_AACS **p)
{
    if (p && *p) {
        _unload(*p);
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

#define IMPL_USER       0
#define IMPL_LIBAACS    1
#define IMPL_LIBMMBD    2

static void *_open_libaacs(int *impl_id)
{
    const char * const libaacs[] = {
      getenv("LIBAACS_PATH"),
      "libaacs",
      "libmmbd",
    };
    unsigned ii;

    for (ii = *impl_id; ii < sizeof(libaacs) / sizeof(libaacs[0]); ii++) {
        if (libaacs[ii]) {
            void *handle = dl_dlopen(libaacs[ii], "0");
            if (handle) {
                *impl_id = ii;
                BD_DEBUG(DBG_BLURAY, "Using %s for AACS\n", libaacs[ii]);
                return handle;
            }
        }
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No usable AACS libraries found!\n");
    return NULL;
}

static BD_AACS *_load(int impl_id)
{
    BD_AACS *p = calloc(1, sizeof(BD_AACS));
    if (!p) {
        return NULL;
    }
    p->impl_id = impl_id;

    p->h_libaacs = _open_libaacs(&p->impl_id);
    if (!p->h_libaacs) {
        X_FREE(p);
        return NULL;
    }

    BD_DEBUG(DBG_BLURAY, "Loading aacs library (%p)\n", p->h_libaacs);

    *(void **)(&p->decrypt_unit) = dl_dlsym(p->h_libaacs, "aacs_decrypt_unit");
    *(void **)(&p->decrypt_bus)  = dl_dlsym(p->h_libaacs, "aacs_decrypt_bus");

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

BD_AACS *libaacs_load(int force_mmbd)
{
    return _load(force_mmbd ? IMPL_LIBMMBD : 0);
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

        /* libmmbd needs dev: for devices */
        if (!p->aacs && p->impl_id == IMPL_LIBMMBD && !strncmp(device, "/dev/", 5)) {
            char *tmp_device = str_printf("dev:%s", device);
            if (tmp_device) {
                p->aacs = open2(tmp_device, keyfile_path, &error_code);
                X_FREE(tmp_device);
            }
        }
    } else if (open) {
        BD_DEBUG(DBG_BLURAY, "Using old aacs_open(), no verbose error reporting available\n");
        p->aacs = open(device, keyfile_path);
    } else {
        BD_DEBUG(DBG_BLURAY, "aacs_open() not found\n");
    }

    if (error_code) {
        /* failed. try next aacs implementation if available. */
        BD_AACS *p2 = _load(p->impl_id + 1);
        if (p2) {
            if (!libaacs_open(p2, device, file_open_handle, file_open_fp, keyfile_path)) {
                /* succeed - swap implementations */
                _unload(p);
                *p = *p2;
                X_FREE(p2);
                return 0;
            }
            /* failed - report original errors */
            libaacs_unload(&p2);
        }
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

int libaacs_decrypt_bus(BD_AACS *p, uint8_t *buf)
{
    if (p && p->aacs && p->decrypt_bus) {
        if (p->decrypt_bus(p->aacs, buf) > 0) {
            return 0;
        }
    }

    BD_DEBUG(DBG_AACS | DBG_CRIT, "Unable to BUS decrypt unit (AACS)!\n");
    return -1;
}

/*
 *
 */

uint32_t libaacs_get_mkbv(BD_AACS *p)
{
    return p ? p->mkbv : 0;
}

int libaacs_get_bec_enabled(BD_AACS *p)
{
    fptr_int get_bec;

    if (!p || !p->h_libaacs) {
        return 0;
    }

    *(void **)(&get_bec) = dl_dlsym(p->h_libaacs, "aacs_get_bus_encryption");
    if (!get_bec) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "aacs_get_bus_encryption() dlsym failed!\n");
        return 0;
    }

    return get_bec(p->aacs) == 3;
}

static const uint8_t *_get_data(BD_AACS *p, const char *func)
{
    fptr_p_void fp;

    *(void **)(&fp) = dl_dlsym(p->h_libaacs, func);
    if (!fp) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "%s() dlsym failed!\n", func);
        return NULL;
    }

    return (const uint8_t*)fp(p->aacs);
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
    case BD_AACS_CONTENT_CERT_ID:    return "CONTENT_CERT_ID";
    case BD_AACS_BDJ_ROOT_CERT_HASH: return "BDJ_ROOT_CERT_HASH";
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
            return _get_data(p, "aacs_get_vid");

        case BD_AACS_MEDIA_PMSN:
            return _get_data(p, "aacs_get_pmsn");

        case BD_AACS_DEVICE_BINDING_ID:
            return _get_data(p, "aacs_get_device_binding_id");

        case BD_AACS_DEVICE_NONCE:
            return _get_data(p, "aacs_get_device_nonce");

        case BD_AACS_MEDIA_KEY:
            return _get_data(p, "aacs_get_mk");

        case BD_AACS_CONTENT_CERT_ID:
            return _get_data(p, "aacs_get_content_cert_id");

        case BD_AACS_BDJ_ROOT_CERT_HASH:
            return _get_data(p, "aacs_get_bdj_root_cert_hash");
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "get_aacs_data(): unknown query %d\n", type);
    return NULL;
}
