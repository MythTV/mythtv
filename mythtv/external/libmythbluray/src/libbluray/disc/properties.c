/*
 * This file is part of libbluray
 * Copyright (C) 2017  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "properties.h"

#include "file/file.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#ifdef _WIN32
/* mingw: PRId64 requires stdio.h ... */
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


#define MAX_PROP_FILE_SIZE   (64*1024)

/*
 * read / write file
 */

static int _read_prop_file(const char *file, char **data)
{
    BD_FILE_H *fp;
    int64_t size = -1;

    *data = NULL;

    if (file_path_exists(file) < 0) {
        BD_DEBUG(DBG_FILE, "Properties file %s does not exist\n", file);
        *data = str_dup("");
        if (!*data) {
            return -1;
        }
        return 0;
    }

    fp = file_open(file, "rb");
    if (!fp) {
        goto unlink;
    }

    size = file_size(fp);
    if (size < 1 || size > MAX_PROP_FILE_SIZE) {
        goto unlink;
    }

    *data = malloc((size_t)size + 1);
    if (!*data) {
        file_close(fp);
        return -1;
    }

    if (file_read(fp, (uint8_t*)*data, size) != (size_t)size) {
        goto unlink;
    }

    file_close(fp);
    (*data)[size] = 0;
    return 0;

 unlink:
    BD_DEBUG(DBG_FILE | DBG_CRIT, "Removing invalid properties file %s (%" PRId64 " bytes)\n", file, size);

    X_FREE(*data);
    if (fp) {
        file_close(fp);
    }
    if (file_unlink(file) < 0) {
        BD_DEBUG(DBG_FILE, "Error removing invalid properties file\n");
    }
    *data = str_dup("");
    return *data ? 0 : -1;
}

static int _write_prop_file(const char *file, const char *data)
{
    BD_FILE_H *fp;
    size_t  size;
    int64_t written;

    size = strlen(data);
    if (size > MAX_PROP_FILE_SIZE) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "Not writing too large properties file: %s is %zu bytes\n", file, size);
        return -1;
    }

    if (file_mkdirs(file) < 0) {
        return -1;
    }

    fp = file_open(file, "wb");
    if (!fp) {
        return -1;
    }

    written = fp->write(fp, (const void*)data, (int64_t)size);
    file_close(fp);

    if (written != (int64_t)size) {
        BD_DEBUG(DBG_FILE, "Writing properties file %s failed\n", file);
        if (file_unlink(file) < 0) {
            BD_DEBUG(DBG_FILE, "Error removing properties file %s\n", file);
        }
        return -1;
    }

    return 0;
}

/*
 *
 */

static char *_scan_prop(char *data, const char *key, size_t *data_size)
{
    size_t key_size = strlen(key);

    while (data) {
        if (!strncmp(data, key, key_size)) {
            data += key_size;
            char *lf = strchr(data, '\n');
            *data_size = lf ? (size_t)(lf - data) : strlen(data);
            return data;
        }

        /* next line */
        data = strchr(data, '\n');
        if (data) {
            data++;
        }
    }

    return NULL;
}

char *properties_get(const char *file, const char *property)
{
    char *key, *data;
    size_t data_size;
    char *result = NULL;

    if (strchr(property, '\n') || strchr(property, '=')) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "Ignoring invalid property '%s'\n", property);
        return NULL;
    }

    if (_read_prop_file(file, &data) < 0) {
        return NULL;
    }

    key = str_printf("%s=", property);
    if (!key) {
        X_FREE(data);
        return NULL;
    }

    result = _scan_prop(data, key, &data_size);
    if (result) {
        result[data_size] = 0;
        result = str_dup(result);
    }

    X_FREE(key);
    X_FREE(data);
    return result;
}

int properties_put(const char *file, const char *property, const char *val)
{
    char *key = NULL, *old_data = NULL, *new_data = NULL;
    char *old_val;
    size_t old_size;
    int result = -1;

    if (strchr(property, '\n') || strchr(property, '=') || strchr(val, '\n')) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "Ignoring invalid property '%s'='%s'\n", property, val);
        goto out;
    }

    if (_read_prop_file(file, &old_data) < 0) {
        goto out;
    }

    key = str_printf("%s=", property);
    if (!key) {
        goto out;
    }

    old_val = _scan_prop(old_data, key, &old_size);
    if (!old_val) {
        new_data = str_printf("%s%s%s\n", old_data, key, val);
    } else {
        *old_val = 0;
        new_data = str_printf("%s%s%s", old_data, val, old_val + old_size);
    }

    if (!new_data) {
        goto out;
    }

    result = _write_prop_file(file, new_data);

 out:
    X_FREE(old_data);
    X_FREE(new_data);
    X_FREE(key);
    return result;
}
