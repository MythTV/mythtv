/*
 *  dvb-mpegtools for the Siemens Fujitsu DVB PCI card
 *
 * Copyright (C) 2000, 2001 Marcus Metzler 
 *            for convergence integrated media GmbH
 * Copyright (C) 2002 Marcus Metzler 
 * 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 * 

 * The author can be reached at mocm@metzlerbros.de, 
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
//#include <libgen.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "ringbuffy.h"
#include "ctools.h"

#ifndef _REMUX_H_
#define _REMUX_H_

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

	typedef struct video_i{
		uint32_t horizontal_size;
		uint32_t vertical_size 	;
		uint32_t aspect_ratio	;
		double framerate	;
		uint32_t video_format;
		uint32_t bit_rate 	;
		uint32_t comp_bit_rate	;
		uint32_t vbv_buffer_size;
		uint32_t CSPF 		;
		uint32_t off;
	} VideoInfo; 		

	typedef struct audio_i{
		int layer;
		uint32_t bit_rate;
		uint32_t frequency;
		uint32_t mode;
		uint32_t mode_extension;
		uint32_t emphasis;
		uint32_t framesize;
		uint32_t off;
	} AudioInfo;



	typedef
	struct PTS_list_struct{
		uint32_t PTS;
		int pos;
		uint32_t dts;
		int spos;
	} PTS_List;

	typedef
	struct frame_list_struct{
		int type;
		int pos;
		uint32_t FRAME;
		uint32_t time;
		uint32_t pts;
		uint32_t dts;
	} FRAME_List;

	typedef
	struct remux_struct{
		ringbuffy vid_buffy;
		ringbuffy aud_buffy;
		PTS_List vpts_list[MAX_PTS];
		PTS_List apts_list[MAX_PTS];
		FRAME_List vframe_list[MAX_FRAME];
		FRAME_List aframe_list[MAX_FRAME];
		int vptsn;
		int aptsn;
		int vframen;
		int aframen;
		long apes;
		long vpes;
		uint32_t vframe;
		uint32_t aframe;
		uint32_t vcframe;
		uint32_t acframe;
		uint32_t vpts;
		uint32_t vdts;
		uint32_t apts;
		uint32_t vpts_old;
		uint32_t apts_old;
		uint32_t SCR;
		uint32_t apts_off;
		uint32_t vpts_off;
		uint32_t apts_delay;
		uint32_t vpts_delay;
		uint32_t dts_delay;
		AudioInfo audio_info;
		VideoInfo video_info;
		int fin;
		int fout;
		long int awrite;
		long int vwrite;
		long int aread;
		long int vread;
		uint32_t group;
		uint32_t groupframe;
		uint32_t muxr;
		int pack_size;
		uint32_t time_off;
	} Remux;

	enum { NONE, I_FRAME, P_FRAME, B_FRAME, D_FRAME };

	void remux(int fin, int fout, int pack_size, int mult);
	void remux2(int fdin, int fdout);
#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif /*_REMUX_H_*/
