/*
 * This file is part of libbluray
 * Copyright (C) 2012   Konstantin Pavlov
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

#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/strutl.h"
#include "util/logging.h"

#define USER_CFG_DIR   "Library/Preferences"
#define USER_CACHE_DIR "Library/Caches"
#define USER_DATA_DIR  "Library"
#define SYSTEM_CFG_DIR "/Library/Preferences"


char *file_get_config_home(void)
{
    const char *user_home = getenv("HOME");
    if (user_home && *user_home) {
        return str_printf("%s/%s", user_home, USER_CFG_DIR);
    }

    BD_DEBUG(DBG_FILE, "Can't find user home directory ($HOME) !\n");
    return NULL;
}

char *file_get_data_home(void)
{
    const char *user_home = getenv("HOME");
    if (user_home && *user_home) {
        return str_printf("%s/%s", user_home, USER_DATA_DIR);
    }

    BD_DEBUG(DBG_FILE, "Can't find user home directory ($HOME) !\n");
    return NULL;
}

char *file_get_cache_home(void)
{
    const char *user_home = getenv("HOME");
    if (user_home && *user_home) {
        return str_printf("%s/%s", user_home, USER_CACHE_DIR);
    }

    BD_DEBUG(DBG_FILE, "Can't find user home directory ($HOME) !\n");
    return NULL;
}

const char *file_get_config_system(const char *dir)
{
    if (!dir) {
        // first call
        return SYSTEM_CFG_DIR;
    }

    return NULL;
}
