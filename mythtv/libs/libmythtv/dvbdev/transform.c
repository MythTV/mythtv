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

 * The author can be reached at marcus@convergence.de, 

 * the project's page is at http://linuxtv.org/dvb/
 */


#include "transform.h"
#include <stdlib.h>
#include <string.h>
#include "ctools.h"

static uint8_t tspid0[TS_SIZE] = { 
	0x47, 0x40, 0x00, 0x10, 0x00, 0x00, 0xb0, 0x11, 
	0x00, 0x00, 0xcb, 0x00, 0x00, 0x00, 0x00, 0xe0, 
	0x10, 0x00, 0x01, 0xe4, 0x00, 0x2a, 0xd6, 0x1a, 
	0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff
};

static	uint8_t tspid1[TS_SIZE] = { 
	0x47, 0x44, 0x00, 0x10, 0x00, 0x02, 0xb0, 0x1c,
	0x00, 0x01, 0xcb, 0x00, 0x00, 0xe0, 0xa0, 0xf0, 
	0x05, 0x48, 0x03, 0x01, 0x00, 0x00, 0x02, 0xe0,
	0xa0, 0xf0, 0x00, 0x03, 0xe0, 0x50, 0xf0, 0x00, 
	0xae, 0xea, 0x4e, 0x48, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff
};

// CRC32 lookup table for polynomial 0x04c11db7
static const uint32_t crc_table[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static uint32_t
calc_crc32 (const uint8_t *sec, uint8_t len)
{
  int i;
  uint32_t crc = 0xffffffff;

  for (i = 0; i < len; i++)
    crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *sec++) & 0xff];

  return crc;
}


uint64_t trans_pts_dts(uint8_t *pts)
{
	uint64_t wts;
	
	wts = ((uint64_t)((pts[0] & 0x0E) << 5) | 
	       ((pts[1] & 0xFC) >> 2)) << 24; 
	wts |= (((pts[1] & 0x03) << 6) |
		((pts[2] & 0xFC) >> 2)) << 16; 
	wts |= (((pts[2] & 0x02) << 6) |
		((pts[3] & 0xFE) >> 1)) << 8;
	wts |= (((pts[3] & 0x01) << 7) |
		((pts[4] & 0xFE) >> 1));
	return wts;
}


void get_pespts(uint8_t *av_pts,uint8_t *pts)
{
	
	pts[0] = 0x21 | 
		((av_pts[0] & 0xC0) >>5);
	pts[1] = ((av_pts[0] & 0x3F) << 2) |
		((av_pts[1] & 0xC0) >> 6);
	pts[2] = 0x01 | ((av_pts[1] & 0x3F) << 2) |
		((av_pts[2] & 0x80) >> 6);
	pts[3] = ((av_pts[2] & 0x7F) << 1) |
		((av_pts[3] & 0x80) >> 7);
	pts[4] = 0x01 | ((av_pts[3] & 0x7F) << 1);
}

uint16_t get_pid(uint8_t *pid)
{
	uint16_t pp = 0;

	pp = (pid[0] & PID_MASK_HI)<<8;
	pp |= pid[1];

	return pp;
}

int write_ts_header(uint16_t pid, uint8_t *counter, int pes_start, 
		    uint8_t *buf, uint8_t length)
{
	int i;
	int c = 0;
	int fill;
	uint8_t tshead[4] = { 0x47, 0x00, 0x00, 0x10}; 
        

	fill = TS_SIZE-4-length;
        if (pes_start) tshead[1] = 0x40;
	if (fill) tshead[3] = 0x30;
        tshead[1] |= (uint8_t)((pid & 0x1F00) >> 8);
        tshead[2] |= (uint8_t)(pid & 0x00FF);
        tshead[3] |= ((*counter)++ & 0x0F) ;
        memcpy(buf,tshead,4);
	c+=4;


	if (fill){
		buf[4] = fill-1;
		c++;
		if (fill >1){
			buf[5] = 0x00;
			c++;
		}
		for ( i = 6; i < fill+4; i++){
			buf[i] = 0xFF;
			c++;
		}
	}

        return c;
}


int write_pes_header(uint8_t id,int length , long PTS, uint8_t *obuf, 
		     int stuffing)
{
	uint8_t le[2];
	uint8_t dummy[3];
	uint8_t *pts;
	uint8_t ppts[5];
	long lpts;
	int c;
	uint8_t headr[3] = {0x00, 0x00, 0x01};
	
	lpts = htonl(PTS);
	pts = (uint8_t *) &lpts;
	
	get_pespts(pts,ppts);

	c = 0;
	memcpy(obuf+c,headr,3);
	c += 3;
	memcpy(obuf+c,&id,1);
	c++;

	le[0] = 0;
	le[1] = 0;
	length -= 6+stuffing;

	le[0] |= ((uint8_t)(length >> 8) & 0xFF); 
	le[1] |= ((uint8_t)(length) & 0xFF); 
	memcpy(obuf+c,le,2);
	c += 2;

	if (id == PADDING_STREAM){
		memset(obuf+c,0xff,length);
		c+= length;
		return c;
	}

	dummy[0] = 0x80;
	dummy[1] = 0;
	dummy[2] = 0;
	if (PTS){
		dummy[1] |= PTS_ONLY;
		dummy[2] = 5+stuffing;
	}
	memcpy(obuf+c,dummy,3);
	c += 3;
	memset(obuf+c,0xFF,stuffing);

	if (PTS){
		memcpy(obuf+c,ppts,5);
		c += 5;
	}
	
	return c;
}


void init_p2p(p2p *p, void (*func)(uint8_t *buf, int count, void *p), 
	      int repack){
	p->found = 0;
	p->cid = 0;
	p->mpeg = 0;
	memset(p->buf,0,MMAX_PLENGTH);
	p->done = 0;
	p->fd1 = -1;
	p->func = func;
	p->bigend_repack = 0;
	p->repack = 0; 
	if ( repack < MAX_PLENGTH && repack > 265 ){
		p->repack = repack-6;
		p->bigend_repack = (uint16_t)htons((short)
						   ((repack-6) & 0xFFFF));
	} else {
		fprintf(stderr, "Repack size %d is out of range\n",repack);
		exit(1);
	}
}



void pes_repack(p2p *p)
{
	int count = 0;
	int repack = p->repack;
	int rest = p->plength;
	uint8_t buf[MAX_PLENGTH];
	int bfill = 0;
	int diff;
	uint16_t length;

	if (rest < 0) {
                fprintf(stderr,"Error in repack\n");
                return;
        }

        if (!repack){
                fprintf(stderr,"forgot to set repack size\n");
                return;
        }

	if (p->plength == repack){
		memcpy(p->buf+4,(char *)&p->bigend_repack,2);
		p->func(p->buf, repack+6, p);
		return;
	}

	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = p->cid;
	memcpy(buf+4,(char *)&p->bigend_repack,2);
	memset(buf+6,0,MAX_PLENGTH-6);

	if (p->mpeg == 2){

		if ( rest > repack){
			memcpy(p->buf+4,(char *)&p->bigend_repack,2);
			p->func(p->buf, repack+6, p);
			count += repack+6;
			rest -= repack;
		} else {
			memcpy(buf,p->buf,9+p->hlength);
			bfill = p->hlength;
			count += 9+p->hlength;
			rest -= p->hlength+3;
		}

		while (rest >= repack-3){
			memset(buf+6,0,MAX_PLENGTH-6);
			buf[6] = 0x80;
			buf[7] = 0x00;
			buf[8] = 0x00;
			memcpy(buf+9,p->buf+count,repack-3);
			rest -= repack-3;
			count += repack-3;
			p->func(buf, repack+6, p);
		}
		
		if (rest){
			diff = repack - 3 - rest - bfill;
			if (!bfill){
				buf[6] = 0x80;
				buf[7] = 0x00;
				buf[8] = 0x00;
			}

			if ( diff < PES_MIN){
				length = rest+ diff + bfill+3; 
				buf[4] = (uint8_t)((length & 0xFF00) >> 8);
				buf[5] = (uint8_t)(length & 0x00FF);
				buf[8] = (uint8_t)(bfill+diff);
				memset(buf+9+bfill,0xFF,diff);
				memcpy(buf+9+bfill+diff,p->buf+count,rest);
			} else {
				length = rest+ bfill+3; 
				buf[4] = (uint8_t)((length & 0xFF00) >> 8);
				buf[5] = (uint8_t)(length & 0x00FF);
				memcpy(buf+9+bfill,p->buf+count,rest);
				bfill += rest+9;
				write_pes_header( PADDING_STREAM, diff, 0,
						  buf+bfill, 0);
			}
			p->func(buf, repack+6, p);
		}
	}	

	if (p->mpeg == 1){

		if ( rest > repack){
			memcpy(p->buf+4,(char *)&p->bigend_repack,2);
			p->func(p->buf, repack+6, p);
			count += repack+6;
			rest -= repack;
		} else {
			memcpy(buf,p->buf,6+p->hlength);
			bfill = p->hlength;
			count += 6;
			rest -= p->hlength;
		}

		while (rest >= repack-1){
			memset(buf+6,0,MAX_PLENGTH-6);
			buf[6] = 0x0F;
			memcpy(buf+7,p->buf+count,repack-1);
			rest -= repack-1;
			count += repack-1;
			p->func(buf, repack+6, p);
		}
		

		if (rest){
			diff = repack - 1 - rest - bfill;

			if ( diff < PES_MIN){
				length = rest+ diff + bfill+1; 
				buf[4] = (uint8_t)((length & 0xFF00) >> 8);
				buf[5] = (uint8_t)(length & 0x00FF);
				memset(buf+6,0xFF,diff);
				if (!bfill){
					buf[6+diff] = 0x0F;
				}
				memcpy(buf+7+diff,p->buf+count,rest+bfill);
			} else {
				length = rest+ bfill+1; 
				buf[4] = (uint8_t)((length & 0xFF00) >> 8);
				buf[5] = (uint8_t)(length & 0x00FF);
				if (!bfill){
					buf[6] = 0x0F;
					memcpy(buf+7,p->buf+count,rest);
					bfill = rest+7;
				} else {
					memcpy(buf+6,p->buf+count,rest+bfill);
					bfill += rest+6;
				}
				write_pes_header( PADDING_STREAM, diff, 0,
						  buf+bfill, 0);
			}
			p->func(buf, repack+6, p);
		}
	}	
}








