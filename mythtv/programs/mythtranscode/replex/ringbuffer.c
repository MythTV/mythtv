/*
 * ringbuffer.c
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

#include <stdlib.h>
#include <string.h>
#include "ringbuffer.h"
#include "pes.h"

#define DEBUG 1
int ring_init (ringbuffer *rbuf, int size)
{
	if (size > 0){
		rbuf->size = size;
		if( !(rbuf->buffer = (uint8_t *) malloc(sizeof(uint8_t)*size)) ){
			fprintf(stderr,"Not enough memory for ringbuffer\n");
			return -1;
		}
	} else {
		fprintf(stderr,"Wrong size for ringbuffer\n");
		return -1;
	}
	rbuf->read_pos = 0;	
	rbuf->write_pos = 0;
	return 0;
}

void ring_clear(ringbuffer *rbuf)
{
	rbuf->read_pos = 0;	
	rbuf->write_pos = 0;
}



void ring_destroy(ringbuffer *rbuf)
{
	free(rbuf->buffer);
}


int ring_write(ringbuffer *rbuf, uint8_t *data, int count)
{

	int free, pos, rest;

	if (count <=0 ) return 0;
       	pos  = rbuf->write_pos;
	rest = rbuf->size - pos;
	free = ring_free(rbuf);

	if ( free < count ){
		if (DEBUG) fprintf(stderr,"ringbuffer overflow %d<%d %d\n", 
				   free, count, rbuf->size);
		return FULL_BUFFER;
	}
	
	if (count >= rest){
		memcpy (rbuf->buffer+pos, data, rest);
		if (count - rest)
			memcpy (rbuf->buffer, data+rest, count - rest);
		rbuf->write_pos = count - rest;
	} else {
		memcpy (rbuf->buffer+pos, data, count);
		rbuf->write_pos += count;
	}

	if (DEBUG>1) fprintf(stderr,"Buffer empty %.2f%%\n", 
			     ring_free(rbuf)*100.0/rbuf->size);
	return count;
}

int ring_peek(ringbuffer *rbuf, uint8_t *data, int count, long off)
{

	int avail, pos, rest;

	if (count <=0 || off+count > rbuf->size || off+count >ring_avail(rbuf)) return -1;
	pos  = (rbuf->read_pos+off)%rbuf->size;
	rest = rbuf->size - pos ;
	avail = ring_avail(rbuf); 

	
	if ( avail < count ){
//		if (DEBUG) fprintf(stderr,"ringbuffer peek underflow %d<%d %d %d\n", 
//				   avail, count, pos, rbuf->write_pos);
		return EMPTY_BUFFER;
	}

	if ( count < rest ){
		memcpy(data, rbuf->buffer+pos, count);
	} else {
		memcpy(data, rbuf->buffer+pos, rest);
		if ( count - rest)
			memcpy(data+rest, rbuf->buffer, count - rest);
	}

	return count;
}

int ring_poke(ringbuffer *rbuf, uint8_t *data, int count, long off)
{

	int avail, pos, rest;

	if (count <=0 || off+count > rbuf->size || off+count >ring_avail(rbuf)) return -1;
	pos  = (rbuf->read_pos+off)%rbuf->size;
	rest = rbuf->size - pos ;
	avail = ring_avail(rbuf); 

	
	if ( avail < count ){
//		if (DEBUG) fprintf(stderr,"ringbuffer peek underflow %d<%d %d %d\n", 
//				   avail, count, pos, rbuf->write_pos);
		return EMPTY_BUFFER;
	}

	if ( count < rest ){
		memcpy(rbuf->buffer+pos, data, count);
	} else {
		memcpy(rbuf->buffer+pos, data, rest);
		if ( count - rest)
			memcpy(rbuf->buffer, data+rest, count - rest);
	}

	return count;
}

int ring_read(ringbuffer *rbuf, uint8_t *data, int count)
{

	int avail, pos, rest;

	if (count <=0 ) return 0;
	pos  = rbuf->read_pos;
	rest = rbuf->size - pos;
	avail = ring_avail(rbuf);
	
	if ( avail < count ){
//		if (DEBUG) fprintf(stderr,"ringbuffer underflow %d<%d %d \n", 
//				   avail, count, rbuf->size);
		return EMPTY_BUFFER;
	}

	if ( count < rest ){
		memcpy(data, rbuf->buffer+pos, count);
		rbuf->read_pos += count;
	} else {
		memcpy(data, rbuf->buffer+pos, rest);
		if ( count - rest)
			memcpy(data+rest, rbuf->buffer, count - rest);
		rbuf->read_pos = count - rest;
	}

	if (DEBUG>1) fprintf(stderr,"Buffer empty %.2f%%\n", 
			     ring_free(rbuf)*100.0/rbuf->size);
	return count;
}

int ring_skip(ringbuffer *rbuf, int count)
{

	int avail, pos, rest;

	if (count <=0 ) return -1;
	pos  = rbuf->read_pos;
	rest = rbuf->size - pos;
	avail = ring_avail(rbuf);

	if ( avail < count ){
//		fprintf(stderr,"ringbuffer skip underflow %d<%d %d %d\n", 
//			avail, count, pos, rbuf->write_pos);
		return EMPTY_BUFFER;
	}
	if ( count < rest ){
		rbuf->read_pos += count;
	} else {
		rbuf->read_pos = count - rest;
	}

	if (DEBUG>1) fprintf(stderr,"Buffer empty %.2f%%\n", 
			     ring_free(rbuf)*100.0/rbuf->size);
	return count;
}



int ring_write_file(ringbuffer *rbuf, int fd, int count)
{

	int free, pos, rest, rr;

	if (count <=0 ) return 0;
       	pos  = rbuf->write_pos;
	rest = rbuf->size - pos;
	free = ring_free(rbuf);

	if ( free < count ){
		if (DEBUG) fprintf(stderr,"ringbuffer overflow %d<%d %d %d\n", 
				   free, count, pos, rbuf->read_pos);
		return FULL_BUFFER;
	}
	
	if (count >= rest){
		rr = read (fd, rbuf->buffer+pos, rest);
		if (rr == rest && count - rest)
			rr += read (fd, rbuf->buffer, count - rest);
		if (rr >=0)
			rbuf->write_pos = (pos + rr) % rbuf->size;
	} else {
		rr = read (fd, rbuf->buffer+pos, count);
		if (rr >=0)
			rbuf->write_pos += rr;
	}

	if (DEBUG>1) fprintf(stderr,"Buffer empty %.2f%%\n", 
			   ring_free(rbuf)*100.0/rbuf->size);
	return rr;
}



int ring_read_file(ringbuffer *rbuf, int fd, int count)
{

	int avail, pos, rest, rr;

	if (count <=0 ) return -1;
	pos  = rbuf->read_pos;
	rest = rbuf->size - pos;
	avail = ring_avail(rbuf);

	if ( avail < count ){
//		if (DEBUG) fprintf(stderr,"ringbuffer underflow %d<%d %d %d\n", 
//				   avail, count, pos, rbuf->write_pos);
		return EMPTY_BUFFER;
	}

	if (count >= rest){
		rr = write (fd, rbuf->buffer+pos, rest);
		if (rr == rest && count - rest)
			rr += write (fd, rbuf->buffer, count - rest);
		if (rr >=0)
			rbuf->read_pos = (pos + rr) % rbuf->size;
	} else {
		rr = write (fd, rbuf->buffer+pos, count);
		if (rr >=0)
			rbuf->read_pos += rr;
	}


	if (DEBUG>1) fprintf(stderr,"Buffer empty %.2f%%\n", 
			     ring_free(rbuf)*100.0/rbuf->size);
	return rr;
}


static void show(uint8_t *buf, int length)
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

void ring_show(ringbuffer *rbuf, int count, long off)
{

	int avail, pos, rest;

	if (count <=0 || off+count > rbuf->size || off+count >ring_avail(rbuf)) return;
	pos  = (rbuf->read_pos+off)%rbuf->size;
	rest = rbuf->size - pos ;
	avail = ring_avail(rbuf); 

	
	if ( avail < count ){
//		if (DEBUG) fprintf(stderr,"ringbuffer peek underflow %d<%d %d %d\n", 
//				   avail, count, pos, rbuf->write_pos);
		return;
	}

	if ( count < rest ){
		show(rbuf->buffer+pos, count);
	} else {
		show(rbuf->buffer+pos, rest);
		if ( count - rest)
			show(rbuf->buffer, count - rest);
	}
}


int dummy_init(dummy_buffer *dbuf, int s)
{
	dbuf->size = s;
	dbuf->fill = 0;
	if (ring_init(&dbuf->time_index, DBUF_INDEX*sizeof(uint64_t)) < 0)
		return -1;
	if (ring_init(&dbuf->data_index, DBUF_INDEX*sizeof(int32_t)) < 0)
		return -1;

	return 0;
}

void dummy_clear(dummy_buffer *dbuf)
{
	dbuf->fill = 0;
	ring_clear(&dbuf->time_index);
	ring_clear(&dbuf->data_index);
}

int dummy_add(dummy_buffer *dbuf, uint64_t time, uint32_t size)
{
	if (dummy_space(dbuf) < size) return -1;
//	fprintf(stderr,"add %d ",dummy_space(dbuf));    
	dbuf->fill += size;
	if (ring_write(&dbuf->time_index, (uint8_t *)&time, sizeof(uint64_t)) < 0) 
		return -2;
	if (ring_write(&dbuf->data_index, (uint8_t *)&size, sizeof(uint32_t)) < 0) 
		return -3;
//	fprintf(stderr," - %d = ",size);    
//	fprintf(stderr,"%d\n",dummy_space(dbuf));    
	return size;
}

int dummy_delete(dummy_buffer *dbuf, uint64_t time)
{
	uint64_t rtime;
	uint32_t size;
	int ex=0;
	uint32_t dsize=0;

	do {
		if (ring_peek(&dbuf->time_index,(uint8_t *) &rtime, 
			      sizeof(uint64_t), 0)<0){
			if (dsize) break;
			else return -1;
		}
		if (ptscmp(rtime,time) < 0){
			ring_read(&dbuf->time_index,(uint8_t *) &rtime, 
				  sizeof(uint64_t));
			ring_read(&dbuf->data_index,(uint8_t *) &size, 
				  sizeof(uint32_t));
			dsize += size;
		} else ex = 1;
	} while (!ex);
//	fprintf(stderr,"delete %d ",dummy_space(dbuf));    
	dbuf->fill -= dsize;
//	fprintf(stderr," + %d = ",dsize);    
//	fprintf(stderr,"%d\n",dummy_space(dbuf));    

	return dsize;
}
