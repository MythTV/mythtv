/*
 * multiplex.h
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
#ifndef _MULTIPLEX_H_
#define _MULTIPLEX_H_

#include "mpg_common.h"
#include "pes.h"
#include "element.h"

#define N_AUDIO 32
#define N_AC3 8


typedef struct multiplex_s{
	int fd_out;
#define REPLEX_MPEG2  0
#define REPLEX_DVD    1
#define REPLEX_HDTV   2
	int otype;
	int startup;
	int finish;

	//muxing options
	uint64_t video_delay;
	uint64_t audio_delay;
	int pack_size;
	unsigned int data_size;
	uint32_t audio_buffer_size;
	uint32_t video_buffer_size;
	uint32_t mux_rate;
	uint32_t muxr;
	uint8_t navpack;
#define TIME_ALWAYS 1
#define TIME_IFRAME 2
	int frame_timestamps;
	int VBR;
	int reset_clocks;
	int write_end_codes;
	int set_broken_link;
	unsigned int vsize, asize;
	int64_t extra_clock;
	uint64_t first_vpts;
	uint64_t first_apts[N_AUDIO];
	uint64_t first_ac3pts[N_AC3];
	uint64_t SCR;
	uint64_t oldSCR;
	uint64_t SCRinc;
	index_unit viu;
	index_unit aiu[N_AUDIO];
	index_unit ac3iu[N_AC3];
	uint64_t apts[N_AUDIO];
	uint64_t ac3pts[N_AC3];
	uint64_t ac3pts_off[N_AC3];
	uint64_t apts_off[N_AUDIO];
	int aframes[N_AUDIO];
	int ac3frames[N_AUDIO];

/* needed from replex */
	int apidn;
	int ac3n;

	dummy_buffer vdbuf;
	dummy_buffer adbuf[N_AUDIO];
	dummy_buffer ac3dbuf[N_AC3];

	ringbuffer *ac3rbuffer;
	ringbuffer *index_ac3rbuffer;
	ringbuffer *arbuffer;
	ringbuffer *index_arbuffer;
	ringbuffer *vrbuffer;
	ringbuffer *index_vrbuffer;

	int (*fill_buffers)(void *p, int f);
	void *priv;
} multiplex_t;

void check_times( multiplex_t *mx, int *video_ok, int *audio_ok, int *ac3_ok,
		  int *start);
void write_out_packs( multiplex_t *mx, int video_ok, 
		      int *audio_ok, int *ac3_ok);
void finish_mpg(multiplex_t *mx);
void init_multiplex( multiplex_t *mx, sequence_t *seq_head, audio_frame_t *aframe,
		     audio_frame_t *ac3frame, int apidn, int ac3n,	
		     uint64_t video_delay, uint64_t audio_delay, int fd,
		     int (*fill_buffers)(void *p, int f),
		     ringbuffer *vrbuffer, ringbuffer *index_vrbuffer,	
		     ringbuffer *arbuffer, ringbuffer *index_arbuffer,
		     ringbuffer *ac3rbuffer, ringbuffer *index_ac3rbuffer,
		     int otype);

void setup_multiplex(multiplex_t *mx);
#endif /* _MULTIPLEX_H_*/
