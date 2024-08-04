/*
 * decode.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "mpeg2config.h"

#include <string.h>	/* memcmp/memset, try to remove */
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

static int mpeg2_accels = 0;

static size_t BUFFER_SIZE = 1194 * 1024;

const mpeg2_info_t * mpeg2_info (mpeg2dec_t * mpeg2dec)
{
    return &(mpeg2dec->info);
}

static inline int skip_chunk (mpeg2dec_t * mpeg2dec, int bytes)
{
    if (!bytes)
	return 0;

    uint8_t *current = mpeg2dec->buf_start;
    uint32_t shift = mpeg2dec->shift;
    uint8_t *limit = current + bytes;

    do {
	uint8_t byte = *current++;
	if (shift == 0x00000100) {
	    mpeg2dec->shift = 0xffffff00;
	    int skipped = current - mpeg2dec->buf_start;
	    mpeg2dec->buf_start = current;
	    return skipped;
	}
	shift = (shift | byte) << 8;
    } while (current < limit);

    mpeg2dec->shift = shift;
    mpeg2dec->buf_start = current;
    return 0;
}

static inline int copy_chunk (mpeg2dec_t * mpeg2dec, int bytes)
{
    if (!bytes)
	return 0;

    uint8_t  *current = mpeg2dec->buf_start;
    uint32_t  shift = mpeg2dec->shift;
    uint8_t  *chunk_ptr = mpeg2dec->chunk_ptr;
    uint8_t  *limit = current + bytes;

    do {
	uint8_t byte = *current++;
	if (shift == 0x00000100) {
	    mpeg2dec->shift = 0xffffff00;
	    mpeg2dec->chunk_ptr = chunk_ptr + 1;
	    int copied = current - mpeg2dec->buf_start;
	    mpeg2dec->buf_start = current;
	    return copied;
	}
	shift = (shift | byte) << 8;
	*chunk_ptr++ = byte;
    } while (current < limit);

    mpeg2dec->shift = shift;
    mpeg2dec->buf_start = current;
    return 0;
}

void mpeg2_buffer (mpeg2dec_t * mpeg2dec, uint8_t * start, uint8_t * end)
{
    mpeg2dec->buf_start = start;
    mpeg2dec->buf_end = end;
}

int mpeg2_getpos (mpeg2dec_t * mpeg2dec)
{
    return mpeg2dec->buf_end - mpeg2dec->buf_start;
}

static inline mpeg2_state_t seek_chunk (mpeg2dec_t * mpeg2dec)
{
    int size = mpeg2dec->buf_end - mpeg2dec->buf_start;
    int skipped = skip_chunk (mpeg2dec, size);
    if (!skipped) {
	mpeg2dec->bytes_since_tag += size;
	return STATE_BUFFER;
    }
    mpeg2dec->bytes_since_tag += skipped;
    mpeg2dec->code = mpeg2dec->buf_start[-1];
    return STATE_INTERNAL_NORETURN;
}

mpeg2_state_t mpeg2_seek_header (mpeg2dec_t * mpeg2dec)
{
    while (!(mpeg2dec->code == 0xb3 ||
	     ((mpeg2dec->code == 0xb7 || mpeg2dec->code == 0xb8 ||
	       !mpeg2dec->code) && mpeg2dec->sequence.width != (unsigned)-1)))
	if (seek_chunk (mpeg2dec) == STATE_BUFFER)
	    return STATE_BUFFER;
    mpeg2dec->chunk_start = mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
    mpeg2dec->user_data_len = 0;
    return ((mpeg2dec->code == 0xb7) ?
	    mpeg2_header_end (mpeg2dec) : mpeg2_parse_header (mpeg2dec));
}

#define RECEIVED(code,state) (((state) << 8) + (code))

