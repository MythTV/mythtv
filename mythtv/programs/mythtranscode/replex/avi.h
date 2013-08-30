/*
 * avi.h
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

#ifndef _AVI_H_
#define _AVI_H_

#include <stdint.h>
#include "mpg_common.h"
#include "pes.h"

#define TAG_IT(a,b,c,d) ((d<<24) | (c<<16) | (b<<8)|a)
#define ALIGN(a) (((4-a)%3)%3)

#define AVI_HASINDEX           0x00000010      
#define AVI_USEINDEX           0x00000020
#define AVI_INTERLEAVED        0x00000100

typedef struct avi_index_s {
	uint32_t id;
	uint32_t flags, len;
	uint32_t off;
} avi_index;

typedef struct avi_audio_s
{
	uint32_t dw_scale, dw_rate;
	uint32_t dw_sample_size;
	uint32_t fps;
	uint32_t type;
	uint32_t initial_frames;
	uint32_t dw_start;
	uint32_t dw_ssize;
} avi_audio_info;

typedef struct avi_video_s
{
	uint16_t width;
	uint16_t height;
	uint32_t dw_scale, dw_rate;
	uint32_t fps;
	uint32_t pos_off;
	uint32_t initial_frames;
	uint32_t dw_start;
} avi_video_info;

#define MAX_TRACK 10
typedef struct {
	int64_t  riff_end;
	int64_t  movi_start;
	int64_t  movi_length;
	int last_size;
	int done;

	uint64_t next_frame;

	int ntracks;
	uint32_t num_idx_alloc;
	uint32_t num_idx_frames;
	uint32_t msec_per_frame;
	uint32_t avih_flags;
	uint32_t total_frames;
	uint32_t init_frames;
	uint32_t width;
	uint32_t height;
	uint32_t nstreams;
	uint32_t vhandler;
	uint32_t ahandler;
	uint32_t vchunks;
	uint32_t achunks;
	uint32_t zero_vchunks;
	uint32_t zero_achunks;
	
	uint32_t current_idx;
	
	avi_video_info vi;
	avi_audio_info ai[MAX_TRACK];

	avi_index *idx;
} avi_context;

int check_riff(avi_context *ac, uint8_t *buf, int len);
int read_avi_header( avi_context *ac, int fd);
void get_avi(pes_in_t *p, uint8_t *buf, int count, void (*func)(pes_in_t *p));
int avi_read_index(avi_context *ac, int fd);
int get_avi_from_index(pes_in_t *p, int fd, avi_context *ac, 
		       void (*func)(pes_in_t *p), int insize);


#endif /* _AVI_H_ */