int filter_pes (uint8_t *buf, int count, p2p *p, int (*func)(p2p *p))
{

	int l;
	unsigned short *pl;
	int c=0;
	int ret = 1;

	uint8_t headr[3] = { 0x00, 0x00, 0x01} ;

	while (c < count && (p->mpeg == 0 ||
			     (p->mpeg == 1 && p->found < 7) ||
			     (p->mpeg == 2 && p->found < 9))
	       &&  (p->found < 5 || !p->done)){
		switch ( p->found ){
		case 0:
		case 1:
			if (buf[c] == 0x00) p->found++;
			else {
				if (p->fd1 >= 0)
					write(p->fd1,buf+c,1);
				p->found = 0;
			}
			c++;
			break;
		case 2:
			if (buf[c] == 0x01) p->found++;
			else if (buf[c] == 0){
				p->found = 2;
			} else {	
				if (p->fd1 >= 0)
					write(p->fd1,buf+c,1);
				p->found = 0;
			}
			c++;
			break;
		case 3:
			p->cid = 0;
			switch (buf[c]){
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
				if (p->fd1 >= 0)
					write(p->fd1,buf+c,1);
				p->done = 1;
			case PRIVATE_STREAM1:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
				p->found++;
				p->cid = buf[c];
				c++;
				break;
			default:
				if (p->fd1 >= 0)
					write(p->fd1,buf+c,1);
				p->found = 0;
				break;
			}
			break;
			

		case 4:
			if (count-c > 1){
				pl = (unsigned short *) (buf+c);
				p->plength =  ntohs(*pl);
				p->plen[0] = buf[c];
				c++;
				p->plen[1] = buf[c];
				c++;
				p->found+=2;
			} else {
				p->plen[0] = buf[c];
				p->found++;
				return 1;
			}
			break;
		case 5:
			p->plen[1] = buf[c];
			c++;
			pl = (unsigned short *) p->plen;
			p->plength = ntohs(*pl);
			p->found++;
			break;


		case 6:
			if (!p->done){
				p->flag1 = buf[c];
				c++;
				p->found++;
				if ( (p->flag1 & 0xC0) == 0x80 ) p->mpeg = 2;
				else {
					p->hlength = 0;
					p->which = 0;
					p->mpeg = 1;
					p->flag2 = 0;
				}
			}
			break;

		case 7:
			if ( !p->done && p->mpeg == 2){
				p->flag2 = buf[c];
				c++;
				p->found++;
			}	
			break;

		case 8:
			if ( !p->done && p->mpeg == 2){
				p->hlength = buf[c];
				c++;
				p->found++;
			}
			break;
			
		default:

			break;
		}
	}

	if (!p->plength) p->plength = MMAX_PLENGTH-6;


	if ( p->done || ((p->mpeg == 2 && p->found >= 9)  || 
	     (p->mpeg == 1 && p->found >= 7)) ){
		switch (p->cid){
			
		case AUDIO_STREAM_S ... AUDIO_STREAM_E:			
		case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		case PRIVATE_STREAM1:

			memcpy(p->buf, headr, 3);
			p->buf[3] = p->cid;
			memcpy(p->buf+4,p->plen,2);

			if (p->mpeg == 2 && p->found == 9){
				p->buf[6] = p->flag1;
				p->buf[7] = p->flag2;
				p->buf[8] = p->hlength;
			}

			if (p->mpeg == 1 && p->found == 7){
				p->buf[6] = p->flag1;
			}


			if (p->mpeg == 2 && (p->flag2 & PTS_ONLY) &&  
			    p->found < 14){
				while (c < count && p->found < 14){
					p->pts[p->found-9] = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
				}
				if (c == count) return 1;
			}

			if (p->mpeg == 1 && p->which < 2000){

				if (p->found == 7) {
					p->check = p->flag1;
					p->hlength = 1;
				}

				while (!p->which && c < count && 
				       p->check == 0xFF){
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;
				}

				if ( c == count) return 1;
				
				if ( (p->check & 0xC0) == 0x40 && !p->which){
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;

					p->which = 1;
					if ( c == count) return 1;
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;
					p->which = 2;
					if ( c == count) return 1;
				}

				if (p->which == 1){
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;
					p->which = 2;
					if ( c == count) return 1;
				}
				
				if ( (p->check & 0x30) && p->check != 0xFF){
					p->flag2 = (p->check & 0xF0) << 2;
					p->pts[0] = p->check;
					p->which = 3;
				} 

				if ( c == count) return 1;
				if (p->which > 2){
					if ((p->flag2 & PTS_DTS_FLAGS)
					    == PTS_ONLY){
						while (c < count && 
						       p->which < 7){
							p->pts[p->which-2] =
								buf[c];
							p->buf[p->found] = 
								buf[c];
							c++;
							p->found++;
							p->which++;
							p->hlength++;
						}
						if ( c == count) return 1;
					} else if ((p->flag2 & PTS_DTS_FLAGS) 
						   == PTS_DTS){
						while (c < count && 
						       p->which< 12){
							if (p->which< 7)
								p->pts[p->which
								      -2] =
									buf[c];
							p->buf[p->found] = 
								buf[c];
							c++;
							p->found++;
							p->which++;
							p->hlength++;
						}
						if ( c == count) return 1;
					}
					p->which = 2000;
				}
							
			}

			while (c < count && p->found < p->plength+6){
				l = count -c;
				if (l+p->found > p->plength+6)
					l = p->plength+6-p->found;
				memcpy(p->buf+p->found, buf+c, l);
				p->found += l;
				c += l;
			}			
			if(p->found == p->plength+6){
				if (func(p)){ 
					if (p->fd1 >= 0){
						write(p->fd1,p->buf,
						      p->plength+6);
					} 
				} else ret = 0;
			}
			break;
		}


		if ( p->done ){
			if( p->found + count - c < p->plength+6){
				p->found += count-c;
				c = count;
			} else {
				c += p->plength+6 - p->found;
				p->found = p->plength+6;
			}
		}

		if (p->plength && p->found == p->plength+6) {
			p->found = 0;
			p->done = 0;
			p->plength = 0;
			memset(p->buf, 0, MAX_PLENGTH);
			if (c < count)
				return filter_pes(buf+c, count-c, p, func);
		}
	}
	return ret;
}


#define SIZE 4096


int audio_pes_filt(p2p *p)
{
	uint8_t off;

	switch(p->cid){
	case PRIVATE_STREAM1:
		if ( p->cid == p->filter) {
			off = 9+p->buf[8];
			if (p->buf[off] == p->subid){
				return 1;
			}
		}
		break;
	
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:			
		if ( p->cid == p->filter)
			return 1;
		break;

	default:
		return 1;
		break;
	}
	return 0;
}


