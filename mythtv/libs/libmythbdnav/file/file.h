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

#include <util/attributes.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//#ifdef __LINUX__
#define file_open file_open_linux
#define DIR_SEP "/"
//#endif

#define file_close(X) X->close(X)
#define file_seek(X,Y,Z) X->seek(X,Y,Z)
#define file_tell(X) X->tell(X)
#define file_eof(X) X->eof(X)
#define file_read(X,Y,Z) X->read(X,Y,Z)
#define file_write(X,Y,Z) X->write(X,Y,Z)

typedef struct file FILE_H;
struct file
{
    void* internal;
    void (*close)(FILE_H *file);
    int64_t (*seek)(FILE_H *file, int64_t offset, int32_t origin);
    int64_t (*tell)(FILE_H *file);
    int (*eof)(FILE_H *file);
    int (*read)(FILE_H *file, uint8_t *buf, int64_t size);
    int (*write)(FILE_H *file, const uint8_t *buf, int64_t size);
};

BD_PRIVATE FILE_H *file_open_linux(const char* filename, const char *mode);

#ifdef __cplusplus
};
#endif

#endif /* FILE_H_ */
