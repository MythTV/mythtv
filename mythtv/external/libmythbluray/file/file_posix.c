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
#include "file_mythiowrapper.h"
#include "util/macro.h"
#include "util/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#define	ftello	_ftelli64
#define	fseeko	_fseeki64
#endif	//	#ifdef WIN32

static void file_close_linux(BD_FILE_H *file)
{
    if (file) {
        fclose((FILE *)file->internal);

        BD_DEBUG(DBG_FILE, "Closed LINUX file (%p)\n", file);

        X_FREE(file);
    }
}

static int64_t file_seek_linux(BD_FILE_H *file, int64_t offset, int32_t origin)
{
#if defined(__MINGW32__)
    return fseeko64((FILE *)file->internal, offset, origin);
#else
    return fseeko((FILE *)file->internal, offset, origin);
#endif
}

static int64_t file_tell_linux(BD_FILE_H *file)
{
#if defined(__MINGW32__)
    return ftello64((FILE *)file->internal);
#else
    return ftello((FILE *)file->internal);
#endif
}

static int file_stat_linux(BD_FILE_H *file, struct stat *buf)
{
    return fstat(fileno((FILE *)file->internal), buf);
}

static int file_eof_linux(BD_FILE_H *file)
{
    return feof((FILE *)file->internal);
}

static int64_t file_read_linux(BD_FILE_H *file, uint8_t *buf, int64_t size)
{
    return fread(buf, 1, size, (FILE *)file->internal);
}

static int64_t file_write_linux(BD_FILE_H *file, const uint8_t *buf, int64_t size)
{
    return fwrite(buf, 1, size, (FILE *)file->internal);
}

static BD_FILE_H *file_open_linux(const char* filename, const char *mode)
{
    if (strncmp(filename, "myth://", 7) == 0)
        return file_open_mythiowrapper(filename, mode);

    FILE *fp = NULL;
    BD_FILE_H *file = malloc(sizeof(BD_FILE_H));

    BD_DEBUG(DBG_FILE, "Opening LINUX file %s... (%p)\n", filename, file);
    file->close = file_close_linux;
    file->seek = file_seek_linux;
    file->read = file_read_linux;
    file->write = file_write_linux;
    file->tell = file_tell_linux;
    file->eof = file_eof_linux;
    file->stat = file_stat_linux;

#ifdef WIN32
    wchar_t wfilename[MAX_PATH], wmode[8];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename, -1, wfilename, MAX_PATH) &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wmode, 8) &&
        (fp = _wfopen(wfilename, wmode))) {
#else
    if ((fp = fopen(filename, mode))) {
#endif
        file->internal = fp;

        return file;
    }

    BD_DEBUG(DBG_FILE, "Error opening file! (%p)\n", file);

    X_FREE(file);

    return NULL;
}

BD_FILE_H* (*file_open)(const char* filename, const char *mode) = file_open_linux;

BD_FILE_OPEN file_open_default(void)
{
    return file_open_linux;
}
