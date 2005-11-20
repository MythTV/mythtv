/*
 * ts.c: MPEG TS functions for replex
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "ts.h"

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


int find_pids_pos(uint16_t *vpid, uint16_t *apid, uint16_t *ac3pid,uint8_t *buf, int len, int *vpos, int *apos, int *ac3pos)
{
	int c=0;
	int found=0;

	if (!vpid || !apid || !ac3pid || !buf || !len) return 0;

	*vpid = 0;
	*apid = 0;
	*ac3pid = 0;

	while ( c+TS_SIZE < len){
		if (buf[c] == buf[c+TS_SIZE]) break;
		c++;
	}

	while(found<2 && c < len){
		if (buf[c+1] & PAY_START) {
			int off = 4;
			
			if ( buf[c+3] & ADAPT_FIELD) {  // adaptation field?
				off += buf[c+4] + 1;
			}
			
			if (off < TS_SIZE-4){
				if (!*vpid && (buf[c+off+3] & 0xF0) == 0xE0){
					*vpid = get_pid(buf+c+1);
					if (vpos) *vpos = c+off+3;
					found++;
				}
				if (!*ac3pid && buf[c+off+3] == 0xBD){
					int l=off+4;
					int f=0;

					while ( l < TS_SIZE && f<2){
						uint8_t b=buf[c+l];
						switch(f){
						case 0:
							if ( b == 0x0b) 
								f = 1;
							break;
							
						case 1:
							if ( b == 0x77)
								f = 2;
							else if ( b != 0x0b) 
								f = 0;
						}
						l++;
					}	
					if (f==2){
						*ac3pid = get_pid(buf+c+1);
						if (ac3pos) *ac3pos = c+off+3;
						found++;
					}
				}
				if (!*apid && ((buf[c+off+3] & 0xF0) == 0xC0 ||
					       (buf[c+off+3] & 0xF0) == 0xD0)){
					*apid = get_pid(buf+c+1);
					if (apos) *apos = c+off+3;
					found++;
				}
			}
		} 
		c+= TS_SIZE;
	}
	return found;
}


int find_pids(uint16_t *vpid, uint16_t *apid, uint16_t *ac3pid,uint8_t *buf, int len)
{
	return find_pids_pos(vpid, apid, ac3pid, buf, len, NULL, NULL, NULL);
}
