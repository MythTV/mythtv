/*
 * replex.h
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

#ifndef _REPLEX_H_
#define _REPLEX_H_

#include <stdint.h>
#include "mpg_common.h"
#include "ts.h"
#include "element.h"
#include "ringbuffer.h"
#include "avi.h"
#include "multiplex.h"

enum { S_SEARCH, S_FOUND, S_ERROR };

struct replex {
#define REPLEX_TS  0
#define REPLEX_PS  1
#define REPLEX_AVI 2
	int itype;
	int otype;
	int ignore_pts; 
	int keep_pts;
	int fix_sync;
	uint64_t inflength;
	uint64_t finread;
	int lastper;
	int avi_rest;
	int avi_vcount;
	int fd_in;
	int fd_out;
	int finish;
	int demux;
	int dmx_out[N_AC3+N_AUDIO+1];
	int analyze;
	avi_context ac;
	int vdr;

	uint64_t video_delay;
	uint64_t audio_delay;

#define VIDEO_BUF (6*1024*1024)
#define AUDIO_BUF (VIDEO_BUF/10)
#define AC3_BUF   (VIDEO_BUF/10)
#define INDEX_BUF (32000*32)
	int audiobuf;
	int ac3buf;
	int videobuf;

    int ext_count;
    int exttype[N_AUDIO];
    int exttypcnt[N_AUDIO];
    audio_frame_t extframe[N_AUDIO];
    ringbuffer extrbuffer[N_AUDIO];
    ringbuffer index_extrbuffer[N_AUDIO];

  //ac3 
	int ac3n;
	uint16_t ac3_id[N_AC3];
	pes_in_t pac3[N_AC3];
	index_unit current_ac3index[N_AC3];
	int ac3pes_abort[N_AC3];
	ringbuffer ac3rbuffer[N_AC3];
	ringbuffer index_ac3rbuffer[N_AC3];
	uint64_t ac3frame_count[N_AC3];
	audio_frame_t ac3frame[N_AC3];
	uint64_t first_ac3pts[N_AC3];
	int ac3_state[N_AUDIO];
	uint64_t last_ac3pts[N_AC3];

// mpeg audio
	int apidn;
	uint16_t apid[N_AUDIO];
	pes_in_t paudio[N_AUDIO];
	index_unit current_aindex[N_AUDIO];
	int apes_abort[N_AUDIO];
	ringbuffer arbuffer[N_AUDIO];
	ringbuffer index_arbuffer[N_AUDIO];
	uint64_t aframe_count[N_AUDIO];
	audio_frame_t aframe[N_AUDIO];
	uint64_t first_apts[N_AUDIO];
	int audio_state[N_AUDIO];
	uint64_t last_apts[N_AUDIO];

//mpeg video
        uint16_t vpid;
	int first_iframe;
	pes_in_t pvideo;
	index_unit current_vindex;
	int vpes_abort;
	ringbuffer vrbuffer;
	ringbuffer index_vrbuffer;
	uint64_t vframe_count;
	uint64_t vgroup_count;
	sequence_t seq_head;
	uint64_t first_vpts;
	int video_state;
	uint64_t last_vpts;

	void *priv;
	int scan_found;
};

void init_index(index_unit *iu);
#endif