void filter_audio_from_pes(int fdin, int fdout, uint8_t id, uint8_t subid)
{
		p2p p;
		int count = 1;
		uint8_t buf[2048];

		init_p2p(&p, NULL, 2048);
		p.fd1 = -1;
		p.filter = id;
		p.subid = subid;

		while (count > 0){
			count = read(fdin,buf,2048);
			if(filter_pes(buf,count,&p,audio_pes_filt))
				write(fdout,buf,2048);
		}
}


void pes_filt(p2p *p)
{
	int factor = p->mpeg-1;

	if ( p->cid == p->filter) {
		if (p->es)
			write(p->fd1,p->buf+p->hlength+6+3*factor, 
			      p->plength-p->hlength-3*factor);
		else
			write(p->fd1,p->buf,p->plength+6);
	}
}

void extract_from_pes(int fdin, int fdout, uint8_t id, int es)
{
		p2p p;
		int count = 1;
		uint8_t buf[SIZE];

		init_p2p(&p, NULL, 2048);
		p.fd1 = fdout;
		p.filter = id;
		p.es = es;

		while (count > 0){
			count = read(fdin,buf,SIZE);
			get_pes(buf,count,&p,pes_filt);
		}
}


void pes_dfilt(p2p *p)
{
	int factor = p->mpeg-1;
	int fd =0;
	int head=0;
	int type = NOPES;
	int streamid;
	int c = 6+p->hlength+3*factor;
	

	switch ( p->cid ) {
	case PRIVATE_STREAM1:
		streamid = p->buf[c];
		head = 4; 
		if ((streamid & 0xF8) == 0x80+p->es-1){
			fd = p->fd1;
			type = AC3;
		}
		break;
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:			
		fd = p->fd1;
		type = AUDIO;
		break;
	case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		fd = p->fd2;
		type = VIDEO;
		break;
	}
	
	if (p->es && !p->startv && type == VIDEO){
		int found = 0;
		
		if  ( p->flag2 & PTS_DTS ) 
			p->vpts = trans_pts_dts(p->pts); 
		else return;

		while ( !found && c+3 < p->plength+6 ){
			if ( p->buf[c] == 0x00 && 
			     p->buf[c+1] == 0x00 && 
			     p->buf[c+2] == 0x01 &&
			     p->buf[c+3] == 0xb3) 
				found = 1;
			else c++;
		}
		if (found){
			p->startv = 1;
			write(fd, p->buf+c, p->plength+6-c);
		}
		fd = 0;
	} 

		
	if ( p->es && !p->starta && type == AUDIO){
		int found = 0;
		if  ( p->flag2 & PTS_DTS ) 
			p->apts = trans_pts_dts(p->pts);  
		else return;

		if (p->startv)
			while ( !found && c+1 < p->plength+6){
				if ( p->buf[c] == 0xFF && 
				     (p->buf[c+1] & 0xF8) == 0xF8)
					found = 1;
				else c++;
			}
		if (found){
			p->starta = 1;
			write(fd, p->buf+c, p->plength+6-c);
		}
		fd = 0;
	} 

	if ( p->es && !p->starta && type == AC3){
		if  ( p->flag2 & PTS_DTS ) 
			p->apts = trans_pts_dts(p->pts);  
		else return;

		if (p->startv){
			c+= ((p->buf[c+2] << 8)| p->buf[c+3]);
			p->starta = 1;
			write(fd, p->buf+c, p->plength+6-c);
		}
		fd = 0;
	} 


	if (fd){
		if (p->es)
			write(fd,p->buf+p->hlength+6+3*factor+head, 
			      p->plength-p->hlength-3*factor-head);
		else
			write(fd,p->buf,p->plength+6);
	}
} 


#define PD_SIZE 1024*1024
int64_t pes_dmx( int fdin, int fdouta, int fdoutv, int es)
{
	p2p p;
	int count = 1;
	uint8_t buf[PD_SIZE];
	uint64_t length = 0;
	uint64_t l = 0;
	int verb = 0;
	int percent, oldPercent = -1;
 	
	init_p2p(&p, NULL, MAX_PLENGTH-1);
	p.fd1 = fdouta;
	p.fd2 = fdoutv;
	p.es = es;
	p.startv = 0;
	p.starta = 0;
	p.apts=-1;
	p.vpts=-1;
	
	if (fdin != STDIN_FILENO) verb = 1; 
	
	if (verb) {
		length = lseek(fdin, 0, SEEK_END);
		lseek(fdin,0,SEEK_SET);
	}
	
	while (count > 0){
		count = read(fdin,buf,PD_SIZE);
		l += count;
		if (verb){
			percent = 100 * l / length;
 		
			if (percent != oldPercent) {
				fprintf(stderr, "Demuxing %d %%\r", percent);
				oldPercent = percent;
			}
		}
		get_pes(buf,count,&p,pes_dfilt);
	}
	
	return (int64_t)p.vpts - (int64_t)p.apts;
	
}



static void pes_in_ts(p2p *p)
{
	int l, pes_start;
	uint8_t obuf[TS_SIZE];
	long int c = 0;
	int length = p->plength+6;
	uint16_t pid;
	uint8_t *counter;
	pes_start = 1;
	switch ( p->cid ) {
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:			
		pid = p->pida;
		counter = &p->acounter;
		break;
	case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		pid = p->pidv;
		counter = &p->acounter;

		tspid0[3] |= (p->count0++) 
			& 0x0F ;
		tspid1[3] |= (p->count1++) 
			& 0x0F ;
	
		tspid1[24]  = p->pidv;
		tspid1[23] |= (p->pidv >> 8) & 0x3F;
		tspid1[29]  = p->pida;
		tspid1[28] |= (p->pida >> 8) & 0x3F;
		
		p->func(tspid0,188,p);
		p->func(tspid1,188,p);
		break;
	default:
		return;
	}

	while ( c < length ){
		memset(obuf,0,TS_SIZE);
		if (length - c >= TS_SIZE-4){
			l = write_ts_header(pid, counter, pes_start
					     , obuf, TS_SIZE-4);
			memcpy(obuf+l, p->buf+c, TS_SIZE-l);
			c += TS_SIZE-l;
		} else { 
			l = write_ts_header(pid, counter, pes_start
					     , obuf, length-c);
			memcpy(obuf+l, p->buf+c, TS_SIZE-l);
			c = length;
		}
		p->func(obuf,188,p);
		pes_start = 0;
	}
}

static
void write_out(uint8_t *buf, int count,void  *p)
{
	write(STDOUT_FILENO, buf, count);
}


void pes_to_ts2( int fdin, int fdout, uint16_t pida, uint16_t pidv)
{
	p2p p;
	int count = 1;
	uint8_t buf[SIZE];
	uint64_t length = 0;
	uint64_t l = 0;
	int verb = 0;
	
	init_p2p(&p, NULL, 2048);
	p.fd1 = fdout;
	p.pida = pida;
	p.pidv = pidv;
	p.acounter = 0;
	p.vcounter = 0;
	p.count1 = 0;
	p.count0 = 0;
	p.func = write_out;
		
	if (fdin != STDIN_FILENO) verb = 1; 

	if (verb) {
		length = lseek(fdin, 0, SEEK_END);
		lseek(fdin,0,SEEK_SET);
	}

	while (count > 0){
		count = read(fdin,buf,SIZE);
		l += count;
		if (verb)
			fprintf(stderr,"Writing TS  %2.2f %%\r",
				100.*l/length);

		get_pes(buf,count,&p,pes_in_ts);
	}
		
}


#define IN_SIZE TS_SIZE*10
#define IPACKS 2048
void find_avpids(int fd, uint16_t *vpid, uint16_t *apid)
{
        uint8_t buf[IN_SIZE];
        int count;
        int i;  
        int off =0;

        while ( *apid == 0 || *vpid == 0){
                count = read(fd, buf, IN_SIZE);
                for (i = 0; i < count-7; i++){
                        if (buf[i] == 0x47){
                                if (buf[i+1] & 0x40){
                                        off = 0;
                                        if ( buf[3+i] & 0x20)//adapt field?
                                                off = buf[4+i] + 1;
                                        switch(buf[i+7+off]){
                                        case VIDEO_STREAM_S ... VIDEO_STREAM_E:
                                                *vpid = get_pid(buf+i+1);
                                                break;
                                        case PRIVATE_STREAM1:
                                        case AUDIO_STREAM_S ... AUDIO_STREAM_E:
                                                *apid = get_pid(buf+i+1);
                                                break;
                                        }
                                }
                                i += 187;
                        }
                        if (*apid != 0 && *vpid != 0) break;
                }
        }
}

