/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "crc.h"

/* Incrementally calculate CRC for DCT-2000 series cable boxes */
/* (pass 0 for initial CRC) */
uint16_t makecrc(uint16_t crc, const uint8_t *data, unsigned int len)
{
	uint8_t d, j;

	for ( ; len-- ; data++ )
		for ( j=0,d=*data ; j<8 ; j++,d>>=1 )
			crc = (crc >> 1) ^ (((crc ^ d) & 1) ? 0x8408 : 0);

	return crc;
}

uint16_t makecrc_byte(uint16_t crc, uint8_t c)
{
	return makecrc(crc,&c,1);
}
