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

#ifndef RINGBUFFY_H
#define RINGBUFFY_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

#define FULL_BUFFER  -1000
typedef struct ringbuffy{
	int read_pos;
	int write_pos;
	int size;
	char *buffy;
} ringbuffy;

int  ring_init (ringbuffy *rbuf, int size);
void ring_destroy(ringbuffy *rbuf);
int ring_write(ringbuffy *rbuf, char *data, int count);
int ring_read(ringbuffy *rbuf, char *data, int count);
int ring_write_file(ringbuffy *rbuf, int fd, int count);
int ring_read_file(ringbuffy *rbuf, int fd, int count);
int ring_rest(ringbuffy *rbuf);
int ring_peek(ringbuffy *rbuf, char *data, int count, long off);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif /* RINGBUFFY_H */
