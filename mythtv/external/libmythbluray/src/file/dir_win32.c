/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  John Stebbins
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

#include "file.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"

#include <stdlib.h>
#include <string.h>
#if defined(HAVE_DIRENT_H)
#   include <dirent.h>
#endif

#include <io.h>
#include <windows.h>


typedef struct {
    intptr_t            handle;
    struct _wfinddata_t info;
} dir_data_t;

static void _dir_close_win32(BD_DIR_H *dir)
{
    if (dir) {
        dir_data_t *priv = (dir_data_t*)dir->internal;

        _findclose(priv->handle);

        BD_DEBUG(DBG_DIR, "Closed WIN32 dir (%p)\n", (void*)dir);

        X_FREE(dir->internal);
        X_FREE(dir);
    }
}

static int _dir_read_win32(BD_DIR_H *dir, BD_DIRENT *entry)
{
    dir_data_t *priv = (dir_data_t*)dir->internal;

    if (!priv->info.name[0]) {
        return 1;
    }
    if (!WideCharToMultiByte(CP_UTF8, 0, priv->info.name, -1, entry->d_name, sizeof(entry->d_name), NULL, NULL)) {
        return -1;
    }

    priv->info.name[0] = 0;
    _wfindnext(priv->handle, &priv->info);

    return 0;
}

static dir_data_t *_open_impl(const char *dirname)
{
    dir_data_t *priv;
    char *filespec;
    wchar_t wfilespec[MAX_PATH];
    int result;

    filespec = str_printf("%s" DIR_SEP "*", dirname);
    if (!filespec) {
        return NULL;
    }

    result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filespec, -1, wfilespec, MAX_PATH);
    X_FREE(filespec);
    if (!result) {
        return NULL;
    }

    priv = calloc(1, sizeof(dir_data_t));
    if (!priv) {
        return NULL;
    }

    priv->handle = _wfindfirst(wfilespec, &priv->info);
    if (priv->handle == -1) {
        X_FREE(priv);
    }

    return priv;
}

static BD_DIR_H *_dir_open_win32(const char* dirname)
{
    BD_DIR_H *dir = calloc(1, sizeof(BD_DIR_H));
    dir_data_t *priv = NULL;

    BD_DEBUG(DBG_DIR, "Opening WIN32 dir %s... (%p)\n", dirname, (void*)dir);

    if (!dir) {
        return NULL;
    }

    priv = _open_impl(dirname);
    if (priv) {
        dir->close = _dir_close_win32;
        dir->read = _dir_read_win32;
        dir->internal = priv;

        return dir;
    }

    BD_DEBUG(DBG_DIR, "Error opening dir %s\n", dirname);
    X_FREE(dir);

    return NULL;
}

BD_DIR_H* (*dir_open)(const char* dirname) = _dir_open_win32;

BD_DIR_OPEN dir_open_default(void)
{
    return _dir_open_win32;
}