void find_bavpids(uint8_t *buf, int count, uint16_t *vpid, uint16_t *apid)
{
        int i;  
        int founda = 0;
        int foundb = 0;
        int off = 0;
        
        *vpid = 0;
        *apid = 0;
        for (i = 0; i < count-7; i++){
                if (buf[i] == 0x47){
                        if ((buf[i+1] & 0xF0) == 0x40){
                                off = 0;
                                if ( buf[3+i] & 0x20)  // adaptation field?
                                        off = buf[4+i] + 1;
                                
                                if (buf[off+i+4] == 0x00 && 
                                    buf[off+i+5] == 0x00 &&
                                    buf[off+i+6] == 0x01){
                                        switch(buf[off+i+7]){
                                        case VIDEO_STREAM_S ... VIDEO_STREAM_E:
                                                *vpid = get_pid(buf+i+1);
                                                foundb=1;
                                                break;
                                        case PRIVATE_STREAM1:
                                        case AUDIO_STREAM_S ... AUDIO_STREAM_E:
                                                *apid = get_pid(buf+i+1);
                                                founda=1;
                                                break;
                                        }
                                }
                        }
                        i += 187;
                }
                if (founda && foundb) break;
        }
}


void ts_to_pes( int fdin, uint16_t pida, uint16_t pidv, int ps)
{
	
	uint8_t buf[IN_SIZE];
	uint8_t mbuf[TS_SIZE];
	int i;
	int count = 1;
	uint16_t pid;
	uint16_t dummy;
	ipack pa, pv;
	ipack *p;

	if (fdin != STDIN_FILENO && (!pida || !pidv))
		find_avpids(fdin, &pidv, &pida);

	init_ipack(&pa, IPACKS,write_out, ps);
	init_ipack(&pv, IPACKS,write_out, ps);

 	if ((count = save_read(fdin,mbuf,TS_SIZE))<0)
	    perror("reading");

	for ( i = 0; i < 188 ; i++){
		if ( mbuf[i] == 0x47 ) break;
	}
	if ( i == 188){
		fprintf(stderr,"Not a TS\n");
		return;
	} else {
		memcpy(buf,mbuf+i,TS_SIZE-i);
		if ((count = save_read(fdin,mbuf,i))<0)
			perror("reading");
		memcpy(buf+TS_SIZE-i,mbuf,i);
		i = 188;
	}
	count = 1;
	while (count > 0){
		if ((count = save_read(fdin,buf+i,IN_SIZE-i)+i)<0)
			perror("reading");
		

		if (!pidv){
                        find_bavpids(buf+i, IN_SIZE-i, &pidv, &dummy);
                        if (pidv) fprintf(stderr, "vpid %d (0x%02x)\n",
					  pidv,pidv);
                } 

                if (!pida){
                        find_bavpids(buf+i, IN_SIZE-i, &dummy, &pida);
                        if (pida) fprintf(stderr, "apid %d (0x%02x)\n",
					  pida,pida);
                } 


		for( i = 0; i < count; i+= TS_SIZE){
			uint8_t off = 0;

			if ( count - i < TS_SIZE) break;

			pid = get_pid(buf+i+1);
			if (!(buf[3+i]&0x10)) // no payload?
				continue;
			if ( buf[1+i]&0x80){
				fprintf(stderr,"Error in TS for PID: %d\n", 
					pid);
			}
			if (pid == pidv){
				p = &pv;
			} else {
				if (pid == pida){
					p = &pa;
				} else continue;
			}

			if ( buf[1+i]&0x40) {
				if (p->plength == MMAX_PLENGTH-6){
					p->plength = p->found-6;
					p->found = 0;
					send_ipack(p);
					reset_ipack(p);
				}
			}

			if ( buf[3+i] & 0x20) {  // adaptation field?
				off = buf[4+i] + 1;
			}
        
			instant_repack(buf+4+off+i, TS_SIZE-4-off, p);
		}
		i = 0;

	}

}


#define INN_SIZE 2*IN_SIZE
void insert_pat_pmt( int fdin, int fdout)
{
	
	uint8_t buf[INN_SIZE];
	uint8_t mbuf[TS_SIZE];
	int i;
	int count = 1;
	uint16_t pida = 0;
	uint16_t pidv = 0;
	int written,c;
	uint8_t c0 = 0;
	uint8_t c1 = 0;
        uint8_t pmt_len;
        uint32_t crc32;


	find_avpids(fdin, &pidv, &pida);
	
 	count = save_read(fdin,mbuf,TS_SIZE);
	for ( i = 0; i < 188 ; i++){
		if ( mbuf[i] == 0x47 ) break;
	}
	if ( i == 188){
		fprintf(stderr,"Not a TS\n");
		return;
	} else {
		memcpy(buf,mbuf+i,TS_SIZE-i);
		count = save_read(fdin,mbuf,i);
		memcpy(buf+TS_SIZE-i,mbuf,i);
		i = 188;
	}
	
	count = 1;
        /* length is not correct, but we only create a very small
         * PMT, so it doesn't matter :-)
         */
        pmt_len = tspid1[7] + 3;
	while (count > 0){
		tspid1[24]  = pidv;
		tspid1[23] |= (pidv >> 8) & 0x3F;
		tspid1[29]  = pida;
		tspid1[28] |= (pida >> 8) & 0x3F;
                crc32 = calc_crc32 (&tspid1[5], pmt_len - 4);
                tspid1[5 + pmt_len - 4] = (crc32 & 0xff000000) >> 24;
                tspid1[5 + pmt_len - 3] = (crc32 & 0x00ff0000) >> 16;
                tspid1[5 + pmt_len - 2] = (crc32 & 0x0000ff00) >>  8;
                tspid1[5 + pmt_len - 1] = (crc32 & 0x000000ff) >>  0;
		
		write(fdout,tspid0,188);
		write(fdout,tspid1,188);

		count = save_read(fdin,buf+i,INN_SIZE-i);
		
		written = 0;
		while (written < IN_SIZE){
			c = write(fdout,buf,INN_SIZE);
			if (c>0) written += c;
		}
		tspid0[3] &= 0xF0 ;
		tspid0[3] |= (c0++)& 0x0F ;

		tspid1[3] &= 0xF0 ;
		tspid1[3] |= (c1++)& 0x0F ;
	
		i=0;
	}

}

