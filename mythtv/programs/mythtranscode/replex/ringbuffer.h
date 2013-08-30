/*
 * ringbuffer.h
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

#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

#define FULL_BUFFER  -1000
#define EMPTY_BUFFER  -1000
	typedef struct ringbuffer {
		int read_pos;
		int write_pos;
		uint32_t size;
		uint8_t *buffer;
	} ringbuffer;


#define DBUF_INDEX 10000

	typedef struct dummy_buffer_s {
		uint32_t size;
		uint32_t fill;
		ringbuffer time_index;
		ringbuffer data_index;
	} dummy_buffer;


	int  ring_init (ringbuffer *rbuf, int size);
	int  ring_reinit (ringbuffer *rbuf, int size);
	void ring_clear(ringbuffer *rbuf);
	void ring_destroy(ringbuffer *rbuf);
	int ring_write(ringbuffer *rbuf, uint8_t *data, int count);
	int ring_read(ringbuffer *rbuf, uint8_t *data, int count);
	int ring_write_file(ringbuffer *rbuf, int fd, int count);
	int ring_read_file(ringbuffer *rbuf, int fd, int count);
	int ring_peek(ringbuffer *rbuf, uint8_t *data, unsigned int count,
                      uint32_t off);
	int ring_poke(ringbuffer *rbuf, uint8_t *data, unsigned int count,
                      uint32_t off);
	int ring_skip(ringbuffer *rbuf, int count);

	static inline int ring_wpos(ringbuffer *rbuf)
	{
		return rbuf->write_pos;
	}
	
	static inline int ring_rpos(ringbuffer *rbuf)
	{
		return rbuf->read_pos;
	}

	static inline int ring_posdiff(ringbuffer *rbuf, int pos1, int pos2){
		int diff;
		
		diff = (pos2%rbuf->size) - (pos1%rbuf->size);
		if (diff < 0) diff += rbuf->size;
		return diff;
	}

	static inline int ring_wdiff(ringbuffer *rbuf, int pos){
		return ring_posdiff(rbuf, rbuf->write_pos,pos);
	}

	static inline int ring_rdiff(ringbuffer *rbuf, int pos){
		return ring_posdiff(rbuf, rbuf->read_pos,pos);
	}

	static inline unsigned int ring_free(ringbuffer *rbuf){
		int free;
		free = rbuf->read_pos - rbuf->write_pos;
		if (free <= 0) free += rbuf->size;
		//Note: free is gauranteed to be >=1 from the above
		return free - 1;
	}

	static inline unsigned int ring_avail(ringbuffer *rbuf){
		int avail;
		avail = rbuf->write_pos - rbuf->read_pos;
		if (avail < 0) avail += rbuf->size;
		
		return avail;
	}



	static inline uint32_t dummy_space(dummy_buffer *dbuf)
	{
		return (dbuf->size - dbuf->fill);
	}
	int dummy_delete(dummy_buffer *dbuf, uint64_t time);
	int dummy_add(dummy_buffer *dbuf, uint64_t time, uint32_t size);
	void dummy_clear(dummy_buffer *dbuf);
	int dummy_init(dummy_buffer *dbuf, int s);
        void dummy_destroy(dummy_buffer *dbuf);
	void ring_show(ringbuffer *rbuf, unsigned int count, uint32_t off);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif /* _RINGBUFFER_H_ */
