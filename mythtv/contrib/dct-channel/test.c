#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "crc.h"
#include "serial.h"
#include "packet.h"
#include "debug.h"
#include "remote.h"
#include "opt.h"
#include "version.h"

int verb_count=10;
extern int seq_ack;
extern int seq_req;

void handler(packet *p);
extern void (*packet_received_handler)(packet *p);

void handler(packet *p)
{
	return;
}

int main() {
	packet_received_handler = &handler;

	if((serial_init("/dev/ttyS0"))<0) 
		err(1,"ttyS0");

	atexit(serial_close);

	packet p;
	packet s;

	packet_build_initialize_1(&p);
	serial_sendpacket(&p);
	packet_build_initialize_2(&p);
	serial_sendpacket(&p);
	while(serial_getpacket(&s,5000)>=0)
		;
	return 0;
}
