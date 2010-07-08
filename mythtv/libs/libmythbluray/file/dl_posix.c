/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
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

#include "dl.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"

#include <stdlib.h>
#ifdef USING_MINGW
#include "compat.h"
#else
#include <dlfcn.h>
#endif
#include <string.h>

void   *dl_dlopen  ( const char* path, const char *version )
{
    char *name;
    void *result;

#if defined(__APPLE__)
    static const char ext[] = ".dylib";
    version = NULL;
#else
    static const char ext[] = ".so";
#endif

    if (version) {
        name = str_printf("%s%s.%s", path, ext, version);
    } else {
        name = str_printf("%s%s", path, ext);
    }

    DEBUG(DBG_BDPLUS, "searching for library '%s' ...\n", name);

    result = dlopen(name, RTLD_LAZY);
    if (!result) {
        DEBUG(DBG_FILE, "can't open library '%s': %s\n", name, dlerror());
    }

    X_FREE(name);

    return result;
}

void   *dl_dlsym   ( void* handle, const char* symbol )
{
    void *result = dlsym(handle, symbol);

    if (!result) {
        DEBUG(DBG_FILE | DBG_CRIT, "dlsym(%p, '%s') failed: %s\n", handle, symbol, dlerror());
    }

    return result;
}

int     dl_dlclose ( void* handle )
{
    return dlclose(handle);
}
