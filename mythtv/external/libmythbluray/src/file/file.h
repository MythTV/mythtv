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

#ifndef FILE_H_
#define FILE_H_

#include "filesystem.h"

#include "util/attributes.h"

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
# define DIR_SEP "\\"
# define DIR_SEP_CHAR '\\'
#else
# define DIR_SEP "/"
# define DIR_SEP_CHAR '/'
#endif

/*
 * file access
 */

//#define file_eof(X) X->eof(X)
//#define file_write(X,Y,Z) (size_t)X->write(X,Y,Z)

static inline void file_close(BD_FILE_H *fp)
{
    fp->close(fp);
}

static inline int64_t file_tell(BD_FILE_H *fp)
{
    return fp->tell(fp);
}

static inline BD_USED int64_t file_seek(BD_FILE_H *fp, int64_t offset, int32_t origin)
{
    return fp->seek(fp, offset, origin);
}

static inline BD_USED size_t file_read(BD_FILE_H *fp, uint8_t *buf, size_t size)
{
    return (size_t)fp->read(fp, buf, (int64_t)size);
}

BD_PRIVATE int64_t file_size(BD_FILE_H *fp);

BD_PRIVATE extern BD_FILE_H* (*file_open)(const char* filename, const char *mode);

BD_PRIVATE BD_FILE_OPEN file_open_default(void);


/*
 * directory access
 */

#define dir_close(X) X->close(X)
#define dir_read(X,Y) X->read(X,Y)

BD_PRIVATE extern BD_DIR_H* (*dir_open)(const char* dirname);

BD_PRIVATE BD_DIR_OPEN dir_open_default(void);

/*
 * local filesystem
 */

BD_PRIVATE int file_unlink(const char *file);
BD_PRIVATE int file_path_exists(const char *path);
BD_PRIVATE int file_mkdir(const char *dir);
BD_PRIVATE int file_mkdirs(const char *path);

#endif /* FILE_H_ */