mpeg2_state_t mpeg2_parse (mpeg2dec_t * mpeg2dec)
{
    if (mpeg2dec->action) {
	mpeg2_state_t state = mpeg2dec->action (mpeg2dec);
	if ((int)state > (int)STATE_INTERNAL_NORETURN)
	    return state;
    }

    while (1) {
	while ((unsigned) (mpeg2dec->code - mpeg2dec->first_decode_slice) <
	       mpeg2dec->nb_decode_slices) {
	    int size_buffer = mpeg2dec->buf_end - mpeg2dec->buf_start;
	    int size_chunk = (mpeg2dec->chunk_buffer + BUFFER_SIZE -
                              mpeg2dec->chunk_ptr);
	    int copied = 0;
	    if (size_buffer <= size_chunk) {
		copied = copy_chunk (mpeg2dec, size_buffer);
		if (!copied) {
		    mpeg2dec->bytes_since_tag += size_buffer;
		    mpeg2dec->chunk_ptr += size_buffer;
		    return STATE_BUFFER;
		}
	    } else {
		copied = copy_chunk (mpeg2dec, size_chunk);
		if (!copied) {
		    /* filled the chunk buffer without finding a start code */
		    mpeg2dec->bytes_since_tag += size_chunk;
		    mpeg2dec->action = seek_chunk;
		    return STATE_INVALID;
		}
	    }
	    mpeg2dec->bytes_since_tag += copied;

	    mpeg2_slice (&(mpeg2dec->decoder), mpeg2dec->code,
			 mpeg2dec->chunk_start);
	    mpeg2dec->code = mpeg2dec->buf_start[-1];
	    mpeg2dec->chunk_ptr = mpeg2dec->chunk_start;
	}
	if ((unsigned) (mpeg2dec->code - 1) >= 0xb0 - 1)
	    break;
	if (seek_chunk (mpeg2dec) == STATE_BUFFER)
	    return STATE_BUFFER;
    }

    mpeg2dec->action = mpeg2_seek_header;
    switch (mpeg2dec->code) {
    case 0x00:
	return mpeg2dec->state;
    case 0xb3:
    case 0xb7:
    case 0xb8:
	return (mpeg2dec->state == STATE_SLICE) ? STATE_SLICE : STATE_INVALID;
    default:
	mpeg2dec->action = seek_chunk;
	return STATE_INVALID;
    }
}

