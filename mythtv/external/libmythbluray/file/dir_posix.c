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
#include "file_mythiowrapper.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"

#include <stdlib.h>
#include <string.h>
#if defined(HAVE_DIRENT_H)
#   include <dirent.h>
#endif
#if defined(_WIN32)
#   include <io.h>
#   include <windows.h>
#endif

#if defined(_WIN32)
typedef struct {
    long                handle;
    struct _wfinddata_t info;
} dir_data_t;
#endif

static void dir_close_posix(BD_DIR_H *dir)
{
    if (dir) {
#if defined(_WIN32)
        dir_data_t *priv = (dir_data_t*)dir->internal;
        _findclose(priv->handle);
        X_FREE(dir->internal);
#else
        closedir((DIR *)dir->internal);
#endif

        BD_DEBUG(DBG_DIR, "Closed POSIX dir (%p)\n", dir);

        X_FREE(dir);
    }
}

static int dir_read_posix(BD_DIR_H *dir, BD_DIRENT *entry)
{
#if defined(_WIN32)
    dir_data_t *priv = (dir_data_t*)dir->internal;

    if (!priv->info.name[0]) {
        return 1;
    }
    WideCharToMultiByte(CP_UTF8, 0, priv->info.name, -1, entry->d_name, sizeof(entry->d_name), NULL, NULL);

    priv->info.name[0] = 0;
    _wfindnext(priv->handle, &priv->info);

#else
    struct dirent e, *p_e;
    int result;

    result = readdir_r((DIR*)dir->internal, &e, &p_e);
    if (result) {
        return -result;
    } else if (p_e == NULL) {
        return 1;
    }
    strncpy(entry->d_name, e.d_name, 256);
#endif

    return 0;
}

static BD_DIR_H *dir_open_posix(const char* dirname)
{
    if(strncmp(dirname, "myth://", 7) == 0)
        return dir_open_mythiowrapper(dirname);		
    
    BD_DIR_H *dir = malloc(sizeof(BD_DIR_H));

    BD_DEBUG(DBG_DIR, "Opening POSIX dir %s... (%p)\n", dirname, dir);
    dir->close = dir_close_posix;
    dir->read = dir_read_posix;

#if defined(_WIN32)
    char       *filespec = str_printf("%s/*", dirname);
    dir_data_t *priv     = calloc(1, sizeof(dir_data_t));

    dir->internal = priv;

    wchar_t wfilespec[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filespec, -1, wfilespec, MAX_PATH))
        priv->handle = _wfindfirst(wfilespec, &priv->info);
    else
        priv->handle = -1;

    X_FREE(filespec);

    if (priv->handle != -1) {
        return dir;
    }

    X_FREE(dir->internal);

#else
    DIR *dp = NULL;

    if ((dp = opendir(dirname))) {
        dir->internal = dp;

        return dir;
    }
#endif

    BD_DEBUG(DBG_DIR, "Error opening dir! (%p)\n", dir);

    X_FREE(dir);

    return NULL;
}

BD_DIR_H* (*dir_open)(const char* dirname) = dir_open_posix;