void get_pes (uint8_t *buf, int count, p2p *p, void (*func)(p2p *p))
{

	int l;
	unsigned short *pl;
	int c=0;

	uint8_t headr[3] = { 0x00, 0x00, 0x01} ;

	while (c < count && (p->mpeg == 0 ||
			     (p->mpeg == 1 && p->found < 7) ||
			     (p->mpeg == 2 && p->found < 9))
	       &&  (p->found < 5 || !p->done)){
		switch ( p->found ){
		case 0:
		case 1:
			if (buf[c] == 0x00) p->found++;
			else p->found = 0;
			c++;
			break;
		case 2:
			if (buf[c] == 0x01) p->found++;
			else if (buf[c] == 0){
				p->found = 2;
			} else p->found = 0;
			c++;
			break;
		case 3:
			p->cid = 0;
			switch (buf[c]){
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
				p->done = 1;
			case PRIVATE_STREAM1:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
				p->found++;
				p->cid = buf[c];
				c++;
				break;
			default:
				p->found = 0;
				break;
			}
			break;
			

		case 4:
			if (count-c > 1){
				pl = (unsigned short *) (buf+c);
				p->plength =  ntohs(*pl);
				p->plen[0] = buf[c];
				c++;
				p->plen[1] = buf[c];
				c++;
				p->found+=2;
			} else {
				p->plen[0] = buf[c];
				p->found++;
				return;
			}
			break;
		case 5:
			p->plen[1] = buf[c];
			c++;
			pl = (unsigned short *) p->plen;
			p->plength = ntohs(*pl);
			p->found++;
			break;


		case 6:
			if (!p->done){
				p->flag1 = buf[c];
				c++;
				p->found++;
				if ( (p->flag1 & 0xC0) == 0x80 ) p->mpeg = 2;
				else {
					p->hlength = 0;
					p->which = 0;
					p->mpeg = 1;
					p->flag2 = 0;
				}
			}
			break;

		case 7:
			if ( !p->done && p->mpeg == 2){
				p->flag2 = buf[c];
				c++;
				p->found++;
			}	
			break;

		case 8:
			if ( !p->done && p->mpeg == 2){
				p->hlength = buf[c];
				c++;
				p->found++;
			}
			break;
			
		default:

			break;
		}
	}

	if (!p->plength) p->plength = MMAX_PLENGTH-6;


	if ( p->done || ((p->mpeg == 2 && p->found >= 9)  || 
	     (p->mpeg == 1 && p->found >= 7)) ){
		switch (p->cid){
			
		case AUDIO_STREAM_S ... AUDIO_STREAM_E:			
		case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		case PRIVATE_STREAM1:

			memcpy(p->buf, headr, 3);
			p->buf[3] = p->cid;
			memcpy(p->buf+4,p->plen,2);

			if (p->mpeg == 2 && p->found == 9){
				p->buf[6] = p->flag1;
				p->buf[7] = p->flag2;
				p->buf[8] = p->hlength;
			}

			if (p->mpeg == 1 && p->found == 7){
				p->buf[6] = p->flag1;
			}


			if (p->mpeg == 2 && (p->flag2 & PTS_ONLY) &&  
			    p->found < 14){
				while (c < count && p->found < 14){
					p->pts[p->found-9] = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
				}
				if (c == count) return;
			}

			if (p->mpeg == 1 && p->which < 2000){

				if (p->found == 7) {
					p->check = p->flag1;
					p->hlength = 1;
				}

				while (!p->which && c < count && 
				       p->check == 0xFF){
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;
				}

				if ( c == count) return;
				
				if ( (p->check & 0xC0) == 0x40 && !p->which){
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;

					p->which = 1;
					if ( c == count) return;
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;
					p->which = 2;
					if ( c == count) return;
				}

				if (p->which == 1){
					p->check = buf[c];
					p->buf[p->found] = buf[c];
					c++;
					p->found++;
					p->hlength++;
					p->which = 2;
					if ( c == count) return;
				}
				
				if ( (p->check & 0x30) && p->check != 0xFF){
					p->flag2 = (p->check & 0xF0) << 2;
					p->pts[0] = p->check;
					p->which = 3;
				} 

				if ( c == count) return;
				if (p->which > 2){
					if ((p->flag2 & PTS_DTS_FLAGS)
					    == PTS_ONLY){
						while (c < count && 
						       p->which < 7){
							p->pts[p->which-2] =
								buf[c];
							p->buf[p->found] = 
								buf[c];
							c++;
							p->found++;
							p->which++;
							p->hlength++;
						}
						if ( c == count) return;
					} else if ((p->flag2 & PTS_DTS_FLAGS) 
						   == PTS_DTS){
						while (c < count && 
						       p->which< 12){
							if (p->which< 7)
								p->pts[p->which
								      -2] =
									buf[c];
							p->buf[p->found] = 
								buf[c];
							c++;
							p->found++;
							p->which++;
							p->hlength++;
						}
						if ( c == count) return;
					}
					p->which = 2000;
				}
							
			}

			while (c < count && p->found < p->plength+6){
				l = count -c;
				if (l+p->found > p->plength+6)
					l = p->plength+6-p->found;
				memcpy(p->buf+p->found, buf+c, l);
				p->found += l;
				c += l;
			}			
			if(p->found == p->plength+6)
				func(p);
			
			break;
		}


		if ( p->done ){
			if( p->found + count - c < p->plength+6){
				p->found += count-c;
				c = count;
			} else {
				c += p->plength+6 - p->found;
				p->found = p->plength+6;
			}
		}

		if (p->plength && p->found == p->plength+6) {
			p->found = 0;
			p->done = 0;
			p->plength = 0;
			memset(p->buf, 0, MAX_PLENGTH);
			if (c < count)
				get_pes(buf+c, count-c, p, func);
		}
	}
	return;
}




void setup_pes2ts( p2p *p, uint32_t pida, uint32_t pidv, 
		   void (*ts_write)(uint8_t *buf, int count, void *p))
{
	init_p2p( p, ts_write, 2048);
	p->pida = pida;
	p->pidv = pidv;
	p->acounter = 0;
	p->vcounter = 0;
	p->count1 = 0;
	p->count0 = 0;
}

void kpes_to_ts( p2p *p,uint8_t *buf ,int count )
{
	get_pes(buf,count, p,pes_in_ts);
}


void setup_ts2pes( p2p *pa, p2p *pv, uint32_t pida, uint32_t pidv, 
		   void (*pes_write)(uint8_t *buf, int count, void *p))
{
	init_p2p( pa, pes_write, 2048);
	init_p2p( pv, pes_write, 2048);
	pa->pid = pida;
	pv->pid = pidv;
}

void kts_to_pes( p2p *p, uint8_t *buf) // don't need count (=188)
{
	uint8_t off = 0;
	uint16_t pid = 0;

	if (!(buf[3]&PAYLOAD)) // no payload?
		return;

	pid = get_pid(buf+1);
			
	if (pid != p->pid) return;
	if ( buf[1]&0x80){
		fprintf(stderr,"Error in TS for PID: %d\n", 
			pid);
	}

	if ( buf[1]&PAY_START) {
		if (p->plength == MMAX_PLENGTH-6){
			p->plength = p->found-6;
			p->found = 0;
			pes_repack(p);
		}
	}

	if ( buf[3] & ADAPT_FIELD) {  // adaptation field?
		off = buf[4] + 1;
		if (off+4 > 187) return;
	}
        
	get_pes(buf+4+off, TS_SIZE-4-off, p , pes_repack);
}




// instant repack


void reset_ipack(ipack *p)
{
	p->found = 0;
	p->cid = 0;
	p->plength = 0;
	p->flag1 = 0;
	p->flag2 = 0;
	p->hlength = 0;
	p->mpeg = 0;
	p->check = 0;
	p->which = 0;
	p->done = 0;
	p->count = 0;
	p->size = p->size_orig;
}

void init_ipack(ipack *p, int size,
		void (*func)(uint8_t *buf,  int size, void *priv), int ps)
{
	if ( !(p->buf = malloc(size)) ){
		fprintf(stderr,"Couldn't allocate memory for ipack\n");
		exit(1);
	}
	p->ps = ps;
	p->size_orig = size;
	p->func = func;
	reset_ipack(p);
	p->has_ai = 0;
	p->has_vi = 0;
	p->start = 0;
	p->muxr = 0;
	p->start_header = 0;
	p->vi.bit_rate = 0;
	p->ai.bit_rate = 0;
}

void free_ipack(ipack * p)
{
	if (p->buf) free(p->buf);
}



int get_vinfo(uint8_t *mbuf, int count, VideoInfo *vi, int pr)
{
	uint8_t *headr;
	int found = 0;
        int sw;
	int form = -1;
	int c = 0;

	while (found < 4 && c+4 < count){
		uint8_t *b;

		b = mbuf+c;
		if ( b[0] == 0x00 && b[1] == 0x00 && b[2] == 0x01
		     && b[3] == 0xb3) found = 4;
		else {
			c++;
		}
	}

	if (! found) return -1;
	c += 4;
	if (c+12 >= count) return -1;
	headr = mbuf+c;

	vi->horizontal_size	= ((headr[1] &0xF0) >> 4) | (headr[0] << 4);
	vi->vertical_size	= ((headr[1] &0x0F) << 8) | (headr[2]);
    
        sw = (int)((headr[3]&0xF0) >> 4) ;

        switch( sw ){
	case 1:
		if (pr)
			fprintf(stderr,"Videostream: ASPECT: 1:1");
		vi->aspect_ratio = 100;        
		break;
	case 2:
		if (pr)
			fprintf(stderr,"Videostream: ASPECT: 4:3");
                vi->aspect_ratio = 133;        
		break;
	case 3:
		if (pr)
			fprintf(stderr,"Videostream: ASPECT: 16:9");
                vi->aspect_ratio = 177;        
		break;
	case 4:
		if (pr)
			fprintf(stderr,"Videostream: ASPECT: 2.21:1");
                vi->aspect_ratio = 221;        
		break;

        case 5 ... 15:
		if (pr)
			fprintf(stderr,"Videostream: ASPECT: reserved");
                vi->aspect_ratio = 0;        
		break;

        default:
                vi->aspect_ratio = 0;        
                return -1;
	}

	if (pr)
		fprintf(stderr,"  Size = %dx%d",vi->horizontal_size,
			vi->vertical_size);

        sw = (int)(headr[3]&0x0F);

        switch ( sw ) {
	case 1:
		if (pr)
			fprintf(stderr,"  FRate: 23.976 fps");
                vi->framerate = 24000/1001.;
		form = -1;
		break;
	case 2:
		if (pr)
			fprintf(stderr,"  FRate: 24 fps");
                vi->framerate = 24;
		form = -1;
		break;
	case 3:
		if (pr)
			fprintf(stderr,"  FRate: 25 fps");
                vi->framerate = 25;
		form = VIDEO_MODE_PAL;
		break;
	case 4:
		if (pr)
			fprintf(stderr,"  FRate: 29.97 fps");
                vi->framerate = 30000/1001.;
		form = VIDEO_MODE_NTSC;
		break;
	case 5:
		if (pr)
			fprintf(stderr,"  FRate: 30 fps");
                vi->framerate = 30;
		form = VIDEO_MODE_NTSC;
		break;
	case 6:
		if (pr)
			fprintf(stderr,"  FRate: 50 fps");
                vi->framerate = 50;
		form = VIDEO_MODE_PAL;
		break;
	case 7:
		if (pr)
			fprintf(stderr,"  FRate: 60 fps");
                vi->framerate = 60;
		form = VIDEO_MODE_NTSC;
		break;
	}

	vi->bit_rate = 400*(((headr[4] << 10) & 0x0003FC00UL) 
			    | ((headr[5] << 2) & 0x000003FCUL) | 
			    (((headr[6] & 0xC0) >> 6) & 0x00000003UL));
	
	if (pr){
		fprintf(stderr,"  BRate: %.2f Mbit/s",(vi->bit_rate)/1000000.);
		fprintf(stderr,"\n");
	}
        vi->video_format = form;

	vi->off = c-4;
	return c-4;
}

