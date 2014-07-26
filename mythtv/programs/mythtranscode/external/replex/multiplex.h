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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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
#define REPLEX_TS_SD  3
#define REPLEX_TS_HD  4
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
	int is_ts;
	int reset_clocks;
	int write_end_codes;
	int set_broken_link;
	unsigned int vsize, extsize;
	int64_t extra_clock;
	uint64_t SCR;
	uint64_t oldSCR;
	uint64_t SCRinc;
	index_unit viu;

	dummy_buffer vdbuf;

	extdata_t ext[N_AUDIO];
	int extcnt;

	ringbuffer *extrbuffer;
	ringbuffer *index_extrbuffer;
	ringbuffer *vrbuffer;
	ringbuffer *index_vrbuffer;

	int (*fill_buffers)(void *p, int f);
	void *priv;
} multiplex_t;

void check_times( multiplex_t *mx, int *video_ok, int *ext_ok, int *start);
void write_out_packs( multiplex_t *mx, int video_ok, int *ext_ok);
void finish_mpg(multiplex_t *mx);
void init_multiplex( multiplex_t *mx, sequence_t *seq_head,
		     audio_frame_t *extframe, int *exttype, int *exttypcnt,
		     uint64_t video_delay, uint64_t audio_delay, int fd,
		     int (*fill_buffers)(void *p, int f),
		     ringbuffer *vrbuffer, ringbuffer *index_vrbuffer,	
		     ringbuffer *extrbuffer, ringbuffer *index_extrbuffer,
		     int otype);

void setup_multiplex(multiplex_t *mx);
#endif /* _MULTIPLEX_H_*/
