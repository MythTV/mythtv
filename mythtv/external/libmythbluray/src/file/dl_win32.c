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
#include "file.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *dlerror(char *buf, int buf_size)
{
    DWORD error_code = GetLastError();
    wchar_t wbuf[256];

    if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK,
                        NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        wbuf, sizeof(wbuf)/sizeof(wbuf[0]), NULL) ||
        !WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, buf_size, NULL, NULL)) {

#ifdef _MSC_VER
        _snprintf(buf, buf_size, "error %d", (int)error_code);
#else
        snprintf(buf, buf_size, "error %d", (int)error_code);
#endif
    }

    return buf;
}

void *dl_dlopen(const char *path, const char *version)
{
    (void)version;

    wchar_t wname[MAX_PATH];
    char *name;
    void *result;
    int iresult;

    name = str_printf("%s.dll", path);
    if (!name) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "out of memory\n");
        return NULL;
    }

    iresult = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, MAX_PATH);
    X_FREE(name);

    if (!iresult) {
        BD_DEBUG(DBG_FILE, "can't convert file name '%s'\n", path);
        return NULL;
    }

    result = LoadLibraryW(wname);

    if (!result) {
        char buf[128];
        BD_DEBUG(DBG_FILE, "can't open library '%s': %s\n", path, dlerror(buf, sizeof(buf)));
    }

    return result;
}

void *dl_dlsym(void *handle, const char *symbol)
{
    void *result = (void *)GetProcAddress((HMODULE)handle, symbol);

    if (!result) {
        char buf[128];
        BD_DEBUG(DBG_FILE, "GetProcAddress(%p, '%s') failed: %s\n", handle, symbol, dlerror(buf, sizeof(buf)));
    }

    return result;
}

int dl_dlclose(void *handle)
{
    FreeLibrary((HMODULE)handle);
    return 0;
}

// path-separator
#define PATH_SEPARATOR '\\'

const char *dl_get_path(void)
{
    static char *lib_path    = NULL;
    static int   initialized = 0;

    if (!initialized) {
        initialized = 1;

        static char path[MAX_PATH];
        HMODULE hModule;
        wchar_t wpath[MAX_PATH];

        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              (LPCTSTR)&dl_get_path, &hModule)) {

            DWORD dw = GetModuleFileNameW(hModule, wpath, MAX_PATH);
            if (dw > 0 && dw < MAX_PATH) {

                if (WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, MAX_PATH, NULL, NULL)) {

                    lib_path = path;
                }
            }
        }

        if (lib_path) {
            /* cut library name from path */
            char *p = strrchr(lib_path, PATH_SEPARATOR);
            if (p) {
                *(p+1) = 0;
            }
            BD_DEBUG(DBG_FILE, "library file is %s\n", lib_path);
        } else {
            BD_DEBUG(DBG_FILE | DBG_CRIT, "Can't determine libbluray.dll install path\n");
        }
    }

    return lib_path;
}
