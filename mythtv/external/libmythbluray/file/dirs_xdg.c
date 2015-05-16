/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dirs.h"

#include "util/strutl.h"
#include "util/logging.h"

#include <stdlib.h>
#include <string.h>

/*
 * Based on XDG Base Directory Specification
 * http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */

#define USER_CFG_DIR   ".config"
#define USER_CACHE_DIR ".cache"
#define USER_DATA_DIR  ".local/share"
#define SYSTEM_CFG_DIR "/etc/xdg"


char *file_get_config_home(void)
{
    const char *xdg_home = getenv("XDG_CONFIG_HOME");
    if (xdg_home && *xdg_home) {
        return str_dup(xdg_home);
    }

    const char *user_home = getenv("HOME");
    if (user_home && *user_home) {
        return str_printf("%s/%s", user_home, USER_CFG_DIR);
    }

    BD_DEBUG(DBG_FILE, "Can't find user home directory ($HOME) !\n");
    return  NULL;
}

char *file_get_data_home(void)
{
    const char *xdg_home = getenv("XDG_DATA_HOME");
    if (xdg_home && *xdg_home) {
        return str_dup(xdg_home);
    }

    const char *user_home = getenv("HOME");
    if (user_home && *user_home) {
        return str_printf("%s/%s", user_home, USER_DATA_DIR);
    }

    BD_DEBUG(DBG_FILE, "Can't find user home directory ($HOME) !\n");
    return NULL;
}

char *file_get_cache_home(void)
{
    const char *xdg_cache = getenv("XDG_CACHE_HOME");
    if (xdg_cache && *xdg_cache) {
        return str_dup(xdg_cache);
    }

    const char *user_home = getenv("HOME");
    if (user_home && *user_home) {
        return str_printf("%s/%s", user_home, USER_CACHE_DIR);
    }

    BD_DEBUG(DBG_FILE, "Can't find user home directory ($HOME) !\n");
    return NULL;
}

const char *file_get_config_system(const char *dir)
{
    static char *dirs = NULL; // "dir1\0dir2\0...\0dirN\0\0"

    if (!dirs) {
        const char *xdg_sys = getenv("XDG_CONFIG_DIRS");

        if (xdg_sys && *xdg_sys) {

            dirs = calloc(1, strlen(xdg_sys) + 2);
            if (!dirs) {
                return NULL;
            }
            strcpy(dirs, xdg_sys);

            char *pt = dirs;
            while (NULL != (pt = strchr(pt, ':'))) {
                *pt++ = 0;
            }

        } else {
            dirs = str_printf("%s%c%c", SYSTEM_CFG_DIR, 0, 0);
        }
    }

    if (!dir) {
        // first call
        dir = dirs;
    } else {
        // next call
        dir += strlen(dir) + 1;
        if (!*dir) {
            // end of list
            dir = NULL;
        }
    }

    return dir;
}
