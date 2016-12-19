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

#if defined(__MINGW32__)
/* ftello64() and fseeko64() prototypes from stdio.h */
#   undef __STRICT_ANSI__
#endif

#include "file.h"
#include "util/macro.h"
#include "util/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <windows.h>

static void _file_close(BD_FILE_H *file)
{
    if (file) {
        if (fclose((FILE *)file->internal)) {
            BD_DEBUG(DBG_FILE | DBG_CRIT, "Error closing WIN32 file (%p)\n", (void*)file);
        }

        BD_DEBUG(DBG_FILE, "Closed WIN32 file (%p)\n", (void*)file);

        X_FREE(file);
    }
}

static int64_t _file_seek(BD_FILE_H *file, int64_t offset, int32_t origin)
{
#if defined(__MINGW32__)
    return fseeko64((FILE *)file->internal, offset, origin);
#else
    return _fseeki64((FILE *)file->internal, offset, origin);
#endif
}

static int64_t _file_tell(BD_FILE_H *file)
{
#if defined(__MINGW32__)
    return ftello64((FILE *)file->internal);
#else
    return _ftelli64((FILE *)file->internal);
#endif
}

#if 0
static int _file_eof(BD_FILE_H *file)
{
    return feof((FILE *)file->internal);
}
#endif

static int64_t _file_read(BD_FILE_H *file, uint8_t *buf, int64_t size)
{
    if (size > 0 && size < BD_MAX_SSIZE) {
        return (int64_t)fread(buf, 1, (size_t)size, (FILE *)file->internal);
    }

    BD_DEBUG(DBG_FILE | DBG_CRIT, "Ignoring invalid read of size %"PRId64" (%p)\n", size, (void*)file);
    return 0;
}

static int64_t _file_write(BD_FILE_H *file, const uint8_t *buf, int64_t size)
{
    if (size > 0 && size < BD_MAX_SSIZE) {
        return (int64_t)fwrite(buf, 1, (size_t)size, (FILE *)file->internal);
    }

    if (size == 0) {
        if (fflush((FILE *)file->internal)) {
            BD_DEBUG(DBG_FILE, "fflush() failed (%p)\n", (void*)file);
            return -1;
        }
        return 0;
    }

    BD_DEBUG(DBG_FILE | DBG_CRIT, "Ignoring invalid write of size %"PRId64" (%p)\n", size, (void*)file);
    return 0;
}

static BD_FILE_H *_file_open(const char* filename, const char *mode)
{
    BD_FILE_H *file;
    FILE *fp;
    wchar_t wfilename[MAX_PATH], wmode[8];

    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename, -1, wfilename, MAX_PATH) ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wmode, 8)) {

        BD_DEBUG(DBG_FILE, "Error opening file %s\n", filename);
        return NULL;
    }

    fp = _wfopen(wfilename, wmode);
    if (!fp) {
        BD_DEBUG(DBG_FILE, "Error opening file %s\n", filename);
        return NULL;
    }

    file = calloc(1, sizeof(BD_FILE_H));
    if (!file) {
        BD_DEBUG(DBG_FILE | DBG_CRIT, "Error opening file %s (out of memory)\n", filename);
        fclose(fp);
        return NULL;
    }

    file->internal = fp;
    file->close    = _file_close;
    file->seek     = _file_seek;
    file->read     = _file_read;
    file->write    = _file_write;
    file->tell     = _file_tell;
    //file->eof      = _file_eof;

    BD_DEBUG(DBG_FILE, "Opened WIN32 file %s (%p)\n", filename, (void*)file);
    return file;
}

BD_FILE_H* (*file_open)(const char* filename, const char *mode) = _file_open;

BD_FILE_OPEN file_open_default(void)
{
    return _file_open;
}

int file_unlink(const char *file)
{
    wchar_t wfile[MAX_PATH];

    if (!MultiByteToWideChar(CP_UTF8, 0, file, -1, wfile, MAX_PATH)) {
        return -1;
    }

    return _wremove(wfile);
}

int file_path_exists(const char *path)
{
    wchar_t wpath[MAX_PATH];

    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH)) {
        return -1;
    }

    DWORD dwAttrib = GetFileAttributesW(wpath);
    if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return -1;
}

int file_mkdir(const char *dir)
{
    wchar_t wdir[MAX_PATH];

    if (!MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH)) {
        return -1;
    }
    if (!CreateDirectoryW(wdir, NULL))
        return -1;
    return 0;
}
