/*
 * ringbuffer.c
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

#include <cstdlib>
#include <cstring>
#include "ringbuffer.h"
#include "pes.h"

#include "libmythbase/mythlogging.h"

#define DEBUG true

int ring_init (ringbuffer *rbuf, int size)
{
	if (size > 0){
		rbuf->size = size;
		rbuf->buffer = (uint8_t *) malloc(sizeof(uint8_t)*size);
		if (rbuf->buffer == nullptr)
                {
			LOG(VB_GENERAL, LOG_ERR,
			    "Not enough memory for ringbuffer");
			return -1;
		}
	} else {
		LOG(VB_GENERAL, LOG_ERR, "Wrong size for ringbuffer");
		return -1;
	}
	rbuf->read_pos = 0;	
	rbuf->write_pos = 0;
	return 0;
}

int ring_reinit (ringbuffer *rbuf, int size)
{
	if (size > (int)(rbuf->size)) {
		auto *tmpalloc = (uint8_t *) realloc(rbuf->buffer,
							sizeof(uint8_t)*size);
		if (! tmpalloc)
			return -1;
		rbuf->buffer = tmpalloc;
		if (rbuf->write_pos < rbuf->read_pos)
		{
			unsigned int delta = size - rbuf->size;
			memmove(rbuf->buffer + rbuf->read_pos + delta,
				rbuf->buffer + rbuf->read_pos,
				rbuf->size - rbuf->read_pos);
			rbuf->read_pos += delta;
		}
		rbuf->size = size;
	}
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
	if (count <=0 ) return 0;
	int pos	 = rbuf->write_pos;
	int rest = rbuf->size - pos;
	int free = ring_free(rbuf);

	if ( free < count ){
		if (DEBUG) {
			LOG(VB_GENERAL, LOG_ERR,
			    QString("ringbuffer overflow %1<%2 %3")
			    .arg(free).arg(count).arg(rbuf->size));
		}
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

	if (DEBUG>1)
		LOG(VB_GENERAL, LOG_ERR, QString("Buffer empty %1%%")
		    .arg(ring_free(rbuf)*100.0/rbuf->size, 0,'f',2,QChar('0')));
	return count;
}

int ring_peek(ringbuffer *rbuf, uint8_t *data, unsigned int count, uint32_t off)
{
	if (off+count > rbuf->size || off+count >ring_avail(rbuf))
		return -1;
	unsigned int pos  = (rbuf->read_pos+off)%rbuf->size;
	unsigned int rest = rbuf->size - pos ;
	unsigned int avail = ring_avail(rbuf);
	
	if ( avail < count ){
#if 0
		if (DEBUG)
			LOG(VB_GENERAL, LOG_ERR,
			    QString("ringbuffer peek underflow %1<%2 %3 %4")
			    .arg(avail).arg(count).arg(pos).arg(rbuf->write_pos));
#endif
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

int ring_peek(ringbuffer *rbuf, peek_poke_vec& data, unsigned int count, uint32_t off)
{
    return ring_peek(rbuf, data.data(), count, off);
}

int ring_peek(ringbuffer *rbuf, peek_poke_vec& data, uint32_t off)
{
    return ring_peek(rbuf, data.data(), data.size(), off);
}

int ring_poke(ringbuffer *rbuf, uint8_t *data, unsigned int count, uint32_t off)
{
	if (off+count > rbuf->size || off+count >ring_avail(rbuf))
		return -1;
	unsigned int pos  = (rbuf->read_pos+off)%rbuf->size;
	unsigned int rest = rbuf->size - pos ;
	unsigned int avail = ring_avail(rbuf);
	
	if ( avail < count ){
#if 0
		if (DEBUG)
			LOG(VB_GENERAL, LOG_ERR,
			    QString("ringbuffer peek underflow %1<%2 %3 %4")
			    .arg(avail).arg(count).arg(pos).arg(rbuf->write_pos));
#endif
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

int ring_poke(ringbuffer *rbuf, peek_poke_vec& data, unsigned int count, uint32_t off)
{
    return ring_poke(rbuf, data.data(), count, off);
}

int ring_poke(ringbuffer *rbuf, peek_poke_vec& data, uint32_t off)
{
    return ring_poke(rbuf, data.data(), data.size(), off);
}

int ring_read(ringbuffer *rbuf, uint8_t *data, int count)
{
	if (count <=0 ) return 0;
	int pos  = rbuf->read_pos;
	int rest = rbuf->size - pos;
	int avail = ring_avail(rbuf);
	
	if ( avail < count ){
#if 0
		if (DEBUG)
			LOG(VB_GENERAL, LOG_ERR,
			    QString("ringbuffer underflow %1<%2 %3 \n")
			    .arg(avail).arg(count).arg(rbuf->size));
#endif
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

	if (DEBUG>1)
		LOG(VB_GENERAL, LOG_ERR, QString("Buffer empty %1%%")
		    .arg(ring_free(rbuf)*100.0/rbuf->size, 0,'f',2,QChar('0')));
	return count;
}

int ring_skip(ringbuffer *rbuf, int count)
{
	if (count <=0 ) return -1;
	int pos  = rbuf->read_pos;
	int rest = rbuf->size - pos;
	int avail = ring_avail(rbuf);

	if ( avail < count ){
#if 0
		LOG(VB_GENERAL, LOG_ERR,
		    QString("ringbuffer skip underflow %1<%2 %3 %4\n")
		    .arg(avail).arg(count).arg(pos).arg(rbuf->write_pos));
#endif
		return EMPTY_BUFFER;
	}
	if ( count < rest ){
		rbuf->read_pos += count;
	} else {
		rbuf->read_pos = count - rest;
	}

	if (DEBUG>1)
	    LOG(VB_GENERAL, LOG_ERR, QString("Buffer empty %1%%")
		.arg(ring_free(rbuf)*100.0/rbuf->size, 0,'f',2,QChar('0')));
	return count;
}



int ring_write_file(ringbuffer *rbuf, int fd, int count)
{
	int rr = 0;

	if (count <=0 ) return 0;
	int pos	 = rbuf->write_pos;
	int rest = rbuf->size - pos;
	int free = ring_free(rbuf);

	if ( free < count ){
		if (DEBUG) {
			LOG(VB_GENERAL, LOG_ERR,
			    QString("ringbuffer overflow %1<%2 %3 %4\n")
			    .arg(free).arg(count).arg(pos).arg(rbuf->read_pos));
		}
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

	if (DEBUG>1)
	    LOG(VB_GENERAL, LOG_ERR, QString("Buffer empty %.2f%%")
		.arg(ring_free(rbuf)*100.0/rbuf->size, 0,'f',2,QChar('0')));
	return rr;
}



int ring_read_file(ringbuffer *rbuf, int fd, int count)
{
	int rr = 0;

	if (count <=0 ) return -1;
	int pos  = rbuf->read_pos;
	int rest = rbuf->size - pos;
	int avail = ring_avail(rbuf);

	if ( avail < count ){
#if 0
		if (DEBUG)
			LOG(VB_GENERAL, LOG_ERR,
			    QString("ringbuffer underflow %1<%2 %3 %4")
			    .arg(avail).arg(count).arg(pos).arg(rbuf->write_pos));
#endif
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


	if (DEBUG>1)
		LOG(VB_GENERAL, LOG_ERR, QString("Buffer empty %1%%")
		    .arg(ring_free(rbuf)*100.0/rbuf->size, 0,'f',2,QChar('0')));
	return rr;
}


static void show(uint8_t *buf, int length)
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

void ring_show(ringbuffer *rbuf, unsigned int count, uint32_t off)
{
	if (off+count > rbuf->size || off+count >ring_avail(rbuf))
		return;
	unsigned int pos  = (rbuf->read_pos+off)%rbuf->size;
	unsigned int rest = rbuf->size - pos ;
	unsigned int avail = ring_avail(rbuf);
	
	if ( avail < count ){
#if 0
		if (DEBUG)
			LOG(VB_GENERAL, LOG_ERR,
			    QString("ringbuffer peek underflow %1<%2 %3 %4\n")
			    .arg(avail).arg(count).arg(pos).arg(rbuf->write_pos));
#endif
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

void dummy_destroy(dummy_buffer *dbuf)
{
        ring_destroy(&dbuf->time_index);
        ring_destroy(&dbuf->data_index);
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
#if 0
	LOG(VB_GENERAL, LOG_INFO, QString("add %1 ").arg(dummy_space(dbuf)));
#endif
	dbuf->fill += size;
	if (ring_write(&dbuf->time_index, (uint8_t *)&time, sizeof(uint64_t)) < 0) 
		return -2;
	if (ring_write(&dbuf->data_index, (uint8_t *)&size, sizeof(uint32_t)) < 0) 
		return -3;
#if 0
	LOG(VB_GENERAL, LOG_INFO,
            QString(" - %1 = %2").arg(size).arg(dummy_space(dbuf)));
#endif
	return size;
}

int dummy_delete(dummy_buffer *dbuf, uint64_t time)
{
	uint64_t rtime = 0;
	uint32_t size = 0;
	int ex=0;
	uint32_t dsize=0;

	while (ex == 0)
	{
		if (ring_peek(&dbuf->time_index,(uint8_t *) &rtime, 
			      sizeof(uint64_t), 0)<0){
			if (dsize) break;
			return -1;
		}
		if (ptscmp(rtime,time) < 0){
			ring_read(&dbuf->time_index,(uint8_t *) &rtime, 
				  sizeof(uint64_t));
			ring_read(&dbuf->data_index,(uint8_t *) &size, 
				  sizeof(uint32_t));
			dsize += size;
		} else {
			ex = 1;
		}
	}
#if 0
	LOG(VB_GENERAL, LOG_INFO, QString("delete %1 ").arg(dummy_space(dbuf)));
#endif
	dbuf->fill -= dsize;
#if 0
	LOG(VB_GENERAL, LOG_INFO,
            QString(" + %1 = %2").arg(dsize).arg(dummy_space(dbuf)));
#endif

	return dsize;
}

#if 0
static void dummy_print(dummy_buffer *dbuf)
{
   uint64_t rtime = 0;
   uint32_t size = 0;
   int avail = ring_avail(&dbuf->time_index) / sizeof(uint64_t);
   for (int i = 0; i < avail; i++) {
       ring_peek(&dbuf->time_index,(uint8_t *) &rtime, 
			      sizeof(uint64_t), i * sizeof(uint64_t));
       ring_peek(&dbuf->data_index,(uint8_t *) &size, 
			      sizeof(uint32_t), i * sizeof(uint32_t));

       LOG(VB_GENERAL, LOG_INFO, QString("%1 : %2 %3").arg(i)
	   .arg(rtime).arg(size));
   }
   LOG(VB_GENERAL, LOG_INFO, QString("Used: %d Free: %d data-free: %d")
       .arg(avail).arg(1000-avail).arg(dbuf->size - dbuf->fill));
}
#endif
