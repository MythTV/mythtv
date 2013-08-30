/*
 * ts.h
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

#ifndef _TS_H_
#define _TS_H_

#include "ringbuffer.h"
#include "mpg_common.h"

#define TS_SIZE        188
#define TRANS_ERROR    0x80
#define PAY_START      0x40
#define TRANS_PRIO     0x20
#define PID_MASK_HI    0x1F

//flags
#define TRANS_SCRMBL1  0x80
#define TRANS_SCRMBL2  0x40
#define ADAPT_FIELD    0x20
#define PAYLOAD        0x10
#define COUNT_MASK     0x0F

// adaptation flags
#define DISCON_IND     0x80
#define RAND_ACC_IND   0x40
#define ES_PRI_IND     0x20
#define PCR_FLAG       0x10
#define OPCR_FLAG      0x08
#define SPLICE_FLAG    0x04
#define TRANS_PRIV     0x02
#define ADAP_EXT_FLAG  0x01

// adaptation extension flags
#define LTW_FLAG       0x80
#define PIECE_RATE     0x40
#define SEAM_SPLICE    0x20

#define TS_VIDPID      4101
#define TS_MP2PID      4201
#define TS_AC3PID      4301
uint16_t get_pid(uint8_t *pid);
int find_pids(uint16_t *vpid, uint16_t *apid, uint16_t *ac3pid,uint8_t *buf, int len);
int find_pids_pos(uint16_t *vpid, uint16_t *apid, uint16_t *ac3pid,uint8_t *buf, int len, int *vpos, int *apos, int *ac3pos);

int write_video_ts(uint64_t vpts, uint64_t vdts, uint64_t SCR,
	     uint8_t *buf, int *vlength, uint8_t ptsdts, ringbuffer *vrbuffer);
int write_audio_ts(int n, uint64_t pts, 
	     uint8_t *buf, int *alength, uint8_t ptsdts, ringbuffer *arbuffer);
int write_ac3_ts(int n, uint64_t pts, uint8_t *buf, int *alength,
	 uint8_t ptsdts, int nframes, ringbuffer *ac3rbuffer);
void write_ts_patpmt(extdata_t *ext, int extcnt, uint8_t prog_num,
			uint8_t *buf);
#endif /*_TS_H_*/