extern unsigned int bitrates[3][16];
extern uint32_t freq[4];

int get_ainfo(uint8_t *mbuf, int count, AudioInfo *ai, int pr)
{
	uint8_t *headr;
	int found = 0;
	int c = 0;
	int fr =0;
	
	while (!found && c < count){
		uint8_t *b = mbuf+c;

		if ( b[0] == 0xff && (b[1] & 0xf8) == 0xf8)
			found = 1;
		else {
			c++;
		}
	}	

	if (!found) return -1;

	if (c+3 >= count) return -1;
        headr = mbuf+c;

	ai->layer = (headr[1] & 0x06) >> 1;

        if (pr)
		fprintf(stderr,"Audiostream: Layer: %d", 4-ai->layer);


	ai->bit_rate = bitrates[(3-ai->layer)][(headr[2] >> 4 )]*1000;

	if (pr){
		if (ai->bit_rate == 0)
			fprintf (stderr,"  Bit rate: free");
		else if (ai->bit_rate == 0xf)
			fprintf (stderr,"  BRate: reserved");
		else
			fprintf (stderr,"  BRate: %d kb/s", ai->bit_rate/1000);
	}

	fr = (headr[2] & 0x0c ) >> 2;
	ai->frequency = freq[fr]*100;
	
	if (pr){
		if (ai->frequency == 3)
			fprintf (stderr, "  Freq: reserved\n");
		else
			fprintf (stderr,"  Freq: %2.1f kHz\n", 
				 ai->frequency/1000.);
	}
	ai->off = c;
	return c;
}

unsigned int ac3_bitrates[32] =
    {32,40,48,56,64,80,96,112,128,160,192,224,256,320,384,448,512,576,640,
     0,0,0,0,0,0,0,0,0,0,0,0,0};

