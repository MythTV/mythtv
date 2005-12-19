/*
 * mpg_common.h
 *        
 *
 * Copyright (C) 2003 Marcus Metzler <mocm@metzlerbros.de>
 *                    Metzler Brothers Systementwicklung GbR
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _MPG_COMMON_H_
#define _MPG_COMMON_H_

#include <stdint.h>
#include "ringbuffer.h"



typedef struct index_unit_s{
	uint8_t  active;
	uint32_t length;
	uint32_t start;
	uint64_t pts;
	uint64_t dts;
	uint8_t  seq_header;
	uint8_t  seq_end;
	uint8_t  gop;
	uint8_t  end_seq;
	uint8_t  frame;
	uint8_t  gop_off;
	uint8_t  frame_off;
	uint8_t  frame_start;
	uint8_t  err;
	uint32_t framesize;
} index_unit;

#define NO_ERR    0
#define FRAME_ERR 1


void show_buf(uint8_t *buf, int length);
int find_mpg_header(uint8_t head, uint8_t *buf, int length);
int find_any_header(uint8_t *head, uint8_t *buf, int length);
uint64_t trans_pts_dts(uint8_t *pts);
int mring_peek( ringbuffer *rbuf, uint8_t *buf, unsigned int l, uint32_t off);
int ring_find_mpg_header(ringbuffer *rbuf, uint8_t head, int off, int le);
int ring_find_any_header(ringbuffer *rbuf, uint8_t *head, int off, int le);

#endif /*_MPG_COMMON_H_*/
