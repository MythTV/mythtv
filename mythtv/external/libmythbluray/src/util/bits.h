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

#include "util/attributes.h"
#include "file/filesystem.h" // BD_FILE_H

#include <stdint.h>
#include <stddef.h>    // size_t


/**
 * \file
 * This file defines functions, structures for handling streams of bits
 */

#define BF_BUF_SIZE   (1024*32)

typedef struct {
    const uint8_t *p_start;
    const uint8_t *p;
    const uint8_t *p_end;

    int            i_left;    /* i_count number of available bits */
} BITBUFFER;

typedef struct {
    BD_FILE_H *fp;
    uint8_t    buf[BF_BUF_SIZE];
    BITBUFFER  bb;
    int64_t    pos;   /* file offset of buffer start (buf[0]) */
    int64_t    end;   /* size of file */
    size_t     size;  /* bytes in buf */
} BITSTREAM;

BD_PRIVATE void bb_init( BITBUFFER *bb, const uint8_t *p_data, size_t i_data );
BD_PRIVATE int  bs_init( BITSTREAM *bs, BD_FILE_H *fp ) BD_USED;
BD_PRIVATE void bb_seek( BITBUFFER *bb, int64_t off, int whence);
//BD_PRIVATE void bs_seek( BITSTREAM *bs, int64_t off, int whence);
//BD_PRIVATE void bb_seek_byte( BITBUFFER *bb, int64_t off);
BD_PRIVATE int  bs_seek_byte( BITSTREAM *s, int64_t off) BD_USED;
BD_PRIVATE void bb_skip( BITBUFFER *bb, size_t i_count );
BD_PRIVATE void bs_skip( BITSTREAM *bs, size_t i_count );  /* note: i_count must be less than BF_BUF_SIZE */
BD_PRIVATE uint32_t bb_read( BITBUFFER *bb, int i_count );
BD_PRIVATE uint32_t bs_read( BITSTREAM *bs, int i_count );

static inline int64_t bb_pos( const BITBUFFER *bb )
{
    return 8 * ( bb->p - bb->p_start ) + 8 - bb->i_left;
}

static inline int64_t bs_pos( const BITSTREAM *bs )
{
    return bs->pos * 8 + bb_pos(&bs->bb);
}

static inline int64_t bs_end( const BITSTREAM *bs )
{
    return bs->end * 8;
}

static inline int bb_eof( const BITBUFFER *bb )
{
    return bb->p >= bb->p_end ? 1: 0 ;
}
/*
static inline int bs_eof( const BITSTREAM *bs )
{
    return file_eof(bs->fp) && bb_eof(&bs->bb);
}
*/

static inline int64_t bs_avail( const BITSTREAM *bs )
{
    return bs_end(bs) - bs_pos(bs);
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
static inline void bs_read_string( BITSTREAM *s, char *buf, int i_count )
{
    bs_read_bytes(s, (uint8_t*)buf, i_count);
    buf[i_count] = '\0';
}

static inline uint32_t bb_show( BITBUFFER *bb, int i_count )
{
    BITBUFFER     bb_tmp = *bb;
    return bb_read( &bb_tmp, i_count );
}

static inline int bb_is_align( BITBUFFER *bb, uint32_t mask )
{
    int64_t off = bb_pos(bb);

    return !(off & mask);
}

static inline int bs_is_align( BITSTREAM *s, uint32_t mask )
{
    int64_t off = bs_pos(s);

    return !(off & mask);
}

#endif