mpeg2_state_t mpeg2_parse_header (mpeg2dec_t * mpeg2dec)
{
    static int (* s_processHeader[]) (mpeg2dec_t * mpeg2dec) = {
	mpeg2_header_picture, mpeg2_header_extension, mpeg2_header_user_data,
	mpeg2_header_sequence, NULL, NULL, NULL, NULL, mpeg2_header_gop
    };

    mpeg2dec->action = mpeg2_parse_header;
    mpeg2dec->info.user_data = NULL;	mpeg2dec->info.user_data_len = 0;
    while (1) {
	int size_buffer = mpeg2dec->buf_end - mpeg2dec->buf_start;
	int size_chunk = (mpeg2dec->chunk_buffer + BUFFER_SIZE -
		      mpeg2dec->chunk_ptr);
	int copied = 0;
	if (size_buffer <= size_chunk) {
	    copied = copy_chunk (mpeg2dec, size_buffer);
	    if (!copied) {
		mpeg2dec->bytes_since_tag += size_buffer;
		mpeg2dec->chunk_ptr += size_buffer;
		return STATE_BUFFER;
	    }
	} else {
	    copied = copy_chunk (mpeg2dec, size_chunk);
	    if (!copied) {
		/* filled the chunk buffer without finding a start code */
		mpeg2dec->bytes_since_tag += size_chunk;
		mpeg2dec->code = 0xb4;
		mpeg2dec->action = mpeg2_seek_header;
		return STATE_INVALID;
	    }
	}
	mpeg2dec->bytes_since_tag += copied;

	if (s_processHeader[mpeg2dec->code & 0x0b] (mpeg2dec)) {
	    mpeg2dec->code = mpeg2dec->buf_start[-1];
	    mpeg2dec->action = mpeg2_seek_header;
	    return STATE_INVALID;
	}

	mpeg2dec->code = mpeg2dec->buf_start[-1];
	switch (RECEIVED (mpeg2dec->code, mpeg2dec->state)) {

	/* state transition after a sequence header */
	case RECEIVED (0x00, STATE_SEQUENCE):
	case RECEIVED (0xb8, STATE_SEQUENCE):
	    mpeg2_header_sequence_finalize (mpeg2dec);
	    break;

	/* other legal state transitions */
	case RECEIVED (0x00, STATE_GOP):
	    mpeg2_header_gop_finalize (mpeg2dec);
	    break;
	case RECEIVED (0x01, STATE_PICTURE):
	case RECEIVED (0x01, STATE_PICTURE_2ND):
	    mpeg2_header_picture_finalize (mpeg2dec, mpeg2_accels);
	    mpeg2dec->action = mpeg2_header_slice_start;
	    break;

	/* legal headers within a given state */
	case RECEIVED (0xb2, STATE_SEQUENCE):
	case RECEIVED (0xb2, STATE_GOP):
	case RECEIVED (0xb2, STATE_PICTURE):
	case RECEIVED (0xb2, STATE_PICTURE_2ND):
	case RECEIVED (0xb5, STATE_SEQUENCE):
	case RECEIVED (0xb5, STATE_PICTURE):
	case RECEIVED (0xb5, STATE_PICTURE_2ND):
	    mpeg2dec->chunk_ptr = mpeg2dec->chunk_start;
	    continue;

	default:
	    mpeg2dec->action = mpeg2_seek_header;
	    return STATE_INVALID;
	}

	mpeg2dec->chunk_start = mpeg2dec->chunk_ptr = mpeg2dec->chunk_buffer;
	mpeg2dec->user_data_len = 0;
	return mpeg2dec->state;
    }
}

int mpeg2_convert (mpeg2dec_t * mpeg2dec, mpeg2_convert_t convert, void * arg)
{
    mpeg2_convert_init_t convert_init;
    int error = convert (MPEG2_CONVERT_SET, NULL, &(mpeg2dec->sequence), 0,
                         mpeg2_accels, arg, &convert_init);
    if (!error) {
	mpeg2dec->convert = convert;
	mpeg2dec->convert_arg = arg;
	mpeg2dec->convert_id_size = convert_init.id_size;
	mpeg2dec->convert_stride = 0;
    }
    return error;
}

int mpeg2_stride (mpeg2dec_t * mpeg2dec, int stride)
{
    if (!mpeg2dec->convert) {
	if (stride < (int) mpeg2dec->sequence.width)
	    stride = mpeg2dec->sequence.width;
	mpeg2dec->decoder.stride_frame = stride;
    } else {
	mpeg2_convert_init_t convert_init;

	stride = mpeg2dec->convert (MPEG2_CONVERT_STRIDE, NULL,
				    &(mpeg2dec->sequence), stride,
				    mpeg2_accels, mpeg2dec->convert_arg,
				    &convert_init);
	mpeg2dec->convert_id_size = convert_init.id_size;
	mpeg2dec->convert_stride = stride;
    }
    return stride;
}

void mpeg2_set_buf (mpeg2dec_t * mpeg2dec, uint8_t * buf[3], void * id)
{
    mpeg2_fbuf_t *fbuf = 0;

    if (mpeg2dec->custom_fbuf) {
	if (mpeg2dec->state == STATE_SEQUENCE) {
	    mpeg2dec->fbuf[2] = mpeg2dec->fbuf[1];
	    mpeg2dec->fbuf[1] = mpeg2dec->fbuf[0];
	}
	mpeg2_set_fbuf (mpeg2dec, (mpeg2dec->decoder.coding_type ==
				   PIC_FLAG_CODING_TYPE_B));
	fbuf = mpeg2dec->fbuf[0];
    } else {
	fbuf = &(mpeg2dec->fbuf_alloc[mpeg2dec->alloc_index].fbuf);
	mpeg2dec->alloc_index_user = ++mpeg2dec->alloc_index;
    }
    fbuf->buf[0] = buf[0];
    fbuf->buf[1] = buf[1];
    fbuf->buf[2] = buf[2];
    fbuf->id = id;
}

