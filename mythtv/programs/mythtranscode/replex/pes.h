/* 
    Copyright (C) 2003 Marcus Metzler (mocm@metzlerbros.de)

*/

#ifndef _PES_H_
#define _PES_H_

#include <stdint.h>
#include "ringbuffer.h"

#define PS_HEADER_L1    14
#define PS_HEADER_L2    (PS_HEADER_L1+24)
#define PES_MIN         7
#define PES_H_MIN       9

#define SYSTEM_START_CODE_S   0xB9
#define SYSTEM_START_CODE_E   0xFF
#define PACK_START            0xBA
#define SYS_START             0xBB
#define PROG_STREAM_MAP       0xBC
#define PRIVATE_STREAM1       0xBD
#define PADDING_STREAM        0xBE
#define PRIVATE_STREAM2       0xBF
#define AUDIO_STREAM_S        0xC0
#define AUDIO_STREAM_E        0xDF
#define VIDEO_STREAM_S        0xE0
#define VIDEO_STREAM_E        0xEF
#define ECM_STREAM            0xF0
#define EMM_STREAM            0xF1
#define DSM_CC_STREAM         0xF2
#define ISO13522_STREAM       0xF3
#define PROG_STREAM_DIR       0xFF

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

#define MAX_PLENGTH 0xFFFF
#define MMAX_PLENGTH (8*MAX_PLENGTH)

#define MAX_PTS (0x0000000200000000ULL)
#define MAX_PTS2 (300* MAX_PTS)

typedef
struct ps_packet_{
	uint8_t scr[6];
	uint8_t mux_rate[3];
	uint8_t stuff_length;
	uint8_t *data;
	uint8_t sheader_llength[2];
	int sheader_length;
	uint8_t rate_bound[3];
	uint8_t audio_bound;
	uint8_t video_bound;
	uint8_t reserved;
	int npes;
} ps_packet;


typedef
struct pes_in_s{
	int type;
	unsigned int found;
	int withbuf;
	uint8_t *buf;
	ringbuffer *rbuf;
	uint8_t hbuf[260];
	int ini_pos;
	uint8_t cid;
	uint32_t plength;
	uint8_t plen[4];
	uint8_t flag1;
	uint8_t flag2;
	uint8_t hlength;
	uint8_t pts[5];
	uint8_t dts[5];
	int mpeg;
	int done;
	int which;
	void *priv;
} pes_in_t;


void init_pes_in(pes_in_t *p, int type, ringbuffer *rb, int wi);
void get_pes (pes_in_t *p, uint8_t *buf, int count, void (*func)(pes_in_t *p));
void printpts(int64_t pts);
void printptss(int64_t pts);
int64_t ptsdiff(uint64_t pts1, uint64_t pts2);
uint64_t uptsdiff(uint64_t pts1, uint64_t pts2);
int ptscmp(uint64_t pts1, uint64_t pts2);
uint64_t ptsadd(uint64_t pts1, uint64_t pts2);


void write_padding_pes( int pack_size, int apidn, int ac3n, 
			uint64_t SCR, uint64_t muxr, uint8_t *buf);
int write_ac3_pes(  int pack_size, int apidn, int ac3n, int n, uint64_t pts, 
		    uint64_t SCR, 
		    uint32_t muxr, uint8_t *buf, int *alength, uint8_t ptsdts,
		    int nframes,int ac3_off, ringbuffer *ac3rbuffer);
int write_audio_pes(  int pack_size, int apidn, int ac3n, int n, uint64_t pts, 
		      uint64_t SCR, uint32_t muxr, uint8_t *buf, int *alength, 
		      uint8_t ptsdts, 	ringbuffer *arbuffer);
int write_video_pes( int pack_size, int apidn, int ac3n, uint64_t vpts, 
		     uint64_t vdts, uint64_t SCR, uint64_t muxr, 
		     uint8_t *buf, int *vlength, 
		     uint8_t ptsdts, ringbuffer *vrbuffer);
int write_nav_pack(int pack_size, int apidn, int ac3n, uint64_t SCR, uint32_t muxr, 
		   uint8_t *buf);

static inline void ptsdec(uint64_t *pts1, uint64_t pts2)
{
	*pts1= uptsdiff(*pts1, pts2);
}

static inline void ptsinc(uint64_t *pts1, uint64_t pts2)
{
	*pts1 = (*pts1 + pts2)%MAX_PTS2;
}




#endif /*_PES_H_*/
