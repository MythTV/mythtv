/* 
    Ringbuffer Implementation for gtvscreen

    Copyright (C) 2000 Marcus Metzler (mocm@metzlerbros.de)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "ringbuffy.h"

int ring_init (ringbuffy *rbuf, int size)
{
	if (size > 0){
		rbuf->size = size;
		if( !(rbuf->buffy = (char *) malloc(sizeof(char)*size)) ){
			fprintf(stderr,"Not enough memory for ringbuffy\n");
			return -1;
		}
	} else {
		fprintf(stderr,"Wrong size for ringbuffy\n");
		return -1;
	}
	rbuf->read_pos = 0;	
	rbuf->write_pos = 0;
	return 0;
}


void ring_destroy(ringbuffy *rbuf)
{
	free(rbuf->buffy);
}


int ring_write(ringbuffy *rbuf, char *data, int count)
{

	int diff, free, pos, rest;

	if (count <=0 ) return 0;
       	pos  = rbuf->write_pos;
	rest = rbuf->size - pos;
	diff = rbuf->read_pos - pos;
	free = (diff > 0) ? diff-1 : rbuf->size+diff-1;

	if ( free <= 0 ) return FULL_BUFFER;
	if ( free < count ) count = free;
	
	if (count >= rest){
		memcpy (rbuf->buffy+pos, data, rest);
		if (count - rest)
			memcpy (rbuf->buffy, data+rest, count - rest);
		rbuf->write_pos = count - rest;
	} else {
		memcpy (rbuf->buffy+pos, data, count);
		rbuf->write_pos += count;
	}

	return count;
}




int ring_peek(ringbuffy *rbuf, char *data, int count, long off)
{

	int diff, free, pos, rest;

	if (count <=0 ) return 0;
	pos  = rbuf->read_pos+off;
	rest = rbuf->size - pos ;
	diff = rbuf->write_pos - pos;
	free = (diff >= 0) ? diff : rbuf->size+diff;

	if ( free <= 0 ) return FULL_BUFFER;
	if ( free < count ) count = free;
	
	if ( count < rest ){
		memcpy(data, rbuf->buffy+pos, count);
	} else {
		memcpy(data, rbuf->buffy+pos, rest);
		if ( count - rest)
			memcpy(data+rest, rbuf->buffy, count - rest);
	}

	return count;
}

int ring_read(ringbuffy *rbuf, char *data, int count)
{

	int diff, free, pos, rest;

	if (count <=0 ) return 0;
	pos  = rbuf->read_pos;
	rest = rbuf->size - pos;
	diff = rbuf->write_pos - pos;
	free = (diff >= 0) ? diff : rbuf->size+diff;

	if ( rest <= 0 ) return 0;
	if ( free < count ) count = free;
	
	if ( count < rest ){
		memcpy(data, rbuf->buffy+pos, count);
		rbuf->read_pos += count;
	} else {
		memcpy(data, rbuf->buffy+pos, rest);
		if ( count - rest)
			memcpy(data+rest, rbuf->buffy, count - rest);
		rbuf->read_pos = count - rest;
	}

	return count;
}



int ring_write_file(ringbuffy *rbuf, int fd, int count)
{

	int diff, free, pos, rest, rr;

	if (count <=0 ) return 0;
       	pos  = rbuf->write_pos;
	rest = rbuf->size - pos;
	diff = rbuf->read_pos - pos;
	free = (diff > 0) ? diff-1 : rbuf->size+diff-1;

	if ( rest <= 0 ) return 0;
	if ( free < count ) count = free;
	
	if (count >= rest){
		rr = read (fd, rbuf->buffy+pos, rest);
		if (rr == rest && count - rest)
			rr += read (fd, rbuf->buffy, count - rest);
		if (rr >=0)
			rbuf->write_pos = (pos + rr) % rbuf->size;
	} else {
		rr = read (fd, rbuf->buffy+pos, count);
		if (rr >=0)
			rbuf->write_pos += rr;
	}

	return rr;
}



int ring_read_file(ringbuffy *rbuf, int fd, int count)
{

	int diff, free, pos, rest, rr;

	if (count <=0 ) return 0;
	pos  = rbuf->read_pos;
	rest = rbuf->size - pos;
	diff = rbuf->write_pos - pos;
	free = (diff >= 0) ? diff : rbuf->size+diff;

	if ( free <= 0 ) return FULL_BUFFER;
	if ( free < count ) count = free;

	if (count >= rest){
		rr = write (fd, rbuf->buffy+pos, rest);
		if (rr == rest && count - rest)
			rr += write (fd, rbuf->buffy, count - rest);
		if (rr >=0)
			rbuf->read_pos = (pos + rr) % rbuf->size;
	} else {
		rr = write (fd, rbuf->buffy+pos, count);
		if (rr >=0)
			rbuf->read_pos += rr;
	}


	return rr;
}

int ring_rest(ringbuffy *rbuf){
       	int diff, free, pos, rest;
	pos  = rbuf->read_pos;
	rest = rbuf->size - pos;
	diff = rbuf->write_pos - pos;
	free = (diff >= 0) ? diff : rbuf->size+diff;
	
	return free;
}
