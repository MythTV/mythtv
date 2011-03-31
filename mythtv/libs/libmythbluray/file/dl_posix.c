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

#if defined(_WIN32)
#   include <windows.h>
#elif defined(HAVE_DLFCN_H)
#   include <dlfcn.h>
#elif defined(HAVE_SYS_DL_H)
#   include <sys/dl.h>
#endif

#if defined(_WIN32)
static const char *dlerror(char *buf, int buf_size)
{
    DWORD error_code = GetLastError();
    wchar_t wbuf[256];
  
    if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                       FORMAT_MESSAGE_MAX_WIDTH_MASK,
                       NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       wbuf, sizeof(wbuf)/sizeof(wbuf[0]), NULL)) {
        WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, buf_size, NULL, NULL);
    } else {
        snprintf(buf, buf_size, "error %d", (int)error_code);
    }
    
    return buf;
}
#endif

void   *dl_dlopen  ( const char* path, const char *version )
{
    char *name;
    void *result;

#if defined(__APPLE__)
    static const char ext[] = ".dylib";
    version = NULL;
#elif defined(_WIN32)
    static const char ext[] = ".dll";
    version = NULL;
#else
    static const char ext[] = ".so";
#endif

    if (version) {
        name = str_printf("%s%s.%s", path, ext, version);
    } else {
        name = str_printf("%s%s", path, ext);
    }

    BD_DEBUG(DBG_FILE, "searching for library '%s' ...\n", name);

#if defined(_WIN32)
    wchar_t wname[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, MAX_PATH);
    result = LoadLibraryW(wname);

    if (!result) {
        char buf[128];
        BD_DEBUG(DBG_FILE, "can't open library '%s': %s\n", name, dlerror(buf, sizeof(buf)));
    }
#else
    result = dlopen(name, RTLD_LAZY);

    if (!result) {
        BD_DEBUG(DBG_FILE, "can't open library '%s': %s\n", name, dlerror());
    }
#endif

    X_FREE(name);

    return result;
}

void   *dl_dlsym   ( void* handle, const char* symbol )
{
#if defined(_WIN32)
    void *result = (void *)GetProcAddress(handle, symbol);

    if (!result) {
        char buf[128];
        BD_DEBUG(DBG_FILE | DBG_CRIT, "GetProcAddress(%p, '%s') failed: %s\n", handle, symbol, dlerror(buf, sizeof(buf)));
    }
#else
    void *result = dlsym(handle, symbol);

    if (!result) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "dlsym(%p, '%s') failed: %s\n", handle, symbol, dlerror());
    }
#endif

    return result;
}

int     dl_dlclose ( void* handle )
{
#if defined(_WIN32)
    FreeLibrary(handle);
    return 0;
#else
    return dlclose(handle);
#endif
}
