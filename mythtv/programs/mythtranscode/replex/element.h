/*
 * element.h
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _ELEMENT_H_
#define _ELEMENT_H_

#include <stdint.h>

#include "ringbuffer.h"

#define PICTURE_START_CODE    0x00
#define SLICE_START_CODE_S    0x01

#define SLICE_START_CODE_E    0xAF
#define EXCEPT_SLICE          0xb0

#define USR_DATA_START_CODE   0xB2
#define SEQUENCE_HDR_CODE     0xB3
#define SEQUENCE_ERR_CODE     0xB4
#define EXTENSION_START_CODE  0xB5
#define SEQUENCE_END_CODE     0xB7
#define GROUP_START_CODE      0xB8

#define SEQUENCE_EXTENSION           0x01
#define SEQUENCE_DISPLAY_EXTENSION   0x02
#define PICTURE_CODING_EXTENSION     0x08
#define QUANT_MATRIX_EXTENSION       0x03
#define PICTURE_DISPLAY_EXTENSION    0x07

#define I_FRAME 0x01
#define P_FRAME 0x02
#define B_FRAME 0x03
#define D_FRAME 0x04

#define OFF_SIZE 4
#define FIRST_FIELD 0
#define SECOND_FIELD 1
#define VIDEO_FRAME_PICTURE 0x03

#define MPG_TIMESTEP 90000ULL
enum { VIDEO_NONE=0, VIDEO_PAL, VIDEO_NTSC};

#define CSPF_FLAG      0x04
#define INTRAQ_FLAG    0x02
#define NONINTRAQ_FLAG 0x01

#define AUDIO_SYNCWORD 0x7ff /* or is it 0xfff */
#define AC3_SYNCWORD   0x0b77

#define NOPULLDOWN 0
#define PULLDOWN32 1
#define PULLDOWN23 2

#define CLOCK_MS        27000ULL
#define CLOCK_PER    27000000000ULL
#define SEC_PER      (CLOCK_PER/s->frame_rate)


enum {
	NONE=0,AC3, MPEG_AUDIO, LPCM, MAX_TYPES
};

typedef struct sequence_s{
	int set;
	int ext_set;
	uint16_t h_size;
	uint16_t v_size;
	uint8_t  aspect_ratio;
	uint32_t  frame_rate;
	uint32_t bit_rate;
	uint32_t vbv_buffer_size;
	uint8_t  flags;
	uint8_t  intra_quant[64];
	uint8_t  non_intra_quant[64];
	int video_format;
	uint8_t  profile;
	uint8_t  progressive;
	uint8_t  chroma;
	uint8_t  pulldown_set;
	uint8_t  pulldown;
	uint8_t  current_frame;
	uint8_t  current_tmpref;
} sequence_t;

typedef
struct audio_frame_s{
	int set;
	int layer;
	uint32_t bit_rate;
	uint32_t frequency;
	uint32_t mode;
	uint32_t mode_extension;
	uint32_t emphasis;
	uint32_t framesize;
	uint32_t frametime;
	uint32_t off;
	char     language[4];
} audio_frame_t;

void pts2time(uint64_t pts, uint8_t *buf, int len);
int find_audio_sync(ringbuffer *rbuf, uint8_t *buf, int off, int type, int le);
int find_audio_s(uint8_t *rbuf, int off, int type, int le);
int get_video_info(ringbuffer *rbuf, sequence_t *s, int off, int le);
int get_audio_info(ringbuffer *rbuf, audio_frame_t *af, int off, int le); 
int get_ac3_info(ringbuffer *rbuf, audio_frame_t *af, int off, int le);
uint64_t add_pts_audio(uint64_t pts, audio_frame_t *aframe, uint64_t frames);
uint64_t next_ptsdts_video(uint64_t *pts, sequence_t *s, uint64_t fcount, uint64_t gcount);
void fix_audio_count(uint64_t *acount, audio_frame_t *aframe, 
		     uint64_t origpts, uint64_t pts);
void fix_video_count(sequence_t *s, uint64_t *frame, uint64_t origpts, 
		     uint64_t pts, uint64_t origdts, uint64_t dts);

int check_audio_header(ringbuffer *rbuf, audio_frame_t * af, 
		       int  off, int le, int type);
int get_video_ext_info(ringbuffer *rbuf, sequence_t *s, int off, int le);

#endif /*_ELEMENT_H_*/
