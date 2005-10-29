/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#include "serial.h"
#include "crc.h"
#include "debug.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/poll.h>

int serial_fd;
#define LOCKTRIES 60

/* Open and initialize serial port.
   Returns 0 on success, or -1 if error */
int serial_init(char *port)
{
	struct termios t;
	int locktried = 0;

	if ((serial_fd=open(port,O_RDWR,O_NONBLOCK))<0)
		return -1;
 trylock:
	if (flock(serial_fd,LOCK_EX|LOCK_NB)<0) {
		if ((errno==EAGAIN) && locktried<LOCKTRIES) {
			verb("%s is locked; waiting...\n",port);
			locktried++;
			sleep(1);
			goto trylock;
		}
		verb("Failed to get lock: %s\n",strerror(errno));
		return -1;
	}
	if (locktried>0) {
		verb("Just got lock, so giving box time to settle\n");
		sleep(2);
	}	
	if (tcgetattr(serial_fd,&t)<0)
		return -1;
	t.c_cflag |= CLOCAL;
	if (tcsetattr(serial_fd,TCSANOW,&t)<0)
		return -1;
	
	bzero(&t,sizeof(t));
	t.c_iflag=IGNBRK|IGNPAR;
	t.c_cflag=CS8|CREAD|CLOCAL;
	t.c_cc[VMIN]=1;
	if (cfsetispeed(&t,B9600)==-1)
		return -1;
	if (tcsetattr(serial_fd,TCSANOW,&t)==-1)
		return -1;

	return 0;
}

void serial_flush(void) {
  char junk[1024];
  fcntl(serial_fd, F_SETFL, fcntl(serial_fd, F_GETFL, 0) | O_NONBLOCK);
  while(read(serial_fd,junk,1024)>0)
    continue;
  fcntl(serial_fd, F_SETFL, fcntl(serial_fd, F_GETFL, 0) & ~O_NONBLOCK);
}

/* Close a serial port */
void serial_close(void)
{
	flock(serial_fd,LOCK_UN);
	close(serial_fd);
}

/* returns -1 on timeout/error, otherwise 0 */
int serial_getnextbyte(char *ch, int timeout_msec)
{
	struct pollfd pfd;

	pfd.fd = serial_fd;
	pfd.events = POLLIN|POLLERR;

	/* Wait 2 seconds for the byte */
	if (poll(&pfd, 1, timeout_msec)!=1)
		return -1;
	if (pfd.revents & POLLERR)
		return -1;
	
	if (read(serial_fd, ch, 1)!=1)
		return -1;

	return 0;
}

/* Read a packet from the serial port and return it.
   Return 0 on success, or -1 on error/timeout */
int serial_getpacket(packet *p, int timeout_msec)
{
	char ch;
	packet *new;

	packet_stream_init();

 more:
	if (serial_getnextbyte(&ch,timeout_msec)<0) {
		debug("Serial timeout\n");
		return -1;
	}
	if ((new=packet_stream_add(ch))==NULL)
		goto more;

	memcpy(p,new,sizeof(packet));
	
	packet_received(p);

	return 0;
}

void serial_write(unsigned char c)
{
	write(serial_fd,&c,1);
}

void serial_escape_write(unsigned char c)
{
	if (c==0x10) serial_write(c);
	serial_write(c);
}
	
/* Send a packet. */
void serial_sendpacket(packet *p)
{
	int i;

	serial_write(0x10);
	serial_write(p->type);
	serial_escape_write(MSB(p->len));
	serial_escape_write(LSB(p->len));
	serial_escape_write(p->seq);
	serial_escape_write(p->id);
	for(i=0;i<p->len-2;i++)
		serial_escape_write(p->payload[i]);
	serial_escape_write(MSB(p->crc));
	serial_escape_write(LSB(p->crc));
	serial_write(0x10);
	serial_write(0x03);

	packet_sent(p);
}
