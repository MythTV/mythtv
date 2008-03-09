/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

/* 
 * Process data from the DCT2000 and fill data structures accordingly.
 */ 

#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define MSB(x) ((uint8_t)(((x)>>8)&0xff))
#define LSB(x) ((uint8_t)((x)&0xff))
#define WORD(msb,lsb) ((uint16_t)((((uint8_t)(msb))<<8) | ((uint8_t)(lsb))))

typedef enum {
	id_pc = 0x04,
	id_dct = 0x40
} enum_id;

typedef enum {
	cmd_statusreq = 0x21,
	cmd_status1 = 0x20,
	cmd_status2 = 0xa1,
	cmd_key = 0x22,
	cmd_init = 0x00
} enum_cmd;

/* Payload is fixed length buffer for simplicity; extra data is
   ignored on receipt. */
#define MAX_PAYLOAD 128
typedef struct {
	uint8_t type;
	uint16_t len;
	uint8_t seq;
	uint8_t id;
	uint8_t payload[128];
	uint16_t crc;
} packet;

void packet_stream_init(void);
packet *packet_stream_add(uint8_t c);

void packet_fill_crc(packet *p);
void packet_build_channelquery(packet *p);
void packet_build_initialize_1(packet *p);
void packet_build_initialize_2(packet *p);
void packet_build_ack(packet *in, packet *out);
void packet_build_keypress(packet *p, uint8_t key);
void packet_fill_seq_ack(packet *p);
void packet_fill_seq(packet *p);
void packet_received(packet *p);
void packet_sent(packet *p);

int packet_is_ack(packet *p, packet *ack);
int packet_extract_channel(packet *p);

#endif