uint32_t ac3_freq[4] = {480, 441, 320, 0};
uint32_t ac3_frames[3][32] =
    {{64,80,96,112,128,160,192,224,256,320,384,448,512,640,768,896,1024,
      1152,1280,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {69,87,104,121,139,174,208,243,278,348,417,487,557,696,835,975,1114,
      1253,1393,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {96,120,144,168,192,240,288,336,384,480,576,672,768,960,1152,1344,
      1536,1728,1920,0,0,0,0,0,0,0,0,0,0,0,0,0}}; 

int get_ac3info(uint8_t *mbuf, int count, AudioInfo *ai, int pr)
{
	uint8_t *headr;
	int found = 0;
	int c = 0;
	uint8_t frame;
	int fr = 0;

	while ( !found  && c < count){
		uint8_t *b = mbuf+c;
		if ( b[0] == 0x0b &&  b[1] == 0x77 )
			found = 1;
		else {
			c++;
		}
	}	


	if (!found){
		return -1;
	}
	ai->off = c;

	if (c+5 >= count) return -1;

	ai->layer = 0;  // 0 for AC3
        headr = mbuf+c+2;

	frame = (headr[2]&0x3f);
	ai->bit_rate = ac3_bitrates[frame>>1]*1000;

	if (pr) fprintf (stderr,"  BRate: %d kb/s", ai->bit_rate/1000);

	fr = (headr[2] & 0xc0 ) >> 6;
	ai->frequency = freq[fr]*100;
	if (pr) fprintf (stderr,"  Freq: %d Hz\n", ai->frequency);

	ai->framesize = ac3_frames[fr][frame >> 1];
	if ((frame & 1) &&  (fr == 1)) ai->framesize++;
	ai->framesize = ai->framesize << 1;
	if (pr) fprintf (stderr,"  Framesize %d\n", ai->framesize);

	return c;
}


void ps_pes(ipack *p)
{
	int check;
	uint8_t pbuf[PS_HEADER_L2];
	uint32_t SCR = 0;
	ipack *pv, *pa;

	pv = pa = p;
	//fprintf(stderr, "PS_PES: MPEG %d ID 0x%2x\n", p->mpeg, p->buf[3]);
	if (p->mpeg == 2){
		switch(p->buf[3]){
		case VIDEO_STREAM_S ... VIDEO_STREAM_E:
			if (p->pa)
				pa = p->pa;
			if (!p->has_vi){
				if(get_vinfo(p->buf, p->count, &p->vi,1) >=0) {
					p->has_vi = 1;
				}
			} 			
			break;

		case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			if (p->pv)
				pv = p->pv;
			if (!p->has_ai){
				if(get_ainfo(p->buf, p->count, &p->ai,1) >=0) {
					p->has_ai = 1;
				}
			} 
			break;
		}

		if (pv->has_vi && pv->vi.bitrate && !pv->muxr){
			pv->muxr = (pv->vi.bit_rate+pa->ai.bitrate)/400;
		}
		//fprintf(stderr, "PS_PES: start %d muxr %d vi %d b7 0x%2x has_ai %d\n",
		//		p->start_header, p->muxr, p->vi.bit_rate, p->buf[7], p->has_ai);

		if ( pv->start_header && pv->muxr && (p->buf[7] & PTS_ONLY) && (p->has_ai ||
				       p->buf[9+p->buf[8]+4] == 0xb3)){  
			SCR = trans_pts_dts(p->pts)-3600;
			
			check = write_ps_header(pbuf,
						SCR,
						pv->muxr, 1, 0, 0, 1, 1, 1, 
						0, 0, 0, 0, 0, 0);

			p->func(pbuf, check , p->data);
		}

		if (pv->muxr && !pv->start_header && pv->vi.bit_rate){
			SCR = trans_pts_dts(p->pts)-3600;
			check = write_ps_header(pbuf,
						SCR, 
						pv->muxr, 1, 0, 0, 1, 1, 1, 
						0xC0, 0, 64, 0xE0, 1, 460);
			pv->start_header = 1;
			p->func(pbuf, check , p->data);
		}

		if (pv->start_header)
			p->func(p->buf, p->count, p->data);
	}
}

void send_ipack(ipack *p)
{
	int streamid=0;
	int off;
	int ac3_off = 0;
	AudioInfo ai;
	int nframes= 0;
	int f=0;

	if (p->count < 10) return;
	p->buf[3] = p->cid;
	p->buf[4] = (uint8_t)(((p->count-6) & 0xFF00) >> 8);
	p->buf[5] = (uint8_t)((p->count-6) & 0x00FF);

	
	if (p->cid == PRIVATE_STREAM1){

		off = 9+p->buf[8];
		streamid = p->buf[off];
		if ((streamid & 0xF8) == 0x80){
			ai.off = 0;
			ac3_off = ((p->buf[off+2] << 8)| p->buf[off+3]);
			if (ac3_off < p->count)
				f=get_ac3info(p->buf+off+3+ac3_off, 
					      p->count-ac3_off, &ai,0);
			if ( !f ){
				nframes = (p->count-off-3-ac3_off)/ 
					ai.framesize + 1;
				p->buf[off+1] = nframes;
				p->buf[off+2] = (ac3_off >> 8)& 0xFF;
				p->buf[off+3] = (ac3_off)& 0xFF;
				
				ac3_off +=  nframes * ai.framesize - p->count;
			}
		}
	} 
	
	if (p->ps) ps_pes(p);
	else p->func(p->buf, p->count, p->data);

	switch ( p->mpeg ){
	case 2:		
		
		p->buf[6] = 0x80;
		p->buf[7] = 0x00;
		p->buf[8] = 0x00;
		p->count = 9;

		if (p->cid == PRIVATE_STREAM1 && (streamid & 0xF8)==0x80 ){
			p->count += 4;
			p->buf[9] = streamid;
			p->buf[10] = 0;
			p->buf[11] = (ac3_off >> 8)& 0xFF;
			p->buf[12] = (ac3_off)& 0xFF;
		}
		
		break;
	case 1:
		p->buf[6] = 0x0F;
		p->count = 7;
		break;
	}

}


static void write_ipack(ipack *p, uint8_t *data, int count)
{
	AudioInfo ai;
	uint8_t headr[3] = { 0x00, 0x00, 0x01} ;
	int diff =0;

	if (p->count < 6){
		if (trans_pts_dts(p->pts) > trans_pts_dts(p->last_pts))
			memcpy(p->last_pts, p->pts, 5);
		p->count = 0;
		memcpy(p->buf+p->count, headr, 3);
		p->count += 6;
	}
	if ( p->size == p->size_orig && p->plength &&
	     (diff = 6+p->plength - p->found + p->count +count) > p->size &&
	     diff < 3*p->size/2){
		
			p->size = diff/2;
//			fprintf(stderr,"size: %d \n",p->size);
	}

	if (p->cid == PRIVATE_STREAM1 && p->count == p->hlength+9){
		if ((data[0] & 0xF8) != 0x80){
			int ac3_off;

			ac3_off = get_ac3info(data, count, &ai,0);
			if (ac3_off>=0 && ai.framesize){
				p->buf[p->count] = 0x80;
				p->buf[p->count+1] = (p->size - p->count
						      - 4 - ac3_off)/ 
					ai.framesize + 1;
				p->buf[p->count+2] = (ac3_off >> 8)& 0xFF;
				p->buf[p->count+3] = (ac3_off)& 0xFF;
				p->count+=4;
				
			}
		}
	}

	if (p->count + count < p->size){
		memcpy(p->buf+p->count, data, count); 
		p->count += count;
	} else {
		int rest = p->size - p->count;
		if (rest < 0) rest = 0;
		memcpy(p->buf+p->count, data, rest);
		p->count += rest;
//		fprintf(stderr,"count: %d \n",p->count);
		send_ipack(p);
		if (count - rest > 0)
			write_ipack(p, data+rest, count-rest);
	}
}

void instant_repack (uint8_t *buf, int count, ipack *p)
{

	int l;
	unsigned short *pl;
	int c=0;

	while (c < count && (p->mpeg == 0 ||
			     (p->mpeg == 1 && p->found < 7) ||
			     (p->mpeg == 2 && p->found < 9))
	       &&  (p->found < 5 || !p->done)){
		switch ( p->found ){
		case 0:
		case 1:
			if (buf[c] == 0x00) p->found++;
			else p->found = 0;
			c++;
			break;
		case 2:
			if (buf[c] == 0x01) p->found++;
			else if (buf[c] == 0){
				p->found = 2;
			} else p->found = 0;
			c++;
			break;
		case 3:
			p->cid = 0;
			switch (buf[c]){
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
				p->done = 1;
			case PRIVATE_STREAM1:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
				p->found++;
				p->cid = buf[c];
				c++;
				break;
			default:
				p->found = 0;
				break;
			}
			break;
			

		case 4:
			if (count-c > 1){
				pl = (unsigned short *) (buf+c);
				p->plength =  ntohs(*pl);
				p->plen[0] = buf[c];
				c++;
				p->plen[1] = buf[c];
				c++;
				p->found+=2;
			} else {
				p->plen[0] = buf[c];
				p->found++;
				return;
			}
			break;
		case 5:
			p->plen[1] = buf[c];
			c++;
			pl = (unsigned short *) p->plen;
			p->plength = ntohs(*pl);
			p->found++;
			break;


		case 6:
			if (!p->done){
				p->flag1 = buf[c];
				c++;
				p->found++;
				if ( (p->flag1 & 0xC0) == 0x80 ) p->mpeg = 2;
				else {
					p->hlength = 0;
					p->which = 0;
					p->mpeg = 1;
					p->flag2 = 0;
				}
			}
			break;

		case 7:
			if ( !p->done && p->mpeg == 2){
				p->flag2 = buf[c];
				c++;
				p->found++;
			}	
			break;

		case 8:
			if ( !p->done && p->mpeg == 2){
				p->hlength = buf[c];
				c++;
				p->found++;
			}
			break;
			
		default:

			break;
		}
	}


	if (c == count) return;

	if (!p->plength) p->plength = MMAX_PLENGTH-6;


	if ( p->done || ((p->mpeg == 2 && p->found >= 9)  || 
	     (p->mpeg == 1 && p->found >= 7)) ){
		switch (p->cid){
			
		case AUDIO_STREAM_S ... AUDIO_STREAM_E:			
		case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		case PRIVATE_STREAM1:
			
			if (p->mpeg == 2 && p->found == 9){
				write_ipack(p, &p->flag1, 1);
				write_ipack(p, &p->flag2, 1);
				write_ipack(p, &p->hlength, 1);
			}

			if (p->mpeg == 1 && p->found == 7){
				write_ipack(p, &p->flag1, 1);
			}


			if (p->mpeg == 2 && (p->flag2 & PTS_ONLY) &&  
			    p->found < 14){
				while (c < count && p->found < 14){
					p->pts[p->found-9] = buf[c];
					write_ipack(p, buf+c, 1);
					c++;
					p->found++;
				}
				if (c == count) return;
			}
			
			if (p->mpeg == 1 && p->which < 2000){

				if (p->found == 7) {
					p->check = p->flag1;
					p->hlength = 1;
				}

				while (!p->which && c < count && 
				       p->check == 0xFF){
					p->check = buf[c];
					write_ipack(p, buf+c, 1);
					c++;
					p->found++;
					p->hlength++;
				}

				if ( c == count) return;
				
				if ( (p->check & 0xC0) == 0x40 && !p->which){
					p->check = buf[c];
					write_ipack(p, buf+c, 1);
					c++;
					p->found++;
					p->hlength++;

					p->which = 1;
					if ( c == count) return;
					p->check = buf[c];
					write_ipack(p, buf+c, 1);
					c++;
					p->found++;
					p->hlength++;
					p->which = 2;
					if ( c == count) return;
				}

				if (p->which == 1){
					p->check = buf[c];
					write_ipack(p, buf+c, 1);
					c++;
					p->found++;
					p->hlength++;
					p->which = 2;
					if ( c == count) return;
				}
				
				if ( (p->check & 0x30) && p->check != 0xFF){
					p->flag2 = (p->check & 0xF0) << 2;
					p->pts[0] = p->check;
					p->which = 3;
				} 

				if ( c == count) return;
				if (p->which > 2){
					if ((p->flag2 & PTS_DTS_FLAGS)
					    == PTS_ONLY){
						while (c < count && 
						       p->which < 7){
							p->pts[p->which-2] =
								buf[c];
							write_ipack(p,buf+c,1);
							c++;
							p->found++;
							p->which++;
							p->hlength++;
						}
						if ( c == count) return;
					} else if ((p->flag2 & PTS_DTS_FLAGS) 
						   == PTS_DTS){
						while (c < count && 
						       p->which< 12){
							if (p->which< 7)
								p->pts[p->which
								      -2] =
									buf[c];
							write_ipack(p,buf+c,1);
							c++;
							p->found++;
							p->which++;
							p->hlength++;
						}
						if ( c == count) return;
					}
					p->which = 2000;
				}
							
			}

			while (c < count && p->found < p->plength+6){
				l = count -c;
				if (l+p->found > p->plength+6)
					l = p->plength+6-p->found;
				write_ipack(p, buf+c, l);
				p->found += l;
				c += l;
			}	
		
			break;
		}


		if ( p->done ){
			if( p->found + count - c < p->plength+6){
				p->found += count-c;
				c = count;
			} else {
				c += p->plength+6 - p->found;
				p->found = p->plength+6;
			}
		}

		if (p->plength && p->found == p->plength+6) {
			send_ipack(p);
			reset_ipack(p);
			if (c < count)
				instant_repack(buf+c, count-c, p);
		}
	}
	return;
}

void write_out_es(uint8_t *buf, int count,void  *priv)
{
	ipack *p = (ipack *) priv;
	uint8_t payl = buf[8]+9+p->start-1;

	write(p->fd, buf+payl, count-payl);
	p->start = 1;
}

void write_out_pes(uint8_t *buf, int count,void  *priv)
{
	ipack *p = (ipack *) priv;
	write(p->fd, buf, count);
}



int64_t ts_demux(int fdin, int fdv_out,int fda_out,uint16_t pida,
		  uint16_t pidv, int es)
{
	uint8_t buf[IN_SIZE];
	uint8_t mbuf[TS_SIZE];
	int i;
	int count = 1;
	uint16_t pid;
	ipack pa, pv;
	ipack *p;
	uint8_t *sb;
	int64_t apts=0;
	int64_t apos=0;
	int64_t vpts=0;
	int64_t vpos=0;
	int verb = 0;
	uint64_t length =0;
	uint64_t l=0;
	int perc =0;
	int last_perc =0;

	if (fdin != STDIN_FILENO) verb = 1; 

	if (verb) {
		length = lseek(fdin, 0, SEEK_END);
		lseek(fdin,0,SEEK_SET);
	}

	if (!pida || !pidv)
		find_avpids(fdin, &pidv, &pida);

	if (es){
		init_ipack(&pa, MAX_PLENGTH-1,write_out_es, 0);
		init_ipack(&pv, MAX_PLENGTH-1,write_out_es, 0);
	} else {
		init_ipack(&pa, IPACKS,write_out_pes, 0);
		init_ipack(&pv, IPACKS,write_out_pes, 0);
	} 
	pa.fd = fda_out;
	pv.fd = fdv_out;
	pa.data = (void *)&pa;
	pv.data = (void *)&pv;

 	count = save_read(fdin,mbuf,TS_SIZE);
	if (count) l+=count;
	for ( i = 0; i < 188 ; i++){
		if ( mbuf[i] == 0x47 ) break;
	}
	if ( i == 188){
		fprintf(stderr,"Not a TS\n");
		return 0;
	} else {
		memcpy(buf,mbuf+i,TS_SIZE-i);
		count = save_read(fdin,mbuf,i);
		if (count) l+=count;
		memcpy(buf+TS_SIZE-i,mbuf,i);
		i = 188;
	}
	
	count = 1;
	while (count > 0){
		count = save_read(fdin,buf+i,IN_SIZE-i)+i;
		if (count) l+=count;
		if (verb && perc >last_perc){
			perc = (100*l)/length;
			fprintf(stderr,"Reading TS  %d %%\r",perc);
			last_perc = perc;
		}
		
		for( i = 0; i < count; i+= TS_SIZE){
			uint8_t off = 0;

			if ( count - i < TS_SIZE) break;

			pid = get_pid(buf+i+1);
			if (!(buf[3+i]&0x10)) // no payload?
				continue;
			if ( buf[1+i]&0x80){
				fprintf(stderr,"Error in TS for PID: %d\n", 
					pid);
			}
			if (pid == pidv){
				p = &pv;
			} else {
				if (pid == pida){
					p = &pa;
				} else continue;
			}

			if ( buf[3+i] & 0x20) {  // adaptation field?
				off = buf[4+i] + 1;
			}

			if ( buf[1+i]&0x40) {
				if (p->plength == MMAX_PLENGTH-6){
					p->plength = p->found-6;
					p->found = 0;
					send_ipack(p);
					reset_ipack(p);
				}
				sb = buf+4+off+i;
				if( es && 
				    !p->start && (sb[7] & PTS_DTS_FLAGS)){
					uint8_t *pay = sb+sb[8]+9; 
					int l = TS_SIZE - 13 - off - sb[8];
					if ( pid == pidv &&   
					     (p->start = 
					      get_vinfo( pay, l,&p->vi,1)+1) >0
						){
						vpts = trans_pts_dts(sb+9);
						vpos = (uint64_t)(l-count+pay);
						printf("vpts : %fs\n",
						       vpts/90000.);
					}
					if ( pid == pida && es==1 && 
					     (p->start = 
					      get_ainfo( pay, l,&p->ai,1)+1) >0
						){
						apts = trans_pts_dts(sb+9);
						printf("apts : %fs\n",
						       apts/90000.);
					}
					if ( pid == pida && es==2 && 
					     (p->start = 
					      get_ac3info( pay, l,&p->ai,1)+1) >0
						){
						apts = trans_pts_dts(sb+9);
						apos = (uint64_t)(l-count+pay);
						printf("apts : %fs\n",
						       apts/90000.);
					}
				}
			}

			if (p->start)
				instant_repack(buf+4+off+i, TS_SIZE-4-off, p);
		}
		i = 0;

	}

	fprintf(stderr, "VPOS-APOS %d",(int)(vpos-apos));
	return (vpts-apts);
}


void ts2es_opt(int fdin,  uint16_t pidv, ipack *p, int verb)
{
	uint8_t buf[IN_SIZE];
	uint8_t mbuf[TS_SIZE];
	int i;
	int count = 1;
	uint64_t length =0;
	uint64_t l=0;
	int perc =0;
	int last_perc =0;
	uint16_t pid;

	if (verb) {
		length = lseek(fdin, 0, SEEK_END);
		lseek(fdin,0,SEEK_SET);
	}

 	count = save_read(fdin,mbuf,TS_SIZE);
	if (count) l+=count;
	for ( i = 0; i < 188 ; i++){
		if ( mbuf[i] == 0x47 ) break;
	}
	if ( i == 188){
		fprintf(stderr,"Not a TS\n");
		return;
	} else {
		memcpy(buf,mbuf+i,TS_SIZE-i);
		count = save_read(fdin,mbuf,i);
		if (count) l+=count;
		memcpy(buf+TS_SIZE-i,mbuf,i);
		i = 188;
	}
	
	count = 1;
	while (count > 0){
		count = save_read(fdin,buf+i,IN_SIZE-i)+i;
		if (count) l+=count;
		if (verb && perc >last_perc){
			perc = (100*l)/length;
			fprintf(stderr,"Reading TS  %d %%\r",perc);
			last_perc = perc;
		}

		for( i = 0; i < count; i+= TS_SIZE){
			uint8_t off = 0;

			if ( count - i < TS_SIZE) break;

			pid = get_pid(buf+i+1);
			if (!(buf[3+i]&0x10)) // no payload?
				continue;
			if ( buf[1+i]&0x80){
				fprintf(stderr,"Error in TS for PID: %d\n", 
					pid);
			}
			if (pid != pidv){
				continue;
			}

			if ( buf[3+i] & 0x20) {  // adaptation field?
				off = buf[4+i] + 1;
			}

			if ( buf[1+i]&0x40) {
				if (p->plength == MMAX_PLENGTH-6){
					p->plength = p->found-6;
					p->found = 0;
					send_ipack(p);
					reset_ipack(p);
				}
			}

			instant_repack(buf+4+off+i, TS_SIZE-4-off, p);
		}
		i = 0;

	}
}

void ts2es(int fdin,  uint16_t pidv)
{
	ipack p;
	int verb = 0;

	init_ipack(&p, IPACKS,write_out_es, 0);
	p.fd = STDOUT_FILENO;
	p.data = (void *)&p;

	if (fdin != STDIN_FILENO) verb = 1; 

	ts2es_opt(fdin, pidv, &p, verb);
}


void change_aspect(int fdin, int fdout, int aspect)
{
	ps_packet ps;
	pes_packet pes;
	int neof,i;

	do {
		init_ps(&ps);
		neof = read_ps(fdin,&ps);
		write_ps(fdout,&ps);
		for (i = 0; i < ps.npes; i++){
			uint8_t *buf;
			int c = 0;
			int l;

			init_pes(&pes);
			read_pes(fdin, &pes);

			buf = pes.pes_pckt_data;

			switch (pes.stream_id){
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
				l=pes.length;
				break;
			default:
				l = 0;
				break;
			}
			while ( c < l - 6){
				if (buf[c] == 0x00 && 
				    buf[c+1] == 0x00 &&
				    buf[c+2] == 0x01 && 
				    buf[c+3] == 0xB3) {
					c += 4;
					buf[c+3] &= 0x0f;
					buf[c+3] |= aspect;
				}
				else c++;
			}
			write_pes(fdout,&pes);
		}
	} while( neof > 0 );
}
