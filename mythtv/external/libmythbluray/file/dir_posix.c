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

static void _dir_close_posix(BD_DIR_H *dir)
{
    if (dir) {
        closedir((DIR *)dir->internal);

        BD_DEBUG(DBG_DIR, "Closed POSIX dir (%p)\n", (void*)dir);

        X_FREE(dir);
    }
}

static int _dir_read_posix(BD_DIR_H *dir, BD_DIRENT *entry)
{
    struct dirent e, *p_e;
    int result;

    result = readdir_r((DIR*)dir->internal, &e, &p_e);
    if (result) {
        return -result;
    } else if (p_e == NULL) {
        return 1;
    }
    strncpy(entry->d_name, e.d_name, sizeof(entry->d_name));
    entry->d_name[sizeof(entry->d_name) - 1] = 0;

    return 0;
}

static BD_DIR_H *_dir_open_posix(const char* dirname)
{
    BD_DIR_H *dir = calloc(1, sizeof(BD_DIR_H));

    BD_DEBUG(DBG_DIR, "Opening POSIX dir %s... (%p)\n", dirname, (void*)dir);
    if (!dir) {
        return NULL;
    }

    dir->close = _dir_close_posix;
    dir->read = _dir_read_posix;

    if ((dir->internal = opendir(dirname))) {
        return dir;
    }

    BD_DEBUG(DBG_DIR, "Error opening dir! (%p)\n", (void*)dir);

    X_FREE(dir);

    return NULL;
}

BD_DIR_H* (*dir_open)(const char* dirname) = _dir_open_posix;

BD_DIR_OPEN dir_open_default(void)
{
    return _dir_open_posix;
}
