/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

/* Process data from the DCT2000 and fill data structures accordingly.
 * 
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <stdint.h>

#include "crc.h"
#include "serial.h"
#include "packet.h"
#include "debug.h"

int seq_ack, seq_req;
/* CRC includes type, len msb, len lsb, seq, id, payload */

/* Global state; only deals with one stream at a time */
packet stream_p;
int stream_byte;
int stream_escaped;
uint16_t stream_crc;
enum {
	wait_type, wait_len_msb, wait_len_lsb, wait_seq,
	wait_id, wait_payload, wait_crc_msb, wait_crc_lsb,
	wait_end
} stream_state;

void (*packet_received_handler)(packet *p) = NULL;

void packet_stream_init(void)
{
	stream_byte = 0;
	stream_escaped = 0;
	stream_state = wait_type;
}

packet *packet_stream_add(uint8_t c)
{
	if (c == 0x10 && !stream_escaped) {
		stream_escaped = 1;
		return NULL;
	}

	switch(stream_state)
	{
	case wait_type:
		stream_p.type = c;
		if (!stream_escaped) 
			debug("Start not escaped; waiting more\n");
		else
			stream_state = wait_len_msb;
		break;
	case wait_len_msb:
		stream_p.len = WORD(c,0);
		stream_state = wait_len_lsb;
		break;
	case wait_len_lsb:
		stream_p.len |= WORD(0,c);
		stream_state = wait_seq;
		break;
	case wait_seq:
		stream_p.seq = c;
		stream_state = wait_id;
		break;
	case wait_id:
		stream_p.id = c;
		stream_state = wait_payload;
		break;
	case wait_payload:
		if (stream_byte < (stream_p.len-2)) {
			if (stream_byte < MAX_PAYLOAD)
				stream_p.payload[stream_byte] = c;
			stream_byte++;
			break;
		}
		stream_state = wait_crc_msb;
		/* Fall through */
	case wait_crc_msb:
		stream_crc = WORD(c,0);
		stream_state = wait_crc_lsb;
		break;
	case wait_crc_lsb:
		stream_crc |= WORD(0,c);
		packet_fill_crc(&stream_p);
		if (stream_p.crc != stream_crc)
			debug("Bad CRC: expected 0x%04x, got 0x%04x\n",
			      stream_p.crc, stream_crc);
		stream_state = wait_end;
		break;
	case wait_end:
		if (c!=0x03 || !stream_escaped) 
			debug("Bad end: expected escaped 0x03, got "
			      "%sescaped 0x%02x\n",stream_escaped?"":"un",c);
		return &stream_p;
	}
	stream_escaped = 0;
	return NULL;
}

void packet_fill_crc(packet *p)
{
	uint16_t crc = 0;

	crc = makecrc_byte(crc, p->type);
	crc = makecrc_byte(crc, MSB(p->len));
	crc = makecrc_byte(crc, LSB(p->len));
	crc = makecrc_byte(crc, p->seq);
	crc = makecrc_byte(crc, p->id);
	crc = makecrc(crc, p->payload, p->len-2);

	p->crc = crc;
}

void packet_build_channelquery(packet *p)
{
	p->type = 0x78;
	p->len = 3;
	packet_fill_seq(p);
	p->id = id_pc;
	p->payload[0] = cmd_statusreq;
	packet_fill_crc(p);
}

void packet_build_initialize_1(packet *p)
{
	p->type = 0x70;
	p->len = 2;
	p->seq = 0x03;		/* Fixed seq. for init */
	p->id = id_pc;
	packet_fill_crc(p);
}

void packet_build_initialize_2(packet *p)
{
	p->type = 0x78;
	p->len = 3;
	p->seq = 0x03;		/* Fixed seq. for init */
	p->id = id_pc;
	p->payload[0] = cmd_init;
	packet_fill_crc(p);
}

void packet_build_ack(packet *in, packet *out)
{
	if (in->seq & 0x80)
		warnx("building a response to an ACK!\n");
	out->type = in->type;
	out->len = 2;
	packet_fill_seq_ack(out);
	out->id = id_pc;
	packet_fill_crc(out);
}

void packet_build_keypress(packet *p, uint8_t key)
{
	p->type = 0x78;
	p->len = 4;
	packet_fill_seq(p);
	p->id = id_pc;
	p->payload[0] = cmd_key;
	p->payload[1] = key;
	packet_fill_crc(p);
}

void packet_fill_seq_ack(packet *p)
{
	seq_ack = (seq_ack + 1) & 0x3;
	p->seq = 0x80 | (seq_req << 4) | seq_ack;
}

void packet_fill_seq(packet *p)
{
	seq_req = (seq_req + 1) & 0x3;
	p->seq = (seq_req << 4) | seq_ack;
}

void packet_received(packet *p)
{
	debug("RECV: type 0x%02x, len %d, id 0x%02x, seq 0x%02x\n",
	     p->type, p->len, p->id, p->seq);
	if (p->len>2) {
		int i;
		debug("      data");
		for(i=0;i<p->len-2;i++) 
			debug(" 0x%02x",p->payload[i]);
		debug("\n");
	}

	if (packet_received_handler != NULL) {
		(*packet_received_handler)(p);
		return;
	}

	seq_ack = p->seq & 0x3;
	seq_req = (p->seq >> 4) & 0x3;
	if (!(p->seq & 0x80)) {
		packet r;
		debug("Sending automatic ACK\n");
		packet_build_ack(p,&r);
		serial_sendpacket(&r);
	}
}

void packet_sent(packet *p)
{
	debug("SENT: type 0x%02x, len %d, id 0x%02x, seq 0x%02x\n",
	     p->type, p->len, p->id, p->seq);
	if (p->len>2) {
		int i;
		debug("      data");
		for(i=0;i<p->len-2;i++) 
			debug(" 0x%02x",p->payload[i]);
		debug("\n");
	}		

	seq_ack = p->seq & 0x3;
	seq_req = (p->seq >> 4) & 0x3;
}

int packet_is_ack(packet *p, packet *ack)
{
	if (ack->len != 0x02) 
		return 0;

	if (ack->type != p->type)
		return 0;

	if (ack->seq != (((p->seq + 0x01) & 0x33) | 0x80))
		debug("Wrong sequence number in ACK?\n");

	return 1;
}

int packet_extract_channel(packet *p)
{
	uint16_t chan;
	if (p->type!=0x78 || p->len<0x04) {
		debug("channel status: wrong type or too short\n");
		return -1;
	}
	if (p->payload[0]!=cmd_status1 &&
	   p->payload[0]!=cmd_status2) {
		debug("channel status: wrong payload 0x%02x\n",p->payload[0]);
		return -1;
	}
	chan = WORD(p->payload[1],p->payload[2]);
	return chan;
}
