/*
 * mpg_common.c: COMMON MPEG functions for replex
 *        
 *
 * Copyright (C) 2003 - 2006
 *                    Marcus Metzler <mocm@metzlerbros.de>
 *                    Metzler Brothers Systementwicklung GbR
 *           (C) 2006 Reel Multimedia
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

#include <stdio.h>
#include "element.h"
#include "pes.h"
#include "ts.h"

void show_buf(uint8_t *buf, int length)
{
	int i,j,r;

	fprintf(stderr,"\n");
	for (i=0; i<length; i+=16){
		for (j=0; j < 8 && j+i<length; j++)
			fprintf(stderr,"0x%02x ", (int)(buf[i+j]));
		for (r=j; r<8; r++) 			
			fprintf(stderr,"     ");

		fprintf(stderr,"  ");

		for (j=8; j < 16 && j+i<length; j++)
			fprintf(stderr,"0x%02x ", (int)(buf[i+j]));
		for (r=j; r<16; r++) 			
			fprintf(stderr,"     ");

		for (j=0; j < 16 && j+i<length; j++){
			switch(buf[i+j]){
			case '0'...'Z':
			case 'a'...'z':
				fprintf(stderr,"%c", buf[i+j]);
				break;
			default:
				fprintf(stderr,".");
			}
		}
		fprintf(stderr,"\n");
	}
}

//----------------------------------------------------------------------------
#define CMP_CORE(offset) \
        x=Data[i+1+offset]; \
        if (x<2) { \
                if (x==0) { \
                        if ( Data[i+offset]==0 && Data[i+2+offset]==1) \
                                return i+offset; \
                        else if (Data[i+2+offset]==0 && Data[i+3+offset]==1) \
                                return i+1+offset; \
                } \
                else if (x==1 && Data[i-1+offset]==0 && Data[i+offset]==0 && (i+offset)>0) \
                         return i-1+offset; \
         }

int FindPacketHeader(const uint8_t *Data, int s, int l)
{
        int i;
        uint8_t x;

        if (l>12) {
                for(i=s;i<l-12;i+=12) { 
                        CMP_CORE(0);
                        CMP_CORE(3);
                        CMP_CORE(6);
                        CMP_CORE(9);
                }

                for(; i<l-3; i+=3) {
                        CMP_CORE(0);
		}	       
        }
        else {
                for(i=s; i<l-3; i+=3) {
                        CMP_CORE(0);
		}
        }
        return -1;
}
//----------------------------------------------------------------------------

int find_mpg_header(uint8_t head, uint8_t *buf, int Count)
{
	int n=0;
	uint8_t *Data=buf;
        while(n<Count) {
                int x;
                x=FindPacketHeader(Data, 0, Count - n); // returns position of first 00
                if (x!=-1) {
                        Data+=x;
                        n+=x;
			if (Data[3]==head) {
                                return n; 
                        }
                        n+=3;
                        Data+=3;
                        Count-=3;
                }
                else
                        break;
        }
        return -1;

}

int find_any_header(uint8_t *head, uint8_t *buf, int Count)
{
	int n=0;
	uint8_t *Data=buf;
        while(n<Count) {
                int x;
                x=FindPacketHeader(Data, 0, Count - n); // returns position of first 00
                if (x!=-1) {
                        Data+=x;
                        n+=x;
			*head=Data[3];
			return n; 
                }
                else
                        break;
        }
        return -1;

}

int find_mpg_headerx(uint8_t head, uint8_t *buf, int length)
{

	int c = 0;
	int found=0;

	if (length <0) return -1;

	while (found < 4 && c < length){
		switch(found){

		case 0:
			if (buf[c] == 0x00) found = 1;
			break;

		case 1:
			if (buf[c] == 0x00) found = 2;
			else found = 0;
			break;
			
		case 2:
			if (buf[c] == 0x01) found = 3;
			else if (buf[c] != 0x00) found = 0;
			break;

		case 3:
			if (buf[c] == head) return c-3;
			else found = 0;
			break;
		}
		c++;
	}
	return -1;
}


int find_any_headerx(uint8_t *head, uint8_t *buf, int length)
{

	int c = 0;
	int found=0;

	if (length <0) return -1;

	while (found < 4 && c < length){
		switch(found){

		case 0:
			if (buf[c] == 0x00) found = 1;
			break;

		case 1:
			if (buf[c] == 0x00) found = 2;
			else found = 0;
			break;
			
		case 2:
			if (buf[c] == 0x01) found = 3;
			else if (buf[c] != 0x00) found = 0;
			break;

		case 3:
			*head = buf[c];
			return c-3;
			break;
		}
		c++;
	}
	return -1;
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

	wts = wts*300ULL;
	return wts;
}


int mring_peek( ringbuffer *rbuf, uint8_t *buf, int l, long off)
{
        int c = 0;

        if (ring_avail(rbuf)+off <= l)
                return -1;

        c = ring_peek(rbuf, buf, l, off);
        return c+off;
}



int ring_find_mpg_header(ringbuffer *rbuf, uint8_t head, int off, int le)
{

	int c = 0;
	int found = 0;

	c = off;
	while ( c-off < le){
		uint8_t b;
		if ( mring_peek(rbuf, &b, 1, c) <0) return -1;
		switch(found){

		case 0:
			if (b == 0x00) found = 1;
			break;

		case 1:
			if (b == 0x00) found = 2;
			else found = 0;
			break;
			
		case 2:
			if (b == 0x01) found = 3;
			else if (b != 0x00) found = 0;
			break;

		case 3:
			if (b == head) return c-3-off;
			else found = 0;
			break;
		}
		c++;
		
	}
	if (found) return -2;
	else return -1;
}


#define PEEK_SIZE (512+1024)
int ring_find_any_headery(ringbuffer *rbuf, uint8_t *head, int off, int le)
{
	uint8_t buf[PEEK_SIZE];
//	int xoff;
	int res,x;
//	int found;
	int n=off;
	int peek_snip;
//	printf("# %i %i\n",off,le);
	while(le>0) {
		if (le>PEEK_SIZE)
			peek_snip=PEEK_SIZE;
		else
			peek_snip=le;

		res=mring_peek(rbuf, buf, peek_snip, n);
		if (res<0) {
			peek_snip=ring_avail(rbuf);
//			printf("PP %i\n",peek_snip);
			if (peek_snip>PEEK_SIZE)
				peek_snip=PEEK_SIZE;
			if (peek_snip>le)
				peek_snip=le;
			res=mring_peek(rbuf,buf,peek_snip,n);
			if (res<0)
				return -1;
		}
//		printf("ZZ %i %i\n",peek_snip,n);
		x=FindPacketHeader(buf, 0, peek_snip); // returns position of first 00
                if (x!=-1 && x<=peek_snip-4) {
                        n+=x-off;
			
			*head=buf[x+3];
			return n; 
                }
		if (peek_snip<=4) {
			int i;
			for(i=0;i<peek_snip;i++)
				if (buf[i]==0)
					return -2;
			return -1;
		}

		n+=peek_snip-4;
		le-=peek_snip-4;
	}
	return -1; // Not found
}

int ring_find_any_headerx(ringbuffer *rbuf, uint8_t *head, int off, int le)
{

	int c = 0;
	int found =0;

	c = off;
	while ( c-off < le){
		uint8_t b;
		if ( mring_peek(rbuf, &b, 1, c) <0){
			return -1;
		}
		switch(found){

		case 0:
			if (b == 0x00) found = 1;
			break;

		case 1:
			if (b == 0x00) found = 2;
			else found = 0;
			break;
			
		case 2:
			if (b == 0x01) found = 3;
			else if (b != 0x00) found = 0;
			break;

		case 3:
			*head = b;
			return c-3-off;
			break;
		}
		c++;
	}
	if (found) return -2;
	else return -1;
}

int ring_find_any_header(ringbuffer *rbuf, uint8_t *head, int off, int le)
{
	uint8_t a=0;
//	uint8_t b=0;
	int x;
//	int y;

	x=ring_find_any_headery(rbuf, &a, off, le);
#if 0
	y=ring_find_any_headery(rbuf, &b, off, le);
	if (x!=y || a!=b)
		printf("MY %i %i, ORG %i %i\n",y, b, x, a);
#endif
	*head=a;
	return x;
}
