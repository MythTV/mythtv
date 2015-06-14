/*
 * This file is part of libbluray
 * Copyright (C) 2014  Petri Hintukainen
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

#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#include <stdio.h>  // SEEK_*
#include <string.h> // strchr


int64_t file_size(BD_FILE_H *fp)
{
    int64_t pos    = file_tell(fp);
    int64_t res1   = file_seek(fp, 0, SEEK_END);
    int64_t length = file_tell(fp);
    int64_t res2   = file_seek(fp, pos, SEEK_SET);

    if (res1 < 0 || res2 < 0 || pos < 0 || length < 0) {
        return -1;
    }

    return length;
}

int file_mkdirs(const char *path)
{
    int result = 0;
    char *dir = str_dup(path);
    char *end = dir;
    char *p;

    if (!dir) {
        return -1;
    }

    /* strip file name */
    if (!(end = strrchr(end, DIR_SEP_CHAR))) {
        X_FREE(dir);
        return -1;
    }
    *end = 0;

    /* tokenize, stop to first existing dir */
    while ((p = strrchr(dir, DIR_SEP_CHAR))) {
        if (!file_path_exists(dir)) {
            break;
        }
        *p = 0;
    }

    /* create missing dirs */
    p = dir;
    while (p < end) {

        /* concatenate next non-existing dir */
        while (*p) p++;
        if (p >= end) break;
        *p = DIR_SEP_CHAR;

        result = file_mkdir(dir);
        if (result < 0) {
            BD_DEBUG(DBG_FILE | DBG_CRIT, "Error creating directory %s\n", dir);
            break;
        }
        BD_DEBUG(DBG_FILE, "  created directory %s\n", dir);
    }

    X_FREE(dir);
    return result;
}