void mpeg2_custom_fbuf (mpeg2dec_t * mpeg2dec, int custom_fbuf)
{
    mpeg2dec->custom_fbuf = custom_fbuf;
}

void mpeg2_skip (mpeg2dec_t * mpeg2dec, int skip)
{
    mpeg2dec->first_decode_slice = 1;
    mpeg2dec->nb_decode_slices = skip ? 0 : (0xb0 - 1);
}

void mpeg2_slice_region (mpeg2dec_t * mpeg2dec, int start, int end)
{
    start = clamp(start, 1, 0xb0);
    end = clamp(end, start, 0xb0);
    mpeg2dec->first_decode_slice = start;
    mpeg2dec->nb_decode_slices = end - start;
}

void mpeg2_tag_picture (mpeg2dec_t * mpeg2dec, uint32_t tag, uint32_t tag2)
{
    mpeg2dec->tag_previous = mpeg2dec->tag_current;
    mpeg2dec->tag2_previous = mpeg2dec->tag2_current;
    mpeg2dec->tag_current = tag;
    mpeg2dec->tag2_current = tag2;
    mpeg2dec->num_tags++;
    mpeg2dec->bytes_since_tag = 0;
}

uint32_t mpeg2_accel (uint32_t accel)
{
    if (!mpeg2_accels) {
	mpeg2_accels = mpeg2_detect_accel (accel) | MPEG2_ACCEL_DETECT;
	mpeg2_cpu_state_init (mpeg2_accels);
	mpeg2_idct_init (mpeg2_accels);
	mpeg2_mc_init (mpeg2_accels);
    }
    return mpeg2_accels & ~MPEG2_ACCEL_DETECT;
}

void mpeg2_reset (mpeg2dec_t * mpeg2dec, int full_reset)
{
    mpeg2dec->buf_start = mpeg2dec->buf_end = NULL;
    mpeg2dec->num_tags = 0;
    mpeg2dec->shift = 0xffffff00;
    mpeg2dec->code = 0xb4;
    mpeg2dec->action = mpeg2_seek_header;
    mpeg2dec->state = STATE_INVALID;
    mpeg2dec->first = 1;

    mpeg2_reset_info(&(mpeg2dec->info));
    mpeg2dec->info.gop = NULL;
    mpeg2dec->info.user_data = NULL;
    mpeg2dec->info.user_data_len = 0;
    if (full_reset) {
	mpeg2dec->info.sequence = NULL;
	mpeg2_header_state_init (mpeg2dec);
    }

}

mpeg2dec_t * mpeg2_init (void)
{
    mpeg2_accel (MPEG2_ACCEL_DETECT);

    mpeg2dec_t *mpeg2dec = (mpeg2dec_t *) mpeg2_malloc (sizeof (mpeg2dec_t),
                                                        MPEG2_ALLOC_MPEG2DEC);
    if (mpeg2dec == NULL)
	return NULL;

    memset (mpeg2dec->decoder.DCTblock, 0, sizeof(int16_t) * 64);
    memset (mpeg2dec->quantizer_matrix, 0, sizeof(uint8_t) * 64 * 4);

    mpeg2dec->chunk_buffer = (uint8_t *) mpeg2_malloc (BUFFER_SIZE + 4,
						       MPEG2_ALLOC_CHUNK);

    mpeg2dec->sequence.width = (unsigned)-1;
    mpeg2_reset (mpeg2dec, 1);

    return mpeg2dec;
}

void mpeg2_close (mpeg2dec_t * mpeg2dec)
{
    mpeg2_header_state_init (mpeg2dec);
    mpeg2_free (mpeg2dec->chunk_buffer);
    mpeg2_free (mpeg2dec);
}
