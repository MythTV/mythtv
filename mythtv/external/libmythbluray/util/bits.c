/*****************************************************************************
 * bits.c : Bit handling helpers
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

#include "bits.h"

#include <stdio.h>

#include "mythiowrapper.h"

/**
 * \file
 * This file defines functions, structures for handling streams of bits
 */

void bb_init( BITBUFFER *bb, uint8_t *p_data, size_t i_data )
{
    bb->p_start = p_data;
    bb->p       = bb->p_start;
    bb->p_end   = bb->p_start + i_data;
    bb->i_left  = 8;
}

void bs_init( BITSTREAM *bs, BD_FILE_H *fp )
{
    struct stat buf;

    bs->fp = fp;
    bs->pos = 0;
// Original libbluray code
//    file_seek(bs->fp, 0, SEEK_END);
//    bs->end = file_tell(bs->fp);
// Instead use 'stat' so we don't flush our readahead buffer
// Optimize here for now until we can optimize in the RingBuffer itself
    if (file_stat(bs->fp, &buf) == 0)
        bs->end = buf.st_size;
    else
        bs->end = 0;

    file_seek(bs->fp, 0, SEEK_SET);
    bs->size = file_read(bs->fp, bs->buf, BF_BUF_SIZE);
    bb_init(&bs->bb, bs->buf, bs->size);
}

void bb_seek( BITBUFFER *bb, off_t off, int whence)
{
    off_t b;

    switch (whence) {
        case SEEK_CUR:
            off = (bb->p - bb->p_start) * 8 + off;
            break;
        case SEEK_END:
            off = (bb->p_end - bb->p_start) * 8 - off;
            break;
        case SEEK_SET:
        default:
            break;
    }
    b = off >> 3;
    bb->p = &bb->p_start[b];

    ssize_t i_tmp = bb->i_left - (off & 0x07);
    if (i_tmp <= 0) {
        bb->i_left = 8 + i_tmp;
        bb->p++;
    } else {
        bb->i_left = i_tmp;
    }
}

void bs_seek( BITSTREAM *bs, off_t off, int whence)
{
    off_t b;

    switch (whence) {
        case SEEK_CUR:
            off = bs->pos * 8 + (bs->bb.p - bs->bb.p_start) * 8 + off;
            break;
        case SEEK_END:
            off = bs->end * 8 - off;
            break;
        case SEEK_SET:
        default:
            break;
    }
    b = off >> 3;
    if (b >= bs->end)
    {
        if (BF_BUF_SIZE < bs->end) {
            bs->pos = bs->end - BF_BUF_SIZE;
            file_seek(bs->fp, BF_BUF_SIZE, SEEK_END);
        } else {
            bs->pos = 0;
            file_seek(bs->fp, 0, SEEK_SET);
        }
        bs->size = file_read(bs->fp, bs->buf, BF_BUF_SIZE);
        bb_init(&bs->bb, bs->buf, bs->size);
        bs->bb.p = bs->bb.p_end;
    } else if (b < bs->pos || b >= (bs->pos + BF_BUF_SIZE)) {
        file_seek(bs->fp, b, SEEK_SET);
        bs->pos = b;
        bs->size = file_read(bs->fp, bs->buf, BF_BUF_SIZE);
        bb_init(&bs->bb, bs->buf, bs->size);
    } else {
        b -= bs->pos;
        bs->bb.p = &bs->bb.p_start[b];
        bs->bb.i_left = 8 - (off & 0x07);
    }
}

uint32_t bb_read( BITBUFFER *bb, int i_count )
{
    static const uint32_t i_mask[33] = {
        0x00,
        0x01,      0x03,      0x07,      0x0f,
        0x1f,      0x3f,      0x7f,      0xff,
        0x1ff,     0x3ff,     0x7ff,     0xfff,
        0x1fff,    0x3fff,    0x7fff,    0xffff,
        0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
        0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
        0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
        0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff
    };
    int      i_shr;
    uint32_t i_result = 0;

    while( i_count > 0 ) {

        if( bb->p >= bb->p_end ) {
            break;
        }

        i_shr = bb->i_left - i_count;
        if( i_shr >= 0 ) {
            /* more in the buffer than requested */
            i_result |= ( *bb->p >> i_shr )&i_mask[i_count];
            bb->i_left -= i_count;
            if( bb->i_left == 0 ) {
                bb->p++;
                bb->i_left = 8;
            }
            return( i_result );
        } else {
            /* less in the buffer than requested */
           i_result |= (*bb->p&i_mask[bb->i_left]) << -i_shr;
           i_count  -= bb->i_left;
           bb->p++;
           bb->i_left = 8;
        }
    }

    return( i_result );
}

uint32_t bs_read( BITSTREAM *bs, int i_count )
{
    ssize_t left;
    int bytes = (i_count + 7) >> 3;

    if (bs->bb.p + bytes >= bs->bb.p_end) {
        bs->pos = bs->pos + (bs->bb.p - bs->bb.p_start);
        left = bs->bb.i_left;
        file_seek(bs->fp, bs->pos, SEEK_SET);
        bs->size = file_read(bs->fp, bs->buf, BF_BUF_SIZE);
        bb_init(&bs->bb, bs->buf, bs->size);
        bs->bb.i_left = left;
    }
    return bb_read(&bs->bb, i_count);
}

void bb_skip( BITBUFFER *bb, ssize_t i_count )
{
    bb->i_left -= i_count;

    if( bb->i_left <= 0 ) {
        const int i_bytes = ( -bb->i_left + 8 ) / 8;

        bb->p += i_bytes;
        bb->i_left += 8 * i_bytes;
    }
}

void bs_skip( BITSTREAM *bs, ssize_t i_count )
{
    int left;
    int bytes = i_count >> 3;

    if (bs->bb.p + bytes >= bs->bb.p_end) {
        bs->pos = bs->pos + (bs->bb.p - bs->bb.p_start);
        left = bs->bb.i_left;
        file_seek(bs->fp, bs->pos, SEEK_SET);
        bs->size = file_read(bs->fp, bs->buf, BF_BUF_SIZE);
        bb_init(&bs->bb, bs->buf, bs->size);
        bs->bb.i_left = left;
    }
    bb_skip(&bs->bb, i_count);
}
