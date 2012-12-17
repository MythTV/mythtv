/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
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

#if !defined(_PES_BUFFER_H_)
#define _PES_BUFFER_H_

#include <util/attributes.h>

#include <stdint.h>


typedef struct pes_buffer_s PES_BUFFER;
struct pes_buffer_s {
    uint8_t  *buf;
    uint32_t  len;  // payload length
    unsigned  size; // allocated size

    int64_t   pts;
    int64_t   dts;

    struct pes_buffer_s *next;
};


BD_PRIVATE PES_BUFFER *pes_buffer_alloc(int size) BD_ATTR_MALLOC;
BD_PRIVATE void        pes_buffer_free(PES_BUFFER **); // free list of buffers

BD_PRIVATE void        pes_buffer_append(PES_BUFFER **head, PES_BUFFER *buf); // append buf to list
BD_PRIVATE void        pes_buffer_remove(PES_BUFFER **head, PES_BUFFER *buf); // remove buf from list and free it

#endif // _PES_BUFFER_H_
