/* 
   RTjpeg (C) Justin Schoeman 1998 (justin@suntiger.ee.up.ac.za)
   
   With modifications by:
   (c) 1998, 1999 by Joerg Walter <trouble@moes.pmnet.uni-oldenburg.de>
   and
   (c) 1999 by Wim Taymans <wim.taymans@tvd.be>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public  
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,  
but WITHOUT ANY WARRANTY; without even the implied warranty of   
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public   
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

        Justin Schoeman:
	  e-mail:
        	justin@suntiger.ee.up.ac.za
	  snail mail:
		EE&C Engineering
		University of Pretoria
		Pretoria
		0002
		South Africa

Changes: 0.3.0a - documentation fixes
	 0.3.0b - Endian fixes thanks to Calle Lejdfors
	 0.3.1  - Hide struct RTjpeg_t for apps (unreleased)
	 0.3.2  - Cleanups + QT fixes (christian.tusche@stud.uni-goettingen.de)
*/

#ifndef __RTJPEG_H__
#define __RTJPEG_H__

/*
 * Macros and definitions used internally to RTjpeg
 */

#define RTJPEG_FILE_VERSION 0
#define RTJPEG_HEADER_SIZE 12

#ifdef WORDS_BIGENDIAN
#define RTJPEG_SWAP_WORD(a) ( ((a) << 24) | \
			(((a) << 8) & 0x00ff0000) | \
			(((a) >> 8) & 0x0000ff00) | \
			((unsigned long)(a) >>24) )
#define RTJPEG_SWAP_HALFWORD(a) ( (((a) << 8) & 0xff00) | \
			(((a) >> 8) & 0x00ff) )
#else
#define RTJPEG_SWAP_WORD(a) (a)
#define RTJPEG_SWAP_HALFWORD(a) (a)
#endif

#ifndef _STDINT_H
#include <stdint.h>
#endif

/*
 * Internal structures - your applications should NEVER play with these 
 * directly.
 */

#ifdef __RTJPEG_INTERNAL__

#ifdef MMX
#include "mmx.h"
#endif

typedef struct {
#if 1
	int16_t block[64] __attribute__ ((aligned (32)));
	int32_t ws[64*4] __attribute__ ((aligned (32)));
	int32_t lqt[64] __attribute__ ((aligned (32)));
	int32_t cqt[64] __attribute__ ((aligned (32)));
	int32_t liqt[64] __attribute__ ((aligned (32)));
	int32_t ciqt[64] __attribute__ ((aligned (32)));
#else
	int16_t block[64];
	int32_t ws[64*4];
	int32_t lqt[64];
	int32_t cqt[64];
	int32_t liqt[64];
	int32_t ciqt[64];
#endif
	int lb8;
	int cb8;
	int Ywidth;
	int Cwidth;
	int Ysize;
	int Csize;
	int16_t *old;
	int16_t *old_start;
	int key_count;

	int width;
	int height;
	int Q;
	int f;
#ifdef MMX
	mmx_t lmask;
	mmx_t cmask;
#else
	uint16_t lmask;
	uint16_t cmask;
#endif
	int key_rate;
} RTjpeg_t;

#else

typedef void RTjpeg_t;

#endif /* __RTJPEG_INTERNAL__ */

typedef struct {
	uint32_t framesize;
	uint8_t headersize;
	uint8_t version;
	uint16_t width;
	uint16_t height;
	uint8_t quality;
	uint8_t key;
	uint8_t data;
} RTjpeg_frameheader;

/* This is the interface you should use */

/* Format definitions */

#define RTJ_YUV420 0
#define RTJ_YUV422 1
#define RTJ_RGB8 2

/* Initialisation functions */

extern RTjpeg_t *RTjpeg_init();
extern void RTjpeg_close(RTjpeg_t *rtj);
extern int RTjpeg_set_quality(RTjpeg_t *rtj, int *quality);
extern int RTjpeg_set_format(RTjpeg_t *rtj, int *format);
extern int RTjpeg_set_size(RTjpeg_t *rtj, int *w, int *h);
extern int RTjpeg_set_intra(RTjpeg_t *rtj, int *key, int *lm, int *cm);

/* Compress/Decompress functions */

extern int RTjpeg_compress(RTjpeg_t *rtj, int8_t *sp, uint8_t **planes);
extern int RTjpeg_nullcompress(RTjpeg_t *rtj, int8_t *sp);
extern void RTjpeg_decompress(RTjpeg_t *rtj, int8_t *sp, uint8_t **planes);

/* Colour space conversion functions */

void RTjpeg_yuv420rgb32(RTjpeg_t *rtj, uint8_t **planes, uint8_t **rows);
void RTjpeg_yuv420bgr32(RTjpeg_t *rtj, uint8_t **planes, uint8_t **rows);
void RTjpeg_yuv420rgb24(RTjpeg_t *rtj, uint8_t **planes, uint8_t **rows);
void RTjpeg_yuv420bgr24(RTjpeg_t *rtj, uint8_t **planes, uint8_t **rows);
void RTjpeg_yuv420rgb16(RTjpeg_t *rtj, uint8_t **planes, uint8_t **rows);
void RTjpeg_yuv420rgb8(RTjpeg_t *rtj, uint8_t **planes, uint8_t **rows);

void RTjpeg_yuv422rgb24(RTjpeg_t *rtj, uint8_t **planes, uint8_t **rows);
#define RTjpeg_yuv422rgb8(x, y, z) RTjpeg_yuv420rgb8(x, y, z)

/* Compatibility functions */

void RTjpeg_get_tables(RTjpeg_t *rtj, uint32_t *tables);
void RTjpeg_set_tables(RTjpeg_t *rtj, uint32_t *tables);

#define RTJPEG_COMPAT_STREAM(sp) RTjpeg_frame_data(sp)
#define RTJPEG_COMPAT_LENGTH(sp) (RTjpeg_frame_framesize(sp) - RTjpeg_frame_headersize(sp))

/* Header manipulations - NEVER manipulate the header directly */

#define RTjpeg_frame_framesize(sp) RTJPEG_SWAP_WORD(((RTjpeg_frameheader *)(sp))->framesize)
#define RTjpeg_frame_headersize(sp) (((RTjpeg_frameheader *)(sp))->headersize)
#define RTjpeg_frame_version(sp) (((RTjpeg_frameheader *)(sp))->version)
#define RTjpeg_frame_width(sp) RTJPEG_SWAP_HALFWORD(((RTjpeg_frameheader *)(sp))->width)
#define RTjpeg_frame_height(sp) RTJPEG_SWAP_HALFWORD(((RTjpeg_frameheader *)(sp))->height)
#define RTjpeg_frame_quality(sp) (((RTjpeg_frameheader *)(sp))->quality)
#define RTjpeg_frame_key(sp) (((RTjpeg_frameheader *)(sp))->key)
#define RTjpeg_frame_data(sp) (&(((RTjpeg_frameheader *)(sp))->data))

#endif /* __RTjpeg_h__ */
