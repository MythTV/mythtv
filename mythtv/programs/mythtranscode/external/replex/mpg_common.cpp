/*
 * mpg_common.c: COMMON MPEG functions for replex
 *        
 *
 * Copyright (C) 2003 Marcus Metzler <mocm@metzlerbros.de>
 *                    Metzler Brothers Systementwicklung GbR
 * Changes to use MythTV logging
 * Copyright (C) 2011 Gavin Hurlbut <ghurlbut@mythtv.org>
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

#include <cstdio>
#include <cstring>
#include "element.h"
#include "pes.h"
#include "ts.h"

#include "libmythbase/mythlogging.h"

void show_buf(uint8_t *buf, int length)
{
	QString buffer;

	for (int i=0; i<length; i+=16){
		int j = 0;
		for (j=0; j < 8 && j+i<length; j++)
			buffer += QString("0x%1 ").arg(buf[i+j],16,2,QChar('0'));
		for (int r=j; r<8; r++)
			buffer += "	";

		buffer += "  ";

		for (j=8; j < 16 && j+i<length; j++)
			buffer += QString("0x%1 ").arg(buf[i+j],16,2,QChar('0'));
		for (int r=j; r<16; r++)
			buffer += "	";

		for (j=0; j < 16 && j+i<length; j++){
			switch(buf[i+j]){
			case '0'...'Z':
			case 'a'...'z':
				buffer += QString(QChar(buf[i+j]));
				break;
			default:
				buffer += ".";
			}
		}
		LOG(VB_GENERAL, LOG_INFO, buffer);
	}
}



int find_mpg_header(uint8_t head, const uint8_t *buf, int length)
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


int find_any_header(uint8_t *head, const uint8_t *buf, int length)
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


uint64_t trans_pts_dts(const uint8_t *pts)
{
	uint64_t wts = ((uint64_t)((pts[0] & 0x0E) << 5) |
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


int mring_peek( ringbuffer *rbuf, uint8_t *buf, unsigned int l, uint32_t off)
{
        int c = 0;

        if (ring_avail(rbuf)+off <= l)
                return -1;

        c = ring_peek(rbuf, buf, l, off);
        return c+off;
}


int mring_peek( ringbuffer *rbuf, peek_poke_vec &buf, unsigned int l, uint32_t off)
{
    return mring_peek(rbuf, buf.data(), l, off);
}


int ring_find_mpg_header(ringbuffer *rbuf, uint8_t head, int off, int le)
{

	int c = 0;
	int found = 0;

	c = off;
	while ( c-off < le){
		uint8_t b = 0;
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
	return -1;
}


int ring_find_any_header(ringbuffer *rbuf, uint8_t *head, int off, int le)
{

	int c = 0;
	int found =0;

	c = off;
	while ( c-off < le){
		uint8_t b = 0;
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
	return -1;
}

