/*****************************************************************************
 * bits.h : Bit handling helpers
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 *****************************************************************************/

#ifndef BD_BITS_H
#define BD_BITS_H

#include "file/file.h"

#include <unistd.h>
#include <stdio.h>

/**
 * \file
 * This file defines functions, structures for handling streams of bits
 */

#define BF_BUF_SIZE   (1024*32)

typedef struct {
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    ssize_t  i_left;    /* i_count number of available bits */
} BITBUFFER;

typedef struct {
    BD_FILE_H *fp;
    uint8_t    buf[BF_BUF_SIZE];
    BITBUFFER  bb;
    off_t      pos;
    off_t      end;
    size_t     size;
} BITSTREAM;

BD_PRIVATE void bb_init( BITBUFFER *bb, uint8_t *p_data, size_t i_data );
BD_PRIVATE void bs_init( BITSTREAM *bs, BD_FILE_H *fp );
BD_PRIVATE void bb_seek( BITBUFFER *bb, off_t off, int whence);
BD_PRIVATE void bs_seek( BITSTREAM *bs, off_t off, int whence);
BD_PRIVATE void bb_skip( BITBUFFER *bb, ssize_t i_count );
BD_PRIVATE void bs_skip( BITSTREAM *bs, ssize_t i_count );
BD_PRIVATE uint32_t bb_read( BITBUFFER *bb, int i_count );
BD_PRIVATE uint32_t bs_read( BITSTREAM *bs, int i_count );

static inline off_t bb_pos( const BITBUFFER *bb )
{
    return 8 * ( bb->p - bb->p_start ) + 8 - bb->i_left;
}

static inline off_t bs_pos( const BITSTREAM *bs )
{
    return bs->pos * 8 + bb_pos(&bs->bb);
}

static inline off_t bs_end( const BITSTREAM *bs )
{
    return bs->end * 8;
}

static inline int bb_eof( const BITBUFFER *bb )
{
    return bb->p >= bb->p_end ? 1: 0 ;
}

static inline int bs_eof( const BITSTREAM *bs )
{
    return file_eof(bs->fp) && bb_eof(&bs->bb);
}

static inline void bb_seek_byte( BITBUFFER *bb, off_t off)
{
    bb_seek(bb, off << 3, SEEK_SET);
}

static inline void bs_seek_byte( BITSTREAM *s, off_t off)
{
    bs_seek(s, off << 3, SEEK_SET);
}

static inline void bb_read_bytes( BITBUFFER *bb, uint8_t *buf, int i_count )
{
    int ii;

    for (ii = 0; ii < i_count; ii++) {
        buf[ii] = bb_read(bb, 8);
    }
}

static inline void bs_read_bytes( BITSTREAM *s, uint8_t *buf, int i_count )
{
    int ii;

    for (ii = 0; ii < i_count; ii++) {
        buf[ii] = bs_read(s, 8);
    }
}

static inline uint32_t bb_show( BITBUFFER *bb, int i_count )
{
    BITBUFFER     bb_tmp = *bb;
    return bb_read( &bb_tmp, i_count );
}

static inline int bb_is_align( BITBUFFER *bb, uint32_t mask )
{
    off_t off = bb_pos(bb);

    return !(off & mask);
}

static inline int bs_is_align( BITSTREAM *s, uint32_t mask )
{
    off_t off = bs_pos(s);

    return !(off & mask);
}

#endif
