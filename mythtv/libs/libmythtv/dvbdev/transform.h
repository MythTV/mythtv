/*
 *  dvb-mpegtools for the Siemens Fujitsu DVB PCI card
 *
 * Copyright (C) 2000, 2001 Marcus Metzler 
 *            for convergence integrated media GmbH
 * Copyright (C) 2002  Marcus Metzler 
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

#ifndef _TS_TRANSFORM_H_
#define _TS_TRANSFORM_H_

#include <stdint.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "remux.h"

#define PROG_STREAM_MAP  0xBC
#ifndef PRIVATE_STREAM1
#define PRIVATE_STREAM1  0xBD
#endif
#define PADDING_STREAM   0xBE
#ifndef PRIVATE_STREAM2
#define PRIVATE_STREAM2  0xBF
#endif
#define AUDIO_STREAM_S   0xC0
#define AUDIO_STREAM_E   0xDF
#define VIDEO_STREAM_S   0xE0
#define VIDEO_STREAM_E   0xEF
#define ECM_STREAM       0xF0
#define EMM_STREAM       0xF1
#define DSM_CC_STREAM    0xF2
#define ISO13522_STREAM  0xF3
#define PROG_STREAM_DIR  0xFF

#define BUFFYSIZE    10*MAX_PLENGTH
#define MAX_PTS      8192
#define MAX_FRAME    8192
#define MAX_PACK_L   4096
#define PS_HEADER_L1    14
#define PS_HEADER_L2    (PS_HEADER_L1+18)
#define MAX_H_SIZE   (PES_H_MIN + PS_HEADER_L1 + 5)
#define PES_MIN         7
#define PES_H_MIN       9

//flags2
#define PTS_DTS_FLAGS    0xC0
#define ESCR_FLAG        0x20
#define ES_RATE_FLAG     0x10
#define DSM_TRICK_FLAG   0x08
#define ADD_CPY_FLAG     0x04
#define PES_CRC_FLAG     0x02
#define PES_EXT_FLAG     0x01

//pts_dts flags 
#define PTS_ONLY         0x80
#define PTS_DTS          0xC0

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


#define MAX_PLENGTH 0xFFFF
#define MMAX_PLENGTH (8*MAX_PLENGTH)

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

#define P2P_LENGTH 2048

	enum{NOPES, AUDIO, VIDEO, AC3};

	typedef struct p2pstruct {
		int found;
		uint8_t buf[MMAX_PLENGTH];
		uint8_t cid;
		uint8_t subid;
		uint32_t plength;
		uint8_t plen[2];
		uint8_t flag1;
		uint8_t flag2;
		uint8_t hlength;
		uint8_t pts[5];
		int mpeg;
		uint8_t check;
		int fd1;
		int fd2;
		int es;
		int filter;
		int which;
		int done;
		int repack;
		uint16_t bigend_repack;
		void (*func)(uint8_t *buf, int count, void *p);
		int startv;
                int starta;
		int64_t apts;
                int64_t vpts;
		uint16_t pid;
		uint16_t pida;
		uint16_t pidv;
		uint8_t acounter;
		uint8_t vcounter;
		uint8_t count0;
		uint8_t count1;
		void *data;
	} p2p;

	
	uint64_t trans_pts_dts(uint8_t *pts);
	int write_ts_header(uint16_t pid, uint8_t *counter, int pes_start, 
			    uint8_t *buf, uint8_t length);
	uint16_t get_pid(uint8_t *pid);
	void init_p2p(p2p *p, void (*func)(uint8_t *buf, int count, void *p),
		      int repack);
	void get_pes (uint8_t *buf, int count, p2p *p, void (*func)(p2p *p));
	void get_pes (uint8_t *buf, int count, p2p *p, void (*func)(p2p *p));
	void pes_repack(p2p *p);
	void setup_pes2ts( p2p *p, uint32_t pida, uint32_t pidv, 
			   void (*ts_write)(uint8_t *buf, int count, void *p));
	void kpes_to_ts( p2p *p,uint8_t *buf ,int count );
	void setup_ts2pes( p2p *pa, p2p *pv, uint32_t pida, uint32_t pidv, 
			   void (*pes_write)(uint8_t *buf, int count, void *p));
	void kts_to_pes( p2p *p, uint8_t *buf);
	void pes_repack(p2p *p);
	void extract_from_pes(int fdin, int fdout, uint8_t id, int es);
	int64_t pes_dmx(int fdin, int fdouta, int fdoutv, int es);
	void pes_to_ts2( int fdin, int fdout, uint16_t pida, uint16_t pidv);
	void ts_to_pes( int fdin, uint16_t pida, uint16_t pidv, int pad);
	int get_ainfo(uint8_t *mbuf, int count, AudioInfo *ai, int pr);
	int get_vinfo(uint8_t *mbuf, int count, VideoInfo *vi, int pr);
	int get_ac3info(uint8_t *mbuf, int count, AudioInfo *ai, int pr);
	void filter_audio_from_pes(int fdin, int fdout, uint8_t id, 
				   uint8_t subid);


//instant repack

	typedef struct ipack_s {
		int size;
		int size_orig;
		int found;
		int ps;
		int has_ai;
		int has_vi;
		AudioInfo ai;
		VideoInfo vi;
		uint8_t *buf;
		uint8_t cid;
		uint32_t plength;
		uint8_t plen[2];
		uint8_t flag1;
		uint8_t flag2;
		uint8_t hlength;
		uint8_t pts[5];
		uint8_t last_pts[5];
		int mpeg;
		uint8_t check;
		int which;
		int done;
		void *data;
		void (*func)(uint8_t *buf,  int size, void *priv);
		int count;
		int start;
		int fd;
	} ipack;

	void instant_repack (uint8_t *buf, int count, ipack *p);
	void init_ipack(ipack *p, int size,
			void (*func)(uint8_t *buf,  int size, void *priv),
			int pad);
	void free_ipack(ipack * p);
	void send_ipack(ipack *p);
	void reset_ipack(ipack *p);		     
	void ps_pes(ipack *p);
	// use with ipack structure, repack size and callback func 

	int64_t ts_demux(int fd_in, int fdv_out,int fda_out,uint16_t pida,
			  uint16_t pidv, int es);

	void ts2es(int fdin,  uint16_t pidv);
	void ts2es_opt(int fdin,  uint16_t pidv, ipack *p, int verb);
	void insert_pat_pmt( int fdin, int fdout);
	void change_aspect(int fdin, int fdout, int aspect);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif /* _TS_TRANSFORM_H_*/



