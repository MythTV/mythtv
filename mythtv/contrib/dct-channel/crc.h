/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#ifndef CRC_H
#define CRC_H

#include <stdint.h>

uint16_t makecrc(uint16_t crc, const uint8_t *data, unsigned int len);
uint16_t makecrc_byte(uint16_t crc, uint8_t c);

#endif
